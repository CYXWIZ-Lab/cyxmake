package com.cyxwiz.cyxmake.actions

import com.intellij.openapi.actionSystem.AnAction
import com.intellij.openapi.actionSystem.AnActionEvent
import com.cyxwiz.cyxmake.services.CyxMakeProjectService
import com.cyxwiz.cyxmake.ui.CyxMakeToolWindowFactory

/**
 * Build project action
 */
class BuildAction : AnAction() {
    override fun actionPerformed(e: AnActionEvent) {
        val project = e.project ?: return
        val service = CyxMakeProjectService.getInstance(project)

        // Show tool window
        CyxMakeToolWindowFactory.showToolWindow(project)

        service.build(
            onOutput = { text ->
                CyxMakeToolWindowFactory.appendOutput(project, text)
            },
            onComplete = { success, _ ->
                if (success) {
                    CyxMakeToolWindowFactory.appendOutput(project, "\n[CyxMake] Build successful\n")
                } else {
                    CyxMakeToolWindowFactory.appendOutput(project, "\n[CyxMake] Build failed\n")
                }
            }
        )
    }

    override fun update(e: AnActionEvent) {
        val project = e.project
        e.presentation.isEnabled = project != null
    }
}

/**
 * Build Release action
 */
class BuildReleaseAction : AnAction() {
    override fun actionPerformed(e: AnActionEvent) {
        val project = e.project ?: return
        val service = CyxMakeProjectService.getInstance(project)

        CyxMakeToolWindowFactory.showToolWindow(project)
        service.build(
            buildType = "Release",
            onOutput = { CyxMakeToolWindowFactory.appendOutput(project, it) }
        )
    }
}

/**
 * Build Debug action
 */
class BuildDebugAction : AnAction() {
    override fun actionPerformed(e: AnActionEvent) {
        val project = e.project ?: return
        val service = CyxMakeProjectService.getInstance(project)

        CyxMakeToolWindowFactory.showToolWindow(project)
        service.build(
            buildType = "Debug",
            onOutput = { CyxMakeToolWindowFactory.appendOutput(project, it) }
        )
    }
}

/**
 * Clean action
 */
class CleanAction : AnAction() {
    override fun actionPerformed(e: AnActionEvent) {
        val project = e.project ?: return
        val service = CyxMakeProjectService.getInstance(project)

        CyxMakeToolWindowFactory.showToolWindow(project)
        service.clean(
            onOutput = { CyxMakeToolWindowFactory.appendOutput(project, it) }
        )
    }
}

/**
 * Analyze action
 */
class AnalyzeAction : AnAction() {
    override fun actionPerformed(e: AnActionEvent) {
        val project = e.project ?: return
        val service = CyxMakeProjectService.getInstance(project)

        CyxMakeToolWindowFactory.showToolWindow(project)
        service.analyze(
            onOutput = { CyxMakeToolWindowFactory.appendOutput(project, it) }
        )
    }
}

/**
 * Fix errors action
 */
class FixErrorsAction : AnAction() {
    override fun actionPerformed(e: AnActionEvent) {
        val project = e.project ?: return
        val service = CyxMakeProjectService.getInstance(project)

        CyxMakeToolWindowFactory.showToolWindow(project)
        CyxMakeToolWindowFactory.appendOutput(project, "[CyxMake] Analyzing errors...\n")

        service.fixErrors(
            onOutput = { CyxMakeToolWindowFactory.appendOutput(project, it) }
        )
    }
}

/**
 * Open REPL action
 */
class OpenReplAction : AnAction() {
    override fun actionPerformed(e: AnActionEvent) {
        val project = e.project ?: return

        // Open terminal with CyxMake REPL
        com.intellij.openapi.wm.ToolWindowManager.getInstance(project)
            .getToolWindow("Terminal")?.show {
                // Send cyxmake command to terminal
            }
    }
}
