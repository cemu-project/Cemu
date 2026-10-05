#include "input/emulated/GamePadTouch.h"

#include <iostream>
#include <sstream>
#include <stdexcept>

static void Check(bool condition, const char* message)
{
	if (!condition)
		throw std::runtime_error(message);
}

static void CheckPosition(GamePadTouch::RawPosition actual, GamePadTouch::RawPosition expected)
{
	Check(actual.x == expected.x && actual.y == expected.y, "raw touch coordinate mismatch");
}

int main()
{
	try
	{
		GamePadTouch touch;
		touch.SetPosition(0, 150, 80);
		touch.SetPosition(1, 300, 200);
		touch.SetPosition(2, 853, 479);
		const std::array<bool, 3> idle{}, first{true, false, false}, second{false, true, false}, both{true, true, false};
		Check(!touch.Read({}, idle).down, "unmapped input must not touch");
		const auto expected = GamePadTouch::FromLogical({150, 80});
		Check(expected.x == 776 && expected.y == 3221, "inverse VPAD calibration changed");
		auto sample = touch.Read({}, first);
		Check(sample.down, "button press must start touch");
		CheckPosition(sample.position, expected);
		// Count down edges, not samples: holding must be one continuous contact.
		size_t downEdges = 1;
		bool previousDown = sample.down;
		for (size_t i = 0; i < 100; ++i)
		{
			sample = touch.Read({}, first);
			downEdges += sample.down && !previousDown;
			previousDown = sample.down;
			CheckPosition(sample.position, expected);
		}
		Check(downEdges == 1, "held input repeated a tap");
		sample = touch.Read({}, idle);
		Check(!sample.down, "release must terminate touch");
		CheckPosition(sample.position, expected);
		for (size_t i = 0; i < 10; ++i)
		{
			Check(touch.Read({}, first).down, "rapid sampled press lost");
			Check(!touch.Read({}, idle).down, "rapid sampled release lost");
		}
		CheckPosition(touch.Read({}, both).position, expected);
		const auto other = GamePadTouch::FromLogical({300, 200});
		CheckPosition(touch.Read({}, second).position, other);
		const auto pointer = GamePadTouch::FromNormalized(0.5f, 0.75f);
		CheckPosition(touch.Read(pointer, both).position, pointer);
		CheckPosition(touch.Read(pointer, idle).position, pointer);
		CheckPosition(touch.Read({}, first).position, expected);
		Check(!touch.Read({}, idle).down, "pointer release corrupted synthetic state");

		// Compare against the original v2.6 pointer conversion over the screen.
		for (sint32 x = 0; x <= 854; ++x)
		{
			const float normalized = static_cast<float>(x) / 854;
			const auto raw = GamePadTouch::FromNormalized(normalized, normalized);
			Check(raw.x == static_cast<uint16>(normalized * 3883.0f + 92.0f), "mouse X conversion regression");
			Check(raw.y == static_cast<uint16>(4095.0f - normalized * 3694.0f - 254.0f), "mouse Y conversion regression");
		}
		CheckPosition(GamePadTouch::FromNormalized(0, 0), {92, 3841});
		CheckPosition(GamePadTouch::FromNormalized(1, 1), {3975, 147});
		for (sint32 x = 0; x < GamePadTouch::kWidth; ++x)
		{
			for (sint32 y = 0; y < GamePadTouch::kHeight; ++y)
			{
				const auto raw = GamePadTouch::FromLogical({x, y});
				// vpad.cpp::_tpRawToResolution at the native 854x480 resolution.
				const auto calibratedX = static_cast<sint32>((static_cast<double>(raw.x - 92) / 3883.0) * 854);
				const auto calibratedY = static_cast<sint32>((static_cast<double>(4095 - raw.y - 254) / 3694.0) * 480);
				Check(calibratedX == x && calibratedY == y, "logical pixel calibration round trip failed");
			}
		}

		pugi::xml_document doc;
		auto root = doc.append_child("emulated_controller");
		touch.Save(root);
		std::ostringstream xml;
		doc.save(xml);
		pugi::xml_document reloaded;
		Check(static_cast<bool>(reloaded.load_string(xml.str().c_str())), "profile XML failed to parse");
		GamePadTouch restored;
		restored.Load(reloaded.document_element());
		for (size_t i = 0; i < GamePadTouch::kBindingCount; ++i)
		{
			const auto a = touch.GetPosition(i), b = restored.GetPosition(i);
			Check(a.x == b.x && a.y == b.y, "coordinate profile round trip failed");
		}
		pugi::xml_document old;
		Check(static_cast<bool>(old.load_string("<emulated_controller><type>Wii U GamePad</type><toggle_display>1</toggle_display></emulated_controller>")), "old profile invalid");
		restored.Load(old.document_element());
		Check(restored.GetPosition(0).x == 0 && restored.GetPosition(0).y == 0, "old profile defaults failed");
		Check(!restored.Read({}, idle).down, "old profile enabled touch without binding");
		pugi::xml_document invalid;
		invalid.load_string("<emulated_controller><gamepad_touch><entry slot='-1'><x>123</x></entry><entry slot='99'/><entry slot='0'><x>-5</x><y>9999</y></entry></gamepad_touch></emulated_controller>");
		restored.Load(invalid.document_element());
		Check(restored.GetPosition(0).x == 0 && restored.GetPosition(0).y == 479, "invalid coordinates were not clamped");
		restored.SetPosition(0, 9999, -10);
		Check(restored.GetPosition(0).x == 853 && restored.GetPosition(0).y == 0, "setter bounds failed");

		// No display geometry is an input to Read: repeat after pointer positions change.
		for (const auto normalized : {0.0f, 0.25f, 1.0f})
		{
			touch.Read(GamePadTouch::FromNormalized(normalized, normalized), idle);
			CheckPosition(touch.Read({}, first).position, expected);
		}
		std::cout << "GamePad touch sampling, hold/release, arbitration, conversion, bounds and XML tests passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
