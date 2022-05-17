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
    defaultConfig {
        applicationId = "org.androidaudioplugin.aap_midi_device_service.app"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
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
        kotlinCompilerExtensionVersion = compose_version
    }
    packagingOptions {
        exclude ("META-INF/AL2.0")
        exclude ("META-INF/LGPL2.1")
        if (enable_asan)
            jniLibs.useLegacyPackaging = true
    }
}

dependencies {
    implementation ("androidx.core:core-ktx:1.7.0")
    implementation ("org.jetbrains.kotlin:kotlin-stdlib-jdk7:$kotlin_version")
    implementation ("androidx.appcompat:appcompat:1.4.1")
    implementation ("androidx.preference:preference-ktx:1.2.0")
    implementation (project(":androidaudioplugin"))
    implementation (project(":androidaudioplugin-midi-device-service"))
    implementation ("com.google.android.material:material:1.6.0")
    implementation ("androidx.compose.ui:ui:$compose_version")
    implementation ("androidx.compose.material:material:$compose_version")
    implementation ("androidx.compose.ui:ui-tooling:$compose_version")
    implementation ("androidx.lifecycle:lifecycle-runtime-ktx:2.4.1")
    implementation ("androidx.activity:activity-compose:1.4.0")
    implementation ("androidx.webkit:webkit:1.4.0")
    testImplementation ("junit:junit:4.13.2")
    androidTestImplementation ("androidx.test.ext:junit:1.1.3")
    androidTestImplementation ("androidx.test.espresso:espresso-core:3.4.0")
    androidTestImplementation ("androidx.compose.ui:ui-test-junit4:$compose_version")
}
