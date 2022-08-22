#pragma once

#include <tuple>
#include <wx/math.h>
#include "util/math/vector3.h"
#define DEG2RAD(__d__) ((__d__ * M_PI) / 180.0f )
#define RAD2DEG(__r__) ((__r__ * 180.0f) / M_PI)


template<typename T> 
class Quaternion
{
public:
	T w;
	T x;
	T y;
	T z;

	Quaternion();
	Quaternion(const T& w, const T& x, const T& y, const T& z);
	Quaternion(float x, float y, float z);

	void Assign(float w, float x, float y, float z)
	{
		this->w = w;
		this->x = x;
		this->y = y;
		this->z = z;
	}

	static Quaternion FromAngleAxis(float inAngle, float inX, float inY, float inZ);

	std::tuple<Vector3<T>, Vector3<T>, Vector3<T>> GetRotationMatrix();
	std::tuple<Vector3<T>, Vector3<T>, Vector3<T>> GetTransposedRotationMatrix();

	// normalize but keep W
	void NormalizeXYZ()
	{
		const T xyzTargetLength = sqrtf((T)1.0 - w * w);
		const T lengthScaler = xyzTargetLength / sqrtf(x * x + y * y + z * z);
		x *= lengthScaler;
		y *= lengthScaler;
		z *= lengthScaler;
	}

	void NormalizeXYZW()
	{
		const T lengthScaler = 1.0f / sqrtf(w * w + x * x + y * y + z * z);
		w *= lengthScaler;
		x *= lengthScaler;
		y *= lengthScaler;
		z *= lengthScaler;
	}

	Vector3<T> GetVectorX() const
	{
		return Vector3<T>(
			2.0f * (w * w + x * x) - 1.0f,
			2.0f * (x * y - w * z),
			2.0f * (x * z + w * y));
	}

	Vector3<T> GetVectorY() const
	{
		return Vector3<T>(
			2.0f * (x * y + w * z),
			2.0f * (w * w + y * y) - 1.0f,
			2.0f * (y * z - w * x)
			);
	}

	Vector3<T> GetVectorZ() const
	{
		return Vector3<T>(
			2.0f * (x * z - w * y),
			2.0f * (y * z + w * x),
			2.0f * (w * w + z * z) - 1.0f);
	}

	Quaternion& operator*=(const Quaternion<T>& rhs)
	{
		Assign(w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z,
			w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
			w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
			w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w);
		return *this;
	}

	Quaternion& operator+=(const Quaternion<T>& rhs)
	{
		w += rhs.w;
		x += rhs.x;
		y += rhs.y;
		z += rhs.z;
		return *this;
	}

	friend Quaternion operator*(Quaternion<T> lhs, const Quaternion<T>& rhs)
	{
		lhs *= rhs;
		return lhs;
	}

};

template <typename T>
Quaternion<T>::Quaternion() 
	: w(), x(), y(), z()
{}

template <typename T>
Quaternion<T>::Quaternion(const T& w, const T& x, const T& y, const T& z)
	: w(w), x(x), y(y), z(z)
{}

template <typename T>
Quaternion<T>::Quaternion(float x, float y, float z)
{
	float pitch = DEG2RAD(x);
	float yaw = DEG2RAD(y);
	float roll = DEG2RAD(z);

	float cyaw = cos(0.5f * yaw);
	float cpitch = cos(0.5f * pitch);
	float croll = cos(0.5f * roll);
	float syaw = sin(0.5f * yaw);
	float spitch = sin(0.5f * pitch);
	float sroll = sin(0.5f * roll);

	float cyawcpitch = cyaw * cpitch;
	float syawspitch = syaw * spitch;
	float cyawspitch = cyaw * spitch;
	float syawcpitch = syaw * cpitch;

	this->w = cyawcpitch * croll + syawspitch * sroll;
	this->x = cyawspitch * croll + syawcpitch * sroll;
	this->y = syawcpitch * croll - cyawspitch * sroll;
	this->z = cyawcpitch * sroll - syawspitch * croll;
}

template<typename T>
Quaternion<T> Quaternion<T>::FromAngleAxis(float inAngle, float inX, float inY, float inZ)
{
	Quaternion<T> result = Quaternion<T>(cosf(inAngle * 0.5f), inX, inY, inZ);
	result.NormalizeXYZ();
	return result;
}

template <typename T>
std::tuple<Vector3<T>, Vector3<T>, Vector3<T>> Quaternion<T>::GetRotationMatrix()
{
	float sqw = w*w;
	float sqx = x*x;
	float sqy = y*y;
	float sqz = z*z;

	// invs (inverse square length) is only required if quaternion is not already normalised
	float invs = 1.0f / (sqx + sqy + sqz + sqw);
	
	Vector3<T> v1, v2, v3;
	v1.x = (sqx - sqy - sqz + sqw) * invs; // since sqw + sqx + sqy + sqz =1/invs*invs
	v2.y = (-sqx + sqy - sqz + sqw) * invs;
	v3.z = (-sqx - sqy + sqz + sqw) * invs;

	float tmp1 = x*y;
	float tmp2 = z*w;
	v2.x = 2.0 * (tmp1 + tmp2)*invs;
	v1.y = 2.0 * (tmp1 - tmp2)*invs;

	tmp1 = x*z;
	tmp2 = y*w;
	v3.x = 2.0 * (tmp1 - tmp2)*invs;
	v1.z = 2.0 * (tmp1 + tmp2)*invs;

	tmp1 = y*z;
	tmp2 = x*w;
	v3.y = 2.0 * (tmp1 + tmp2)*invs;
	v2.z = 2.0 * (tmp1 - tmp2)*invs;

	return std::make_tuple(v1, v2, v3);
}

template <typename T>
std::tuple<Vector3<T>, Vector3<T>, Vector3<T>> Quaternion<T>::GetTransposedRotationMatrix()
{
	float sqw = w*w;
	float sqx = x*x;
	float sqy = y*y;
	float sqz = z*z;

	// invs (inverse square length) is only required if quaternion is not already normalised
	float invs = 1.0f / (sqx + sqy + sqz + sqw);

	Vector3<T> v1, v2, v3;
	v1.x = (sqx - sqy - sqz + sqw) * invs; // since sqw + sqx + sqy + sqz =1/invs*invs
	v2.y = (-sqx + sqy - sqz + sqw) * invs;
	v3.z = (-sqx - sqy + sqz + sqw) * invs;

	float tmp1 = x*y;
	float tmp2 = z*w;
	v1.y = 2.0 * (tmp1 + tmp2)*invs;
	v2.x = 2.0 * (tmp1 - tmp2)*invs;

	tmp1 = x*z;
	tmp2 = y*w;
	v1.z = 2.0 * (tmp1 - tmp2)*invs;
	v3.x = 2.0 * (tmp1 + tmp2)*invs;

	tmp1 = y*z;
	tmp2 = x*w;
	v2.z = 2.0 * (tmp1 + tmp2)*invs;
	v3.y = 2.0 * (tmp1 - tmp2)*invs;

	return std::make_tuple(v1, v2, v3);
}

using Quaternionf = Quaternion<float>;