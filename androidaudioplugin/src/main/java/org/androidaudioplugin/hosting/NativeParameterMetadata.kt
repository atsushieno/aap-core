package org.androidaudioplugin.hosting

import android.util.Log
import androidx.annotation.Keep
import java.util.concurrent.CompletableFuture

/** Canonical publisher shared by aliases and the higher-level Kotlin instance wrapper. */
internal object NativeParameterMetadata {
    // Instance IDs come from each plugin service, so the plugin ID is part of the key.
    private data class Key(val client: Long, val pluginId: String, val instanceId: Int)
    private val entries = mutableMapOf<Key, Entry>()

    @Keep
    class Entry(val client: Long, val pluginId: String, val instanceId: Int) {
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
            NativeRemotePluginInstance.readParameterMetadataSnapshot(client, pluginId, instanceId)?.let(::onNativeMetadataChanged)
        }

        fun invalidate() {
            closed = true
            publisher.close()
        }

        fun onNativeDestroyed() {
            invalidate()
            synchronized(entries) { entries.remove(Key(client, pluginId, instanceId), this) }
        }
    }

    fun get(client: Long, pluginId: String, instanceId: Int): Entry {
        val key = Key(client, pluginId, instanceId)
        val (entry, create) = synchronized(entries) {
            val existing = entries[key]
            if (existing != null) existing to false else Entry(client, pluginId, instanceId).let {
                entries[key] = it
                it to true
            }
        }
        if (create) {
            try {
                // JNI and native teardown can call back into the registry; do not hold its lock.
                NativeRemotePluginInstance.connectMetadataPublisher(client, pluginId, instanceId, entry)
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
