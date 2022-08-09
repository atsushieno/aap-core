
buildscript {
    val kotlin_version: String by extra("1.7.0")
    val dokka_version: String by extra("1.7.0")
    val compose_version: String by extra("1.2.0")
    val aap_version: String by extra("0.7.4")
    val enable_asan: Boolean by extra(false)

    repositories {
        google()
        mavenCentral()
        maven ("https://plugins.gradle.org/m2/")
        maven ("https://maven.pkg.jetbrains.space/public/p/compose/dev")
    }
    dependencies {
        classpath ("com.android.tools.build:gradle:7.2.2")
        classpath ("org.jetbrains.kotlin:kotlin-gradle-plugin:$kotlin_version")
        classpath ("org.jetbrains.dokka:dokka-gradle-plugin:$dokka_version")
    }
}

plugins {
    id ("maven-publish")
    //id("io.github.gradle-nexus.publish-plugin") version "1.1.0"
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
        maven ("https://maven.pkg.jetbrains.space/public/p/compose/dev")
    }
}

tasks.register<Delete>("clean") {
    delete(rootProject.buildDir)
}
