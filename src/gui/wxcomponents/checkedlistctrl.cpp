/////////////////////////////////////////////////////////////////////////////
// Name:        checkedlistctrl.cpp
// Purpose:     wxCheckedListCtrl
// Author:      Unknown ? (found at http://wiki.wxwidgets.org/wiki.pl?WxListCtrl)
// Modified by: Francesco Montorsi
// Created:     2005/06/29
// RCS-ID:      $Id: checkedlistctrl.cpp 1309 2010-05-01 09:49:59Z frm $
// Copyright:   (c) 2005 Francesco Montorsi
// Licence:     wxWidgets licence
/////////////////////////////////////////////////////////////////////////////


// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

// includes
#include "gui/wxcomponents/checkedlistctrl.h"
#include <wx/icon.h>

#include <wx/settings.h>

#if wxUSE_CHECKEDLISTCTRL

// resources

#include "gui/wxcomponents/checked.xpm"
#include "gui/wxcomponents/unchecked.xpm"
#include "gui/wxcomponents/checked_dis.xpm"
#include "gui/wxcomponents/unchecked_dis.xpm"

IMPLEMENT_CLASS(wxCheckedListCtrl, wxListCtrl)
BEGIN_EVENT_TABLE(wxCheckedListCtrl, wxListCtrl)
    EVT_LEFT_DOWN(wxCheckedListCtrl::OnMouseEvent)
END_EVENT_TABLE()

DEFINE_EVENT_TYPE(wxEVT_COMMAND_LIST_ITEM_CHECKED);
DEFINE_EVENT_TYPE(wxEVT_COMMAND_LIST_ITEM_UNCHECKED);



// ------------------
// wxCHECKEDLISTCTRL
// ------------------

bool wxCheckedListCtrl::Create(wxWindow* parent, wxWindowID id, const wxPoint& pt,
        const wxSize& sz, long style, const wxValidator& validator, const wxString& name)
{
	if (!wxListCtrl::Create(parent, id, pt, sz, style, validator, name))
		return FALSE;

    SetImageList(&m_imageList, wxIMAGE_LIST_SMALL);

	// the add order must respect the wxCLC_XXX_IMGIDX defines in the headers !
    m_imageList.Add(wxIcon(unchecked_xpm));
    m_imageList.Add(wxIcon(checked_xpm));
    m_imageList.Add(wxIcon(unchecked_dis_xpm));
    m_imageList.Add(wxIcon(checked_dis_xpm));

	return TRUE;
}

/* static */
int wxCheckedListCtrl::GetItemImageFromAdditionalState(int addstate)
{
	bool checked = (addstate & wxLIST_STATE_CHECKED) != 0;
	bool enabled = (addstate & wxLIST_STATE_ENABLED) != 0;

	if (checked && enabled)
		return wxCLC_CHECKED_IMGIDX;
	else if (checked && !enabled)
		return wxCLC_DISABLED_CHECKED_IMGIDX;
	else if (!checked && enabled)
		return wxCLC_UNCHECKED_IMGIDX;
	
	wxASSERT(!checked && !enabled);		// this is the last possibility
	return wxCLC_DISABLED_UNCHECKED_IMGIDX;
}

wxColour wxCheckedListCtrl::GetBgColourFromAdditionalState(int additionalstate)
{
	if ((additionalstate & wxLIST_STATE_ENABLED) &&
		this->IsEnabled())
		return *wxWHITE;
#ifdef __WXMSW__
	return wxColour(212, 208, 200);
#else
	return wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
#endif
}

/* static */
int wxCheckedListCtrl::GetAndRemoveAdditionalState(long *state, int statemask)
{
	int additionalstate = 0;
	if (!state) return -1;

	// extract the bits we are interested in
	bool checked = (*state & wxLIST_STATE_CHECKED) != 0;
	bool enabled = (*state & wxLIST_STATE_ENABLED) != 0;

	// and set them in a different variable if they are included in the statemask
	if (checked && (statemask & wxLIST_STATE_CHECKED)) additionalstate |= wxLIST_STATE_CHECKED;
	if (enabled && (statemask & wxLIST_STATE_ENABLED)) additionalstate |= wxLIST_STATE_ENABLED;

	// remove them from the original state var...
	*state &= ~wxLIST_STATE_CHECKED;
	*state &= ~wxLIST_STATE_ENABLED;
	return additionalstate;
}

bool wxCheckedListCtrl::GetItem(wxListItem& info) const
{
	// wx internal wxListCtrl::GetItem remove from the state mask the
	// wxLIST_STATE_CHECKED & wxLIST_STATE_ENABLED bits since they
	// are not part of wx standard flags... so we need to check those
	// flags against the original wxListItem's statemask...
	wxListItem original(info);

#ifdef __WXDEBUG__
	// we always want to retrieve also the image state for checking purposes...
	info.m_mask |= wxLIST_MASK_IMAGE;
#endif

	if (!wxListCtrl::GetItem(info))	
		return FALSE;

	// these are our additional supported states: read them from m_stateList
	bool checked = (m_stateList[info.m_itemId] & wxLIST_STATE_CHECKED) != 0;
	bool enabled = (m_stateList[info.m_itemId] & wxLIST_STATE_ENABLED) != 0;

	// now intercept state requests about enable or check mode
	if ((original.m_mask & wxLIST_MASK_STATE) &&
		(original.m_stateMask & wxLIST_STATE_CHECKED)) {
		info.m_state |= (m_stateList[info.m_itemId] & wxLIST_STATE_CHECKED);
		info.m_stateMask |= wxLIST_STATE_CHECKED;
		info.m_mask |= wxLIST_MASK_STATE;		// contains valid info !
	}
	if ((original.m_mask & wxLIST_MASK_STATE) &&
		(original.m_stateMask & wxLIST_STATE_ENABLED)) {
		info.m_state |= (m_stateList[info.m_itemId] & wxLIST_STATE_ENABLED);
		info.m_stateMask |= wxLIST_STATE_ENABLED;
		info.m_mask |= wxLIST_MASK_STATE;		// contains valid info !
	}

	// check that state & image are synch
#ifdef __WXDEBUG__

	wxASSERT_MSG((int)m_stateList.GetCount() == (int)GetItemCount(),
					wxS("Something wrong ! See InsertItem()"));

	// read info by image index
	bool imagecheck = (info.m_image == wxCLC_CHECKED_IMGIDX) ||
						(info.m_image == wxCLC_DISABLED_CHECKED_IMGIDX);
	bool imageenabled = (info.m_image == wxCLC_CHECKED_IMGIDX) ||
						(info.m_image == wxCLC_UNCHECKED_IMGIDX);
	wxASSERT_MSG((checked && imagecheck) || (!checked && !imagecheck),
		wxS("This is item has checked state but it's shown as unchecked (or viceversa)"));
	wxASSERT_MSG((enabled && imageenabled) || (!enabled && !imageenabled),
		wxS("This is item has enabled state but it's shown as disabled (or viceversa)"));
#endif

	return TRUE;
}

bool wxCheckedListCtrl::SetItem(wxListItem& info)
{
	// remove the checked & enabled states from the state flag:
	// we'll store them in our separate array
	int additionalstate = GetAndRemoveAdditionalState(&info.m_state, info.m_stateMask);

	// set image index	
	// we will ignore the info.m_image field since we need
	// to overwrite it...
	if (info.m_mask & wxLIST_MASK_STATE) {

		// if some state is not included in the state mask, then get the state info
		// from our internal state array
		if (!(info.m_stateMask & wxLIST_STATE_ENABLED))
			additionalstate |= (m_stateList[info.m_itemId] & wxLIST_STATE_ENABLED);
		if (!(info.m_stateMask & wxLIST_STATE_CHECKED))
			additionalstate |= (m_stateList[info.m_itemId] & wxLIST_STATE_CHECKED);

		// state is valid: use it to determine the image to set...
		info.m_mask |= wxLIST_MASK_IMAGE;
		info.m_image = GetItemImageFromAdditionalState(additionalstate);

		// since when changing the background color, also the foreground color
		// and the font of the item are changed, we try to respect the user
		// choices of such attributes
		info.SetTextColour(this->GetItemTextColour(info.GetId()));
#if wxCHECK_VERSION(2, 6, 2)
		// before wx 2.6.2 the wxListCtrl::SetItemFont function is missing
		info.SetFont(this->GetItemFont(info.GetId()));
#endif
		
		// change the background color to respect the enabled/disabled status...
		info.SetBackgroundColour(GetBgColourFromAdditionalState(additionalstate));

		m_stateList[info.m_itemId] = additionalstate;

	} else {

		// state is invalid; don't change image
		info.m_mask &= ~wxLIST_MASK_IMAGE;
	}

	// save the changes
	return wxListCtrl::SetItem(info);
}

long wxCheckedListCtrl::InsertItem(wxListItem &info)
{
	int additionalstate = GetAndRemoveAdditionalState(&info.m_state, info.m_stateMask);
	if (!(info.m_mask & wxLIST_MASK_STATE) ||
		!(info.m_stateMask & wxLIST_STATE_ENABLED)) {

		// if not specified, the default additional state is ENABLED
		additionalstate = wxLIST_STATE_ENABLED;
	}

	// we always want to insert items with images...
	info.m_mask |= wxLIST_MASK_IMAGE;
	info.m_image = GetItemImageFromAdditionalState(additionalstate);
	info.SetBackgroundColour(GetBgColourFromAdditionalState(additionalstate));

	int itemcount = GetItemCount();
	wxASSERT_MSG(info.m_itemId <= itemcount, wxS("Invalid index !"));
	wxASSERT_MSG((int)m_stateList.GetCount() == (int)GetItemCount(),
					wxS("Something wrong !"));
	if (info.m_itemId == itemcount) {

		// we are adding a new item at the end of the list
		m_stateList.Add(additionalstate);

	} else {

		// we must shift all following items
		cemu_assert_suspicious();
		//for (int i=itemcount; i > info.m_itemId; i--)
		//	m_stateList[i] = m_stateList[i-1];
		m_stateList[info.m_itemId] = additionalstate;
	}
	
	return wxListCtrl::InsertItem(info);
}

bool wxCheckedListCtrl::SetItemState(long item, long state, long stateMask)
{ 
	wxListItem li; 
	li.SetId(item); 
	li.SetMask(wxLIST_MASK_STATE);
	li.SetState(state); 
	li.SetStateMask(stateMask); 

	// so we are sure to use wxCheckedListCtrl::SetItem
	// (and not wxListCtrl::SetItem)
	return SetItem(li); 
}

int wxCheckedListCtrl::GetItemState(long item, long stateMask) const
{ 
	wxListItem li; 
	li.SetId(item); 
	li.SetMask(wxLIST_MASK_STATE);
	li.SetStateMask(stateMask); 
	
	// so we are sure to use wxCheckedListCtrl::GetItem
	// (and not wxListCtrl::GetItem)
	if (!GetItem(li)) 
		return -1;
	return li.GetState();
}

long wxCheckedListCtrl::SetItem(long index, int col, const wxString& label, int WXUNUSED(imageId))
{
	wxListItem li; 
	li.SetId(index); 
	li.SetColumn(col);	
	li.SetText(label);
	li.SetMask(wxLIST_MASK_TEXT);
	
	// so we are sure to use wxCheckedListCtrl::SetItem
	// (and not wxListCtrl::SetItem)
	return SetItem(li);
}

long wxCheckedListCtrl::InsertItem( long index, const wxString& label, int WXUNUSED(imageIndex) )
{
    wxListItem info;
    info.m_text = label;
    info.m_mask = wxLIST_MASK_TEXT;
    info.m_itemId = index;
    return InsertItem(info);
}

void wxCheckedListCtrl::Check(long item, bool checked)
{ 
	// NB: the "statemask" is not the "mask" of a list item;
	//     in the "mask" you use the wxLIST_MASK_XXXX defines;
	//     in the "statemask" you use the wxLIST_STATE_XXX defines
	//     to set a specific bit of the wxListInfo::m_state var
	if (checked) 
		// the 2nd parameter says: activate the STATE bit relative to CHECK feature
		// the 3rd parameter says: set only *that* bit 
		SetItemState(item, wxLIST_STATE_CHECKED, wxLIST_STATE_CHECKED); 
	else
		SetItemState(item, 0, wxLIST_STATE_CHECKED); 
}

void wxCheckedListCtrl::Enable(long item, bool enable)
{ 	
	if (enable) 
		// the 2nd parameter says: activate the STATE bit relative to ENABLE feature
		// the 3rd parameter says: set only *that* bit 
		SetItemState(item, wxLIST_STATE_ENABLED, wxLIST_STATE_ENABLED);
	else
		SetItemState(item, 0, wxLIST_STATE_ENABLED); 
}

void wxCheckedListCtrl::EnableAll(bool enable)
{
	for (int i=0; i < GetItemCount(); i++)
		Enable(i, enable);
}

void wxCheckedListCtrl::CheckAll(bool check)
{
	for (int i=0; i < GetItemCount(); i++)
		Check(i, check);
}

bool wxCheckedListCtrl::DeleteItem(long item)
{
	// shift our additional state array
	//for (int i=item,max=GetItemCount(); i < max-1; i++)
	//	m_stateList[i] = m_stateList[i+1];
	m_stateList.RemoveAt(item, 1);

	return wxListCtrl::DeleteItem(item);
}

int wxCheckedListCtrl::GetCheckedItemCount() const
{
	int res = 0;
	for (int i=0; i<GetItemCount(); i++)
		if (IsChecked(i))
			res++;

	return res;
}

// event handlers

void wxCheckedListCtrl::OnMouseEvent(wxMouseEvent& event)
{
    if (!event.LeftDown()) {
        event.Skip();
		return;
	}
	
	int flags;
	long item = HitTest(event.GetPosition(), flags);
	if (item == wxNOT_FOUND || !IsEnabled(item)) { 

		// skip this item
		event.Skip(); 
		return; 
	}
	
	// user clicked exactly on the checkbox or on the item row ?
	bool processcheck = (flags & wxLIST_HITTEST_ONITEMICON) || 
		((GetWindowStyle() & wxCLC_CHECK_WHEN_SELECTING) && 
		(flags & wxLIST_HITTEST_ONITEM));

	if (processcheck) {

		wxListEvent ev(wxEVT_NULL, GetId());
		ev.m_itemIndex = item;		

		// send the check event
		if (IsChecked(item)) {

			ev.SetEventType(wxEVT_COMMAND_LIST_ITEM_UNCHECKED);
			Check(item, FALSE);
			AddPendingEvent(ev);

		} else {

			ev.SetEventType(wxEVT_COMMAND_LIST_ITEM_CHECKED);
			Check(item, TRUE);
			AddPendingEvent(ev);
		}
	}

	event.Skip(); 
}

#endif		// wxUSE_CHECKEDLISTCTRL
