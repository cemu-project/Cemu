package info.cemu.cemu.common.string

import java.net.URLDecoder

fun String.urlDecode(enc: String = "UTF-8"): String = URLDecoder.decode(this, enc)

fun String.isContentUri() = startsWith("content://")

fun String.parseHexOrNull(): UInt? {
    return try {
        toUInt(16)
    } catch (_: NumberFormatException) {
        null
    }
}
