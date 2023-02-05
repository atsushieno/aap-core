plugins {
    id ("com.android.application")
    id ("kotlin-android")
}

apply { from ("../../common.gradle") }

// What a mess...
val enable_asan: Boolean by rootProject

android {
    namespace = "org.androidaudioplugin.midideviceservice.app"
    defaultConfig {
        applicationId = "org.androidaudioplugin.aap_midi_device_service.app"
    }

    buildTypes {
        debug {
            packagingOptions {
                doNotStrip ("**/*.so")
            }
        }
        release {
            proguardFiles (getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    buildFeatures {
        compose = true
    }
    composeOptions {
        kotlinCompilerExtensionVersion = libs.versions.compose.get()
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
    implementation (project(":androidaudioplugin-ui-web"))
    implementation (project(":androidaudioplugin-midi-device-service"))

    runtimeOnly (libs.libcxx.provider)

    implementation (libs.androidx.core.ktx)
    implementation (libs.kotlin.stdlib.jdk7)
    implementation (libs.androidx.appcompat)
    implementation (libs.preference.ktx)
    implementation (libs.material)

    implementation (platform(libs.compose.bom.get()))
    androidTestImplementation (platform(libs.compose.bom.get()))

    implementation (libs.compose.ui)
    implementation (libs.compose.material)
    implementation (libs.compose.ui.tooling)

    implementation (libs.lifecycle.runtime.ktx)
    implementation (libs.activity.compose)
    implementation (libs.webkit)

    testImplementation (libs.junit)
    androidTestImplementation (libs.test.ext.junit)
    androidTestImplementation (libs.test.espresso.core)
}
