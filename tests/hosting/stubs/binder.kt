package android.os
class RemoteException : Exception()
interface IBinder {
    fun interface DeathRecipient { fun binderDied() }
    val isBinderAlive: Boolean
    fun linkToDeath(recipient: DeathRecipient, flags: Int)
    fun unlinkToDeath(recipient: DeathRecipient, flags: Int): Boolean
}
