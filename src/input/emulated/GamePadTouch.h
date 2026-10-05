#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <optional>
#include <pugixml.hpp>

// Uses Cemu's base integer types (provided by the precompiled header).
// Coordinates are native GamePad pixels, independent of the host display.
class GamePadTouch
{
public:
	static constexpr sint32 kWidth = 854;
	static constexpr sint32 kHeight = 480;
	static constexpr size_t kBindingCount = 3;

	struct Position
	{
		sint32 x, y;
	};

	struct RawPosition
	{
		uint16 x = 0, y = 0;
	};

	struct Sample
	{
		RawPosition position;
		bool down;
	};

	Position GetPosition(size_t index) const
	{
		const uint32 packed = m_positions.at(index).load(std::memory_order_relaxed);
		return {static_cast<sint32>(packed & 0xffff), static_cast<sint32>(packed >> 16)};
	}

	void SetPosition(size_t index, sint32 x, sint32 y)
	{
		x = std::clamp(x, 0, kWidth - 1);
		y = std::clamp(y, 0, kHeight - 1);
		// Publish X/Y together; settings can be edited while the game samples input.
		m_positions.at(index).store(static_cast<uint32>(x) | (static_cast<uint32>(y) << 16), std::memory_order_relaxed);
	}

	static RawPosition FromNormalized(float x, float y)
	{
		// Same inverse calibration as the v2.6 mouse/position input path.
		return {static_cast<uint16>(x * 3883.0f + 92.0f),
			static_cast<uint16>(4095.0f - y * 3694.0f - 254.0f)};
	}

	static RawPosition FromLogical(Position position)
	{
		// Aim at the pixel center so truncation in VPAD's raw/calibrated
		// conversion cannot move a configured point to the preceding pixel.
		return FromNormalized((static_cast<float>(position.x) + 0.5f) / kWidth,
			(static_cast<float>(position.y) + 0.5f) / kHeight);
	}

	Sample Read(const std::optional<RawPosition>& pointer, const std::array<bool, kBindingCount>& buttons)
	{
		// Wii U touch is single-contact. Keep existing pointer input priority.
		if (pointer)
		{
			m_lastPosition = *pointer;
			return {m_lastPosition, true};
		}
		for (size_t i = 0; i < kBindingCount; ++i)
		{
			if (!buttons[i])
				continue;
			const auto position = GetPosition(i);
			m_lastPosition = FromLogical(position);
			return {m_lastPosition, true};
		}
		// Preserve the last position on release, just like physical touch input.
		return {m_lastPosition, false};
	}

	void Load(const pugi::xml_node& node)
	{
		for (size_t i = 0; i < kBindingCount; ++i)
			SetPosition(i, 0, 0);
		for (const auto entry : node.child("gamepad_touch").children("entry"))
		{
			const auto slot = entry.attribute("slot").as_int(-1);
			if (slot < 0 || static_cast<size_t>(slot) >= kBindingCount)
				continue;
			SetPosition(static_cast<size_t>(slot), entry.child("x").text().as_int(), entry.child("y").text().as_int());
		}
	}

	void Save(pugi::xml_node& node) const
	{
		auto touch = node.append_child("gamepad_touch");
		for (size_t i = 0; i < kBindingCount; ++i)
		{
			const auto position = GetPosition(i);
			auto entry = touch.append_child("entry");
			entry.append_attribute("slot").set_value(static_cast<unsigned int>(i));
			entry.append_child("x").text().set(position.x);
			entry.append_child("y").text().set(position.y);
		}
	}

private:
	std::array<std::atomic<uint32>, kBindingCount> m_positions{};
	RawPosition m_lastPosition{};
};
