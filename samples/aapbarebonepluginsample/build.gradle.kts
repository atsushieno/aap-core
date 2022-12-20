plugins {
    id ("com.android.application")
    id ("kotlin-android")
}

apply { from ("../../common.gradle") }

// What a mess...
val enable_asan: Boolean by rootProject

android {
    namespace = "org.androidaudioplugin.aapbarebonepluginsample"
    defaultConfig {
        applicationId = "org.androidaudioplugin.aapbarebonepluginsample"

        externalNativeBuild {
            cmake {
                arguments ("-DANDROID_STL=c++_shared", "-DAAP_ENABLE_ASAN=" + (if (enable_asan) "1" else "0"))
            }
        }
    }
    buildFeatures {
        prefab = true
    }
    externalNativeBuild {
        cmake {
            version = "3.22.1"
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
}

dependencies {
    implementation (project(":androidaudioplugin"))
    implementation (project(":androidaudioplugin-ui-compose"))
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
    // It is super awkward in AGP or Android Studio, but gradlew --tasks does not list
    // `buildCMakeRelease` while `buildCMakeDebug` is there!
    // Fortunately(?) we can rely on another awkwardness that we reference *debug* version of
    //  -;androidaudioplugin in CMakeLists.txt for target_link_directories() so we can skip
    //  release dependency here (technically we need debug build *always* happen before
    //  release builds, but it always seems to happen.
//    tasks["buildCMakeRelease"].dependsOn(rootProject.project("androidaudioplugin").mergeReleaseNativeLibs)
}
