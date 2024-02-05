plugins {
    id ("com.android.application")
    id ("kotlin-android")
}

apply { from ("../../common.gradle") }

// What a mess...
val enable_asan: Boolean by rootProject

android {
    namespace = "org.androidaudioplugin.aapinstrumentsample"
    defaultConfig {
        applicationId = "org.androidaudioplugin.aapinstrumentsample"

        externalNativeBuild {
            cmake {
                arguments ("-DANDROID_STL=c++_shared", "-DAAP_ENABLE_ASAN=" + (if (enable_asan) "1" else "0"))
            }
        }
    }
    externalNativeBuild {
        cmake {
            version = libs.versions.cmake.get()
            path ("src/main/cpp/CMakeLists.txt")
        }
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
        resources {
            excludes += setOf("META-INF/AL2.0", "META-INF/LGPL2.1")
        }
        if (enable_asan)
            jniLibs.useLegacyPackaging = true
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
    implementation(libs.aap.core)
    implementation(libs.aap.midi.device.service)
    implementation(libs.aap.ui.web)
    implementation(libs.aap.ui.compose)
    implementation(libs.aap.ui.compose.app)
    androidTestImplementation(libs.aap.testing)
    /*
    implementation (project(":androidaudioplugin"))
    implementation (project(":androidaudioplugin-ui-compose"))
    implementation (project(":androidaudioplugin-ui-compose-app"))
    implementation (project(":androidaudioplugin-ui-web"))
    implementation (project(":androidaudioplugin-midi-device-service"))
    androidTestImplementation (project(":androidaudioplugin-testing"))
     */
    implementation (libs.androidx.core.ktx)
    implementation (libs.kotlin.stdlib.jdk8)
    implementation (libs.androidx.appcompat)

    testImplementation (libs.junit)
    androidTestImplementation (libs.test.rules)
    androidTestImplementation (libs.test.ext.junit)
    androidTestImplementation (libs.test.espresso.core)
    //androidTestImplementation (libs.compose.ui.test.junit)
}
