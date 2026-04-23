package info.cemu.cemu.emulation.input

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.view.Surface
import info.cemu.cemu.nativeinterface.NativeInput


class DeviceMotionHandler(context: Context) : SensorEventListener {
    private data class SensorValues(val x: Float, val y: Float, val z: Float)

    private val sensorManager =
        context.getSystemService(Context.SENSOR_SERVICE) as SensorManager
    private val accelerometer =
        sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)
    private val gyroscope =
        sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE)

    private var deviceRotation = Surface.ROTATION_0

    fun setDeviceRotation(deviceRotation: Int) {
        this.deviceRotation = deviceRotation
    }

    private val hasMotionData = accelerometer != null && gyroscope != null
    private var gyroValues = SensorValues(0f, 0f, 0f)
    private var hasGyroData = false
    private var accelValues = SensorValues(0f, 0f, 0f)
    private var hasAccelData = false
    private var isListening = false
    private var isActive = false

    fun setIsListening(isListening: Boolean) {
        this.isListening = isListening
        if (isListening && !isActive) {
            startListening()
        } else if (!isListening && isActive) {
            stopListening()
        }
    }

    fun pauseListening() {
        if (isActive) {
            stopListening()
        }
    }

    fun resumeListening() {
        if (isListening && !isActive) {
            startListening()
        }
    }

    private fun startListening() {
        if (!hasMotionData || isActive) {
            return
        }

        isActive = true

        NativeInput.setDeviceMotionEnabled(true)
        sensorManager.registerListener(this, gyroscope, SensorManager.SENSOR_DELAY_GAME)
        sensorManager.registerListener(this, accelerometer, SensorManager.SENSOR_DELAY_GAME)
    }

    private fun stopListening() {
        if (!hasMotionData || !isActive) {
            return
        }

        isActive = false

        NativeInput.setDeviceMotionEnabled(false)
        sensorManager.unregisterListener(this)
    }

    override fun onSensorChanged(event: SensorEvent) {
        val values = event.values

        if (event.sensor.type == Sensor.TYPE_GYROSCOPE) {
            gyroValues = getSensorEventValues(values)
            hasGyroData = true
        } else if (event.sensor.type == Sensor.TYPE_ACCELEROMETER) {
            accelValues = getSensorEventValues(values)
            hasAccelData = true
        }

        if (hasAccelData && hasGyroData) {
            hasAccelData = false
            hasGyroData = false
            NativeInput.onDeviceMotion(
                event.timestamp,
                gyroValues.x,
                gyroValues.y,
                gyroValues.z,
                accelValues.x / SensorManager.GRAVITY_EARTH,
                accelValues.y / SensorManager.GRAVITY_EARTH,
                accelValues.z / SensorManager.GRAVITY_EARTH,
            )
        }
    }

    override fun onAccuracyChanged(sensor: Sensor, accuracy: Int) {}

    private fun getSensorEventValues(values: FloatArray): SensorValues {
        val x: Float
        val y: Float
        val z = values[2]

        when (deviceRotation) {
            Surface.ROTATION_0 -> {
                x = values[0]
                y = values[1]
            }

            Surface.ROTATION_90 -> {
                x = -values[1]
                y = values[0]
            }

            Surface.ROTATION_180 -> {
                x = -values[0]
                y = -values[1]
            }

            else /*Surface.ROTATION_270*/ -> {
                x = values[1]
                y = -values[0]
            }
        }
        return SensorValues(x, y, z)
    }
}