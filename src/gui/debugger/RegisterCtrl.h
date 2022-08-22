#pragma once
#include "gui/components/TextList.h"
#include "Cafe/HW/Espresso/Debugger/Debugger.h"

class RegisterCtrl : public TextList
{
public:
	RegisterCtrl(wxWindow* parent, const wxWindowID& id, const wxPoint& pos, const wxSize& size, long style);
	void OnGameLoaded();
	
protected:
	void OnDraw(wxDC& dc, sint32 start, sint32 count, const wxPoint& start_position) override;
	void OnMouseMove(const wxPoint& position, uint32 line) override;
	void OnMouseDClick(const wxPoint& position, uint32 line) override;

private:
	PPCSnapshot m_prev_snapshot;
};
