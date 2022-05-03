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
    // FIXME: PREFAB: enable these sections once we migrate to prefab-based solution.
    /*
    buildFeatures {
        prefab true
    }
    */
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
}

// Starting AGP 7.0.0-alpha05, AGP stopped caring build dependencies and it broke builds.
// This is a forcible workarounds to build libandroidaudioplugin.so in prior to referencing it.
gradle.projectsEvaluated {
    tasks["buildCMakeDebug"].dependsOn(rootProject.project("androidaudioplugin").tasks["mergeDebugNativeLibs"])
    // It is super awkward in AGP or Android Studio, but gradlew --tasks does not list
    // `buildCMakeRelease` while `buildCMakeDebug` is there!
    // Fortunately(?) we can rely on another awkwardness that we reference *debug* version of
    //  -;androidaudioplugin in CMakeLists.txt for target_link_directories() so we can skip
    //  release dependency here (technically we need debug build *always* happen before
    //  release builds, but it always seems to happen.
//    tasks["buildCMakeRelease"].dependsOn(rootProject.project("androidaudioplugin").mergeReleaseNativeLibs)
}
