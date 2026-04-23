import com.android.build.gradle.internal.tasks.factory.dependsOn
import java.io.IOException
import java.security.MessageDigest
import java.util.regex.Pattern
import javax.xml.bind.DatatypeConverter

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.compose)
    alias(libs.plugins.kotlin.serialization)
    alias(libs.plugins.kotlinx.gettext)
    alias(libs.plugins.aboutlibraries.android)
}

fun String.runCommand(workingDir: File = File(".")): String? {
    try {
        val proc = ProcessBuilder(*trim().split("\\s".toRegex()).toTypedArray())
            .directory(workingDir)
            .redirectOutput(ProcessBuilder.Redirect.PIPE)
            .redirectError(ProcessBuilder.Redirect.PIPE)
            .start()
        assert(proc.waitFor(1, TimeUnit.MINUTES))
        return proc.inputStream.bufferedReader().readText()
    } catch (e: IOException) {
        e.printStackTrace()
        return null
    }
}

fun getGitHash(): String? = "git log --format=%h -1".runCommand()?.trim()

val versionMajor: Int? = System.getenv("EMULATOR_VERSION_MAJOR")?.toIntOrNull()
val versionMinor: Int? = System.getenv("EMULATOR_VERSION_MINOR")?.toIntOrNull()

fun getVersionName(): String {
    if (versionMajor != null && versionMinor != null)
        return "$versionMajor.$versionMinor"
    return getGitHash() ?: "1.0"
}

val cemuDataFilesFolder = "../../../bin"

android {
    namespace = "info.cemu.cemu"
    compileSdk = 36
    ndkVersion = "29.0.14206865"
    defaultConfig {
        applicationId = "info.cemu.cemu"
        minSdk = 30
        targetSdk = 35
        versionName = getVersionName()
        versionCode = 1
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    androidResources {
        ignoreAssetsPattern = "!*cemu.mo:"
    }

    sourceSets.getByName("main") {
        assets {
            directories.add(cemuDataFilesFolder)
        }
    }

    packaging {
        jniLibs.useLegacyPackaging = true
    }

    val keystoreFilePath: String? = System.getenv("ANDROID_STORE_FILE")

    signingConfigs {
        if (keystoreFilePath != null) {
            create("release") {
                storeFile = file(keystoreFilePath)
                storePassword = System.getenv("ANDROID_KEY_STORE_PASSWORD")
                keyAlias = System.getenv("ANDROID_KEY_ALIAS")
                keyPassword = System.getenv("ANDROID_KEY_STORE_PASSWORD")
            }
        }
    }

    buildTypes {
        debug {
            applicationIdSuffix = ".debug"
        }
        release {
            isMinifyEnabled = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
            signingConfig = if (keystoreFilePath != null) {
                signingConfigs.getByName("release")
            } else {
                signingConfigs.getByName("debug")
            }
        }
    }

    externalNativeBuild {
        cmake {
            version = "3.25.0+"
            path = file("../../../CMakeLists.txt")
        }
    }

    defaultConfig {
        @Suppress("UnstableApiUsage")
        externalNativeBuild {
            cmake {
                arguments(
                    "-DANDROID_STL=c++_shared",
                    "-DENABLE_VCPKG=ON",
                    "-DVCPKG_TARGET_ANDROID=ON",
                    "-DENABLE_SDL=OFF",
                    "-DENABLE_HIDAPI=OFF",
                    "-DENABLE_WXWIDGETS=OFF",
                    "-DENABLE_OPENGL=OFF",
                    "-DENABLE_BLUEZ=OFF",
                    "-DBUNDLE_SPEEX=ON",
                    "-DENABLE_DISCORD_RPC=OFF",
                    "-DENABLE_NSYSHID_LIBUSB=OFF",
                    "-DENABLE_WAYLAND=OFF",
                )
                if (versionMajor != null && versionMinor != null) {
                    arguments.addAll(
                        arrayOf(
                            "-DEMULATOR_VERSION_MAJOR=$versionMajor",
                            "-DEMULATOR_VERSION_MINOR=$versionMinor"
                        )
                    )
                }
                // abiFilters("arm64-v8a", "x86_64")
                abiFilters("arm64-v8a")
            }
        }
    }

    buildFeatures {
        buildConfig = true
        compose = true
    }
}

abstract class ComputeCemuDataFilesHashTask : DefaultTask() {
    private val ignoreFilePatterns = arrayOf(
        Pattern.compile(".*cemu\\.mo"),
        Pattern.compile(".*Cemu_(?:debug|release)"),
    )

    @get:InputDirectory
    abstract val cemuDataFolder: DirectoryProperty

    @get:OutputFile
    val hashFile: RegularFileProperty = project.objects.fileProperty().convention(
        project.layout.projectDirectory.dir("src/main/assets").file("hash.txt")
    )

    private fun isFileIgnored(file: File): Boolean {
        return ignoreFilePatterns.any { pattern -> pattern.matcher(file.path).matches() }
    }

    @TaskAction
    fun computeCemuDataFilesHash() {
        val cemuDataFilesDir = cemuDataFolder.get().asFile
        val hashOutFile = hashFile.get().asFile

        val digest = MessageDigest.getInstance("SHA-256")

        if (!cemuDataFilesDir.isDirectory) {
            hashOutFile.writeText("invalid")
            return
        }

        cemuDataFilesDir.walkTopDown()
            .filter { it.isFile && !isFileIgnored(it) }
            .sortedBy { it.path }
            .forEach {
                val relativePath = it.relativeTo(cemuDataFilesDir).path.toByteArray()
                digest.update(relativePath)
                digest.update(it.readBytes())
            }

        hashOutFile.writeText(DatatypeConverter.printHexBinary(digest.digest()))
    }
}

val computeCemuDataFilesHashTask =
    tasks.register<ComputeCemuDataFilesHashTask>("computeCemuDataFilesHash") {
        cemuDataFolder.set(File(cemuDataFilesFolder))
    }
tasks.preBuild.dependsOn(computeCemuDataFilesHashTask)

gettext {
    potFile.set(File(projectDir, "cemu_kt.pot"))
    keywords.set(listOf("tr", "trNoop"))
}

dependencies {
    implementation(libs.aboutlibraries.compose.m3)
    implementation(libs.androidx.datastore)
    implementation(libs.kotlinx.gettext)
    implementation(libs.kotlinx.serialization.json)
    implementation(libs.androidx.activity.compose)
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.documentfile)
    implementation(libs.androidx.ui)
    implementation(libs.androidx.ui.graphics)
    implementation(libs.androidx.compose.material3)
    implementation(libs.androidx.compose.ui)
    testImplementation(libs.junit)
    testImplementation(libs.archunit.junit4)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(platform(libs.androidx.compose.bom))
    androidTestImplementation(libs.androidx.ui.test.junit4)
    debugImplementation(libs.androidx.ui.tooling)
    debugImplementation(libs.androidx.ui.test.manifest)
    implementation(libs.ktor.client.okhttp)
    implementation(libs.androidx.appcompat)
    implementation(libs.google.android.material)
    implementation(libs.androidx.navigation.compose)
    implementation(libs.androidx.core.ktx)
}
