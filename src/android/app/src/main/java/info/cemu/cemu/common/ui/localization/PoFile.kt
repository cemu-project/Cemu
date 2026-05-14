package info.cemu.cemu.common.ui.localization

import java.io.InputStream

private enum class PoKey { MSG_ID, MSG_STR, NONE }

fun parsePoFile(source: InputStream): Map<String, String> {
    val reader = source.bufferedReader()
    var key = PoKey.NONE
    var msgId = ""
    var msg = ""
    val strings = mutableMapOf<String, String>()

    while (true) {
        val line = reader.readLine() ?: break

        when {
            line.isBlank() -> {
                if (msgId.isEmpty() || msg.isEmpty()) {
                    continue
                }

                strings[msgId] = msg

                key = PoKey.NONE
                msgId = ""
                msg = ""
            }

            line.startsWith("msgid ") -> {
                key = PoKey.MSG_ID
                msgId = line.substringAfter("msgid ").unescape()
            }

            line.startsWith("msgstr ") -> {
                key = PoKey.MSG_STR
                msg = line.substringAfter("msgstr ").unescape()
            }

            // Ignore entry if it has plural forms or context, the core code doesn't use them.
            line.startsWith("msgid_plural ") || line.startsWith("msgctxt ") -> {
                key = PoKey.NONE
                msg = ""
                msgId = ""
            }

            else -> when (key) {
                PoKey.MSG_ID -> msgId += line.unescape()
                PoKey.MSG_STR -> msg += line.unescape()
                PoKey.NONE -> {}
            }
        }
    }

    return strings
}

private fun String.unescape(): String {
    return trim()
        .trim('"')
        .replace("\\\"", "\"")
        .replace("\\n", "\n")
}