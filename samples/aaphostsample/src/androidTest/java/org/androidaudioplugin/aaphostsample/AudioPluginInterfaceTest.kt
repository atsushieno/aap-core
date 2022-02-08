package org.androidaudioplugin.aaphostsample

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.os.SharedMemory
import androidx.test.core.app.ApplicationProvider
import androidx.test.rule.ServiceTestRule
import org.androidaudioplugin.AudioPluginHostHelper
import org.androidaudioplugin.AudioPluginInterface
import org.junit.Rule
import org.junit.Test

class AudioPluginInterfaceTest {
    private val applicationContext = ApplicationProvider.getApplicationContext<Context>()

    @get:Rule
    val serviceRule = ServiceTestRule()

    // avoid No tests found
    @Test
    fun dummy() {
    }

    // It likely requires shared memory extension, but well, only if we really would like to test it...
    //@Test
    fun basicDirectServiceOperations() {
        val pluginId = "urn:org.androidaudioplugin/samples/aapbarebonepluginsample/FlatFilter"
        val services = AudioPluginHostHelper.queryAudioPluginServices(applicationContext)
        val pluginInfo = services.flatMap { s -> s.plugins.filter { p -> p.pluginId == pluginId} }.first()

        val portInfo = pluginInfo.getPort(0)
        assert(portInfo.name == "Left In")

        val intent = Intent(AudioPluginHostHelper.AAP_ACTION_NAME)
        intent.component = ComponentName(pluginInfo.packageName, pluginInfo.localName)
        val binder = serviceRule.bindService(intent)
        val iface = AudioPluginInterface.Stub.asInterface(binder)

        val frameCount = 4096

        val instanceId = iface.beginCreate(pluginId, 44100)
        assert(instanceId >= 0)
        val instanceId2 = iface.beginCreate(pluginId, 44100) // can create multiple times
        assert(instanceId2 >= 0)
        assert(instanceId != instanceId2)

        iface.endCreate(instanceId)
        iface.endCreate(instanceId2)

        // FIXME: we have to set up shared memory FDs and call prepareMemory(), which is not doable
        //  at this moment in the java stub.

        iface.prepare(instanceId, frameCount, pluginInfo.getPortCount())
        iface.prepare(instanceId2, frameCount, pluginInfo.getPortCount())

        iface.activate(instanceId)
        iface.activate(instanceId2)

        iface.deactivate(instanceId)
        iface.deactivate(instanceId2)

        iface.destroy(instanceId2)
        iface.destroy(instanceId)

        serviceRule.unbindService()
    }
}
