plugins {
    id ("com.android.library")
    id ("kotlin-android")
    id ("maven-publish")
    id ("signing")
}

apply { from ("../common.gradle") }

android {
    ext["description"] = "AndroidAudioPlugin - sample host engine (not for public use)"

    defaultConfig {
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles (getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    namespace = "org.androidaudioplugin.samples.host.engine"
}

apply { from ("../publish-pom.gradle") }

dependencies {
    implementation (project(":androidaudioplugin"))
    implementation (project(":androidaudioplugin-midi-device-service"))

    implementation (libs.androidx.core.ktx)
    implementation (libs.kotlin.stdlib.jdk7)
    implementation (libs.androidx.appcompat)
    implementation(libs.coroutines.core)
    implementation(libs.coroutines.android)
    implementation(libs.ktmidi)
    implementation(libs.mugene)

    testImplementation (libs.junit)
    androidTestImplementation (libs.test.ext.junit)
    androidTestImplementation (libs.test.espresso.core)
}
