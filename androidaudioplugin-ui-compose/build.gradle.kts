plugins {
    id ("com.android.library")
    id ("kotlin-android")
    id ("maven-publish")
    id ("signing")
}

apply { from ("../common.gradle") }

android {
    ext["description"] = "AndroidAudioPlugin - UI (Jetpack Compose)"

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
    buildFeatures {
        compose = true
    }
    composeOptions {
        kotlinCompilerExtensionVersion = libs.versions.compose.get()
    }
    namespace = "org.androidaudioplugin.ui.compose"
}

apply { from ("../publish-pom.gradle") }

dependencies {
    implementation (project(":androidaudioplugin"))
    implementation (project(":androidaudioplugin-samples-host-engine"))
    implementation (libs.androidx.core.ktx)
    implementation (libs.kotlin.stdlib.jdk7)
    implementation (libs.androidx.appcompat)

    implementation (libs.compose.ui)
    implementation (libs.compose.material)
    implementation (libs.compose.ui.tooling)

    implementation (libs.navigation.compose)

    androidTestImplementation (libs.junit)
    androidTestImplementation (libs.test.ext.junit)
    androidTestImplementation (libs.test.espresso.core)
}
