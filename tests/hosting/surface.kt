package org.androidaudioplugin.hosting
import kotlin.reflect.*
fun main() {
    for (type in listOf(AudioPluginServiceConnector::class, AudioPluginServiceConnector.Connection::class,
                        AudioPluginServiceConnector.Companion::class)) {
        println("CLASS ${type.qualifiedName} ${type.visibility}")
        for (constructor in type.constructors.sortedBy { it.toString() })
            println("CONSTRUCTOR ${constructor.visibility} ${constructor.parameters.map { "${it.name}:${it.type}:${it.isOptional}" }}")
        for (member in type.members.filter { it.visibility == KVisibility.PUBLIC }.sortedBy { it.toString() }) {
            val flags = when (member) {
                is KFunction<*> -> "suspend=${member.isSuspend}"
                is KMutableProperty<*> -> "var"
                else -> "val"
            }
            println("MEMBER ${member.name} ${member.returnType} $flags ${member.parameters.map { "${it.name}:${it.type}:${it.isOptional}" }}")
        }
    }
}
