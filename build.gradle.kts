
buildscript {
    val enable_asan: Boolean by extra(false)

    repositories {
        google()
        mavenCentral()
        maven ("https://plugins.gradle.org/m2/")
        maven ("https://maven.pkg.jetbrains.space/public/p/compose/dev")
    }
    dependencies {
        classpath (libs.tools.build.gradle)
        classpath (libs.kotlin.gradle.plugin)
        classpath (libs.dokka.gradle.plugin)
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
