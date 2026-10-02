package android.util
object Log {
    fun e(tag: String, message: String, error: Throwable? = null) = 0
    fun d(tag: String, message: String) = 0
    fun w(tag: String, message: String, error: Throwable? = null) = 0
}
