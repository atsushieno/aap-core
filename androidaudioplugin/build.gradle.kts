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
    namespace = "org.androidaudioplugin"
    this.ext["description"] = "AndroidAudioPlugin - core"

    defaultConfig {

        externalNativeBuild {
            cmake {
                arguments ("-DANDROID_STL=c++_shared", "-DAAP_ENABLE_ASAN=" + (if (enable_asan) "1" else "0"))
            }
        }
    }

    buildTypes {
        debug {
            packaging.jniLibs.keepDebugSymbols.add("**/*.so")
            externalNativeBuild {
                cmake {
                    cppFlags ("-Werror")
                }
            }
        }
        release {
            isMinifyEnabled = false
            proguardFiles (
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    buildFeatures {
        prefab = true
        prefabPublishing = true
        aidl = true
    }
    externalNativeBuild {
        cmake {
            version = libs.versions.cmake.get()
            path ("src/main/cpp/CMakeLists.txt")
        }
    }
    prefab {
        create("androidaudioplugin") {
            headers = "../include"
        }
    }
    // https://github.com/google/prefab/issues/127
    packaging.jniLibs.excludes.add("**/libc++_shared.so")

    lint {
        disable.add("EnsureInitializerMetadata")
    }
}

apply { from ("../publish-pom.gradle") }

dependencies {
    implementation (libs.androidx.core.ktx)
    implementation (libs.kotlin.stdlib.jdk8)
    implementation (libs.preference.ktx) {
        exclude(group = "androidx.lifecycle", module = "lifecycle-viewmodel")
        exclude(group = "androidx.lifecycle", module = "lifecycle-viewmodel-ktx")
    }
    implementation(libs.coroutines.core) {
        exclude(group = "org.jetbrains.kotlin", module = "kotlin-stdlib-jdk8")
    }
    implementation (libs.lifecycle.service)
    implementation (libs.startup.runtime)
    testImplementation (libs.junit)
    androidTestImplementation (libs.test.core)
    androidTestImplementation (libs.test.rules)
    androidTestImplementation (libs.test.runner)
    androidTestImplementation (libs.test.ext.junit)
    androidTestImplementation (libs.test.espresso.core)
}
