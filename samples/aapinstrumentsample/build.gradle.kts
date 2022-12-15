plugins {
    id ("com.android.application")
    id ("kotlin-android")
}

apply { from ("../../common.gradle") }

// What a mess...
val enable_asan: Boolean by rootProject

android {
    defaultConfig {
        applicationId = "org.androidaudioplugin.aapinstrumentsample"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        externalNativeBuild {
            cmake {
                arguments ("-DANDROID_STL=c++_shared")
            }
        }
    }
    buildFeatures {
        prefab = true
    }
    externalNativeBuild {
        cmake {
            version = "3.18.1"
            path ("src/main/cpp/CMakeLists.txt")
        }
    }
    buildTypes {
        debug {
            packagingOptions {
                doNotStrip ("**/*.so")
            }
        }
        release {
            isMinifyEnabled = false
            proguardFiles (getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    packagingOptions {
        resources {
            excludes += setOf("META-INF/AL2.0", "META-INF/LGPL2.1")
        }
        if (enable_asan)
            jniLibs.useLegacyPackaging = true
    }
    namespace = "org.androidaudioplugin.aapinstrumentsample"
}

dependencies {
    implementation (project(":androidaudioplugin"))
    implementation (project(":androidaudioplugin-ui-compose"))
    implementation (project(":androidaudioplugin-midi-device-service"))
    androidTestImplementation (project(":androidaudioplugin-testing"))
    implementation (libs.androidx.core.ktx)
    implementation (libs.kotlin.stdlib.jdk7)
    implementation (libs.androidx.appcompat)

    testImplementation (libs.junit)
    androidTestImplementation (libs.test.rules)
    androidTestImplementation (libs.test.ext.junit)
    androidTestImplementation (libs.test.espresso.core)
    androidTestImplementation (libs.compose.ui.test.junit)
}

// Starting AGP 7.0.0-alpha05, AGP stopped caring build dependencies and it broke builds.
// This is a forcible workarounds to build libandroidaudioplugin.so in prior to referencing it.
gradle.projectsEvaluated {
    tasks["buildCMakeDebug"].dependsOn(rootProject.project("androidaudioplugin").tasks["mergeDebugNativeLibs"])
    tasks["buildCMakeRelWithDebInfo"].dependsOn(rootProject.project("androidaudioplugin").tasks["mergeReleaseNativeLibs"])
}
