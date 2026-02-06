plugins {
    alias(libs.plugins.android.library)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.dokka)
    alias(libs.plugins.vanniktech.maven.publish)
    signing
}

apply { from ("../common.gradle") }

// What a mess...
val enable_asan: Boolean by rootProject
version = libs.versions.aap.core.get()

android {
    namespace = "org.androidaudioplugin"
    this.ext["description"] = "AndroidAudioPlugin - core"

    defaultConfig {

        externalNativeBuild {
            cmake {
                arguments ("-DANDROID_HOME=" + android.sdkDirectory.path, "-DANDROID_STL=c++_shared", "-DAAP_ENABLE_ASAN=" + (if (enable_asan) "1" else "0"))
            }
        }
    }

    buildTypes {
        debug {
            packaging.jniLibs.keepDebugSymbols.add("**/*.so")
            externalNativeBuild {
                cmake {
                    cppFlags ("-Werror")
                }
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

    buildFeatures {
        prefab = true
        prefabPublishing = true
        aidl = true
    }
    externalNativeBuild {
        cmake {
            version = libs.versions.cmake.get()
            path ("src/main/cpp/CMakeLists.txt")
        }
    }
    prefab {
        create("androidaudioplugin") {
            headers = "../include"
        }
    }
    // https://github.com/google/prefab/issues/127
    packaging.jniLibs.excludes.add("**/libc++_shared.so")

    lint {
        disable.add("EnsureInitializerMetadata")
    }
}

dependencies {
    implementation (libs.androidx.core.ktx)
    implementation (libs.kotlin.stdlib.jdk8)
    implementation (libs.preference.ktx) {
        exclude(group = "androidx.lifecycle", module = "lifecycle-viewmodel")
        exclude(group = "androidx.lifecycle", module = "lifecycle-viewmodel-ktx")
    }
    implementation(libs.coroutines.core) {
        exclude(group = "org.jetbrains.kotlin", module = "kotlin-stdlib-jdk8")
    }
    implementation (libs.lifecycle.service)
    implementation (libs.startup.runtime)
    implementation(libs.accompanist.permissions)
    testImplementation (libs.junit)
    androidTestImplementation (libs.test.ext.junit)
    androidTestImplementation (libs.test.espresso.core)
}

val gitProjectName = "aap-core"
val packageName = project.name
val packageDescription = android.ext["description"].toString()
// my common settings
val packageUrl = "https://github.com/atsushieno/$gitProjectName"
val licenseName = "MIT"
val licenseUrl = "https://github.com/atsushieno/$gitProjectName/blob/main/LICENSE"
val devId = "atsushieno"
val devName = "Atsushi Eno"
val devEmail = "atsushieno@gmail.com"

// Common copy-pasted
mavenPublishing {
    publishToMavenCentral()
    if (project.hasProperty("mavenCentralUsername") || System.getenv("ORG_GRADLE_PROJECT_mavenCentralUsername") != null)
        signAllPublications()
    coordinates(group.toString(), project.name, version.toString())
    pom {
        name.set(packageName)
        description.set(packageDescription)
        url.set(packageUrl)
        scm { url.set(packageUrl) }
        licenses { license { name.set(licenseName); url.set(licenseUrl) } }
        developers { developer { id.set(devId); name.set(devName); email.set(devEmail) } }
    }
}
