/**
 * CyxMake VS Code Extension
 * AI-powered build automation
 */

import * as vscode from 'vscode';
import * as cp from 'child_process';
import * as path from 'path';

// Extension state
let outputChannel: vscode.OutputChannel;
let statusBarItem: vscode.StatusBarItem;
let diagnosticCollection: vscode.DiagnosticCollection;
let isBuilding = false;

/**
 * Extension activation
 */
export function activate(context: vscode.ExtensionContext) {
    console.log('CyxMake extension activated');

    // Create output channel
    outputChannel = vscode.window.createOutputChannel('CyxMake');

    // Create status bar item
    statusBarItem = vscode.window.createStatusBarItem(
        vscode.StatusBarAlignment.Left,
        100
    );
    statusBarItem.command = 'cyxmake.showStatus';
    updateStatusBar('ready');

    // Create diagnostic collection
    diagnosticCollection = vscode.languages.createDiagnosticCollection('cyxmake');

    // Register commands
    const commands = [
        vscode.commands.registerCommand('cyxmake.build', () => runCyxMake('build')),
        vscode.commands.registerCommand('cyxmake.buildRelease', () => runCyxMake('build', 'Release')),
        vscode.commands.registerCommand('cyxmake.buildDebug', () => runCyxMake('build', 'Debug')),
        vscode.commands.registerCommand('cyxmake.clean', () => runCyxMake('clean')),
        vscode.commands.registerCommand('cyxmake.analyze', () => runCyxMake('init')),
        vscode.commands.registerCommand('cyxmake.fixErrors', fixBuildErrors),
        vscode.commands.registerCommand('cyxmake.openRepl', openRepl),
        vscode.commands.registerCommand('cyxmake.configure', openConfiguration),
        vscode.commands.registerCommand('cyxmake.showStatus', showStatus),
    ];

    commands.forEach(cmd => context.subscriptions.push(cmd));

    // Register task provider
    context.subscriptions.push(
        vscode.tasks.registerTaskProvider('cyxmake', new CyxMakeTaskProvider())
    );

    // Add to context subscriptions
    context.subscriptions.push(outputChannel);
    context.subscriptions.push(statusBarItem);
    context.subscriptions.push(diagnosticCollection);

    // Show status bar if enabled
    const config = vscode.workspace.getConfiguration('cyxmake');
    if (config.get('showStatusBar')) {
        statusBarItem.show();
    }

    // Auto-analyze on workspace open
    if (config.get('autoAnalyze')) {
        runCyxMake('init', undefined, true);
    }

    // Set context for when clauses
    vscode.commands.executeCommand('setContext', 'cyxmake.enabled', true);
}

/**
 * Extension deactivation
 */
export function deactivate() {
    console.log('CyxMake extension deactivated');
}

/**
 * Run CyxMake command
 */
async function runCyxMake(
    command: string,
    buildType?: string,
    silent: boolean = false
): Promise<boolean> {
    if (isBuilding) {
        vscode.window.showWarningMessage('CyxMake: Build already in progress');
        return false;
    }

    const workspaceFolder = vscode.workspace.workspaceFolders?.[0];
    if (!workspaceFolder) {
        vscode.window.showErrorMessage('CyxMake: No workspace folder open');
        return false;
    }

    const config = vscode.workspace.getConfiguration('cyxmake');
    const cyxmakePath = config.get<string>('executablePath') || 'cyxmake';
    const defaultBuildType = buildType || config.get<string>('defaultBuildType') || 'Debug';

    // Build command arguments
    const args: string[] = [command];
    if (command === 'build' && buildType) {
        args.push('--type', buildType);
    }

    isBuilding = true;
    updateStatusBar('building');

    if (!silent) {
        outputChannel.clear();
        outputChannel.show(true);
        outputChannel.appendLine(`[CyxMake] Running: ${cyxmakePath} ${args.join(' ')}`);
        outputChannel.appendLine(`[CyxMake] Working directory: ${workspaceFolder.uri.fsPath}`);
        outputChannel.appendLine('');
    }

    return new Promise((resolve) => {
        const process = cp.spawn(cyxmakePath, args, {
            cwd: workspaceFolder.uri.fsPath,
            shell: true
        });

        let stdout = '';
        let stderr = '';

        process.stdout?.on('data', (data) => {
            const text = data.toString();
            stdout += text;
            if (!silent) {
                outputChannel.append(text);
            }
        });

        process.stderr?.on('data', (data) => {
            const text = data.toString();
            stderr += text;
            if (!silent) {
                outputChannel.append(text);
            }
        });

        process.on('close', (code) => {
            isBuilding = false;

            if (code === 0) {
                updateStatusBar('success');
                if (!silent) {
                    outputChannel.appendLine('');
                    outputChannel.appendLine('[CyxMake] Build successful');
                    vscode.window.showInformationMessage('CyxMake: Build successful');
                }
                diagnosticCollection.clear();
                vscode.commands.executeCommand('setContext', 'cyxmake.hasErrors', false);
                resolve(true);
            } else {
                updateStatusBar('error');
                if (!silent) {
                    outputChannel.appendLine('');
                    outputChannel.appendLine(`[CyxMake] Build failed with code ${code}`);
                    vscode.window.showErrorMessage('CyxMake: Build failed');
                }
                parseErrors(stdout + stderr, workspaceFolder.uri.fsPath);
                vscode.commands.executeCommand('setContext', 'cyxmake.hasErrors', true);
                resolve(false);
            }
        });

        process.on('error', (err) => {
            isBuilding = false;
            updateStatusBar('error');
            outputChannel.appendLine(`[CyxMake] Error: ${err.message}`);
            vscode.window.showErrorMessage(`CyxMake: ${err.message}`);
            resolve(false);
        });
    });
}

/**
 * Parse build errors and create diagnostics
 */
function parseErrors(output: string, workspacePath: string) {
    const config = vscode.workspace.getConfiguration('cyxmake');
    if (!config.get('diagnostics.enabled')) {
        return;
    }

    const showWarnings = config.get('diagnostics.showWarnings');
    const diagnosticsMap = new Map<string, vscode.Diagnostic[]>();

    // GCC/Clang error pattern: file:line:column: error/warning: message
    const errorPattern = /^(.+?):(\d+):(\d+):\s+(error|warning):\s+(.+)$/gm;

    let match;
    while ((match = errorPattern.exec(output)) !== null) {
        const [, file, lineStr, colStr, severity, message] = match;
        const line = parseInt(lineStr, 10) - 1;
        const col = parseInt(colStr, 10) - 1;

        if (severity === 'warning' && !showWarnings) {
            continue;
        }

        const filePath = path.isAbsolute(file) ? file : path.join(workspacePath, file);
        const range = new vscode.Range(line, col, line, col + 1);
        const diagnostic = new vscode.Diagnostic(
            range,
            message,
            severity === 'error'
                ? vscode.DiagnosticSeverity.Error
                : vscode.DiagnosticSeverity.Warning
        );
        diagnostic.source = 'CyxMake';

        if (!diagnosticsMap.has(filePath)) {
            diagnosticsMap.set(filePath, []);
        }
        diagnosticsMap.get(filePath)!.push(diagnostic);
    }

    // Apply diagnostics
    diagnosticCollection.clear();
    for (const [filePath, diagnostics] of diagnosticsMap) {
        const uri = vscode.Uri.file(filePath);
        diagnosticCollection.set(uri, diagnostics);
    }
}

/**
 * Fix build errors using AI
 */
async function fixBuildErrors() {
    const config = vscode.workspace.getConfiguration('cyxmake');

    if (!config.get('ai.enabled')) {
        const enable = await vscode.window.showWarningMessage(
            'AI features are not enabled. Enable them?',
            'Yes', 'No'
        );
        if (enable === 'Yes') {
            await vscode.commands.executeCommand(
                'workbench.action.openSettings',
                'cyxmake.ai'
            );
        }
        return;
    }

    outputChannel.show(true);
    outputChannel.appendLine('[CyxMake] Analyzing errors and generating fixes...');

    // Run CyxMake with fix command
    const workspaceFolder = vscode.workspace.workspaceFolders?.[0];
    if (!workspaceFolder) {
        return;
    }

    const cyxmakePath = config.get<string>('executablePath') || 'cyxmake';

    const process = cp.spawn(cyxmakePath, ['fix'], {
        cwd: workspaceFolder.uri.fsPath,
        shell: true
    });

    process.stdout?.on('data', (data) => {
        outputChannel.append(data.toString());
    });

    process.stderr?.on('data', (data) => {
        outputChannel.append(data.toString());
    });

    process.on('close', (code) => {
        if (code === 0) {
            vscode.window.showInformationMessage('CyxMake: Fixes applied');
            // Rebuild to verify
            runCyxMake('build');
        } else {
            vscode.window.showWarningMessage('CyxMake: Some fixes could not be applied');
        }
    });
}

/**
 * Open CyxMake REPL in terminal
 */
function openRepl() {
    const config = vscode.workspace.getConfiguration('cyxmake');
    const cyxmakePath = config.get<string>('executablePath') || 'cyxmake';

    const terminal = vscode.window.createTerminal({
        name: 'CyxMake REPL',
        cwd: vscode.workspace.workspaceFolders?.[0]?.uri.fsPath
    });

    terminal.show();
    terminal.sendText(cyxmakePath);
}

/**
 * Open configuration
 */
function openConfiguration() {
    vscode.commands.executeCommand('workbench.action.openSettings', 'cyxmake');
}

/**
 * Show status
 */
async function showStatus() {
    const workspaceFolder = vscode.workspace.workspaceFolders?.[0];
    if (!workspaceFolder) {
        vscode.window.showInformationMessage('CyxMake: No workspace open');
        return;
    }

    outputChannel.show(true);
    runCyxMake('status', undefined, false);
}

/**
 * Update status bar
 */
function updateStatusBar(state: 'ready' | 'building' | 'success' | 'error') {
    switch (state) {
        case 'ready':
            statusBarItem.text = '$(tools) CyxMake';
            statusBarItem.tooltip = 'CyxMake: Ready';
            statusBarItem.backgroundColor = undefined;
            break;
        case 'building':
            statusBarItem.text = '$(sync~spin) CyxMake: Building...';
            statusBarItem.tooltip = 'CyxMake: Building...';
            statusBarItem.backgroundColor = undefined;
            break;
        case 'success':
            statusBarItem.text = '$(check) CyxMake';
            statusBarItem.tooltip = 'CyxMake: Build successful';
            statusBarItem.backgroundColor = undefined;
            break;
        case 'error':
            statusBarItem.text = '$(error) CyxMake';
            statusBarItem.tooltip = 'CyxMake: Build failed';
            statusBarItem.backgroundColor = new vscode.ThemeColor(
                'statusBarItem.errorBackground'
            );
            break;
    }
}

/**
 * CyxMake Task Provider
 */
class CyxMakeTaskProvider implements vscode.TaskProvider {
    provideTasks(): vscode.ProviderResult<vscode.Task[]> {
        const tasks: vscode.Task[] = [];

        const commands = [
            { command: 'build', label: 'Build' },
            { command: 'build --type Debug', label: 'Build Debug' },
            { command: 'build --type Release', label: 'Build Release' },
            { command: 'clean', label: 'Clean' },
            { command: 'init', label: 'Analyze' },
        ];

        for (const cmd of commands) {
            const task = this.createTask(cmd.command, cmd.label);
            tasks.push(task);
        }

        return tasks;
    }

    resolveTask(task: vscode.Task): vscode.ProviderResult<vscode.Task> {
        const command = task.definition.command;
        if (command) {
            return this.createTask(command, task.name);
        }
        return undefined;
    }

    private createTask(command: string, label: string): vscode.Task {
        const config = vscode.workspace.getConfiguration('cyxmake');
        const cyxmakePath = config.get<string>('executablePath') || 'cyxmake';

        const definition: vscode.TaskDefinition = {
            type: 'cyxmake',
            command
        };

        const execution = new vscode.ShellExecution(`${cyxmakePath} ${command}`);

        const task = new vscode.Task(
            definition,
            vscode.TaskScope.Workspace,
            label,
            'cyxmake',
            execution,
            '$cyxmake'
        );

        task.group = vscode.TaskGroup.Build;
        task.presentationOptions = {
            reveal: vscode.TaskRevealKind.Always,
            panel: vscode.TaskPanelKind.Shared
        };

        return task;
    }
}
