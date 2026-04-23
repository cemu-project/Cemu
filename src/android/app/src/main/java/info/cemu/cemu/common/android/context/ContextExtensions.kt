package info.cemu.cemu.common.android.context

import android.content.Context
import android.os.Build
import android.os.Vibrator
import android.os.VibratorManager
import java.io.File

fun Context.internalFolder(): File {
    val externalFilesDir = getExternalFilesDir(null)
    if (externalFilesDir != null) {
        return externalFilesDir
    }
    return filesDir
}

fun Context.getDeviceVibrator(): Vibrator {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        val vibratorManager = getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as VibratorManager
        return vibratorManager.defaultVibrator
    } else {
        @Suppress("deprecation")
        return getSystemService(Context.VIBRATOR_SERVICE) as Vibrator
    }
}