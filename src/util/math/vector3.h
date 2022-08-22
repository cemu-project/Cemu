#pragma once
#include <cassert>

template <typename T>
class Vector3 {
public:
	T x;
	T y;
	T z;

	Vector3() : x{}, y{}, z{} {}
	Vector3(T x, T y, T z) : x(x), y(y), z(z) {}

	template <typename U=T>
	Vector3(const Vector3<U>& v)
		: x((T)v.x), y((T)v.y), z((T)v.z) {}

	float Length() const
	{
		return std::sqrt((x * x) + (y * y) + (z * z));
	}

	float Dot(const Vector3& v) const
	{
		return x * v.x + y * v.y + z * v.z;
	}

	Vector3 Cross(const Vector3& v) const
	{
		return Vector3(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
	}

	Vector3 Normalized() const
	{
		const auto len = Length();
		if (len == 0)
			return {};

		return *this / len;
	}

	Vector3& Normalize()
	{
		const auto len = Length();
		if (len != 0) 
			*this /= len;

		return *this;
	}

	//Vector3& Scale(const Vector3& v)
	//{
	//	*this *= v;
	//	return *this;
	//}

	void Scale(const float s)
	{
		this->x *= s;
		this->y *= s;
		this->z *= s;
	}

	Vector3& RotateX(float theta);
	Vector3& RotateY(float theta);
	Vector3& RotateZ(float theta);


	bool operator==(const Vector3& v)
	{
		return x == v.x && y == v.y && z == v.z;
	}

	bool operator!=(const Vector3& v)
	{
		return !(*this == v);
	}

	template <typename U = T>
	Vector3& operator+=(const Vector3<U>& v)
	{
		x += (T)v.x;
		y += (T)v.y;
		z += (T)v.z;
		return *this;
	}

	template <typename U = T>
	Vector3& operator-=(const Vector3<U>& v)
	{
		x -= (T)v.x;
		y -= (T)v.y;
		z -= (T)v.z;
		return *this;
	}

	template <typename U = T>
	Vector3& operator*=(const Vector3<U>& v)
	{
		x *= (T)v.x;
		y *= (T)v.y;
		z *= (T)v.z;
		return *this;
	}

	template <typename U = T>
	Vector3& operator/=(const Vector3<U>& v)
	{
		assert(v.x != 0 && v.y != 0 && v.z != 0);
		x /= (T)v.x;
		y /= (T)v.y;
		z /= (T)v.z;
		return *this;
	}

	template <typename U = T>
	Vector3 operator+(const Vector3<U>& v) const
	{
		return Vector3(x + (T)v.x, y + (T)v.y, z + (T)v.z);
	}

	template <typename U = T>
	Vector3 operator-(const Vector3<U>& v) const
	{
		return Vector3(x - (T)v.x, y - (T)v.y, z - (T)v.z);
	}

	template <typename U = T>
	Vector3 operator*(const Vector3<U>& v) const
	{
		return Vector3(x * (T)v.x, y * (T)v.y, z * (T)v.z);
	}

	template <typename U = T>
	Vector3 operator/(const Vector3<U>& v) const
	{
		assert(v.x != 0 && v.y != 0 && v.z != 0);
		return Vector3(x / (T)v.x, y / (T)v.y, z / (T)v.z);
	}

	Vector3& operator+=(T v)
	{
		x += v;
		y += v;
		z += v;
		return *this;
	}
	Vector3& operator-=(T v)
	{
		x -= v;
		y -= v;
		z -= v;
		return *this;
	}
	Vector3& operator*=(T v)
	{
		x *= v;
		y *= v;
		z *= v;
		return *this;
	}
	Vector3& operator/=(T v)
	{
		assert(v != 0);
		x /= v;
		y /= v;
		z /= v;
		return *this;
	}

	Vector3 operator+(T v) const
	{
		return Vector3(x + v, y + v, z + v);
	}
	Vector3 operator-(T v) const
	{
		return Vector3(x - v, y - v, z - v);
	}
	Vector3 operator*(T v) const
	{
		return Vector3(x * v, y * v, z * v);
	}
	Vector3 operator/(T v) const
	{
		assert(v != 0);
		return Vector3(x / (T)v, y / (T)v, z / (T)v);
	}

	bool IsZero() const
	{
		return x == 0 && y == 0 && z == 0;
	}
	bool HasZero() const
	{
		return x == 0 || y == 0 || z == 0;
	}
};



template <typename T>
Vector3<T>& Vector3<T>::RotateX(float theta)
{
	const float sin = std::sin(theta);
	const float cos = std::cos(theta);
	y = y * cos - z * sin;
	z = y * sin + z * cos;
	return *this;
}

template <typename T>
Vector3<T>& Vector3<T>::RotateY(float theta)
{
	const float sin = std::sin(theta);
	const float cos = std::cos(theta);
	x = x * cos + z * sin;
	z = -x * sin + z * cos;
	return *this;
}

template <typename T>
Vector3<T>& Vector3<T>::RotateZ(float theta)
{
	const float sin = std::sin(theta);
	const float cos = std::cos(theta);
	x = x * cos - y * sin;
	y = x * sin + y * cos;
	return *this;
}


using Vector3f = Vector3<float>;
using Vector3i = Vector3<int>;
