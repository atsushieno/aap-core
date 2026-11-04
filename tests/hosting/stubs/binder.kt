package android.os
class RemoteException : Exception()
interface IBinder {
    fun interface DeathRecipient { fun binderDied() }
    val isBinderAlive: Boolean
    fun linkToDeath(recipient: DeathRecipient, flags: Int)
    fun unlinkToDeath(recipient: DeathRecipient, flags: Int): Boolean
}
object Build {
    object VERSION { const val SDK_INT = 34 }
    object VERSION_CODES { const val UPSIDE_DOWN_CAKE = 34 }
}
