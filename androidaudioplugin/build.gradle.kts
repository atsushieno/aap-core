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
            packagingOptions {
                jniLibs.keepDebugSymbols.add("**/*.so")
            }
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
        prefabPublishing = true
    }
    externalNativeBuild {
        cmake {
            version = "3.22.1"
            path ("src/main/cpp/CMakeLists.txt")
        }
    }
    prefab {
        create("androidaudioplugin") {
            name = "androidaudioplugin"
            headers = "../include"
        }
    }
    // https://github.com/google/prefab/issues/127
    packagingOptions {
        jniLibs.excludes.add("**/libc++_shared.so")
    }

    lint {
        disable.add("EnsureInitializerMetadata")
    }

    // FIXME: it is annoying to copy this everywhere, but build.gradle.kts is incapable of importing this fragment...
    // It's been long time until I got this working, and I have no idea why it started working.
    //  If you don't get this working, you are not alone: https://github.com/atsushieno/android-audio-plugin-framework/issues/85
    // Also note that you have to use custom sdk channel so far: ./gradlew testDevice1DebugAndroidTest -Pandroid.sdk.channel=3
    testOptions {
        devices {
            this.register<com.android.build.api.dsl.ManagedVirtualDevice>("testDevice1") {
                device = "Pixel 5"
                apiLevel = 30
                systemImageSource = "aosp-atd"
                //abi = "x86"
            }
        }
    }
}

apply { from ("../publish-pom.gradle") }

dependencies {
    implementation (libs.androidx.core.ktx)
    implementation (libs.kotlin.stdlib.jdk7)
    implementation(libs.coroutines.core)
    implementation (libs.startup.runtime)
    testImplementation (libs.junit)
    androidTestImplementation (libs.test.core)
    androidTestImplementation (libs.test.rules)
    androidTestImplementation (libs.test.runner)
    androidTestImplementation (libs.test.ext.junit)
    androidTestImplementation (libs.test.espresso.core)
}
