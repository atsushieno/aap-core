package org.androidaudioplugin.ui.compose.app

import android.content.Context
import android.webkit.WebView
import android.view.View
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshotFlow
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.compose.ui.window.Popup
import androidx.compose.ui.window.PopupProperties
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.hosting.NativeRemotePluginInstance
import org.androidaudioplugin.ui.web.WebUIHostHelper
import kotlin.math.roundToInt

@OptIn(ExperimentalComposeUiApi::class)
@Composable
fun PluginSurfaceControlUI(pluginInfo: PluginInformation,
                           viewportWidth: Dp,
                           viewportHeight: Dp,
                           contentWidth: Dp,
                           contentHeight: Dp,
                           createSurfaceView: (Context) -> View,
                           detachSurfaceView: (View) -> Unit,
                           onViewportChanged: (Int, Int, Int, Int, Int, Int) -> Unit = { _, _, _, _, _, _ -> },
                           onResize: (Int, Int) -> Unit = { _, _ -> },
                           onCloseClick: () -> Unit = {}) {
    var offsetX by remember { mutableStateOf(0f) }
    var offsetY by remember { mutableStateOf(0f) }
    var scrollX by remember { mutableStateOf(0) }
    var scrollY by remember { mutableStateOf(0) }
    val density = LocalDensity.current
    val viewportWidthPx = with(density) { viewportWidth.roundToPx() }
    val viewportHeightPx = with(density) { viewportHeight.roundToPx() }
    val contentWidthPx = with(density) { contentWidth.roundToPx() }
    val contentHeightPx = with(density) { contentHeight.roundToPx() }
    val maxScrollX = (contentWidthPx - viewportWidthPx).coerceAtLeast(0)
    val maxScrollY = (contentHeightPx - viewportHeightPx).coerceAtLeast(0)
    val effectiveScrollX = scrollX.coerceIn(0, maxScrollX)
    val effectiveScrollY = scrollY.coerceIn(0, maxScrollY)
    val scrollbarThickness = 12.dp

    LaunchedEffect(viewportWidthPx, viewportHeightPx, effectiveScrollX, effectiveScrollY) {
        onViewportChanged(
            viewportWidthPx,
            viewportHeightPx,
            contentWidthPx,
            contentHeightPx,
            effectiveScrollX,
            effectiveScrollY
        )
    }

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
                    .border(1.dp, Color.Black)) {
                    AndroidView(modifier = Modifier
                        .width(viewportWidth)
                        .height(viewportHeight),
                        factory = { ctx -> createSurfaceView(ctx) },
                        onRelease = { view: View -> detachSurfaceView(view) },
                        onReset = { _: View -> }
                    )
                }

                if (maxScrollY > 0) {
                    ScrollbarTrack(
                        modifier = Modifier
                            .width(scrollbarThickness)
                            .height(viewportHeight),
                        scrollValue = effectiveScrollY,
                        maxValue = maxScrollY,
                        isHorizontal = false,
                        onScrollValueChange = { scrollY = it }
                    )
                }
            }
            Row {
                if (maxScrollX > 0) {
                    ScrollbarTrack(
                        modifier = Modifier
                            .width(viewportWidth)
                            .height(scrollbarThickness),
                        scrollValue = effectiveScrollX,
                        maxValue = maxScrollX,
                        isHorizontal = true,
                        onScrollValueChange = { scrollX = it }
                    )
                } else {
                    Spacer(Modifier.width(viewportWidth).height(scrollbarThickness))
                }
                ResizeHandle(
                    modifier = Modifier.width(scrollbarThickness).height(scrollbarThickness),
                    viewportWidthPx = viewportWidthPx,
                    viewportHeightPx = viewportHeightPx,
                    onResize = onResize
                )
            }
        }
    }
}

@Composable
private fun ResizeHandle(
    modifier: Modifier,
    viewportWidthPx: Int,
    viewportHeightPx: Int,
    onResize: (Int, Int) -> Unit
) {
    val currentViewportWidthPx by rememberUpdatedState(viewportWidthPx)
    val currentViewportHeightPx by rememberUpdatedState(viewportHeightPx)
    var dragStartWidth by remember { mutableStateOf(0) }
    var dragStartHeight by remember { mutableStateOf(0) }
    var totalDragX by remember { mutableStateOf(0f) }
    var totalDragY by remember { mutableStateOf(0f) }

    Box(modifier
        .background(Color.DarkGray.copy(alpha = 0.5f))
        .pointerInput(Unit) {
            detectDragGestures(
                onDragStart = {
                    dragStartWidth = currentViewportWidthPx
                    dragStartHeight = currentViewportHeightPx
                    totalDragX = 0f
                    totalDragY = 0f
                },
                onDrag = { change, dragAmount ->
                    change.consume()
                    totalDragX += dragAmount.x
                    totalDragY += dragAmount.y
                    val newWidth = (dragStartWidth + totalDragX.roundToInt()).coerceAtLeast(100)
                    val newHeight = (dragStartHeight + totalDragY.roundToInt()).coerceAtLeast(100)
                    onResize(newWidth, newHeight)
                }
            )
        }
    ) {
        Canvas(Modifier.matchParentSize()) {
            val lineColor = Color.White.copy(alpha = 0.7f)
            for (i in 1..3) {
                val t = i * (size.width / 4f)
                drawLine(lineColor, Offset(t, size.height), Offset(size.width, t), strokeWidth = 1.5f, cap = StrokeCap.Round)
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
    var dragStartScrollValue by remember { mutableStateOf(0) }
    var dragTotal by remember { mutableStateOf(0f) }
    val currentScrollValue by rememberUpdatedState(scrollValue)
    val currentOnScrollValueChange by rememberUpdatedState(onScrollValueChange)
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

        val thumbModifier = if (isHorizontal) {
            Modifier
                .width(with(density) { thumbSize.toDp() })
                .fillMaxHeight()
                .offset(x = with(density) { thumbOffset.toDp() })
                .pointerInput(maxValue, maxThumbOffset) {
                    detectDragGestures(
                        onDragStart = {
                            dragStartScrollValue = currentScrollValue
                            dragTotal = 0f
                        },
                        onDrag = { change, dragAmount ->
                            change.consume()
                            dragTotal += dragAmount.x
                            updateScrollFromDrag(
                                dragStartScrollValue,
                                dragTotal,
                                maxValue,
                                maxThumbOffset,
                                currentOnScrollValueChange
                            )
                        }
                    )
                }
        } else {
            Modifier
                .fillMaxWidth()
                .height(with(density) { thumbSize.toDp() })
                .offset(y = with(density) { thumbOffset.toDp() })
                .pointerInput(maxValue, maxThumbOffset) {
                    detectDragGestures(
                        onDragStart = {
                            dragStartScrollValue = currentScrollValue
                            dragTotal = 0f
                        },
                        onDrag = { change, dragAmount ->
                            change.consume()
                            dragTotal += dragAmount.y
                            updateScrollFromDrag(
                                dragStartScrollValue,
                                dragTotal,
                                maxValue,
                                maxThumbOffset,
                                currentOnScrollValueChange
                            )
                        }
                    )
                }
        }

        Box(thumbModifier.background(Color.White.copy(alpha = 0.7f)))
    }
}

private fun updateScrollFromDrag(
    startScrollValue: Int,
    dragAmount: Float,
    maxValue: Int,
    maxThumbOffset: Int,
    onScrollValueChange: (Int) -> Unit
) {
    if (maxThumbOffset > 0) {
        val next = startScrollValue + (dragAmount * maxValue / maxThumbOffset).roundToInt()
        onScrollValueChange(next.coerceIn(0, maxValue))
    }
}

@Composable
fun PluginWebUI(packageName: String, pluginId: String, native: NativeRemotePluginInstance,
                parameterValues: List<Double>,
                onCloseClick: () -> Unit = {}) {
    var offsetX by remember { mutableStateOf(0f) }
    var offsetY by remember { mutableStateOf(0f) }
    var webView by remember { mutableStateOf<WebView?>(null) }

    LaunchedEffect(native, webView) {
        val currentWebView = webView ?: return@LaunchedEffect
        snapshotFlow { parameterValues.toList() }.collect { values ->
            val valuesByParameterId = values.mapIndexed { index, value ->
                native.getParameter(index).id to value
            }.toMap()
            WebUIHostHelper.updateParameterValues(currentWebView, valuesByParameterId)
        }
    }

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
            factory = { ctx: Context ->
                WebUIHostHelper.getWebView(ctx, pluginId, packageName, native).also { webView = it }
            },
            update = { webView = it }
        )
    }
}
