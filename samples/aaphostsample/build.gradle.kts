plugins {
    id ("com.android.application")
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
    buildFeatures {
        viewBinding = true
    }
    defaultConfig {
        applicationId = "org.androidaudioplugin.aaphostsample"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }
    buildFeatures {
        prefab = true
    }
    buildTypes {
        debug {
            packagingOptions {
                jniLibs.keepDebugSymbols.add("**/*.so")
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

    // FIXME: it is annoying to copy this everywhere, but build.gradle.kts is incapable of importing this fragment...
    // It's been long time until I got this working, and I have no idea why it started working.
    //  If you don't get this working, you are not alone: https://github.com/atsushieno/android-audio-plugin-framework/issues/85
    // Also note that you have to use custom sdk channel so far: ./gradlew testDevice2DebugAndroidTest -Pandroid.sdk.channel=3
    testOptions {
        managedDevices {
            devices {
                this.register<com.android.build.api.dsl.ManagedVirtualDevice>("testDevice") {
                    device = "Pixel 5"
                    apiLevel = 30
                    systemImageSource = "aosp-atd"
                    //abi = "x86"
                }
            }
        }
    }
    androidResources {
        noCompress += listOf("sample.wav")
    }
    namespace = "org.androidaudioplugin.aaphostsample"
}

dependencies {
    implementation (project(":androidaudioplugin"))
    implementation (project(":androidaudioplugin-samples-host-engine"))
    implementation (project(":androidaudioplugin-ui-compose"))
    androidTestImplementation (project(":androidaudioplugin-testing"))

    runtimeOnly (libs.libcxx.provider)

    implementation (libs.androidx.core.ktx)
    implementation (libs.kotlin.stdlib.jdk7)
    implementation (libs.androidx.appcompat)
    implementation (libs.coroutines.core)
    implementation (libs.coroutines.android)

    implementation (libs.compose.ui)

    testImplementation (libs.junit)
    androidTestImplementation (libs.test.rules)
    androidTestImplementation (libs.test.ext.junit)
    androidTestImplementation (libs.test.espresso.core)
}

// Starting AGP 7.0.0-alpha05, AGP stopped caring build dependencies and it broke builds.
// This is a forcible workarounds to build libandroidaudioplugin.so in prior to referencing it.
gradle.projectsEvaluated {
    tasks["mergeDebugNativeLibs"].dependsOn(rootProject.project("androidaudioplugin").tasks["mergeDebugNativeLibs"])
    tasks["mergeReleaseNativeLibs"].dependsOn(rootProject.project("androidaudioplugin").tasks["mergeReleaseNativeLibs"])
}
