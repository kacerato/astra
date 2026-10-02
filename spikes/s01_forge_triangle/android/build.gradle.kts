// Spike S-01: o Gradle só empacota (doc 04 §12). A .so e os assets saem do preset CMake
// android-arm64-debug; rode `cmake --build --preset android-arm64-debug` antes.
plugins {
    id("com.android.application") version "9.4.0"
}

val astraRoot = rootDir.resolve("../../..").canonicalFile
val tfPackaging = astraRoot.resolve("third_party/the-forge/Common_3/OS/Android/Packaging/app/src/main")
val cmakeOut = astraRoot.resolve("build/android-arm64-debug")

android {
    namespace = "dev.astra.spike.s01"
    compileSdk = 36
    ndkVersion = "29.0.14206865"

    defaultConfig {
        applicationId = "dev.astra.spike.s01"
        minSdk = 29
        targetSdk = 36
        versionCode = 1
        versionName = "s01"
        resValue("string", "app_name", "Astra S-01")
        resValue("string", "lib_name", "ForgeGame")
        ndk { abiFilters += "arm64-v8a" }
    }

    sourceSets {
        getByName("main") {
            manifest.srcFile("AndroidManifest.xml")
            // ForgeBaseActivity (pacote com.forge.unittest) e o tema vêm do próprio TF: os nomes JNI
            // compilados em AndroidBase.cpp dependem desse pacote.
            java.srcDirs(tfPackaging.resolve("java"))
            res.srcDirs(tfPackaging.resolve("res"))
            assets.srcDirs(cmakeOut.resolve("spikes/s01_forge_triangle/assets"))
            jniLibs.srcDirs(cmakeOut.resolve("jniLibs"))
        }
    }

    androidResources {
        noCompress += listOf("ktx", "spv", "data", "cfg")
    }

    buildFeatures {
        resValues = true
    }

    packaging {
        jniLibs { useLegacyPackaging = false }
    }
}

dependencies {
    implementation("androidx.annotation:annotation:1.9.1")
    // Mesmas versões do Packaging/app/build.gradle do TF 1.63: as libs estáticas do AGDK
    // (paddleboat, memory_advice) chamam essas classes Java.
    implementation("androidx.games:games-controller:2.0.1")
    implementation("androidx.games:games-memory-advice:2.0.0-beta04")
}
