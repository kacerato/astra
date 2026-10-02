// Spike S-02: GameActivity + loop próprio. O Gradle só empacota (doc 04 §12): a .so e os assets saem do
// preset CMake android-arm64-debug (alvo astra_spike_s02).
plugins {
    id("com.android.application") version "9.4.0"
}

val astraRoot = rootDir.resolve("../../..").canonicalFile
val cmakeOut = astraRoot.resolve("build/android-arm64-debug")

android {
    namespace = "dev.astra.spike.s02"
    compileSdk = 36
    ndkVersion = "29.0.14206865"

    defaultConfig {
        applicationId = "dev.astra.spike.s02"
        minSdk = 29
        targetSdk = 36
        versionCode = 1
        versionName = "s02"
        ndk { abiFilters += "arm64-v8a" }
    }

    sourceSets {
        getByName("main") {
            manifest.srcFile("AndroidManifest.xml")
            res.srcDirs("res")
            assets.srcDirs(cmakeOut.resolve("spikes/s02_gameactivity/assets"))
            jniLibs.srcDirs(cmakeOut.resolve("jniLibs-s02"))
        }
    }

    androidResources {
        noCompress += listOf("ktx", "spv", "data", "cfg")
    }

    packaging {
        jniLibs { useLegacyPackaging = false }
    }
}

dependencies {
    // Mesma versão dos headers e da lib estática em third_party/games-activity.
    implementation("androidx.games:games-activity:4.4.2")
    implementation("androidx.appcompat:appcompat:1.7.1")
}
