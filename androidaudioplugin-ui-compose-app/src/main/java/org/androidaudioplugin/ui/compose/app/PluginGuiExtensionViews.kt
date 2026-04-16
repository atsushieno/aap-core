package org.androidaudioplugin.ui.compose.app

import android.content.Context
import android.view.SurfaceView
import android.view.View
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.compose.ui.window.Popup
import androidx.compose.ui.window.PopupProperties
import kotlinx.coroutines.launch
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.hosting.NativeRemotePluginInstance
import org.androidaudioplugin.ui.web.WebUIHostHelper

@OptIn(ExperimentalComposeUiApi::class)
@Composable
fun PluginSurfaceControlUI(pluginInfo: PluginInformation,
                           viewportWidth: Dp,
                           viewportHeight: Dp,
                           contentWidth: Dp,
                           contentHeight: Dp,
                           createSurfaceView: (Context) -> View,
                           detachSurfaceView: (View) -> Unit,
                           onCloseClick: () -> Unit = {}) {
    var offsetX by remember { mutableStateOf(0f) }
    var offsetY by remember { mutableStateOf(0f) }
    val horizontalScrollState = rememberScrollState()
    val verticalScrollState = rememberScrollState()
    val coroutineScope = rememberCoroutineScope()
    val scrollbarThickness = 12.dp

    Popup(
        alignment = Alignment.TopStart,
        offset = IntOffset(offsetX.toInt(), offsetY.toInt()),
        properties = PopupProperties(clippingEnabled = false)
    ) {
        Column(Modifier.padding(10.dp)) {
            // title bar
            Row(Modifier
                .padding(0.dp)
                .width(viewportWidth + scrollbarThickness)
                .pointerInput(Unit) {
                    detectDragGestures { change, dragAmount ->
                        change.consume()
                        offsetX += dragAmount.x
                        offsetY += dragAmount.y
                    }
                }
                .fillMaxWidth()
                .background(Color.DarkGray.copy(alpha = 0.5f))
                .border(1.dp, Color.Black)
            ) {
                TextButton(onClick = { onCloseClick() }) {
                    Text("[X]")
                }
                Text(pluginInfo.displayName)
            }
            Row {
                Box(Modifier
                    .width(viewportWidth)
                    .height(viewportHeight)
                    .clipToBounds()
                    .border(1.dp, Color.Black)) {
                    Box(Modifier
                        .horizontalScroll(horizontalScrollState)
                        .verticalScroll(verticalScrollState)) {
                        AndroidView(modifier = Modifier
                            .width(contentWidth)
                            .height(contentHeight),
                            factory = { ctx -> createSurfaceView(ctx) },
                            onRelease = { view: View -> detachSurfaceView(view) },
                            onReset = { _: View -> }
                        )
                    }
                }

                if (verticalScrollState.maxValue > 0) {
                    ScrollbarTrack(
                        modifier = Modifier
                            .width(scrollbarThickness)
                            .height(viewportHeight),
                        scrollValue = verticalScrollState.value,
                        maxValue = verticalScrollState.maxValue,
                        isHorizontal = false,
                        onScrollValueChange = {
                            coroutineScope.launch { verticalScrollState.scrollTo(it) }
                        }
                    )
                }
            }
            Row {
                if (horizontalScrollState.maxValue > 0) {
                    ScrollbarTrack(
                        modifier = Modifier
                            .width(viewportWidth)
                            .height(scrollbarThickness),
                        scrollValue = horizontalScrollState.value,
                        maxValue = horizontalScrollState.maxValue,
                        isHorizontal = true,
                        onScrollValueChange = {
                            coroutineScope.launch { horizontalScrollState.scrollTo(it) }
                        }
                    )
                }

                if (horizontalScrollState.maxValue > 0 && verticalScrollState.maxValue > 0)
                    Box(Modifier.width(scrollbarThickness).height(scrollbarThickness))
            }
        }
    }
}

@Composable
private fun ScrollbarTrack(
    modifier: Modifier,
    scrollValue: Int,
    maxValue: Int,
    isHorizontal: Boolean,
    onScrollValueChange: (Int) -> Unit
) {
    var trackSize by remember { mutableStateOf(0) }
    val density = LocalDensity.current

    Box(modifier
        .background(Color.Black.copy(alpha = 0.25f))
        .onSizeChanged { trackSize = if (isHorizontal) it.width else it.height }) {
        val contentSize = trackSize + maxValue
        val thumbSize = if (contentSize > 0)
            (trackSize * (trackSize.toFloat() / contentSize)).toInt().coerceAtLeast(24)
        else
            trackSize
        val maxThumbOffset = (trackSize - thumbSize).coerceAtLeast(0)
        val thumbOffset = if (maxValue == 0) 0 else (maxThumbOffset * (scrollValue.toFloat() / maxValue)).toInt()
        val updateScrollFromDrag = { dragAmount: Float ->
            if (maxThumbOffset > 0) {
                val next = scrollValue + (dragAmount * maxValue / maxThumbOffset).toInt()
                onScrollValueChange(next.coerceIn(0, maxValue))
            }
        }

        val thumbModifier = if (isHorizontal) {
            Modifier
                .width(with(density) { thumbSize.toDp() })
                .fillMaxHeight()
                .offset(x = with(density) { thumbOffset.toDp() })
                .pointerInput(maxValue, maxThumbOffset, scrollValue) {
                    detectDragGestures { change, dragAmount ->
                        change.consume()
                        updateScrollFromDrag(dragAmount.x)
                    }
                }
        } else {
            Modifier
                .fillMaxWidth()
                .height(with(density) { thumbSize.toDp() })
                .offset(y = with(density) { thumbOffset.toDp() })
                .pointerInput(maxValue, maxThumbOffset, scrollValue) {
                    detectDragGestures { change, dragAmount ->
                        change.consume()
                        updateScrollFromDrag(dragAmount.y)
                    }
                }
        }

        Box(thumbModifier.background(Color.White.copy(alpha = 0.7f)))
    }
}

@Composable
fun PluginWebUI(packageName: String, pluginId: String, native: NativeRemotePluginInstance,
                onCloseClick: () -> Unit = {}) {
    var offsetX by remember { mutableStateOf(0f) }
    var offsetY by remember { mutableStateOf(0f) }

    Column(
        Modifier
            .padding(40.dp)
            .offset { IntOffset(offsetX.toInt(), offsetY.toInt()) }) {
        // Titlebar
        Box(Modifier
            .pointerInput(Unit) {
                detectDragGestures { change, dragAmount ->
                    change.consume()
                    offsetX += dragAmount.x
                    offsetY += dragAmount.y
                }
            }
            .fillMaxWidth()
            .background(Color.DarkGray.copy(alpha = 0.3f))
            .border(1.dp, Color.Black)) {
            Row {
                TextButton(onClick = { onCloseClick() }) {
                    Text("[X]")
                }
                Text("Web UI")
            }
        }
        AndroidView(
            modifier = Modifier.border(1.dp, Color.Black),
            factory = { ctx: Context -> WebUIHostHelper.getWebView(ctx, pluginId, packageName, native) }
        )
    }
}
