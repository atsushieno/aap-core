plugins {
    id ("com.android.library")
    id ("kotlin-android")
    id ("maven-publish")
    id ("signing")
}

apply { from ("../common.gradle") }

android {
    ext["description"] = "AndroidAudioPlugin - testing"

    defaultConfig {
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        consumerProguardFiles ("consumer-rules.pro")
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles (getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    namespace = "org.androidaudioplugin.androidaudioplugin.testing"
}

apply { from ("../publish-pom.gradle") }

dependencies {
    implementation (project(":androidaudioplugin"))

    implementation (libs.androidx.core.ktx)
    implementation (libs.kotlin.stdlib.jdk7)
    implementation (libs.androidx.appcompat)
    implementation (libs.coroutines.core)
    implementation (libs.coroutines.android)
    implementation (libs.junit)

    testImplementation (libs.junit)
    testImplementation (libs.test.core)
    androidTestImplementation (libs.test.ext.junit)
    androidTestImplementation (libs.test.espresso.core)
}
