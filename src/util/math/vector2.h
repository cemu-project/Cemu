#pragma once
#include <cassert>

template <typename T>
class Vector2
{
public:
	T x;
	T y;

	Vector2()
		: x{}, y{} {}

	Vector2(T x, T y)
		: x(x), y(y) {}

	template <typename U = T>
	Vector2(const Vector2<U>& v)
		: x((T)v.x), y((T)v.y) {}

	float Length() const
	{
		return (float)std::sqrt((x * x) + (y * y));
	}

	float Cross(const Vector2& v) const
	{
		return x * v.y - y * v.x;
	}

	float Dot(const Vector2& v) const
	{
		return x * v.x + y * v.y;
	}

	Vector2 Ortho() const
	{
		return Vector2(y, -x);
	}

	Vector2 Normalized() const
	{
		const auto len = Length();
		if (len == 0)
			return Vector2<T>();

		return Vector2<T>((T)((float)x / len), (T)((float)y / len));
	}

	Vector2& Normalize()
	{
		const auto len = Length();
		if (len != 0)
		{
			x = (T)((float)x / len);
			y = (T)((float)y / len);
		}

		return *this;
	}

	static Vector2 Max(const Vector2& v1, const Vector2& v2)
	{
		return Vector2(std::max(v1.x, v2.x), std::max(v1.y, v2.y));
	}

	static Vector2 Min(const Vector2& v1, const Vector2& v2)
	{
		return Vector2(std::min(v1.x, v2.x), std::min(v1.y, v2.y));
	}

	bool operator==(const Vector2& v) const
	{
		return x == v.x && y == v.y;
	}
	bool operator!=(const Vector2& v) const
	{
		return !(*this == v);
	}

	Vector2& operator+=(const Vector2& v)
	{
		x += v.x;
		y += v.y;
		return *this;
	}
	Vector2& operator-=(const Vector2& v)
	{
		x -= v.x;
		y -= v.y;
		return *this;
	}

	Vector2 operator+(const Vector2& v) const
	{
		return Vector2(x + v.x, y + v.y);
	}
	Vector2 operator-(const Vector2& v) const
	{
		return Vector2(x - v.x, y - v.y);
	}

	Vector2& operator+=(const T& v)
	{
		x += v;
		y += v;
		return *this;
	}
	Vector2& operator-=(const T& v)
	{
		x -= v;
		y -= v;
		return *this;
	}
	Vector2& operator*=(const T& v)
	{
		x *= v;
		y *= v;
		return *this;
	}
	Vector2& operator/=(const T& v)
	{
		assert(v != 0);
		x /= v;
		y /= v;
		return *this;
	}

	Vector2 operator+(const T& v)
	{
		return Vector2(x + v, y + v);
	}
	Vector2 operator-(const T& v)
	{
		return Vector2(x - v, y - v);
	}
	Vector2 operator*(const T& v)
	{
		return Vector2(x * v, y * v);
	}
	Vector2 operator/(const T& v)
	{
		assert(v != 0);
		return Vector2(x / v, y / v);
	}
};

using Vector2f = Vector2<float>;
using Vector2i = Vector2<int>;

