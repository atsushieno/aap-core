package org.androidaudioplugin.aaphostsample

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import androidx.test.core.app.ApplicationProvider
import androidx.test.rule.ServiceTestRule
import org.androidaudioplugin.AudioPluginHostHelper
import org.androidaudioplugin.AudioPluginInterface
import org.junit.Rule
import org.junit.Test

class AudioPluginInterfaceTest {
    @get:Rule
    val serviceRule = ServiceTestRule()

    @Test
    fun basicDirectServiceOperationsMDA() {
        val pluginId = "lv2:http://drobilla.net/plugins/mda/Delay"
        val context = ApplicationProvider.getApplicationContext<Context>()
        val serviceInfos = AudioPluginHostHelper.queryAudioPluginServices(context)
        val serviceInfo = serviceInfos.first { c -> c.label == "MDA-LV2 Plugins" }
        val pluginInfo = serviceInfo.plugins.first { p -> p.pluginId == pluginId}

        val portInfo = pluginInfo.getPort(0);
        assert(portInfo.name == "L Delay");
        assert(portInfo.default == 0.5f);
        assert(portInfo.minimum == 0f);
        assert(portInfo.maximum == 1.0f);

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

        serviceRule.unbindService()
    }

    @Test
    fun basicDirectServiceOperations() {
        val pluginId = "urn:org.androidaudioplugin/samples/aapbarebonepluginsample/FlatFilter"
        val context = ApplicationProvider.getApplicationContext<Context>()
        val serviceInfos = AudioPluginHostHelper.queryAudioPluginServices(context)
        val serviceInfo = serviceInfos.first { c -> c.label == "AAPBareBoneSamplePlugin" }
        val pluginInfo = serviceInfo.plugins.first { p -> p.pluginId == pluginId}

        val portInfo = pluginInfo.getPort(0);
        assert(portInfo.name == "Left In");

        val intent = Intent(AudioPluginHostHelper.AAP_ACTION_NAME)
        intent.component = ComponentName(pluginInfo.packageName, pluginInfo.localName)
        val binder = serviceRule.bindService(intent)
        val iface = AudioPluginInterface.Stub.asInterface(binder)

        val instanceId = iface.beginCreate(pluginId, 44100)
        assert(instanceId >= 0)
        val instanceId2 = iface.beginCreate(pluginId, 44100) // can create multiple times
        assert(instanceId2 >= 0)
        assert(instanceId != instanceId2)

        iface.endCreate(instanceId)
        iface.endCreate(instanceId2)

        iface.destroy(instanceId2)
        iface.destroy(instanceId)

        serviceRule.unbindService()
    }

    @Test
    fun repeatDirectServieOperations() {
        for (i in 0..5)
            basicDirectServiceOperations()
    }
}
