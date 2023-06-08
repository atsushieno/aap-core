package org.androidaudioplugin

class ParameterInformation(var id: Int, var name: String, var minimumValue: Double = 0.0, var maximumValue: Double = 1.0, var defaultValue: Double = 0.0) {
    // They are used by JNI
    private fun addEnum(index: Int, value: Double, name: String) {
        enumerations.add(EnumerationInformation(index, value, name))
    }
    private fun getEnumCount() = enumerations.size
    private fun getEnum(index: Int) = enumerations[index]

    class EnumerationInformation(var index: Int, var value: Double, var name: String)

    val enumerations: MutableList<EnumerationInformation> = mutableListOf()
}
