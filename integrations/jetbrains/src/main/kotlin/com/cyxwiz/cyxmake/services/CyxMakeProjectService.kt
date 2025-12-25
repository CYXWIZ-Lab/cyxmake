package com.cyxwiz.cyxmake.services

import com.intellij.execution.configurations.GeneralCommandLine
import com.intellij.execution.process.OSProcessHandler
import com.intellij.execution.process.ProcessAdapter
import com.intellij.execution.process.ProcessEvent
import com.intellij.notification.NotificationGroupManager
import com.intellij.notification.NotificationType
import com.intellij.openapi.components.Service
import com.intellij.openapi.project.Project
import com.intellij.openapi.util.Key
import com.cyxwiz.cyxmake.settings.CyxMakeSettings
import java.io.File

@Service(Service.Level.PROJECT)
class CyxMakeProjectService(private val project: Project) {

    enum class BuildState {
        IDLE, BUILDING, SUCCESS, FAILED
    }

    var state: BuildState = BuildState.IDLE
        private set

    private val settings get() = CyxMakeSettings.instance

    /**
     * Run CyxMake command
     */
    fun runCommand(
        command: String,
        args: List<String> = emptyList(),
        onOutput: (String) -> Unit = {},
        onComplete: (Boolean, Int) -> Unit = { _, _ -> }
    ) {
        if (state == BuildState.BUILDING) {
            notify("Build already in progress", NotificationType.WARNING)
            return
        }

        val projectPath = project.basePath ?: return
        val cyxmakePath = settings.executablePath

        // Verify CyxMake exists
        if (!File(cyxmakePath).exists() && !isInPath(cyxmakePath)) {
            notify("CyxMake not found at: $cyxmakePath", NotificationType.ERROR)
            return
        }

        state = BuildState.BUILDING

        val commandLine = GeneralCommandLine()
            .withExePath(cyxmakePath)
            .withParameters(command)
            .withParameters(args)
            .withWorkDirectory(projectPath)
            .withCharset(Charsets.UTF_8)

        try {
            val processHandler = OSProcessHandler(commandLine)

            processHandler.addProcessListener(object : ProcessAdapter() {
                override fun onTextAvailable(event: ProcessEvent, outputType: Key<*>) {
                    onOutput(event.text)
                }

                override fun processTerminated(event: ProcessEvent) {
                    val exitCode = event.exitCode
                    state = if (exitCode == 0) BuildState.SUCCESS else BuildState.FAILED
                    onComplete(exitCode == 0, exitCode)

                    if (exitCode == 0) {
                        notify("Build successful", NotificationType.INFORMATION)
                    } else {
                        notify("Build failed with code $exitCode", NotificationType.ERROR)
                    }
                }
            })

            processHandler.startNotify()
        } catch (e: Exception) {
            state = BuildState.FAILED
            notify("Failed to start CyxMake: ${e.message}", NotificationType.ERROR)
            onComplete(false, -1)
        }
    }

    /**
     * Build project
     */
    fun build(
        buildType: String = settings.defaultBuildType,
        onOutput: (String) -> Unit = {},
        onComplete: (Boolean, Int) -> Unit = { _, _ -> }
    ) {
        runCommand("build", listOf("--type", buildType), onOutput, onComplete)
    }

    /**
     * Clean build
     */
    fun clean(
        onOutput: (String) -> Unit = {},
        onComplete: (Boolean, Int) -> Unit = { _, _ -> }
    ) {
        runCommand("clean", emptyList(), onOutput, onComplete)
    }

    /**
     * Analyze project
     */
    fun analyze(
        onOutput: (String) -> Unit = {},
        onComplete: (Boolean, Int) -> Unit = { _, _ -> }
    ) {
        runCommand("init", emptyList(), onOutput, onComplete)
    }

    /**
     * Fix errors
     */
    fun fixErrors(
        onOutput: (String) -> Unit = {},
        onComplete: (Boolean, Int) -> Unit = { _, _ -> }
    ) {
        runCommand("fix", emptyList(), onOutput, onComplete)
    }

    private fun notify(message: String, type: NotificationType) {
        NotificationGroupManager.getInstance()
            .getNotificationGroup("CyxMake Notifications")
            .createNotification("CyxMake", message, type)
            .notify(project)
    }

    private fun isInPath(command: String): Boolean {
        val pathEnv = System.getenv("PATH") ?: return false
        val pathDirs = pathEnv.split(File.pathSeparator)
        return pathDirs.any { dir ->
            File(dir, command).exists() ||
            File(dir, "$command.exe").exists()
        }
    }

    companion object {
        fun getInstance(project: Project): CyxMakeProjectService =
            project.getService(CyxMakeProjectService::class.java)
    }
}
