package org.androidaudioplugin.hosting

object UmpHelper {
    private val i8x4to32 = { b1: Byte, b2: Byte, b3: Byte, b4: Byte ->
        (b1.toInt() shl 24) + (b2.toInt() shl 16) + (b3.toInt() shl 8) + b4 }

    fun aapUmpSysex8Parameter(parameterId: UInt, parameterValue: Float, group: Byte = 0, channel: Byte = 0, key: Byte = 0, noteId: UByte = 0u) : Array<Int> {
        val streamId: Byte = 0
        val subId: Byte = 0
        val subId2: Byte = 0
        return arrayOf(i8x4to32((0x50 + group).toByte(), 0, streamId, 0x7E),
            i8x4to32(0x7F, subId, subId2, channel),
            i8x4to32(key, noteId.toByte(), (parameterId / 0x80u).toByte(), (parameterId % 0x80u).toByte()),
            parameterValue.toRawBits())
    }
}