#include "input/emulated/VPADController.h"
#include "input/api/Controller.h"
#include "config/ActiveSettings.h"
#include "config/CemuConfig.h"
#include "gui/guiWrapper.h"
#include "gui/input/panels/VPADInputPanel.h"

#include <wx/app.h>
#include <wx/frame.h>
#include <wx/spinctrl.h>
#include <wx/textctrl.h>

#include <iostream>
#include <stdexcept>

// Normally supplied by main.cpp. This harness does not launch the emulator
// or parse launch arguments; link the real GUI/input libraries without startup.
void CemuCommonInit() {}
void requireConsole() {}
void HandlePostUpdate() {}
std::atomic_bool g_isGPUInitFinished = false;

// Exercise the real v2.6 physical state -> mapping -> VPADStatus path without
// a controller device, window, renderer or running game.
class TestController : public ControllerBase
{
public:
	TestController() : ControllerBase("touch-test", "Touch test") {}
	std::string_view api_name() const override { return "Keyboard"; }
	InputAPI::Type api() const override { return InputAPI::Keyboard; }
	bool is_connected() override { return true; }
	bool has_axis() const override { return false; }
	ControllerState raw_state() override
	{
		ControllerState state;
		state.buttons.SetButtonState(kButton0, m_touchDown);
		state.buttons.SetButtonState(kButton1, m_aDown);
		return state;
	}
	bool m_touchDown = false;
	bool m_aDown = false;
};

static void Check(bool condition, const char* message)
{
	if (!condition)
		throw std::runtime_error(message);
}

int main(int argc, char** argv)
{
	try
	{
		// Old mapping IDs must stay stable for existing XML and migrated profiles.
		static_assert(VPADController::kButtonId_A == 1);
		static_assert(VPADController::kButtonId_StickR == 16);
		static_assert(VPADController::kButtonId_Home == 27);
		static_assert(VPADController::kButtonId_Touch1 == 28);
		static_assert(VPADController::kButtonId_Touch3 - VPADController::kButtonId_Touch1 + 1 == GamePadTouch::kBindingCount);
		auto physical = std::make_shared<TestController>();
		physical->calibrate();
		VPADController vpad(0);
		vpad.add_controller(physical);
		vpad.set_mapping(VPADController::kButtonId_Touch1, physical, kButton0);
		vpad.set_mapping(VPADController::kButtonId_A, physical, kButton1);
		vpad.SetTouchPosition(0, 150, 80);
		const auto expected = GamePadTouch::FromLogical({150, 80});
		const BtnRepeat repeat{};
		const auto read = [&]()
		{
			VPADStatus_t status{};
			vpad.VPADRead(status, repeat);
			return status;
		};
		Check(read().tpData.touch == kTpTouchOff, "idle mapped button touched");
		physical->m_touchDown = true;
		auto status = read();
		Check(status.tpData.touch == kTpTouchOn && status.tpData.validity == kTpValid, "mapped button did not touch VPAD");
		Check(status.tpData.x == expected.x && status.tpData.y == expected.y, "VPAD received wrong coordinates");
		Check(status.hold == 0 && status.trig == 0, "touch action generated an ordinary button");
		Check(status.tpProcessed1.x == expected.x && status.tpProcessed2.y == expected.y, "processed touch data not copied");
		for (size_t i = 0; i < 100; ++i)
			Check(read().tpData.touch == kTpTouchOn, "held button interrupted touch");
		physical->m_aDown = true;
		status = read();
		const auto aFlag = vpad.get_emulated_button_flag(VPADController::kButtonId_A);
		Check(status.hold == aFlag && status.trig == aFlag, "normal A mapping broken during touch");
		physical->m_touchDown = false;
		status = read();
		Check(status.tpData.touch == kTpTouchOff && status.tpData.validity == kTpInvalid, "button release did not release VPAD touch");
		Check(status.tpData.x == expected.x && status.tpData.y == expected.y, "release lost last coordinates");
		Check(status.hold == aFlag && status.trig == 0, "touch release disturbed held A");
		physical->m_aDown = false;
		Check(read().release == aFlag, "normal A release broken");
		for (size_t i = 0; i < 10; ++i)
		{
			physical->m_touchDown = true;
			Check(read().tpData.touch == kTpTouchOn, "sampled press lost");
			physical->m_touchDown = false;
			Check(read().tpData.touch == kTpTouchOff, "sampled release lost");
		}
		physical->m_touchDown = true;
		auto& window = gui_getWindowInfo();
		for (const sint32 size : {480, 960, 1920})
		{
			window.phys_width = size;
			window.phys_height = size;
			window.phys_pad_width = size;
			window.phys_pad_height = size;
			window.pad_open = !window.pad_open;
			window.restored_pad_x = size;
			window.restored_pad_y = size;
			status = read();
			Check(status.tpData.x == expected.x && status.tpData.y == expected.y, "window geometry changed synthetic coordinates");
		}
		// Feed the same internal mouse state that the GUI uses, with a known image area.
		GetConfig().fullscreen_scaling = kStretch;
		window.phys_width = 854;
		window.phys_height = 480;
		auto& input = InputManager::instance();
		input.m_main_mouse.position = {427, 240};
		input.m_main_mouse.left_down = true;
		const auto pointer = GamePadTouch::FromNormalized(0.5f, 0.5f);
		status = read();
		Check(status.tpData.touch == kTpTouchOn && status.tpData.x == pointer.x && status.tpData.y == pointer.y, "existing mouse path lost priority");
		input.m_main_mouse.left_down = false;
		status = read();
		Check(status.tpData.x == expected.x && status.tpData.y == expected.y, "synthetic touch did not resume after mouse release");
		physical->m_touchDown = false;
		input.m_main_mouse.left_down_toggle = true;
		Check(read().tpData.touch == kTpTouchOn, "existing short mouse click latch broken");
		Check(read().tpData.touch == kTpTouchOff, "mouse click latch did not release");
		window.pad_open = true;
		window.phys_pad_width = 854;
		window.phys_pad_height = 480;
		input.m_pad_touch.position = {427, 240};
		input.m_pad_touch.left_down = true;
		status = read();
		Check(status.tpData.touch == kTpTouchOn && status.tpData.x == pointer.x && status.tpData.y == pointer.y, "existing native touchscreen path broken");
		input.m_pad_touch.left_down = false;
		Check(read().tpData.touch == kTpTouchOff, "native touchscreen did not release");
		window.pad_open = false;
		physical->m_touchDown = true;
		vpad.delete_mapping(VPADController::kButtonId_Touch1);
		Check(read().tpData.touch == kTpTouchOff, "deleted binding stuck down");

		pugi::xml_document doc;
		auto root = doc.append_child("emulated_controller");
		vpad.save(root);
		VPADController restored(0);
		restored.load(root);
		Check(restored.GetTouchPosition(0).x == 150 && restored.GetTouchPosition(0).y == 80, "VPAD custom settings did not round trip");
		pugi::xml_document old;
		old.load_string("<emulated_controller><toggle_display>1</toggle_display></emulated_controller>");
		restored.load(old.document_element());
		Check(restored.is_screen_active_toggle(), "existing custom setting failed to load");
		Check(restored.GetTouchPosition(0).x == 0 && restored.GetTouchPosition(0).y == 0, "old VPAD profile failed to load");

		// Use the actual InputManager file serializer/loader, isolated from user settings.
		const auto directory = fs::current_path() / "touch-profile-test";
		std::set<fs::path> failedWriteAccess;
		ActiveSettings::SetPaths(false, directory / "test.exe", directory, directory, directory, directory, failedWriteAccess);
		Check(failedWriteAccess.empty(), "test config directory is not writable");
		auto profile = std::make_shared<VPADController>(0);
		profile->add_controller(physical);
		profile->set_mapping(VPADController::kButtonId_Touch1, physical, kButton0);
		profile->set_mapping(VPADController::kButtonId_A, physical, kButton1);
		profile->SetTouchPosition(0, 150, 80);
		auto& manager = InputManager::instance();
		manager.set_controller(profile);
		Check(manager.save(0, "touch"), "input profile save failed");
		Check(manager.load(0, "touch"), "input profile load failed");
		const auto loaded = std::dynamic_pointer_cast<VPADController>(manager.get_controller(0));
		Check(loaded && loaded->GetTouchPosition(0).x == 150 && loaded->GetTouchPosition(0).y == 80, "input profile lost coordinates");
		Check(static_cast<bool>(loaded->get_mapping_controller(VPADController::kButtonId_Touch1)), "input profile lost touch mapping");
		Check(static_cast<bool>(loaded->get_mapping_controller(VPADController::kButtonId_A)), "input profile lost ordinary mapping");
		pugi::xml_document saved;
		const auto filename = directory / "controllerProfiles" / "touch.xml";
		Check(static_cast<bool>(saved.load_file(filename.c_str())), "saved profile XML invalid");
		Check(static_cast<bool>(saved.select_node("/emulated_controller/controller/mappings/entry[mapping='28' and button='0']")), "physical touch button not serialized");
		// Strip the optional feature to recreate an existing v2.6 profile.
		saved.document_element().remove_child("gamepad_touch");
		for (const auto entry : saved.select_nodes("/emulated_controller/controller/mappings/entry[mapping='28']"))
			entry.node().parent().remove_child(entry.node());
		const auto oldFilename = directory / "controllerProfiles" / "old.xml";
		Check(saved.save_file(oldFilename.c_str()), "old test profile write failed");
		Check(manager.load(0, "old"), "existing v2.6 profile failed to load");
		const auto oldLoaded = std::dynamic_pointer_cast<VPADController>(manager.get_controller(0));
		Check(oldLoaded && oldLoaded->GetTouchPosition(0).x == 0, "old profile touch defaults failed");
		Check(!oldLoaded->get_mapping_controller(VPADController::kButtonId_Touch1), "old profile acquired touch binding");
		Check(static_cast<bool>(oldLoaded->get_mapping_controller(VPADController::kButtonId_A)), "old profile lost ordinary binding");
		manager.delete_controller(0);

		// Build the real mapping panel in a hidden frame and exercise its editors.
		wxApp::SetInstance(new wxApp());
		Check(wxEntryStart(argc, argv), "wxWidgets test initialization failed");
		{
			wxFrame frame(nullptr, wxID_ANY, "GamePad touch UI test");
			auto* panel = new VPADInputPanel(&frame);
			panel->load_controller(profile);
			std::vector<wxSpinCtrl*> editors;
			wxTextCtrl* binding = nullptr;
			for (auto* child : panel->GetChildren())
			{
				if (auto* spin = dynamic_cast<wxSpinCtrl*>(child))
					editors.push_back(spin);
				if (auto* text = dynamic_cast<wxTextCtrl*>(child))
				{
					if (reinterpret_cast<uint64>(text->GetClientData()) == VPADController::kButtonId_Touch1)
						binding = text;
				}
			}
			Check(editors.size() == 6 && binding, "touch UI widgets missing");
			Check(editors[0]->GetValue() == 150 && editors[1]->GetValue() == 80, "touch UI did not restore coordinates");
			editors[0]->SetValue(300);
			wxSpinEvent change(wxEVT_SPINCTRL, editors[0]->GetId());
			change.SetEventObject(editors[0]);
			editors[0]->GetEventHandler()->ProcessEvent(change);
			Check(profile->GetTouchPosition(0).x == 300 && profile->GetTouchPosition(0).y == 80, "numeric editor did not save coordinates");
			// Loading another controller must not overwrite its Y using stale UI values.
			auto alternate = std::make_shared<VPADController>(1);
			alternate->SetTouchPosition(0, 123, 456);
			panel->load_controller(alternate);
			Check(alternate->GetTouchPosition(0).x == 123 && alternate->GetTouchPosition(0).y == 456, "UI load changed controller coordinates");
			panel->load_controller(profile);
			profile->delete_mapping(VPADController::kButtonId_Touch1);
			physical->m_touchDown = false;
			wxFocusEvent focus(wxEVT_SET_FOCUS, binding->GetId());
			focus.SetEventObject(binding);
			binding->GetEventHandler()->ProcessEvent(focus);
			panel->on_timer(profile, physical);
			physical->m_touchDown = true;
			panel->on_timer(profile, physical);
			Check(profile->get_mapping_controller(VPADController::kButtonId_Touch1) == physical, "touch widget did not capture physical button");
			panel->load_controller(nullptr);
		}
		wxEntryCleanup();
		std::cout << "Physical state, input mapping, VPAD status, normal buttons and VPAD profile tests passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
