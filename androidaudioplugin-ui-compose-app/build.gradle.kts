import com.vanniktech.maven.publish.AndroidMultiVariantLibrary

plugins {
    alias(libs.plugins.android.library)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.compose.compiler)
    alias(libs.plugins.dokka)
    alias(libs.plugins.vanniktech.maven.publish)
    signing
}

apply { from ("../common.gradle") }

android {
    namespace = "org.androidaudioplugin.ui.compose.app"
    ext["description"] = "AndroidAudioPlugin - Manager App (Compose)"

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles (getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    buildFeatures {
        compose = true
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }
    kotlinOptions {
        jvmTarget = "1.8"
    }
    defaultConfig {
        vectorDrawables {
            useSupportLibrary = true
        }
    }
    packaging {
        resources {
            excludes += "/META-INF/{AL2.0,LGPL2.1}"
        }
    }
}

apply { from ("../publish-pom.gradle") }
// "mavenPublishing" could not resolve reference to com.vanniktech.maven.publish.SonatypeHost. Another reason Gradle should die.
mavenPublishing {
    configure(AndroidMultiVariantLibrary())
    publishToMavenCentral(com.vanniktech.maven.publish.SonatypeHost.CENTRAL_PORTAL)
}

dependencies {
    implementation (project(":androidaudioplugin"))
    implementation (project(":androidaudioplugin-manager"))
    implementation (project(":androidaudioplugin-ui-compose"))
    implementation (project(":androidaudioplugin-ui-web"))
    implementation (libs.compose.audio.controls)
    implementation (libs.androidx.core.ktx)
    implementation (libs.kotlin.stdlib.jdk8)
    implementation (libs.androidx.appcompat)

    implementation (libs.accompanist.drawablepainter)
    implementation (libs.accompanist.permissions)

    implementation(platform(libs.compose.bom))
    implementation (libs.navigation.compose)
    implementation(libs.lifecycle.runtime.ktx)
    implementation(libs.activity.compose)
    implementation(libs.ui.tooling.preview)

    androidTestImplementation (libs.junit)
    androidTestImplementation (libs.test.ext.junit)
    androidTestImplementation (libs.test.espresso.core)
    androidTestImplementation(platform(libs.compose.bom))
}
