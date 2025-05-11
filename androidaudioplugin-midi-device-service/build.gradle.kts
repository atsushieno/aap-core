plugins {
    id ("com.android.library")
    id ("kotlin-android")
    id ("org.jetbrains.dokka")
    id ("maven-publish")
    id ("signing")
}

apply { from ("../common.gradle") }

// What a mess...
val enable_asan: Boolean by rootProject

android {
    namespace = "org.androidaudioplugin.midideviceservice"
    this.ext["description"] = "AndroidAudioPlugin - MidiDeviceService support"

    defaultConfig {
        externalNativeBuild {
            cmake {
                arguments ("-DANDROID_STL=c++_shared", "-DAAP_ENABLE_ASAN=" + (if (enable_asan) "1" else "0"))
            }
        }
    }

    buildTypes {
        debug {
            packaging.jniLibs.keepDebugSymbols.add ("**/*.so")
            externalNativeBuild {
                cmake {
                    // we cannot error out cmidi2.h as we don't compile cmidi2_test.h
                    //cppFlags ("-Werror")
                }
            }
        }
        release {
            isMinifyEnabled = false
            proguardFiles (getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    buildFeatures {
        prefab = true
        prefabPublishing = true
    }
    externalNativeBuild {
        cmake {
            version = libs.versions.cmake.get()
            path ("src/main/cpp/CMakeLists.txt")
        }
    }
    prefab {
        create("androidaudioplugin-midi-device-service") {
            name = "aapmidideviceservice"
        }
    }
    // https://github.com/google/prefab/issues/127
    packaging {
        jniLibs.excludes.add("**/libc++_shared.so")
        jniLibs.excludes.add("**/libandroidaudioplugin.so") // package it separately
    }
}

apply { from ("../publish-pom.gradle") }

dependencies {
    implementation (project(":androidaudioplugin"))
    implementation (libs.androidx.core.ktx)
    implementation (libs.startup.runtime)
    implementation (libs.coroutines.core)
    implementation (libs.oboe)

    testImplementation (libs.junit)
    androidTestImplementation (libs.test.ext.junit)
    androidTestImplementation (libs.test.espresso.core)
}

/*
// Starting AGP 7.0.0-alpha05, AGP stopped caring build dependencies and it broke builds.
// This is a forcible workarounds to build libandroidaudioplugin.so in prior to referencing it.
gradle.projectsEvaluated {
    tasks.findByPath(":androidaudioplugin-midi-device-service:buildCMakeDebug")!!.dependsOn(":androidaudioplugin:prefabDebugPackage")
    tasks.findByPath(":androidaudioplugin-midi-device-service:buildCMakeRelWithDebInfo")!!.dependsOn(":androidaudioplugin:prefabReleasePackage")
    //tasks["mergeDebugNativeLibs"].dependsOn(rootProject.project("androidaudioplugin").tasks["prefabDebugPackage"])
    //tasks["mergeReleaseNativeLibs"].dependsOn(rootProject.project("androidaudioplugin").tasks["prefabReleasePackage"])
}
*/