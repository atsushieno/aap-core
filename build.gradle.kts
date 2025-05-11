
plugins {
    alias(libs.plugins.android.application) apply false
    alias(libs.plugins.android.library) apply false
    alias(libs.plugins.kotlin.android) apply false
    alias(libs.plugins.compose.compiler) apply false
    alias(libs.plugins.dokka) apply false
}

buildscript {
    val enable_asan: Boolean by extra(false)
}

apply { from ("${rootDir}/publish-root.gradle") }

subprojects {
    group = "org.androidaudioplugin"
    repositories {
        google()
        mavenLocal()
        mavenCentral()
        maven ("https://plugins.gradle.org/m2/")
        maven ("https://jitpack.io")
    }
}

tasks.register<Delete>("clean") {
    delete(rootProject.buildDir)
}
