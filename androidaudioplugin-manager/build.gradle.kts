plugins {
    alias(libs.plugins.android.library)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.dokka)
    alias(libs.plugins.vanniktech.maven.publish)
    signing
}

apply { from ("../common.gradle") }

version = libs.versions.aap.core.get()

android {
    namespace = "org.androidaudioplugin.manager"
    this.ext["description"] = "AndroidAudioPlugin - Plugin Manager Backend"

    defaultConfig {
        externalNativeBuild {
            cmake {
                arguments ("-DANDROID_STL=c++_shared")
            }
        }
    }

    buildTypes {
        debug {
            packaging.jniLibs.keepDebugSymbols.add("**/*.so")
            externalNativeBuild {
                cmake {
                    // we cannot error out cmidi2.h as we don't compile cmidi2_test.h
                    //cppFlags ("-Werror")
                }
            }
        }
        release {
            isMinifyEnabled = false
            proguardFiles (getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    buildFeatures {
        prefab = true
        prefabPublishing = true
    }
    externalNativeBuild {
        cmake {
            version = libs.versions.cmake.get()
            path ("src/main/cpp/CMakeLists.txt")
        }
    }
    prefab {
        create("androidaudioplugin-manager") {
            name = "androidaudioplugin-manager"
        }
    }
    // https://github.com/google/prefab/issues/127
    packaging {
        jniLibs.excludes.add("**/libc++_shared.so")
        jniLibs.excludes.add("**/libandroidaudioplugin.so") // package it separately
    }
}

dependencies {
    implementation (project(":androidaudioplugin"))
    implementation (libs.androidx.core.ktx)
    implementation (libs.startup.runtime)
    implementation (libs.coroutines.core)
    implementation (libs.oboe)
    implementation (libs.ktmidi)

    //testImplementation (libs.junit)
    androidTestImplementation (libs.test.ext.junit)
    androidTestImplementation (libs.test.espresso.core)
}

// Workaround for AGP/prefab issue: cmake for consuming modules may generate INTERFACE IMPORTED
// targets when the producing module's .so hasn't been built yet at configure time.
// AGP only tracks prefab_publication.json (not .so content) for cmake cache invalidation,
// so we explicitly order configure after the prefab package task (which contains the .so).
// See also: https://issuetracker.google.com/issues/385751152
gradle.projectsEvaluated {
    val aapProject = rootProject.project("androidaudioplugin")
    tasks.matching { it.name.startsWith("configureCMakeDebug") }.configureEach {
        dependsOn(aapProject.tasks["prefabDebugPackage"])
    }
    tasks.matching { it.name.startsWith("configureCMakeRelWithDebInfo") }.configureEach {
        dependsOn(aapProject.tasks["prefabReleasePackage"])
    }
    tasks["buildCMakeDebug"].dependsOn(aapProject.tasks["mergeDebugNativeLibs"])
    tasks["buildCMakeRelWithDebInfo"].dependsOn(aapProject.tasks["mergeReleaseNativeLibs"])
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
