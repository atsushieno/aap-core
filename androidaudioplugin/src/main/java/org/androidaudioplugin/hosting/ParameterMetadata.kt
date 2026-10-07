package org.androidaudioplugin.hosting

import androidx.annotation.Keep
import org.androidaudioplugin.ParameterInformation
import java.util.Collections

/** An owned parameter record from a complete native metadata publication. */
data class ParameterMetadata(
    val id: Int,
    val name: String,
    val minimumValue: Double,
    val maximumValue: Double,
    val defaultValue: Double,
    val enumerations: List<ParameterEnumerationMetadata>
) {
    /** A fresh mutable record for existing APIs; mutations do not affect this snapshot. */
    fun toParameterInformation(): ParameterInformation =
        ParameterInformation(id, name, minimumValue, maximumValue, defaultValue).also { parameter ->
            parameter.enumerations.addAll(enumerations.map {
                ParameterInformation.EnumerationInformation(it.index, it.value, it.name)
            })
        }
}

data class ParameterEnumerationMetadata(val index: Int, val value: Double, val name: String)

/** Revision and complete parameter list captured together, including a legitimately empty list. */
@Keep
data class ParameterMetadataSnapshot(val revision: Long, val parameters: List<ParameterMetadata>) {
    companion object {
        @JvmStatic
        fun fromNative(revision: Long, parameters: Array<ParameterInformation>): ParameterMetadataSnapshot =
            ParameterMetadataSnapshot(revision, Collections.unmodifiableList(parameters.map { parameter ->
                ParameterMetadata(parameter.id, parameter.name, parameter.minimumValue,
                    parameter.maximumValue, parameter.defaultValue,
                    Collections.unmodifiableList(parameter.enumerations.map {
                        ParameterEnumerationMetadata(it.index, it.value, it.name)
                    }))
            }))
    }
}

fun interface ParameterMetadataChangedListener {
    fun onParameterMetadataChanged(snapshot: ParameterMetadataSnapshot)
}
