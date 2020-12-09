package org.androidaudioplugin

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import androidx.test.core.app.ApplicationProvider
import androidx.test.rule.ServiceTestRule
import junit.framework.Assert
import org.junit.Rule
import org.junit.Test

//
// This test assumes that aappluginsample is installed and in-sync.
//
class AudioPluginServiceTest {
    @get:Rule
    val serviceRule = ServiceTestRule()

    @Test
    fun basicDirectServiceOperations() {
        val pluginId = "lv2:http://drobilla.net/plugins/mda/Delay"
        val context = ApplicationProvider.getApplicationContext<Context>()
        val serviceInfos = AudioPluginHostHelper.queryAudioPluginServices(context)
        val serviceInfo = serviceInfos.first { c -> c.label == "AAPLV2SamplePlugin" }
        val pluginInfo = serviceInfo.plugins.first { p -> p.pluginId == pluginId}
        val intent = Intent(AudioPluginHostHelper.AAP_ACTION_NAME)
        intent.component = ComponentName(pluginInfo.packageName, pluginInfo.localName)
        val binder = serviceRule.bindService(intent)
        val iface = AudioPluginInterface.Stub.asInterface(binder)

        val instanceId = iface.beginCreate(pluginId, 44100)
        assert(instanceId >= 0)
        val instanceId2 = iface.beginCreate(pluginId, 44100) // can create multiple times
        assert(instanceId2 >= 0)
        assert(instanceId != instanceId2)

        iface.destroy(instanceId2)
        iface.destroy(instanceId)
    }
}
