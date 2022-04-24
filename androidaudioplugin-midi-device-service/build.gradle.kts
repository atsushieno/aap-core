plugins {
    id ("com.android.library")
    id ("kotlin-android")
    id ("maven-publish")
    id ("signing")
}

apply { from ("../common.gradle") }

// What a mess...
val kotlin_version: String by rootProject
val dokka_version: String by rootProject
val compose_version: String by rootProject
val aap_version: String by rootProject
val enable_asan: Boolean by rootProject

android {
    this.ext["description"] = "AndroidAudioPlugin - MidiDeviceService support"

    defaultConfig {
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        externalNativeBuild {
            cmake {
                // https://github.com/google/prefab/blob/bccf5a6a75b67add30afbb6d4f7a7c50081d2d86/api/src/main/kotlin/com/google/prefab/api/Android.kt#L243
                arguments ("-DANDROID_STL=c++_shared", "-DAAP_ENABLE_ASAN=" + (if (enable_asan) "1" else "0")) 
            }
        }
    }

    buildTypes {
        debug {
            packagingOptions {
                jniLibs.keepDebugSymbols.add ("**/*.so")
            }
            externalNativeBuild {
                cmake {
                    cppFlags ("-Werror")
                }
            }
        }
        release {
            isMinifyEnabled = false
            proguardFiles (getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    externalNativeBuild {
        cmake {
            version = "3.18.1"
            path ("src/main/cpp/CMakeLists.txt")
        }
    }
    packagingOptions {
        jniLibs.excludes.add ("**libc++_shared.so")
    }
}

apply { from ("../publish-pom.gradle") }

dependencies {
    implementation ("androidx.core:core-ktx:1.7.0")
    implementation ("androidx.startup:startup-runtime:1.1.1")
    implementation (project(":androidaudioplugin"))
    testImplementation ("junit:junit:4.13.2")
    androidTestImplementation ("androidx.test.ext:junit:1.1.3")
    androidTestImplementation ("androidx.test.espresso:espresso-core:3.4.0")
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
