package org.androidaudioplugin.hosting

import android.content.*
import android.os.*
import kotlinx.coroutines.*
import org.androidaudioplugin.*
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.ConcurrentLinkedQueue
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executor
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import kotlin.system.exitProcess

// With no executor, LoadedApk.ServiceDispatcher posts every callback to the one main thread.
private val mainThread = Executors.newSingleThreadExecutor { Thread(it, "main").apply { isDaemon = true } }

private class Platform : Context() {
    val attempts = mutableListOf<ServiceConnection>()
    val releases = mutableListOf<ServiceConnection>()
    val executors = ConcurrentHashMap<ServiceConnection, Executor>()
    var onBind: (ServiceConnection) -> Boolean = { true }
    override fun bindService(intent: Intent, connection: ServiceConnection, flags: Int): Boolean =
        bindService(intent, flags, mainThread, connection)
    override fun bindService(intent: Intent, flags: Int, executor: Executor, connection: ServiceConnection): Boolean {
        synchronized(attempts) { attempts.add(connection) }
        executors[connection] = executor
        return onBind(connection)
    }
    // Delivers the connection on the thread the platform would use for this attempt.
    fun connect(connection: ServiceConnection) =
        executors.getValue(connection).execute { connection.onServiceConnected(null, Binder()) }
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
    // A listener runs once the bind has completed, so invalidating/closing there only ends the connection.
    for (close in listOf(false, true)) {
        val p = Platform(); val c = AudioPluginServiceConnector(p)
        c.onConnectedListeners.add {
            if (close) c.close() else c.invalidateServiceConnection(service.packageName, service.className)
        }
        val result = bind(c)
        p.attempts.last().onServiceConnected(null, Binder())
        result.await().getOrThrow(); check(p.releases.size == 1 && c.connectedServices.isEmpty())
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

private suspend fun cancellationTests() = supervisorScope {
    val platform = Platform(); val connector = AudioPluginServiceConnector(platform)
    val pending = bind(connector); val old = platform.attempts.last()
    pending.cancelAndJoin()
    check(platform.releases == listOf(old) && connector.connectedServices.isEmpty())
    val retry = bind(connector); val fresh = platform.attempts.last()
    old.onServiceConnected(null, Binder()); old.onBindingDied(null)
    check(!retry.isCompleted)
    fresh.onServiceConnected(null, Binder()); retry.await().getOrThrow()
    connector.close(); check(platform.releases == listOf(old, fresh))

    // Cancellation during registration must release the connection: nothing can be using it yet.
    run {
        val p = Platform(); val c = AudioPluginServiceConnector(p); val result = bind(c)
        AudioPluginNatives.onAdd = { result.cancel() }
        p.attempts.last().onServiceConnected(null, Binder())
        result.join()
        AudioPluginNatives.onAdd = {}
        check(p.releases.size == 1 && c.connectedServices.isEmpty())
        c.close()
    }
    // Once registered it may be in use by others: cancelling the caller that asked for it before
    // the result is delivered leaves it there, and the next bind gets it without a platform bind.
    run {
        val p = Platform(); val c = AudioPluginServiceConnector(p); val result = bind(c)
        c.onConnectedListeners.add { result.cancel() }
        p.attempts.last().onServiceConnected(null, Binder())
        result.join()
        check(p.releases.isEmpty() && c.connectedServices.size == 1)
        c.onConnectedListeners.clear()
        bind(c).await().getOrThrow()
        check(p.attempts.size == 1)
        c.close()
        check(p.releases.size == 1)
    }
    // Cancellation while bindService is still running defers the release until it returns.
    val p = Platform(); val c = AudioPluginServiceConnector(p)
    val entered = CountDownLatch(1); val leave = CountDownLatch(1)
    p.onBind = { entered.countDown(); check(leave.await(2, TimeUnit.SECONDS)); true }
    val running = async(Dispatchers.Default) { runCatching { c.bindAudioPluginService(service) } }
    check(entered.await(2, TimeUnit.SECONDS))
    running.cancel()
    check(p.releases.isEmpty())
    leave.countDown(); running.join()
    check(p.releases.size == 1 && c.connectedServices.isEmpty())
    c.close()
    println("PASS: cancellation before delivery, during registration, and during platform bind")
}

// The main thread is free to do anything else while a bind is in progress, including wait for it.
private suspend fun mainThreadCallerTest() = supervisorScope {
    val p = Platform(); val c = AudioPluginServiceConnector(p)
    p.onBind = { p.connect(it); true }
    val done = CompletableDeferred<Result<PluginServiceConnection>>()
    mainThread.execute { done.complete(runCatching { runBlocking { c.bindAudioPluginService(service) } }) }
    withTimeout(2_000) { done.await() }.getOrThrow()
    c.close()
}

// A callback may block, even on another bind, and that holds up nothing else: neither the
// callbacks of other connections nor the later callbacks of its own.
private suspend fun nestedBindTest() = supervisorScope {
    val p = Platform(); val c = AudioPluginServiceConnector(p)
    val other = PluginServiceInformation("test.other", "Service")
    val nested = CompletableDeferred<PluginServiceConnection>()
    val leave = CountDownLatch(1)
    c.onConnectedListeners.add {
        if (it.serviceInfo === service) {
            nested.complete(runBlocking { c.bindAudioPluginService(other) })
            leave.await(5, TimeUnit.SECONDS)
        }
    }
    p.onBind = { p.connect(it); true }
    withTimeout(2_000) { bind(c).await() }.getOrThrow()
    check(withTimeout(2_000) { nested.await() }.serviceInfo === other)
    val first = p.attempts.first()
    p.executors.getValue(first).execute { first.onServiceDisconnected(null) }
    withTimeout(2_000) { while (synchronized(p.releases) { first !in p.releases }) delay(10) }
    check(c.connectedServices.single().serviceInfo === other)
    leave.countDown()
    c.close()
}

// The callbacks of one connection may overlap or arrive in any order. Whichever wins, the attempt
// completes and is released once, and nothing stays registered.
private suspend fun overlappingCallbacksTest() = supervisorScope {
    repeat(100) {
        val p = Platform(); val c = AudioPluginServiceConnector(p); val result = bind(c)
        val attempt = p.attempts.last(); val executor = p.executors.getValue(attempt)
        val errors = ConcurrentLinkedQueue<Throwable>(); val finished = CountDownLatch(2)
        listOf<() -> Unit>({ attempt.onServiceConnected(null, Binder()) }, { attempt.onServiceDisconnected(null) })
            .forEach { callback ->
                executor.execute { runCatching(callback).onFailure { errors.add(it) }; finished.countDown() }
            }
        check(finished.await(2, TimeUnit.SECONDS))
        check(errors.isEmpty()) { "Overlapping callbacks failed: $errors" }
        withTimeout(2_000) { result.await() }
        check(p.releases.size == 1 && c.connectedServices.isEmpty())
        c.close()
    }
}

// A consumer failing does not take the connection away from everyone else.
private suspend fun failingListenerTest() = supervisorScope {
    val p = Platform(); val c = AudioPluginServiceConnector(p)
    c.onConnectedListeners.add { throw IllegalStateException("listener failed") }
    val result = bind(c)
    p.attempts.last().onServiceConnected(null, Binder())
    result.await().getOrThrow()
    check(c.connectedServices.size == 1 && p.releases.isEmpty())
    c.close()
}

private suspend fun deadlineTest() = supervisorScope {
    val platform = Platform(); val connector = AudioPluginServiceConnector(platform)
    val started = System.nanoTime()
    val result = bind(connector).await()
    check(result.exceptionOrNull() is AudioPluginException)
    check(result.exceptionOrNull()!!.message!!.contains("Timed out"))
    val elapsed = (System.nanoTime() - started) / 1_000_000
    check(elapsed in 9_000..15_000) { "Unexpected bind deadline: $elapsed ms" }
    val old = platform.attempts.last()
    check(platform.releases == listOf(old))
    val retry = bind(connector)
    old.onServiceConnected(null, Binder()); old.onNullBinding(null)
    check(!retry.isCompleted)
    platform.attempts.last().onServiceConnected(null, Binder())
    retry.await().getOrThrow(); connector.close()
    check(platform.releases.size == 2)
    println("PASS: real 10-second bind deadline, release, and retry after timeout")
}

// A bind waits for the service, not for what its consumers then do with the connection. Timing
// that out dropped a connection in use, and everything on that service with it.
private suspend fun slowConsumerTest() = supervisorScope {
    val p = Platform(); val c = AudioPluginServiceConnector(p)
    val entered = CountDownLatch(1); val leave = CountDownLatch(1)
    c.onConnectedListeners.add { entered.countDown(); leave.await(15, TimeUnit.SECONDS) }
    p.onBind = { p.connect(it); true }
    val connection = withTimeout(2_000) { bind(c).await() }.getOrThrow()
    check(entered.await(2, TimeUnit.SECONDS))
    delay(10_500)
    check(c.connectedServices.single() === connection && p.releases.isEmpty())
    leave.countDown(); c.close()
}

// A caller that could not run until after the deadline still gets the connection it was given.
private suspend fun lateCallerTest() = supervisorScope {
    val p = Platform(); val c = AudioPluginServiceConnector(p)
    p.onBind = { p.connect(it); true }
    val caller = Executors.newSingleThreadExecutor { Thread(it, "caller").apply { isDaemon = true } }
    val leave = CountDownLatch(1)
    val result = async(caller.asCoroutineDispatcher()) { runCatching { c.bindAudioPluginService(service) } }
    caller.execute { leave.await(15, TimeUnit.SECONDS) }
    delay(10_500)
    leave.countDown()
    val connection = withTimeout(2_000) { result.await() }.getOrThrow()
    check(c.connectedServices.single() === connection && p.releases.isEmpty())
    c.close()
}

private val failures = mutableListOf<String>()

private suspend fun scenario(name: String, test: suspend () -> Unit) {
    try {
        test()
        println("PASS: $name")
    } catch (ex: Exception) {
        println("FAIL: $name: $ex")
        failures += name
    }
}

fun main(args: Array<String>) {
    System.load(args[0])
    runBlocking {
        withTimeout(60_000) {
            lifecycleTests()
            cancellationTests()
            scenario("a bind waited for on the main thread") { mainThreadCallerTest() }
            scenario("a blocked callback holds up nothing else") { nestedBindTest() }
            scenario("100 overlapping connect and disconnect callbacks") { overlappingCallbacksTest() }
            scenario("a failing listener keeps the connection") { failingListenerTest() }
            // These wait out the real deadline, so they share the wait.
            coroutineScope {
                launch { deadlineTest() }
                launch { scenario("a consumer outlasting the deadline keeps the connection") { slowConsumerTest() } }
                launch { scenario("a caller delayed past the deadline gets its connection") { lateCallerTest() } }
            }
        }
    }
    if (failures.isNotEmpty())
        exitProcess(1)
}
