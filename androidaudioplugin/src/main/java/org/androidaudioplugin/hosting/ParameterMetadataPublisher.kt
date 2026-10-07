package org.androidaudioplugin.hosting

import java.util.concurrent.Executor

/** Serializes publication, replay and cancellation; user code never runs under the publisher lock. */
internal class ParameterMetadataPublisher(private val reportError: (Throwable) -> Unit = {}) : AutoCloseable {
    private class Subscription(val executor: Executor, val listener: ParameterMetadataChangedListener) {
        var active = true
        var scheduled = false
        var pending: ParameterMetadataSnapshot? = null
    }

    private val lock = Any()
    private val subscriptions = mutableSetOf<Subscription>()
    private var closed = false
    @Volatile var latest: ParameterMetadataSnapshot? = null
        private set

    fun publish(snapshot: ParameterMetadataSnapshot) {
        val schedule = synchronized(lock) {
            if (closed || snapshot.revision <= (latest?.revision ?: -1L)) return
            latest = snapshot
            subscriptions.mapNotNull { subscription ->
                subscription.pending = snapshot
                if (subscription.scheduled) null else {
                    subscription.scheduled = true
                    subscription
                }
            }
        }
        schedule.forEach(::schedule)
    }

    fun add(executor: Executor, replayCurrent: Boolean, listener: ParameterMetadataChangedListener): AutoCloseable {
        val subscription = Subscription(executor, listener)
        val replay = synchronized(lock) {
            check(!closed) { "Plugin instance is closed" }
            subscriptions.add(subscription)
            subscription.pending = if (replayCurrent) latest else null
            subscription.scheduled = subscription.pending != null
            subscription.scheduled
        }
        if (replay) schedule(subscription)
        return AutoCloseable {
            synchronized(lock) {
                subscription.active = false
                subscription.pending = null
                subscriptions.remove(subscription)
            }
        }
    }

    private fun schedule(subscription: Subscription) {
        try {
            subscription.executor.execute { deliver(subscription) }
        } catch (error: Exception) {
            synchronized(lock) { subscription.scheduled = false }
            reportError(error)
        }
    }

    private fun deliver(subscription: Subscription) {
        while (true) {
            val snapshot = synchronized(lock) {
                if (closed || !subscription.active) {
                    subscription.scheduled = false
                    return
                }
                val pending = subscription.pending
                subscription.pending = null
                if (pending == null) subscription.scheduled = false
                pending
            } ?: return
            try {
                subscription.listener.onParameterMetadataChanged(snapshot)
            } catch (error: Exception) {
                reportError(error)
            }
        }
    }

    override fun close() {
        synchronized(lock) {
            closed = true
            subscriptions.forEach {
                it.active = false
                it.pending = null
            }
            subscriptions.clear()
        }
    }
}
