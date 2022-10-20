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
        compose = true
        prefab = true
    }
    defaultConfig {
        applicationId = "org.androidaudioplugin.samples.aaphostsample2"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        externalNativeBuild {
            cmake {
                arguments("-DANDROID_STL=c++_shared", "-DBUILD_WITH_PREFAB=1")
            }
        }
    }
    buildTypes {
        debug {
            packagingOptions {
                jniLibs.keepDebugSymbols.add("**/*.so")
            }
        }
        release {
            proguardFiles (getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    packagingOptions {
        if (enable_asan)
            jniLibs.useLegacyPackaging = true
    }
    externalNativeBuild {
        cmake {
            version = "3.22.1"
            path ("src/main/cpp/CMakeLists.txt")
        }
    }
    /*
    prefab {
        create("androidaudioplugin-samples-host-engine2") {
            name = "androidaudioplugin-samples-host-engine2"
        }
    }*/

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
    androidResources {
        noCompress += listOf("sample.wav")
    }
    composeOptions {
        kotlinCompilerExtensionVersion = compose_version
    }
    namespace = "org.androidaudioplugin.samples.aaphostsample2"
}

dependencies {
    implementation (project(":androidaudioplugin"))

    implementation ("androidx.core:core-ktx:1.7.0")
    implementation ("org.jetbrains.kotlin:kotlin-stdlib-jdk7:$kotlin_version")
    implementation ("androidx.appcompat:appcompat:1.5.1")

    implementation ("androidx.startup:startup-runtime:1.1.1")

    implementation ("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.6.4")
    implementation ("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.6.4")

    implementation ("androidx.compose.ui:ui:$compose_version")
    implementation ("androidx.compose.material:material:$compose_version")
    implementation ("androidx.compose.ui:ui-tooling:$compose_version")
    implementation ("androidx.navigation:navigation-compose:2.5.2")

    testImplementation ("junit:junit:4.13.2")

    androidTestImplementation ("androidx.test:core:1.4.0")
    androidTestImplementation ("androidx.test:rules:1.4.0")
    androidTestImplementation ("androidx.test:runner:1.4.0")
    androidTestImplementation ("androidx.test.ext:junit:1.1.3")
    androidTestImplementation ("androidx.test.espresso:espresso-core:3.4.0")
}

// Starting AGP 7.0.0-alpha05, AGP stopped caring build dependencies and it broke builds.
// This is a forcible workarounds to build libandroidaudioplugin.so in prior to referencing it.
gradle.projectsEvaluated {
    tasks["mergeDebugNativeLibs"].dependsOn(rootProject.project("androidaudioplugin").tasks["mergeDebugNativeLibs"])
    tasks["mergeReleaseNativeLibs"].dependsOn(rootProject.project("androidaudioplugin").tasks["mergeReleaseNativeLibs"])
}
