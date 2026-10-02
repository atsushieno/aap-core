package org.androidaudioplugin.hosting

import android.content.*
import android.os.*
import kotlinx.coroutines.*
import org.androidaudioplugin.*
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

private class Platform : Context() {
    val attempts = mutableListOf<ServiceConnection>()
    val releases = mutableListOf<ServiceConnection>()
    var onBind: (ServiceConnection) -> Boolean = { true }
    override fun bindService(intent: Intent, connection: ServiceConnection, flags: Int): Boolean {
        synchronized(attempts) { attempts.add(connection) }
        return onBind(connection)
    }
    override fun unbindService(connection: ServiceConnection) {
        synchronized(releases) {
            check(connection !in releases) { "Binding released twice" }
            releases.add(connection)
        }
    }
}
private class Binder : IBinder {
    override var isBinderAlive = true
    private val recipients = mutableSetOf<IBinder.DeathRecipient>()
    override fun linkToDeath(recipient: IBinder.DeathRecipient, flags: Int) {
        if (!isBinderAlive) throw RemoteException()
        recipients.add(recipient)
    }
    override fun unlinkToDeath(recipient: IBinder.DeathRecipient, flags: Int) = recipients.remove(recipient)
    fun die() {
        isBinderAlive = false
        recipients.toList().forEach { it.binderDied() }
    }
}
private val service = PluginServiceInformation("test.plugin", "Service")
private fun CoroutineScope.bind(connector: AudioPluginServiceConnector) = async(start = CoroutineStart.UNDISPATCHED) {
    runCatching { connector.bindAudioPluginService(service) }
}
private suspend fun Deferred<Result<PluginServiceConnection>>.failure() {
    check(withTimeout(2_000) { await() }.exceptionOrNull() is AudioPluginException)
}

private suspend fun lifecycleTests() = supervisorScope {
    // Callback rejection, duplicate/late callbacks, and retry with a stale previous attempt.
    for (terminal in listOf<(ServiceConnection) -> Unit>(
        { it.onNullBinding(null) }, { it.onBindingDied(null) }, { it.onServiceDisconnected(null) },
        { it.onServiceConnected(null, null) }
    )) {
        val platform = Platform(); val connector = AudioPluginServiceConnector(platform)
        val first = bind(connector); val old = platform.attempts.last()
        terminal(old); first.failure()
        check(platform.releases == listOf(old))
        val second = bind(connector); val fresh = platform.attempts.last()
        terminal(old); old.onServiceConnected(null, Binder())
        check(!second.isCompleted && platform.releases == listOf(old))
        fresh.onServiceConnected(null, Binder())
        val connection = second.await().getOrThrow()
        check(connection.platformServiceConnection === fresh)
        // Framework reconnection/repeated notifications cannot resume a completed continuation.
        fresh.onServiceConnected(null, Binder())
        check(connector.connectedServices.single() === connection)
        terminal(old)
        check(connector.connectedServices.single() === connection)
        connector.close(); connector.close()
        check(platform.releases == listOf(old, fresh) && connector.connectedServices.isEmpty())
    }
    for (callback in listOf<(ServiceConnection) -> Unit>(
        { it.onNullBinding(null) }, { it.onBindingDied(null) }
    )) {
        val platform = Platform(); val connector = AudioPluginServiceConnector(platform)
        platform.onBind = { callback(it); check(platform.releases.isEmpty()); true }
        bind(connector).failure()
        check(platform.releases.size == 1)
        connector.close()
    }
    for (mode in listOf("false", "throw", "close")) {
        val platform = Platform(); val connector = AudioPluginServiceConnector(platform)
        platform.onBind = {
            when (mode) {
                "false" -> false
                "throw" -> throw SecurityException("denied")
                else -> { connector.close(); check(platform.releases.isEmpty()); true }
            }
        }
        val result = bind(connector).await()
        check(result.isFailure)
        check(platform.releases.size == 1 && connector.connectedServices.isEmpty())
        if (mode != "close") {
            platform.onBind = { true }
            val retry = bind(connector)
            platform.attempts.last().onNullBinding(null); retry.failure()
        }
        connector.close()
    }
    // Close rejects pending binds and rejects future calls without a platform bind.
    val platform = Platform(); val connector = AudioPluginServiceConnector(platform)
    val pending = bind(connector); connector.close(); pending.failure()
    bind(connector).failure()
    check(platform.attempts.size == 1 && platform.releases.size == 1)

    // Registration failure and a Binder that is already dead cannot leave a pending slot.
    for (dead in listOf(false, true)) {
        val p = Platform(); val c = AudioPluginServiceConnector(p)
        AudioPluginNatives.onAdd = { if (!dead) throw AudioPluginException("native registration failed") }
        val result = bind(c)
        p.attempts.last().onServiceConnected(null, Binder().also { it.isBinderAlive = !dead })
        result.failure(); check(p.releases.size == 1 && c.connectedServices.isEmpty())
        AudioPluginNatives.onAdd = {}
        val retry = bind(c); val binder = Binder()
        p.attempts.last().onServiceConnected(null, binder)
        retry.await().getOrThrow(); binder.die()
        check(p.releases.size == 2 && c.connectedServices.isEmpty())
        c.close()
    }
    // A listener can invalidate/close the connection before its bind completion is delivered.
    for (close in listOf(false, true)) {
        val p = Platform(); val c = AudioPluginServiceConnector(p)
        c.onConnectedListeners.add {
            if (close) c.close() else c.invalidateServiceConnection(service.packageName, service.className)
        }
        val result = bind(c)
        p.attempts.last().onServiceConnected(null, Binder())
        result.failure(); check(p.releases.size == 1 && c.connectedServices.isEmpty())
        if (!close) {
            c.onConnectedListeners.clear()
            val retry = bind(c)
            p.attempts.last().onServiceConnected(null, Binder()); retry.await().getOrThrow()
        }
        c.close()
    }
    // Closure during native registration must reject publication after the call returns.
    run {
        val p = Platform(); val c = AudioPluginServiceConnector(p); val result = bind(c)
        AudioPluginNatives.onAdd = { c.close() }
        p.attempts.last().onServiceConnected(null, Binder())
        AudioPluginNatives.onAdd = {}
        result.failure(); check(p.releases.size == 1 && c.connectedServices.isEmpty())
    }
    // Concurrent terminal callbacks claim cleanup and completion once.
    repeat(100) {
        val p = Platform(); val c = AudioPluginServiceConnector(p); val result = bind(c)
        val connection = p.attempts.last()
        val errors = java.util.concurrent.ConcurrentLinkedQueue<Throwable>()
        val threads = listOf(Thread { runCatching { connection.onNullBinding(null) }.onFailure { errors.add(it) } },
                             Thread { runCatching { c.close() }.onFailure { errors.add(it) } })
        threads.forEach { it.start() }; threads.forEach { it.join() }
        check(errors.isEmpty()); result.failure(); check(p.releases.size == 1)
    }
    println("PASS: binding terminal events, reentrant callbacks, retry identity, closure, failure, and 100 races")
}

fun main(args: Array<String>) {
    System.load(args[0])
    runBlocking { withTimeout(5_000) { lifecycleTests() } }
}
