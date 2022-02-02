plugins {
    id ("com.android.library")
    id ("kotlin-android")
    id ("org.jetbrains.dokka")
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
    this.ext["description"] = "AndroidAudioPlugin - core"

    defaultConfig {
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        consumerProguardFiles ("consumer-rules.pro")

        externalNativeBuild {
            cmake {
                // https://github.com/google/prefab/blob/bccf5a6a75b67add30afbb6d4f7a7c50081d2d86/api/src/main/kotlin/com/google/prefab/api/Android.kt#L243
                arguments ("-DANDROID_STL=c++_shared")
                cppFlags ("-Werror")
            }
        }
    }

    buildTypes {
        debug {
            packagingOptions {
                doNotStrip ("**/*.so")
            }
        }
        release {
            isMinifyEnabled = false
            proguardFiles (
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    externalNativeBuild {
        cmake {
            version = "3.18.1"
            path ("../../native/androidaudioplugin/CMakeLists.txt")
        }
    }

    // FIXME: PREFAB: enable these sections once we migrate to prefab-based solution.
    /*
    buildFeatures {
        prefabPublishing true
    }
    // Workaround for https://issuetracker.google.com/issues/168777344#comment5
    packagingOptions {
        exclude '**.so'
    }
    prefab {
        androidaudioplugin {
            name 'androidaudioplugin'
            // WARNING: It's not working https://github.com/atsushieno/android-audio-plugin-framework/issues/57
            // There is no way to specify more than one include directory.
            // headers '../../native/androidaudioplugin/android/include'
            // FIXME: remove this dummy headers dir hack once https://issuetracker.google.com/issues/172105145 is supported.
            headers '../../dummy-prefab-headers/include'
        }
    }
    */
}

apply { from ("../publish-pom.gradle") }

dependencies {
    implementation ("androidx.core:core-ktx:1.7.0")
    implementation ("org.jetbrains.kotlin:kotlin-stdlib-jdk7:$kotlin_version")
    testImplementation ("junit:junit:4.13.2")
    androidTestImplementation ("androidx.test:core:1.4.0")
    androidTestImplementation ("androidx.test:rules:1.4.0")
    androidTestImplementation ("androidx.test:runner:1.4.0")
    androidTestImplementation ("androidx.test.ext:junit:1.1.3")
    androidTestImplementation ("androidx.test.espresso:espresso-core:3.4.0")
}
