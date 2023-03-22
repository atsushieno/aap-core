plugins {
    id ("com.android.library")
    id ("kotlin-android")
    id ("maven-publish")
    id ("signing")
}

apply { from ("../common.gradle") }

android {
    namespace = "org.androidaudioplugin.ui.compose"
    ext["description"] = "AndroidAudioPlugin - UI (Jetpack Compose)"

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
        kotlinCompilerExtensionVersion = libs.versions.kotlin.compose.compiler.extension.get()
    }
}

apply { from ("../publish-pom.gradle") }

dependencies {
    implementation (project(":androidaudioplugin"))
    implementation (project(":androidaudioplugin-samples-host-engine"))
    implementation (project(":androidaudioplugin-ui-web"))
    implementation (libs.androidx.core.ktx)
    implementation (libs.kotlin.stdlib.jdk8)
    implementation (libs.androidx.appcompat)

    implementation (libs.compose.ui)
    implementation (libs.compose.material)
    implementation (libs.compose.ui.tooling)

    implementation (libs.navigation.compose)

    androidTestImplementation (libs.junit)
    androidTestImplementation (libs.test.ext.junit)
    androidTestImplementation (libs.test.espresso.core)
}
