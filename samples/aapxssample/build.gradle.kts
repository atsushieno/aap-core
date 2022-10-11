plugins {
    id ("com.android.library")
    id ("kotlin-android")
}

apply { from ("../../common.gradle") }

// What a mess...
val kotlin_version: String by rootProject
val dokka_version: String by rootProject
val compose_version: String by rootProject
val aap_version: String by rootProject
val enable_asan: Boolean by rootProject

android {
    defaultConfig {
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
        if (enable_asan)
            jniLibs.useLegacyPackaging = true
    }
}

dependencies {
    implementation (project(":androidaudioplugin"))

    implementation ("androidx.core:core-ktx:1.9.0")
    implementation ("org.jetbrains.kotlin:kotlin-stdlib-jdk7:$kotlin_version")
    implementation ("androidx.appcompat:appcompat:1.4.1")

    androidTestImplementation ("androidx.test:rules:1.4.0")
    androidTestImplementation ("androidx.test.ext:junit:1.1.3")
    androidTestImplementation ("androidx.compose.ui:ui-test-junit4:$compose_version")
    androidTestImplementation (project(":androidaudioplugin-testing"))
}

// Starting AGP 7.0.0-alpha05, AGP stopped caring build dependencies and it broke builds.
// This is a forcible workarounds to build libandroidaudioplugin.so in prior to referencing it.
gradle.projectsEvaluated {
    tasks["buildCMakeDebug"].dependsOn(rootProject.project("androidaudioplugin").tasks["mergeDebugNativeLibs"])
    tasks["buildCMakeRelWithDebInfo"].dependsOn(rootProject.project("androidaudioplugin").tasks["mergeReleaseNativeLibs"])
}
