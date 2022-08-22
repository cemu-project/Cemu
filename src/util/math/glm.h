#pragma once

namespace glm
{
	inline quat normalize_xyz(const quat& q)
	{
		const auto xyzTargetLength = std::sqrt(1.0f - q.w * q.w);
		const auto lengthScaler = xyzTargetLength / sqrtf(q.x * q.x + q.y * q.y + q.z * q.z);
		return quat(q.w, q.x * lengthScaler, q.y * lengthScaler, q.z * lengthScaler);
	}

	inline vec3 GetVectorX(const quat& q)
	{
		return vec3(
			2.0f * (q.w * q.w + q.x * q.x) - 1.0f,
			2.0f * (q.x * q.y - q.w * q.z),
			2.0f * (q.x * q.z + q.w * q.y));
	}

	inline vec3 GetVectorY(const quat& q)
	{
		return vec3(
			2.0f * (q.x * q.y + q.w * q.z),
			2.0f * (q.w * q.w + q.y * q.y) - 1.0f,
			2.0f * (q.y * q.z - q.w * q.x)
		);
	}

	inline vec3 GetVectorZ(const quat& q)
	{
		return vec3 (
			2.0f * (q.x * q.z - q.w * q.y),
			2.0f * (q.y * q.z + q.w * q.x),
			2.0f * (q.w * q.w + q.z * q.z) - 1.0f);
	}
}
