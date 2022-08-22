#include "gui/wxcomponents/checktree.h"
#include "gui/wxcomponents/checked2.xpm"
#include "gui/wxcomponents/checked_d.xpm"
#include "gui/wxcomponents/checked_ld.xpm"
#include "gui/wxcomponents/checked_mo.xpm"
#include "gui/wxcomponents/unchecked2.xpm"
#include "gui/wxcomponents/unchecked_d.xpm"
#include "gui/wxcomponents/unchecked_ld.xpm"
#include "gui/wxcomponents/unchecked_mo.xpm"
#include <wx/icon.h>
#include <wx/imaglist.h>

wxDEFINE_EVENT(wxEVT_CHECKTREE_FOCUS, wxTreeEvent);
wxDEFINE_EVENT(wxEVT_CHECKTREE_CHOICE, wxTreeEvent);

//IMPLEMENT_DYNAMIC_CLASS(wxCheckTree, wxTreeCtrl)

bool on_check_or_label(int flags)
{
	return flags & (wxTREE_HITTEST_ONITEMSTATEICON | wxTREE_HITTEST_ONITEMLABEL) ? true : false;
}

bool on_check(int flags)
{
	return flags & (wxTREE_HITTEST_ONITEMSTATEICON) ? true : false;
}

bool on_label(int flags)
{
	return flags & (wxTREE_HITTEST_ONITEMLABEL) ? true : false;
}

void unhighlight(wxTreeCtrl* m_treeCtrl1, wxTreeItemId& id)
{
	if (!id.IsOk())
		return;

	const int state = m_treeCtrl1->GetItemState(id);
	if (wxCheckTree::UNCHECKED <= state && state < wxCheckTree::UNCHECKED_DISABLED)
	{
		m_treeCtrl1->SetItemState(id, wxCheckTree::UNCHECKED);
	}
	else if (wxCheckTree::CHECKED <= state && state < wxCheckTree::CHECKED_DISABLED)
	{
		m_treeCtrl1->SetItemState(id, wxCheckTree::CHECKED);
	}
}


void mohighlight(wxTreeCtrl* m_treeCtrl1, wxTreeItemId& id, bool toggle)
{
	if (!id.IsOk())
		return;

	const int i = m_treeCtrl1->GetItemState(id);
	if (i < 0)
		return;

	bool is_checked = false;
	if (wxCheckTree::UNCHECKED <= i && i < wxCheckTree::UNCHECKED_DISABLED)
	{
		m_treeCtrl1->SetItemState(id, toggle ? wxCheckTree::CHECKED_MOUSE_OVER : wxCheckTree::UNCHECKED_MOUSE_OVER);
		is_checked = true;
	}
	else if (wxCheckTree::CHECKED <= i && i < wxCheckTree::CHECKED_DISABLED)
	{
		m_treeCtrl1->SetItemState(id, toggle ? wxCheckTree::UNCHECKED_MOUSE_OVER : wxCheckTree::CHECKED_MOUSE_OVER);
		is_checked = false;
	}

	if (toggle)
	{
		wxTreeEvent event2(wxEVT_CHECKTREE_CHOICE, m_treeCtrl1, id);
		event2.SetExtraLong(is_checked ? 1 : 0);
		m_treeCtrl1->ProcessWindowEvent(event2);
	}
}

void ldhighlight(wxTreeCtrl* m_treeCtrl1, wxTreeItemId& id)
{
	if (!id.IsOk())
		return;

	const int i = m_treeCtrl1->GetItemState(id);
	if (wxCheckTree::UNCHECKED <= i && i < wxCheckTree::UNCHECKED_DISABLED)
	{
		m_treeCtrl1->SetItemState(id, wxCheckTree::UNCHECKED_LEFT_DOWN);
	}
	else if (wxCheckTree::CHECKED <= i && i < wxCheckTree::CHECKED_DISABLED)
	{
		m_treeCtrl1->SetItemState(id, wxCheckTree::CHECKED_LEFT_DOWN);
	}
}

wxIMPLEMENT_CLASS(wxCheckTree, wxTreeCtrl);

wxCheckTree::wxCheckTree()
{
	Init();
}

wxCheckTree::wxCheckTree(wxWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
	: wxTreeCtrl(parent, id, pos, size, style)
{
	Init();
}


void wxCheckTree::Init()
{
	wxIcon icons[8] = 
	{
		wxIcon(unchecked2_xpm), wxIcon(unchecked_mo_xpm), wxIcon(unchecked_ld_xpm), wxIcon(unchecked_d_xpm), wxIcon(checked2_xpm), wxIcon(checked_mo_xpm), wxIcon(checked_ld_xpm), wxIcon(checked_d_xpm)
	};

	// Make an state image list containing small icons
	auto states = new wxImageList(icons[0].GetWidth(), icons[0].GetHeight(), true);

	for (const auto& icon : icons)
		states->Add(icon);

	AssignStateImageList(states);

	Connect(wxEVT_TREE_SEL_CHANGING, wxTreeEventHandler( wxCheckTree::On_Tree_Sel_Changed ), nullptr, this);

	Connect(wxEVT_CHAR, wxKeyEventHandler( wxCheckTree::On_Char ), nullptr, this);
	Connect(wxEVT_KEY_DOWN, wxKeyEventHandler( wxCheckTree::On_KeyDown ), nullptr, this);
	Connect(wxEVT_KEY_UP, wxKeyEventHandler( wxCheckTree::On_KeyUp ), nullptr, this);

	Connect(wxEVT_ENTER_WINDOW, wxMouseEventHandler( wxCheckTree::On_Mouse_Enter_Tree ), nullptr, this);
	Connect(wxEVT_LEAVE_WINDOW, wxMouseEventHandler( wxCheckTree::On_Mouse_Leave_Tree ), nullptr, this);
	Connect(wxEVT_LEFT_DCLICK, wxMouseEventHandler( wxCheckTree::On_Left_DClick ), nullptr, this);
	Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler( wxCheckTree::On_Left_Down ), nullptr, this);
	Connect(wxEVT_LEFT_UP, wxMouseEventHandler( wxCheckTree::On_Left_Up ), nullptr, this);
	Connect(wxEVT_MOTION, wxMouseEventHandler( wxCheckTree::On_Mouse_Motion ), nullptr, this);
	Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler( wxCheckTree::On_Mouse_Wheel ), nullptr, this);

	Connect(wxEVT_SET_FOCUS, wxFocusEventHandler( wxCheckTree::On_Tree_Focus_Set ), nullptr, this);
	Connect(wxEVT_KILL_FOCUS, wxFocusEventHandler( wxCheckTree::On_Tree_Focus_Lost ), nullptr, this);
}

void wxCheckTree::Sort(const wxTreeItemId& node, bool recursive)
{
	if (recursive)
	{
		wxTreeItemIdValue cookie;
		for(auto it = GetFirstChild(node, cookie); it.IsOk(); it = GetNextChild(it, cookie))
		{
			Sort(it, true);
		}
	}

	if(GetChildrenCount(node, false) > 0)
		this->SortChildren(node);
}

int wxCheckTree::OnCompareItems(const wxTreeItemId& item1, const wxTreeItemId& item2)
{
	const bool check1 = GetChildrenCount(item1, false) == 0;
	const bool check2 = GetChildrenCount(item2, false) == 0;

	if (!check1 && check2)
		return -1;

	if (check1 && !check2)
		return 1;

	return GetItemText(item1).Lower().compare(GetItemText(item2).Lower());
}


void wxCheckTree::SetItemTextColour(const wxTreeItemId& item, const wxColour& col)
{
	const auto it = m_colors.find(item);
	if (it == m_colors.end())
		m_colors.emplace(std::pair<wxTreeItemId,wxColor>(item, col));
	else
		m_colors[item] = col;

	wxTreeCtrl::SetItemTextColour(item, col);
}

bool wxCheckTree::EnableCheckBox(const wxTreeItemId& item, bool enable)
{
	if (!item.IsOk())
		return false;
	
	const int state = GetItemState(item);

	if (state < 0 || state > CHECKED_DISABLED)
		return false;

	if (enable)
	{
		if (state == UNCHECKED_DISABLED)
			SetItemState(item, UNCHECKED);
		else if (state == CHECKED_DISABLED)
			SetItemState(item, CHECKED);

		const auto it = m_colors.find(item);
		if (it != m_colors.end())
		{
			SetItemTextColour(item, it->second);
			m_colors.erase(it);
		}

		return true;
	}
	if (state == UNCHECKED_DISABLED || state == CHECKED_DISABLED)
	{
		//don't disable a second time or we'll lose the
		//text color information.
		return true;
	}

	if (state == UNCHECKED || state == UNCHECKED_MOUSE_OVER || state == UNCHECKED_LEFT_DOWN)
		SetItemState(item, UNCHECKED_DISABLED);
	else if (state == CHECKED || state == CHECKED_MOUSE_OVER || state == CHECKED_LEFT_DOWN)
		SetItemState(item, CHECKED_DISABLED);

	const wxColor col = GetItemTextColour(item);
	SetItemTextColour(item, wxColour(161, 161, 146));
	m_colors[item] = col;
	return true;
}

bool wxCheckTree::DisableCheckBox(const wxTreeItemId& item)
{
	return EnableCheckBox(item, false);
}

void wxCheckTree::MakeCheckable(const wxTreeItemId& item, bool state)
{
	if (!item.IsOk())
		return;

	const int i = GetItemState(item);
	if (i < 0 || i > CHECKED_DISABLED)
		SetItemState(item, state ? CHECKED : UNCHECKED);
}

bool wxCheckTree::IsCheckable(const wxTreeItemId& item)
{
	if (!item.IsOk())
		return false;

	const int i = GetItemState(item);
	return i >= 0 && i <= CHECKED_DISABLED;
}

void wxCheckTree::Check(const wxTreeItemId& item, bool state)
{
	if (!item.IsOk())
		return;

	const int old_state = GetItemState(item);
	if (UNCHECKED <= old_state && old_state <= CHECKED_DISABLED)
	{
		const bool enable = !(old_state == UNCHECKED_DISABLED || old_state == CHECKED_DISABLED);
		int check = enable ? CHECKED : CHECKED_DISABLED;
		int uncheck = enable ? UNCHECKED : UNCHECKED_DISABLED;
		const int new_state = state ? check : uncheck;

		if (new_state != old_state)
		{
			SetItemState(item, new_state);
		}
	}
}

void wxCheckTree::Uncheck(const wxTreeItemId& item)
{
	Check(item, false);
}

void wxCheckTree::On_Tree_Sel_Changed(wxTreeEvent& event)
{
	wxTreeItemId id = event.GetItem();

	unhighlight(this, last_kf);
	mohighlight(this, id, false);
	last_kf = id;

	event.Skip();
}

void wxCheckTree::On_Char(wxKeyEvent& event)
{
	if (!GetSelection().IsOk() && GetCount() > 0)
	{
		//If there is no selection, any keypress should just select the first item
		wxTreeItemIdValue cookie;
		const auto new_item = HasFlag(wxTR_HIDE_ROOT) ? GetFirstChild(GetRootItem(), cookie) : GetRootItem();
		SelectItem(new_item);
		last_kf = new_item;
		return;
	}

	event.Skip();
}

void wxCheckTree::On_KeyDown(wxKeyEvent& event)
{
	if (event.GetKeyCode() == WXK_SPACE)
	{
		last_kf = this->GetSelection();
		ldhighlight(this, last_kf);
	}

	event.Skip();
}

void wxCheckTree::On_KeyUp(wxKeyEvent& event)
{
	if (event.GetKeyCode() == WXK_SPACE)
	{
		//last_kf = this->GetSelection();
 		mohighlight(this, last_kf, true);
	}
	else if (event.GetKeyCode() == WXK_ESCAPE)
	{
		unhighlight(this, last_kf);
		last_kf = {};
		Unselect();
	}

	event.Skip();
}


void wxCheckTree::On_Mouse_Enter_Tree(wxMouseEvent& event)
{
	if (event.LeftIsDown())
	{
		mouse_entered_tree_with_left_down = true;
	}
}

void wxCheckTree::On_Mouse_Leave_Tree(wxMouseEvent& event)
{
	unhighlight(this, last_mouse_over);
	unhighlight(this, last_left_down);
	last_mouse_over = {};
	last_left_down = {};
}

void wxCheckTree::On_Left_DClick(wxMouseEvent& event)
{
	int flags;

	HitTest(event.GetPosition(), flags);

	//double clicks on buttons can be annoying, so we'll ignore those
	//but all other double clicks will just have 1 more click added to them

	//without this, the check boxes are not as responsive as they should be
	if (!(flags & wxTREE_HITTEST_ONITEMBUTTON))
	{
		On_Left_Down(event);
		On_Left_Up(event);
	}
}

void wxCheckTree::On_Left_Down(wxMouseEvent& event)
{
	int flags;
	wxTreeItemId id = HitTest(event.GetPosition(), flags);
	if (!id.IsOk())
		return;

	int i = GetItemState(id);

	if (id.IsOk() && i >= 0 && on_check(flags))
	{
		last_left_down = id;
		ldhighlight(this, id);
	}
	else if (on_label(flags))
		event.Skip();
}

void wxCheckTree::On_Left_Up(wxMouseEvent& event)
{
	SetFocus();

	int flags;
	wxTreeItemId id = HitTest(event.GetPosition(), flags);
	if (!id.IsOk())
		return;

	int i = GetItemState(id);

	if (mouse_entered_tree_with_left_down)
	{
		mouse_entered_tree_with_left_down = false;

		if (i >= 0 && on_check(flags))
		{
			mohighlight(this, id, false);
			last_mouse_over = id;
		}
	}
	else if (id.IsOk())
	{
		if (flags & wxTREE_HITTEST_ONITEMBUTTON && ItemHasChildren(id))
		{
			if (IsExpanded(id))
			{
				Collapse(id);
			}
			else
			{
				Expand(id);
			}
		}
		else if (i >= 0 && on_check(flags))
		{
			if (id != last_left_down)
			{
				unhighlight(this, last_left_down);
				mohighlight(this, id, false);
			}
			else
			{
				mohighlight(this, id, true);
			}

			last_left_down = wxTreeItemId();
			last_mouse_over = id;
		}
		else if (on_label(flags))
		{
			event.Skip();
		}
		else
		{
			unhighlight(this, last_left_down);
			unhighlight(this, last_mouse_over);
			last_left_down = wxTreeItemId();
			last_mouse_over = wxTreeItemId();
		}
	}
	else
	{
		//id is not ok
		unhighlight(this, last_left_down);
		unhighlight(this, last_mouse_over);
		last_left_down = wxTreeItemId();
		last_mouse_over = wxTreeItemId();
	}
}

void wxCheckTree::On_Mouse_Motion(wxMouseEvent& event)
{
	if (mouse_entered_tree_with_left_down)
	{
		//just ignore everything until the left button is released
		return;
	}

	int flags;
	wxTreeItemId id = HitTest(event.GetPosition(), flags);

	if (!id.IsOk())
	{
		unhighlight(this, last_mouse_over);
		last_mouse_over = {};
	}
	else if (event.LeftIsDown() && last_left_down.IsOk())
	{
		//to match the behavior of ordinary check boxes,
		//if we've moved to a new item while holding the mouse button down
		//we want to set the item where the left down click occured to have
		//mouse over highlight.  And if we return to the box where the
		//left down occured, we want to return it to the having the left down highlight

		//I don't understand why this is the behavior
		//of ordinary check boxes, but I'm goin to match it anyway.

		if (id == last_left_down)
		{
			const auto state = GetItemState(last_left_down);
			if (state != UNCHECKED_LEFT_DOWN && state != CHECKED_LEFT_DOWN)
			{
				ldhighlight(this, last_left_down);
			}
		}
		else
			mohighlight(this, last_left_down, false);
	}
	else
	{
		//4 cases 1 we're still on the same item, but we've moved off the state icon or label
		//        2 we're still on the same item and on the state icon or label - do nothing
		//        3 we're on a new item but not on its state icon or label (or the new item has no state)
		//        4 we're on a new item, it has a state icon, and we're on the state icon or label

		const int state = GetItemState(id);
		if (id == last_mouse_over)
		{
			if (state < 0 || !on_check(flags))
			{
				unhighlight(this, last_mouse_over);
				last_mouse_over = {};
			}
		}
		else
		{
			if (state < 0 || !on_check(flags))
			{
				unhighlight(this, last_mouse_over);
				last_mouse_over = {};
			}
			else
			{
				unhighlight(this, last_mouse_over);
				mohighlight(this, id, false);
				last_mouse_over = id;
			}
		}
	}
}

void wxCheckTree::On_Mouse_Wheel(wxMouseEvent& event)
{
	event.Skip();
}

void wxCheckTree::On_Tree_Focus_Set(wxFocusEvent& event)
{
	//event.Skip();

	//skipping this event will set the last selected item
	//to be highlighted and I want the tree items to only be
	//highlighted by keyboard actions.
}

void wxCheckTree::On_Tree_Focus_Lost(wxFocusEvent& event)
{
	unhighlight(this, last_kf);
	Unselect();
	event.Skip();
}

void wxCheckTree::SetFocusFromKbd()
{
	if (last_kf.IsOk())
		SelectItem(last_kf);

	wxTreeEvent event2(wxEVT_CHECKTREE_FOCUS, this, wxTreeItemId());
	ProcessWindowEvent(event2);

	wxWindow::SetFocusFromKbd();
}
