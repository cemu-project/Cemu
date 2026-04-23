package info.cemu.cemu.common.io

import kotlinx.serialization.ExperimentalSerializationApi
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.decodeFromStream
import java.io.File

@OptIn(ExperimentalSerializationApi::class)
inline fun <reified T> decodeJsonFromFile(file: File): T? {
    return try {
        file.inputStream().use {
            Json.decodeFromStream<T>(it)
        }
    } catch (exception: Exception) {
        null
    }
}
