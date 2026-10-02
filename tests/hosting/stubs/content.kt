package android.content

class ComponentName(val packageName: String, val className: String)
class Intent(val action: String) {
    var component: ComponentName? = null
    fun putExtra(name: String, value: Boolean): Intent = this
}
interface ServiceConnection {
    fun onServiceConnected(name: ComponentName?, binder: android.os.IBinder?)
    fun onServiceDisconnected(name: ComponentName?)
    fun onNullBinding(name: ComponentName?)
    fun onBindingDied(name: ComponentName?)
}
abstract class Context {
    companion object { const val BIND_AUTO_CREATE = 1 }
    abstract fun bindService(intent: Intent, connection: ServiceConnection, flags: Int): Boolean
    abstract fun unbindService(connection: ServiceConnection)
    open fun startForegroundService(intent: Intent): ComponentName? = intent.component
}
