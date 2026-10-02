package org.androidaudioplugin
import android.os.IBinder

class AudioPluginException(message: String, cause: Throwable? = null) : Exception(message, cause)
class PluginServiceInformation(val packageName: String, val className: String)
object AudioPluginService { const val EXTRA_REQUEST_FOREGROUND = "foreground" }
object AudioPluginNatives {
    var registrations = 0
    var removals = 0
    var onAdd: () -> Unit = {}
    fun addBinderForClient(scope: Int, packageName: String, className: String, binder: IBinder) {
        onAdd()
        registrations++
    }
    fun removeBinderForClient(scope: Int, packageName: String, className: String) { removals++ }
}
