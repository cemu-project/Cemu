#pragma once
#include "Mahony.h"
#include "MotionSample.h"

// utility class to translate external motion input (DSU, SDL GamePad sensors) into the values expected by VPAD API (and maybe others in the future)
class WiiUMotionHandler
{
public:
	// gyro is in radians/sec
	void processMotionSample(
		float deltaTime,
		float gx, float gy, float gz,
		float accx, float accy, float accz)
	{
		m_gyro[0] = gx;
		m_gyro[1] = gy;
		m_gyro[2] = gz;
		m_prevAcc[0] = m_acc[0];
		m_prevAcc[1] = m_acc[1];
		m_prevAcc[2] = m_acc[2];
		m_acc[0] = accx;
		m_acc[1] = accy;
		m_acc[2] = accz;
		// integrate acc and gyro samples into IMU
		m_imu.updateIMU(deltaTime, gx, gy, gz, accx, accy, accz);

		// get orientation from IMU
		m_orientation[0] = _radToOrientation(-m_imu.getYawRadians()) - 0.50f;
		m_orientation[1] = _radToOrientation(-m_imu.getPitchRadians()) - 0.50f;
		m_orientation[2] = _radToOrientation(m_imu.getRollRadians());
	}

	MotionSample getMotionSample()
	{
		float q[4];
		m_imu.getQuaternion(q);
		float gBias[3];
		m_imu.getGyroBias(gBias);
		float gyroDebiased[3];
		gyroDebiased[0] = m_gyro[0] - gBias[0];
		gyroDebiased[1] = m_gyro[1] - gBias[1];
		gyroDebiased[2] = m_gyro[2] - gBias[2];
		return MotionSample(m_acc, MotionSample::calculateAccAcceleration(m_prevAcc, m_acc), gyroDebiased, m_orientation, q);
	}
private:

	// VPAD orientation unit is 1.0 = one revolution around the axis
	float _radToOrientation(float rad)
	{
		return rad / (2.0f * 3.14159265f);
	}

	MahonySensorFusion m_imu;
	// state
	float m_gyro[3]{};
	float m_acc[3]{};
	float m_prevAcc[3]{};
	// calculated values
	float m_orientation[3]{};
};