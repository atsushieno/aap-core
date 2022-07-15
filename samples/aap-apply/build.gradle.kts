plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "org.androidaudioplugin.samples.aap_apply"
    compileSdk = 32

    defaultConfig {
        applicationId = "org.androidaudioplugin.samples.aap_apply"
        minSdk = 30
        targetSdk = 32
        versionCode = 1
        versionName = "1.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        vectorDrawables {
            useSupportLibrary = true
        }

        externalNativeBuild {
            cmake {
                arguments("-DANDROID=1")
            }
        }
    }
    buildFeatures {
        prefab = true
    }
    externalNativeBuild {
        cmake {
            version = "3.18.1"
            path ("src/main/cpp/CMakeLists.txt")
        }
    }
    buildTypes {
        debug {
            packagingOptions {
                jniLibs.keepDebugSymbols.add("**/*.so")
            }
        }
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }
    kotlinOptions {
        jvmTarget = "1.8"
    }
    buildFeatures {
        compose = true
    }
    composeOptions {
        kotlinCompilerExtensionVersion = "1.2.0"
    }
    packagingOptions {
        resources {
            excludes += "/META-INF/{AL2.0,LGPL2.1}"
        }
    }
}

dependencies {
    implementation(project(":androidaudioplugin"))

    implementation("androidx.core:core-ktx:1.8.0")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.5.0")
    implementation("androidx.activity:activity-compose:1.5.0")
    implementation("androidx.compose.ui:ui:${rootProject.extra["compose_version"]}")
    implementation("androidx.compose.ui:ui-tooling-preview:${rootProject.extra["compose_version"]}")
    implementation ("androidx.navigation:navigation-compose:2.5.0")
    implementation("androidx.compose.material:material:1.3.0-alpha01")
    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test.ext:junit:1.1.3")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.4.0")
    androidTestImplementation("androidx.compose.ui:ui-test-junit4:${rootProject.extra["compose_version"]}")
    debugImplementation("androidx.compose.ui:ui-tooling:${rootProject.extra["compose_version"]}")
    debugImplementation("androidx.compose.ui:ui-test-manifest:${rootProject.extra["compose_version"]}")
}