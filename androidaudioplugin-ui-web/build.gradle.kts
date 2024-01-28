plugins {
    id ("com.android.library")
    id ("kotlin-android")
    id ("org.jetbrains.dokka")
    id ("maven-publish")
    id ("signing")
}

apply { from ("../common.gradle") }

android {
    namespace = "org.androidaudioplugin.ui.web"
    ext["description"] = "AndroidAudioPlugin - UI (Web)"

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles (getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
}

apply { from ("../publish-pom.gradle") }

dependencies {
    implementation (project(":androidaudioplugin"))
    implementation (libs.androidx.core.ktx)
    implementation (libs.kotlin.stdlib.jdk8)
    implementation (libs.androidx.appcompat)
    implementation (libs.webkit)
    implementation (libs.ktmidi)

    androidTestImplementation (libs.junit)
    androidTestImplementation (libs.test.ext.junit)
    androidTestImplementation (libs.test.espresso.core)
}
