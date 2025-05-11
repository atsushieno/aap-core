import com.vanniktech.maven.publish.AndroidMultiVariantLibrary

plugins {
    alias(libs.plugins.android.library)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.dokka)
    alias(libs.plugins.vanniktech.maven.publish)
    signing
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
// "mavenPublishing" could not resolve reference to com.vanniktech.maven.publish.SonatypeHost. Another reason Gradle should die.
mavenPublishing {
    configure(AndroidMultiVariantLibrary())
    publishToMavenCentral(com.vanniktech.maven.publish.SonatypeHost.CENTRAL_PORTAL)
}

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
