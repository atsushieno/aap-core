package org.androidaudioplugin.ui.compose

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.clipRect
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.FloatBuffer

private fun getSampleVisualizationData(floats: FloatBuffer, size: Int, slots: Int) : Sequence<Pair<Float,Float>> {
    return sequence {
        val sizePerGroup = size / slots
        for (y in 0 until slots) {
            var min = Float.MAX_VALUE
            var max = -Float.MIN_VALUE
            for (i in 0 until size / slots / 2) {
                val idx = y * sizePerGroup + i * 2
                val l = floats[idx]
                if (max < l)
                    max = l
                val r = floats[idx + 1]
                if (min > r)
                    min = r
            }
            yield(Pair(max, min))
        }
    }
}

@Composable
fun WaveformDrawable(waveData: ByteArray, waveformHeight : Dp = 64.dp) {
    val floatBuffer = ByteBuffer.wrap(waveData).order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer()

    Canvas(modifier = Modifier
        .fillMaxWidth()
        .height(waveformHeight)
        .border(width = 1.dp, color = Color.Gray)) {
        clipRect {
            val width = this.size.width.toInt()
            val height = this.size.height

            val visualizationData =
                getSampleVisualizationData(floatBuffer, waveData.size / 4, width).toList()
            var vMin = Float.MAX_VALUE
            var vMax = -Float.MIN_VALUE
            for (pair in visualizationData) {
                val l = pair.first
                if (vMax < l)
                    vMax = l
                val r = pair.second
                if (vMin > r)
                    vMin = r
            }

            for (wp in visualizationData.indices) {
                var i = wp * 2
                val pair = visualizationData[wp]
                val frL = pair.first / vMax
                val hL = frL * height / 2
                drawLine(
                    Color.Black,
                    Offset(i.toFloat(), height / 2),
                    Offset((i + 1).toFloat(), height / 2 - hL)
                )
                i = wp * 2 + 1
                val frR = pair.second / vMin
                val hR = frR * height / 2
                drawLine(
                    Color.Black,
                    Offset(i.toFloat(), height / 2),
                    Offset((i + 1).toFloat(), height / 2 + hR)
                )
            }
        }
    }
}