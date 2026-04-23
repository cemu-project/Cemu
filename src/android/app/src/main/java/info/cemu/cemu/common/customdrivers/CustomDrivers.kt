package info.cemu.cemu.common.customdrivers

import info.cemu.cemu.common.io.decodeJsonFromFile
import info.cemu.cemu.nativeinterface.NativeActiveSettings
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.serialization.Serializable
import java.io.File
import java.nio.file.Path
import kotlin.io.path.Path
import kotlin.io.path.isDirectory

const val META_FILE_NAME = "meta.json"
const val SUPPORTED_SCHEMA_VERSION = 1
const val CUSTOM_DRIVERS_DIR_NAME = "customDrivers"

fun getCustomDriversDir(): Path =
    Path(NativeActiveSettings.getUserDataPath()).resolve(CUSTOM_DRIVERS_DIR_NAME)

@Serializable
data class DriverMetadata(
    val schemaVersion: Int,
    val name: String,
    val description: String,
    val author: String,
    val packageVersion: String,
    val vendor: String,
    val driverVersion: String,
    val minApi: Int,
    val libraryName: String,
)

data class Driver(
    val path: String,
    val metadata: DriverMetadata,
)

suspend fun parseInstalledDrivers(): List<Driver> {
    return withContext(Dispatchers.IO) {
        val customDriversDir = getCustomDriversDir()

        if (!customDriversDir.isDirectory())
            return@withContext emptyList()

        val driverDirs: Array<File> =
            customDriversDir.toFile().listFiles() ?: return@withContext emptyList()

        val drivers = mutableListOf<Driver>()

        for (driverDir in driverDirs) {
            if (!driverDir.isDirectory)
                continue
            val metadata =
                decodeJsonFromFile<DriverMetadata>(driverDir.resolve(META_FILE_NAME))
                    ?: continue
            val driver = Driver(
                path = driverDir.path,
                metadata = metadata,
            )
            drivers.add(driver)
        }

        drivers.sortBy { it.metadata.name }

        return@withContext drivers
    }
}