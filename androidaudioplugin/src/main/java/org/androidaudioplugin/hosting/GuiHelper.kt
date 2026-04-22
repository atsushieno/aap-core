package org.androidaudioplugin.hosting

import android.content.Context
import android.os.Build
import android.view.View
import androidx.annotation.RequiresApi

object GuiHelper {
    data class Size(
        val width: Int,
        val height: Int
    )

    data class ViewportConfiguration(
        val viewportWidth: Int,
        val viewportHeight: Int,
        val contentWidth: Int,
        val contentHeight: Int,
        val scrollX: Int,
        val scrollY: Int
    ) {
        fun clamp(): ViewportConfiguration {
            val clampedViewportWidth = viewportWidth.coerceAtLeast(0)
            val clampedViewportHeight = viewportHeight.coerceAtLeast(0)
            val clampedContentWidth = contentWidth.coerceAtLeast(clampedViewportWidth)
            val clampedContentHeight = contentHeight.coerceAtLeast(clampedViewportHeight)
            val maxScrollX = (clampedContentWidth - clampedViewportWidth).coerceAtLeast(0)
            val maxScrollY = (clampedContentHeight - clampedViewportHeight).coerceAtLeast(0)
            return copy(
                viewportWidth = clampedViewportWidth,
                viewportHeight = clampedViewportHeight,
                contentWidth = clampedContentWidth,
                contentHeight = clampedContentHeight,
                scrollX = scrollX.coerceIn(0, maxScrollX),
                scrollY = scrollY.coerceIn(0, maxScrollY)
            )
        }
    }

    class NativeEmbeddedSurfaceControlHost(
        private val context: Context,
        private val pluginPackageName: String,
        private val pluginId: String,
        private val instanceId: Int
    ) : AutoCloseable {
        private val surfaceControlClient = AudioPluginHostHelper.createSurfaceControl(context)

        val surfaceView: View
            get() = surfaceControlClient.surfaceView

        @RequiresApi(Build.VERSION_CODES.R)
        suspend fun getPreferredSizeOrFallback(fallbackWidth: Int, fallbackHeight: Int): Size {
            val preferredSize = surfaceControlClient.getPreferredSizeNoHandler(
                pluginPackageName,
                pluginId,
                instanceId
            )
            val width = preferredSize.getOrNull(0)?.takeIf { it > 0 } ?: fallbackWidth
            val height = preferredSize.getOrNull(1)?.takeIf { it > 0 } ?: fallbackHeight
            return Size(width, height)
        }

        @RequiresApi(Build.VERSION_CODES.R)
        suspend fun connect(width: Int, height: Int, withIdleHandler: Boolean = false) {
            if (withIdleHandler)
                surfaceControlClient.connectUI(pluginPackageName, pluginId, instanceId, width, height)
            else
                surfaceControlClient.connectUINoHandler(pluginPackageName, pluginId, instanceId, width, height)
        }

        fun show() {
            surfaceView.visibility = View.VISIBLE
        }

        fun hide() {
            surfaceView.visibility = View.GONE
        }

        @RequiresApi(Build.VERSION_CODES.R)
        fun resize(width: Int, height: Int) {
            surfaceControlClient.resizeUI(instanceId, width, height)
        }

        @RequiresApi(Build.VERSION_CODES.R)
        fun configureViewport(configuration: ViewportConfiguration) {
            val clamped = configuration.clamp()
            surfaceControlClient.configureViewport(
                instanceId,
                clamped.viewportWidth,
                clamped.viewportHeight,
                clamped.contentWidth,
                clamped.contentHeight,
                clamped.scrollX,
                clamped.scrollY
            )
        }

        override fun close() {
            surfaceControlClient.close()
        }
    }
}
