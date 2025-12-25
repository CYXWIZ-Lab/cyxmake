package com.cyxwiz.cyxmake.settings

import com.intellij.openapi.application.ApplicationManager
import com.intellij.openapi.components.PersistentStateComponent
import com.intellij.openapi.components.State
import com.intellij.openapi.components.Storage
import com.intellij.util.xmlb.XmlSerializerUtil

@State(
    name = "CyxMakeSettings",
    storages = [Storage("cyxmake.xml")]
)
class CyxMakeSettings : PersistentStateComponent<CyxMakeSettings> {

    var executablePath: String = "cyxmake"
    var defaultBuildType: String = "Debug"
    var parallelJobs: Int = 0
    var autoAnalyze: Boolean = true

    // AI settings
    var aiEnabled: Boolean = false
    var aiProvider: String = "none"
    var aiAutoFix: Boolean = false

    // UI settings
    var showToolWindow: Boolean = true
    var showNotifications: Boolean = true

    override fun getState(): CyxMakeSettings = this

    override fun loadState(state: CyxMakeSettings) {
        XmlSerializerUtil.copyBean(state, this)
    }

    companion object {
        val instance: CyxMakeSettings
            get() = ApplicationManager.getApplication().getService(CyxMakeSettings::class.java)
    }
}
