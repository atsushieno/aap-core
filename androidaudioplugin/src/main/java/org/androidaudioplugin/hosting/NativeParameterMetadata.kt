package org.androidaudioplugin.hosting

import android.util.Log
import androidx.annotation.Keep
import java.util.concurrent.CompletableFuture

/** Canonical publisher shared by aliases and the higher-level Kotlin instance wrapper. */
internal object NativeParameterMetadata {
    private val entries = mutableMapOf<Pair<Long, Int>, Entry>()

    @Keep
    class Entry(val client: Long, val instanceId: Int) {
        val publisher = ParameterMetadataPublisher { Log.e("AAP", "Parameter metadata listener failed", it) }
        @Volatile private var closed = false
        @Volatile var connectionId = -1
            private set
        @Volatile var packageName = ""
            private set
        @Volatile var className = ""
            private set
        val initialized = CompletableFuture<Unit>()

        fun onServiceIdentity(connectionId: Int, packageName: String, className: String) {
            this.connectionId = connectionId
            this.packageName = packageName
            this.className = className
        }

        fun onNativeMetadataChanged(snapshot: ParameterMetadataSnapshot) {
            if (!closed) publisher.publish(snapshot)
        }

        fun captureInitialMetadata() {
            NativeRemotePluginInstance.readParameterMetadataSnapshot(client, instanceId)?.let(::onNativeMetadataChanged)
        }

        fun invalidate() {
            closed = true
            publisher.close()
        }

        fun onNativeDestroyed() {
            invalidate()
            synchronized(entries) { entries.remove(client to instanceId, this) }
        }
    }

    fun get(client: Long, instanceId: Int): Entry {
        val key = client to instanceId
        val (entry, create) = synchronized(entries) {
            val existing = entries[key]
            if (existing != null) existing to false else Entry(client, instanceId).let {
                entries[key] = it
                it to true
            }
        }
        if (create) {
            try {
                // JNI and native teardown can call back into the registry; do not hold its lock.
                NativeRemotePluginInstance.connectMetadataPublisher(client, instanceId, entry)
                entry.captureInitialMetadata()
                entry.initialized.complete(Unit)
            } catch (error: Throwable) {
                entry.onNativeDestroyed()
                entry.initialized.completeExceptionally(error)
                throw error
            }
        } else {
            entry.initialized.join()
        }
        return entry
    }

    fun invalidateConnection(connectionId: Int, packageName: String, className: String) {
        synchronized(entries) {
            entries.values.filter {
                it.connectionId == connectionId && it.packageName == packageName && it.className == className
            }.forEach { it.invalidate() }
        }
    }

    fun invalidateClient(client: Long) {
        synchronized(entries) {
            entries.values.filter { it.client == client }.forEach { it.invalidate() }
        }
    }
}
