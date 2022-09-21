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
}

apply { from ("../publish-pom.gradle") }

dependencies {
    implementation (project(":androidaudioplugin"))
    implementation ("androidx.core:core-ktx:1.7.0")
    implementation ("org.jetbrains.kotlin:kotlin-stdlib-jdk7:$kotlin_version")
    implementation ("androidx.appcompat:appcompat:1.4.1")
    implementation ("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.6.0")
    implementation ("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.6.0")
    implementation ("dev.atsushieno:ktmidi:0.3.19")
    implementation ("dev.atsushieno:mugene:0.2.26")
    testImplementation ("junit:junit:4.13.2")
    androidTestImplementation ("androidx.test.ext:junit:1.1.3")
    androidTestImplementation ("androidx.test.espresso:espresso-core:3.4.0")
}
