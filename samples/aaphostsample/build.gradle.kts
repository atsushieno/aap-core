plugins {
    id ("com.android.application")
    id ("kotlin-android")
}

apply { from ("../../common.gradle") }

// What a mess...
val enable_asan: Boolean by rootProject

android {
    namespace = "org.androidaudioplugin.aaphostsample"
    defaultConfig {
        applicationId = "org.androidaudioplugin.aaphostsample"
    }
    buildFeatures {
        prefab = true
    }
    buildTypes {
        debug {
            packaging {
                jniLibs.keepDebugSymbols.add("**/*.so")
            }
        }
        release {
            isMinifyEnabled = false
            proguardFiles (getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    packaging {
        if (enable_asan)
            jniLibs.useLegacyPackaging = true
    }
    androidResources {
        noCompress += listOf("sample.wav")
    }

    // FIXME: it is annoying to copy this everywhere, but build.gradle.kts is incapable of importing this fragment...
    // It's been long time until I got this working, and I have no idea why it started working.
    //  If you don't get this working, you are not alone: https://github.com/atsushieno/aap-core/issues/85
    // Also note that you have to use custom sdk channel so far: ./gradlew testDevice1DebugAndroidTest -Pandroid.sdk.channel=3
    testOptions {
        managedDevices {
            devices {
                maybeCreate<com.android.build.api.dsl.ManagedVirtualDevice> ("testDevice1").apply {
                    device = "Pixel 2"
                    apiLevel = 30
                    systemImageSource = "aosp-atd"
                }
            }
        }
    }
}

dependencies {
    implementation (project(":androidaudioplugin"))
    implementation (project(":androidaudioplugin-samples-host-engine"))
    implementation (project(":androidaudioplugin-ui-compose-app"))
    androidTestImplementation (project(":androidaudioplugin-testing"))

    runtimeOnly (libs.libcxx.provider)

    implementation (libs.androidx.core.ktx)
    implementation (libs.kotlin.stdlib.jdk8)
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
