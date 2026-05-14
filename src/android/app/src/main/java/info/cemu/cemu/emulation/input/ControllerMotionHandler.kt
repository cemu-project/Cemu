package info.cemu.cemu.emulation.input

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.hardware.input.InputManager
import android.os.Build
import android.view.InputDevice
import info.cemu.cemu.common.android.inputdevice.listGameControllers
import info.cemu.cemu.common.input.InputDeviceListener
import info.cemu.cemu.nativeinterface.NativeInput

class ControllerMotionHandler(private val context: Context) {
    private val inputManager
        get() = context.getSystemService(Context.INPUT_SERVICE) as InputManager?

    private inner class ControllerSensorEventListener(
        private val deviceDescriptor: String,
        inputDevice: InputDevice,
    ) : SensorEventListener {
        private var isActive = false
        private var hasAccelData = false
        private var hasGyroData = false
        private var gyroValues: FloatArray = arrayOf(0.0f, 0.0f, 0.0f).toFloatArray()
        private var accelValues: FloatArray = arrayOf(0.0f, 0.0f, 0.0f).toFloatArray()

        private val sensorManager: SensorManager?
        private val accelerometer: Sensor?
        private val gyroscope: Sensor?

        init {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                sensorManager = inputDevice.sensorManager
                accelerometer = sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)
                gyroscope = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE)
            } else {
                sensorManager = null
                accelerometer = null
                gyroscope = null
            }
        }

        fun register() {
            if (isActive) {
                return
            }

            isActive = true

            if (sensorManager == null || gyroscope == null || accelerometer == null) {
                return
            }

            sensorManager.registerListener(this, gyroscope, SensorManager.SENSOR_DELAY_GAME)
            sensorManager.registerListener(this, accelerometer, SensorManager.SENSOR_DELAY_GAME)
        }

        fun unregister() {
            if (!isActive) {
                return
            }

            isActive = false

            if (sensorManager == null || gyroscope == null || accelerometer == null) {
                return
            }

            sensorManager.unregisterListener(this)
        }

        override fun onAccuracyChanged(sensor: Sensor, accuracy: Int) {}


        override fun onSensorChanged(event: SensorEvent) {
            val values = event.values

            if (event.sensor.type == Sensor.TYPE_GYROSCOPE) {
                gyroValues = values
                hasGyroData = true
            } else if (event.sensor.type == Sensor.TYPE_ACCELEROMETER) {
                accelValues = values
                hasAccelData = true
            }

            if (hasAccelData && hasGyroData) {
                hasAccelData = false
                hasGyroData = false
                NativeInput.onControllerMotion(
                    deviceDescriptor,
                    event.timestamp,
                    gyroValues[0],
                    gyroValues[1],
                    gyroValues[2],
                    accelValues[0] / SensorManager.GRAVITY_EARTH,
                    accelValues[1] / SensorManager.GRAVITY_EARTH,
                    accelValues[2] / SensorManager.GRAVITY_EARTH,
                )
            }
        }
    }

    private val listeners = mutableMapOf<String, ControllerSensorEventListener>()

    fun register() {
        inputManager?.registerInputDeviceListener(inputDeviceListener, null)
        refreshControllers()
        listeners.forEach { (_, listener) -> listener.register() }
    }

    fun unregister() {
        inputManager?.unregisterInputDeviceListener(inputDeviceListener)
        listeners.forEach { (_, listener) -> listener.unregister() }
    }

    private fun refreshControllers() {
        val motionEnabledInputDevices = NativeInput.getMotionEnabledControllerDescriptors().toSet()
        val currentDevices = listGameControllers()
            .filter { motionEnabledInputDevices.contains(it.descriptor) }
            .associateBy { it.descriptor }

        val toRemove = listeners.keys - currentDevices.keys
        for (descriptor in toRemove) {
            listeners[descriptor]?.unregister()
            listeners.remove(descriptor)
        }

        val toAdd = currentDevices.keys - listeners.keys
        for (descriptor in toAdd) {
            val inputDevice = currentDevices[descriptor] ?: continue
            val listener = ControllerSensorEventListener(descriptor, inputDevice)
            listener.register()
            listeners[descriptor] = listener
        }
    }

    private val inputDeviceListener = object : InputDeviceListener {
        override fun onInputDeviceChanged() = refreshControllers()
    }
}