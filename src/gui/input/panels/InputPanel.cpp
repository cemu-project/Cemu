#include "gui/guiWrapper.h"
#include "gui/input/panels/InputPanel.h"

#include <wx/textctrl.h>
#include <wx/wupdlock.h>

#include "gui/helpers/wxHelpers.h"

InputPanel::InputPanel(wxWindow* parent)
	: wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxNO_BORDER | wxWANTS_CHARS)
{
	Bind(wxEVT_LEFT_UP, &InputPanel::on_left_click, this);
}

void InputPanel::on_timer(const EmulatedControllerPtr& emulated_controller, const ControllerPtr& controller)
{
	const auto& state = controller->update_state();

	if(m_focused_element == wxID_NONE)
	{
		return;
	}

	const auto element = dynamic_cast<wxTextCtrl*>(FindWindow(m_focused_element));
	wxASSERT(element);

	const auto mapping = reinterpret_cast<uint64>(element->GetClientData());

	// reset mapping
	if(std::exchange(m_right_down, false) || gui_isKeyDown(PlatformKeyCodes::ESCAPE))
	{
		element->SetBackgroundColour(kKeyColourNormalMode);
		m_color_backup[element->GetId()] = kKeyColourNormalMode;

		emulated_controller->delete_mapping(mapping);
		if(element->IsEmpty())
			reset_focused_element();
		else
			element->SetValue(wxEmptyString);

		return;
	}

	static bool s_was_idle = true;
	if (state.buttons.IsIdle()) 
	{
		s_was_idle = true;
		return;
	}

	if (!s_was_idle) 
	{
		return;
	}
	s_was_idle = false;
	for(const auto& id : state.buttons.GetButtonList())
	{
		if (controller->has_axis())
		{
			// test if one axis direction is pressed more than the other
			if ((id == kAxisXP || id == kAxisXN) && (state.buttons.GetButtonState(kAxisYP) || state.buttons.GetButtonState(kAxisYN)))
			{
				if (std::abs(state.axis.y) > std::abs(state.axis.x))
					continue;
			}
			else if ((id == kAxisYP || id == kAxisYN) && (state.buttons.GetButtonState(kAxisXP) || state.buttons.GetButtonState(kAxisXN)))
			{
				if (std::abs(state.axis.x) > std::abs(state.axis.y))
					continue;
			}
			else if ((id == kRotationXP || id == kRotationXN) && (state.buttons.GetButtonState(kRotationYP) || state.buttons.GetButtonState(kRotationYN)))
			{
				if (std::abs(state.rotation.y) > std::abs(state.rotation.x))
					continue;
			}
			else if ((id == kRotationYP || id == kRotationYN) && (state.buttons.GetButtonState(kRotationXP) || state.buttons.GetButtonState(kRotationXN)))
			{
				if (std::abs(state.rotation.x) > std::abs(state.rotation.y))
					continue;
			}
			else if ((id == kTriggerXP || id == kTriggerXN) && (state.buttons.GetButtonState(kTriggerYP) || state.buttons.GetButtonState(kTriggerYN)))
			{
				if (std::abs(state.trigger.y) > std::abs(state.trigger.x))
					continue;
			}
			else if ((id == kTriggerYP || id == kTriggerYN) && (state.buttons.GetButtonState(kTriggerXP) || state.buttons.GetButtonState(kTriggerXN)))
			{
				if (std::abs(state.trigger.x) > std::abs(state.trigger.y))
					continue;
			}

			// ignore too low button values on configuration
			if (id >= kButtonAxisStart)
			{
				if (controller->get_axis_value(id) < 0.33f) {
					cemuLog_logDebug(LogType::Force, "skipping since value too low {}", controller->get_axis_value(id));
					s_was_idle = true;
					return;
				}
			}
		}

		emulated_controller->set_mapping(mapping, controller, id);
		element->SetValue(controller->get_button_name(id));
		element->SetBackgroundColour(kKeyColourNormalMode);
		m_color_backup[element->GetId()] = kKeyColourNormalMode;
		break;
	}

	if (const auto sibling = get_next_sibling(element))
		sibling->SetFocus();
	else // last element reached 
	{
		reset_focused_element();
		this->SetFocusIgnoringChildren();
	}
}

void InputPanel::reset_configuration()
{
	m_color_backup.clear();

	wxWindowUpdateLocker lock(this);
	for (const auto& c : GetChildren())
	{
		if (auto* text = dynamic_cast<wxTextCtrl*>(c))
		{
			text->SetValue(wxEmptyString);
			text->SetBackgroundColour(kKeyColourNormalMode);
			text->Refresh();
		}
	}
}

void InputPanel::reset_colours()
{
	m_color_backup.clear();

	wxWindowUpdateLocker lock(this);
	for (const auto& c : GetChildren())
	{
		if (auto* text = dynamic_cast<wxTextCtrl*>(c))
		{
			text->SetBackgroundColour(kKeyColourNormalMode);
			text->Refresh();
		}
	}
}


void InputPanel::load_controller(const EmulatedControllerPtr& controller)
{
	reset_configuration();
	if(!controller)
	{
		return;
	}

	if(controller->get_controllers().empty())
	{
		return;
	}

	wxWindowUpdateLocker lock(this);
	for (auto* child : this->GetChildren())
	{
		const auto text = dynamic_cast<wxTextCtrl*>(child);
		if (text == nullptr)
			continue;


		const auto mapping = reinterpret_cast<sint64>(text->GetClientData());
		if (mapping <= 0)
			continue;

		auto button_name = controller->get_mapping_name(mapping);
#if BOOST_OS_WINDOWS
		text->SetLabelText(button_name);
#else
        // SetLabelText doesn't seem to work here for some reason on wxGTK
        text->ChangeValue(button_name);
#endif
	}
}

void InputPanel::set_selected_controller(const EmulatedControllerPtr& emulated_controller, const ControllerPtr& controller)
{
	wxWindowUpdateLocker lock(this);
	for (auto* child : this->GetChildren())
	{
		const auto text = dynamic_cast<wxTextCtrl*>(child);
		if (text == nullptr)
			continue;

		if (text->GetId() == m_focused_element)
			continue;

		const auto mapping = reinterpret_cast<sint64>(text->GetClientData());
		if (mapping <= 0)
			continue;

		const auto mapping_controller = emulated_controller->get_mapping_controller(mapping);
		if (!mapping_controller)
			continue;

		text->SetBackgroundColour(*mapping_controller == *controller ? kKeyColourNormalMode : kKeyColourActiveMode);
		text->Refresh();
	}
}

void InputPanel::bind_hotkey_events(wxTextCtrl* text_ctrl)
{
	text_ctrl->Bind(wxEVT_SET_FOCUS, &InputPanel::on_edit_key_focus, this);
	text_ctrl->Bind(wxEVT_KILL_FOCUS, &InputPanel::on_edit_key_kill_focus, this);
	text_ctrl->Bind(wxEVT_RIGHT_DOWN, &InputPanel::on_right_click, this);
#if BOOST_OS_LINUX
	// Bind to a no-op lambda to disable arrow keys navigation
	text_ctrl->Bind(wxEVT_KEY_DOWN, [](wxKeyEvent &) {});
#endif
}

void InputPanel::on_left_click(wxMouseEvent& event)
{
	if (m_focused_element == wxID_NONE)
		return;

	const auto focuses_element = FindWindow(m_focused_element);
	wxASSERT(focuses_element);

	wxFocusEvent focus(wxEVT_KILL_FOCUS, m_focused_element);
	focus.SetWindow(focuses_element);
	focuses_element->GetEventHandler()->ProcessEvent(focus);

	this->SetFocusIgnoringChildren();
}

void InputPanel::on_edit_key_focus(wxFocusEvent& event)
{
	auto* text = dynamic_cast<wxTextCtrl*>(event.GetEventObject());
	wxASSERT(text);

	m_color_backup[text->GetId()] = text->GetBackgroundColour();

	text->SetBackgroundColour(kKeyColourEditMode);
	#if BOOST_OS_WINDOWS
	text->HideNativeCaret();
	#endif
	text->Refresh();

	m_focused_element = text->GetId();
	event.Skip();
}

void InputPanel::on_edit_key_kill_focus(wxFocusEvent& event)
{
	reset_focused_element();
	event.Skip();
}

void InputPanel::on_right_click(wxMouseEvent& event)
{
	m_right_down = true;
	if(m_focused_element == wxID_NONE)
	{
		auto* text = dynamic_cast<wxTextCtrl*>(event.GetEventObject());
		wxASSERT(text);
		text->SetFocus();
	}
}

bool InputPanel::reset_focused_element()
{
	if (m_focused_element == wxID_NONE)
		return false;

	auto* prev_element = dynamic_cast<wxTextCtrl*>(FindWindow(m_focused_element));
	wxASSERT(prev_element);

	if(m_color_backup.find(prev_element->GetId()) != m_color_backup.cend())
		prev_element->SetBackgroundColour(m_color_backup[prev_element->GetId()]);
	else
		prev_element->SetBackgroundColour(kKeyColourNormalMode);

#if BOOST_OS_WINDOWS
	prev_element->HideNativeCaret();
#endif
	prev_element->Refresh();
	
	m_focused_element = wxID_NONE;
	return true;
}
