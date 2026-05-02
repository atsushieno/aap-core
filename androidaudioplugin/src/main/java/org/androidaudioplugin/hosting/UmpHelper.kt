package org.androidaudioplugin.hosting

object UmpHelper {
    private val i8x4to32 = { b1: Byte, b2: Byte, b3: Byte, b4: Byte ->
        (b1.toInt() shl 24) + (b2.toInt() shl 16) + (b3.toInt() shl 8) + b4 }

    private fun plainToNormalized(minValue: Double, maxValue: Double, plainValue: Double): Double {
        if (!(maxValue > minValue))
            return 0.0
        return ((plainValue - minValue) / (maxValue - minValue)).coerceIn(0.0, 1.0)
    }

    fun normalizedToPlain(minValue: Double, maxValue: Double, normalizedValue: Double): Double {
        if (!(maxValue > minValue))
            return minValue
        return (minValue + normalizedValue.coerceIn(0.0, 1.0) * (maxValue - minValue)).coerceIn(minValue, maxValue)
    }

    fun transportUint32ToPlain(minValue: Double, maxValue: Double, transportValue: Int): Double {
        val normalized = transportValue.toUInt().toDouble() / UInt.MAX_VALUE.toDouble()
        return normalizedToPlain(minValue, maxValue, normalized)
    }

    fun plainToTransportUint32(minValue: Double, maxValue: Double, plainValue: Double): Int {
        val normalized = plainToNormalized(minValue, maxValue, plainValue)
        return (normalized * UInt.MAX_VALUE.toDouble()).toLong().coerceIn(0, UInt.MAX_VALUE.toLong()).toInt()
    }

    fun aapUmpSysex8Parameter(parameterId: UInt, normalizedValue: Int, group: Byte = 0, channel: Byte = 0, key: Byte = 0, noteId: UByte = 0u) : Array<Int> {
        val streamId: Byte = 0
        val subId: Byte = 0
        val subId2: Byte = 0
        return arrayOf(i8x4to32((0x50 + group).toByte(), 0, streamId, 0x7E),
            i8x4to32(0x7F, subId, subId2, channel),
            i8x4to32(key, noteId.toByte(), 0, 0) + (parameterId and 0x3FFFu).toInt(),
            normalizedValue)
    }

    fun aapUmpSysex8ParameterPlain(parameterId: UInt, minValue: Double, maxValue: Double, plainValue: Double, group: Byte = 0, channel: Byte = 0, key: Byte = 0, noteId: UByte = 0u): Array<Int> {
        return aapUmpSysex8Parameter(parameterId, plainToTransportUint32(minValue, maxValue, plainValue), group, channel, key, noteId)
    }
}
