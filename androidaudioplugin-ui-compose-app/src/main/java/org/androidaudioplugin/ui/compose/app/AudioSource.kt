package org.androidaudioplugin.ui.compose.app

import android.content.Context
import android.content.res.AssetFileDescriptor
import java.io.ByteArrayOutputStream
import java.io.DataInputStream
import java.io.IOException
import java.io.InputStream
import org.androidaudioplugin.manager.PluginPlayer
import org.androidaudioplugin.soundpack.SoundpackIdentity
import org.androidaudioplugin.soundpack.SoundpackKind
import org.androidaudioplugin.soundpack.client.SoundpackClient

/** The audio that PluginDetails plays through the plugin on "Play Audio" (aap-core#165): the
 * bundled sample, or an "AAP Sample" that a soundpack provider serves, such as a folder the user
 * shared as "AAP Sample" in the Soundpack Manager. */
sealed class AudioSource {
    /** Shown to the user. */
    abstract val displayName: String

    /** The name the player decodes the data by: its suffix names the format. */
    internal abstract val fileName: String

    internal abstract fun readBytes(context: Context): ByteArray

    /** The sample that comes with androidaudioplugin-manager. */
    data object Bundled : AudioSource() {
        override val displayName = "Default sample"
        override val fileName = PluginPlayer.sample_audio_filename
        override fun readBytes(context: Context) = context.assets.open(fileName).use { it.readBytes() }
    }

    /** An entry of the [SoundpackKind.HOST_SAMPLE] kind. */
    data class Soundpack(
        val identity: SoundpackIdentity,
        override val displayName: String,
        /** Where it is: the pack (a shared folder, for example), and the folder within it. */
        val location: String,
    ) : AudioSource() {
        override val fileName get() = identity.entryPath

        override fun readBytes(context: Context): ByteArray {
            return try {
                SoundpackClient(context, SoundpackKind.HOST_SAMPLE).open(identity).use { session ->
                    session.openEntry()
                }
            } catch (e: Exception) {
                // The provider is gone, or no longer serves it (a removed file, an unshared
                // folder); what failed is in the cause.
                throw IOException("it is no longer available", e)
            }.use { file -> file.readAll() }
        }

        /** Whether both are the same entry, wherever it was resolved from. */
        fun isSameEntry(other: AudioSource) =
            other is Soundpack && other.identity.packId == identity.packId && other.identity.entryPath == identity.entryPath

        private fun AssetFileDescriptor.readAll(): ByteArray {
            if (length > MAX_FILE_BYTES)
                throw IOException(tooLarge(length))
            return createInputStream().use { input ->
                if (length >= 0)
                    ByteArray(length.toInt()).also { DataInputStream(input).readFully(it) }
                else
                    input.readBytesAtMost(MAX_FILE_BYTES) ?: throw IOException(tooLarge(null))
            }
        }

        private fun tooLarge(length: Long?) =
            "it is too large" + (if (length != null) " (${length shr 20} MB)" else "") +
                    "; the limit is ${MAX_FILE_BYTES shr 20} MB"

        companion object {
            // The whole file is read into memory and then decoded, so it is capped.
            private const val MAX_FILE_BYTES = 64 shl 20

            internal fun of(client: SoundpackClient, entry: SoundpackClient.Entry) = Soundpack(
                client.identityOf(entry),
                entry.path.substringAfterLast('/'),
                listOf(entry.pack.label, entry.groupPath).filter { it.isNotEmpty() }.joinToString(" / "))
        }
    }
}

/** Remembers the user's audio source per app, for the next time PluginDetails opens. */
internal object AudioSourcePreferences {
    private const val NAME = "org.androidaudioplugin.ui.compose.app.AudioSource"
    private const val KEY_IDENTITY = "identity"
    private const val KEY_DISPLAY_NAME = "displayName"
    private const val KEY_LOCATION = "location"

    fun load(context: Context): AudioSource {
        val preferences = context.getSharedPreferences(NAME, Context.MODE_PRIVATE)
        val identity = preferences.getString(KEY_IDENTITY, null)?.let(SoundpackIdentity::parse)
            ?: return AudioSource.Bundled
        return AudioSource.Soundpack(identity,
            preferences.getString(KEY_DISPLAY_NAME, null) ?: identity.entryPath.substringAfterLast('/'),
            preferences.getString(KEY_LOCATION, null) ?: "")
    }

    fun save(context: Context, source: AudioSource) {
        val editor = context.getSharedPreferences(NAME, Context.MODE_PRIVATE).edit().clear()
        if (source is AudioSource.Soundpack)
            editor.putString(KEY_IDENTITY, source.identity.format())
                .putString(KEY_DISPLAY_NAME, source.displayName)
                .putString(KEY_LOCATION, source.location)
        editor.apply()
    }
}

private fun InputStream.readBytesAtMost(limit: Int): ByteArray? {
    val output = ByteArrayOutputStream()
    val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
    while (true) {
        val size = read(buffer)
        if (size < 0)
            return output.toByteArray()
        if (output.size() + size > limit)
            return null
        output.write(buffer, 0, size)
    }
}
