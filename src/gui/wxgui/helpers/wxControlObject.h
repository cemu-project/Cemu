#pragma once

#include <wx/control.h>

class wxControlObject : public wxObject
{
public:
	wxControlObject(wxControl* control)
		: m_control(control) {}

	template<typename T>
	T* GetControl() const
	{
		static_assert(std::is_base_of<wxControl, T>::value, "T must inherit from wxControl");
		return dynamic_cast<T*>(m_control);
	}

	wxControl* GetControl() const { return m_control; }

private:
	wxControl* m_control;
};

class wxControlObjects : public wxObject
{
public:
	wxControlObjects(std::vector<wxControl*> controls)
		: m_controls(std::move(controls)) {}

	template<typename T = wxControl>
	T* GetControl(int index) const
	{
		static_assert(std::is_base_of<wxControl, T>::value, "T must inherit from wxControl");
		if (index < 0 || index >= m_controls.size())
			return nullptr;
		
		return dynamic_cast<T*>(m_controls[index]);
	}

	const std::vector<wxControl*>& GetControls() const { return m_controls; }

private:
	std::vector<wxControl*> m_controls;
};