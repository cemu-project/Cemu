package info.cemu.Cemu.input;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;

import info.cemu.Cemu.NativeLibrary;

public class SensorManager implements SensorEventListener {
    private final android.hardware.SensorManager sensorManager;
    private final Sensor accelerometer;
    private final Sensor gyroscope;
    private final boolean hasMotionData;
    private float gyroX, gyroY, gyroZ;
    private float accelX, accelY, accelZ;
    private boolean hasAccelData, hasGyroData;
    private boolean isLandscape = true;

    public SensorManager(Context context) {
        sensorManager = (android.hardware.SensorManager) context.getSystemService(Context.SENSOR_SERVICE);
        accelerometer = sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
        gyroscope = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
        hasMotionData = accelerometer != null && gyroscope != null;
    }

    public void startListening() {
        if (!hasMotionData) {
            return;
        }
        NativeLibrary.setMotionEnabled(true);
        sensorManager.registerListener(this, gyroscope, android.hardware.SensorManager.SENSOR_DELAY_GAME);
        sensorManager.registerListener(this, accelerometer, android.hardware.SensorManager.SENSOR_DELAY_GAME);
    }

    public void setIsLandscape(boolean isLandscape) {
        this.isLandscape = isLandscape;
    }

    public void pauseListening() {
        if (!hasMotionData) {
            return;
        }
        NativeLibrary.setMotionEnabled(false);
        sensorManager.unregisterListener(this);
    }

    @Override
    public void onSensorChanged(SensorEvent event) {
        if (event.sensor.getType() == Sensor.TYPE_ACCELEROMETER) {
            accelX = event.values[0];
            accelY = event.values[1];
            accelZ = event.values[2];
            hasAccelData = true;
        }
        if (event.sensor.getType() == Sensor.TYPE_GYROSCOPE) {
            gyroX = event.values[0];
            gyroY = event.values[1];
            gyroZ = event.values[2];
            hasGyroData = true;
        }
        if (!hasAccelData || !hasGyroData) {
            return;
        }
        hasAccelData = false;
        hasGyroData = false;
        if (isLandscape) {
            NativeLibrary.onMotion(event.timestamp, gyroY, gyroZ, gyroX, accelY, accelZ, accelX);
            return;
        }
        NativeLibrary.onMotion(event.timestamp, gyroX, gyroY, gyroZ, accelX, accelY, accelZ);
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
    }
}
