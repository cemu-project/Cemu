package info.cemu.cemu.common.io

import java.io.InputStream
import java.nio.file.Path

fun copyInputStreamToFile(inputStream: InputStream, filePath: Path, buffer: ByteArray) {
    filePath.toFile().outputStream().use { outputStream ->
        var bytesRead: Int
        while ((inputStream.read(buffer).also { bytesRead = it }) > 0) {
            outputStream.write(buffer, 0, bytesRead)
        }
    }
}
