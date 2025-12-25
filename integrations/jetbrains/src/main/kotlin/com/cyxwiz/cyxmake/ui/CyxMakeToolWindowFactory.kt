package com.cyxwiz.cyxmake.ui

import com.intellij.execution.filters.TextConsoleBuilderFactory
import com.intellij.execution.ui.ConsoleView
import com.intellij.execution.ui.ConsoleViewContentType
import com.intellij.openapi.project.DumbAware
import com.intellij.openapi.project.Project
import com.intellij.openapi.wm.ToolWindow
import com.intellij.openapi.wm.ToolWindowFactory
import com.intellij.openapi.wm.ToolWindowManager
import com.intellij.ui.content.ContentFactory
import java.awt.BorderLayout
import javax.swing.JPanel

class CyxMakeToolWindowFactory : ToolWindowFactory, DumbAware {

    override fun createToolWindowContent(project: Project, toolWindow: ToolWindow) {
        val panel = JPanel(BorderLayout())

        // Create console view
        val consoleBuilder = TextConsoleBuilderFactory.getInstance().createBuilder(project)
        val console = consoleBuilder.console

        panel.add(console.component, BorderLayout.CENTER)

        // Store console reference
        consoleViews[project] = console

        // Create content
        val contentFactory = ContentFactory.getInstance()
        val content = contentFactory.createContent(panel, "Output", false)
        toolWindow.contentManager.addContent(content)

        // Print welcome message
        console.print("[CyxMake] Ready\n", ConsoleViewContentType.SYSTEM_OUTPUT)
    }

    override fun shouldBeAvailable(project: Project): Boolean = true

    companion object {
        private val consoleViews = mutableMapOf<Project, ConsoleView>()

        fun showToolWindow(project: Project) {
            ToolWindowManager.getInstance(project)
                .getToolWindow("CyxMake")?.show()
        }

        fun appendOutput(project: Project, text: String) {
            consoleViews[project]?.print(text, ConsoleViewContentType.NORMAL_OUTPUT)
        }

        fun appendError(project: Project, text: String) {
            consoleViews[project]?.print(text, ConsoleViewContentType.ERROR_OUTPUT)
        }

        fun clear(project: Project) {
            consoleViews[project]?.clear()
        }
    }
}
