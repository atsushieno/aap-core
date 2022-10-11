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
    buildFeatures {
        viewBinding = true
    }
    defaultConfig {
        applicationId = "org.androidaudioplugin.aaphostsample"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }
    buildFeatures {
        prefab = true
    }
    buildTypes {
        debug {
            packagingOptions {
                jniLibs.keepDebugSymbols.add("**/*.so")
            }
        }
        release {
            isMinifyEnabled = false
            proguardFiles (getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    aaptOptions {
        noCompress ("sample.wav")
    }
    packagingOptions {
        if (enable_asan)
            jniLibs.useLegacyPackaging = true
    }

    // FIXME: it is annoying to copy this everywhere, but build.gradle.kts is incapable of importing this fragment...
    // It's been long time until I got this working, and I have no idea why it started working.
    //  If you don't get this working, you are not alone: https://github.com/atsushieno/android-audio-plugin-framework/issues/85
    // Also note that you have to use custom sdk channel so far: ./gradlew testDevice2DebugAndroidTest -Pandroid.sdk.channel=3
    testOptions {
        devices {
            this.register<com.android.build.api.dsl.ManagedVirtualDevice>("testDevice2") {
                device = "Pixel 5"
                apiLevel = 30
                systemImageSource = "aosp-atd"
                //abi = "x86"
            }
        }
    }
}

dependencies {
    implementation (project(":androidaudioplugin"))
    implementation (project(":androidaudioplugin-samples-host-engine"))
    implementation (project(":androidaudioplugin-ui-compose"))

    runtimeOnly ("dev.atsushieno:libcxx-provider:24.0.8215888")

    implementation ("androidx.core:core-ktx:1.9.0")
    implementation ("org.jetbrains.kotlin:kotlin-stdlib-jdk7:$kotlin_version")
    implementation ("androidx.appcompat:appcompat:1.4.1")

    implementation ("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.6.4")
    implementation ("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.6.4")

    implementation ("androidx.compose.ui:ui:$compose_version")

    testImplementation ("junit:junit:4.13.2")

    androidTestImplementation ("androidx.test:core:1.4.0")
    androidTestImplementation ("androidx.test:rules:1.4.0")
    androidTestImplementation ("androidx.test:runner:1.4.0")
    androidTestImplementation ("androidx.test.ext:junit:1.1.3")
    androidTestImplementation ("androidx.test.espresso:espresso-core:3.4.0")
}

// Starting AGP 7.0.0-alpha05, AGP stopped caring build dependencies and it broke builds.
// This is a forcible workarounds to build libandroidaudioplugin.so in prior to referencing it.
/*
gradle.projectsEvaluated {
    mergeDebugNativeLibs.dependsOn(rootProject.project("androidaudioplugin").mergeDebugNativeLibs)
    mergeReleaseNativeLibs.dependsOn(rootProject.project("androidaudioplugin").mergeReleaseNativeLibs)
}
*/
