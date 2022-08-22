#pragma once
/////////////////////////////////////////////////////////////////////////////
// Name:        checkedlistctrl.h
// Purpose:     wxCheckedListCtrl
// Author:      Unknown ? (found at http://wiki.wxwidgets.org/wiki.pl?WxListCtrl)
// Modified by: Francesco Montorsi
// Created:     2005/06/29
// RCS-ID:      $Id: checkedlistctrl.h 448 2007-03-06 22:06:38Z frm $
// Copyright:   (c) 2005 Francesco Montorsi
// Licence:     wxWidgets licence
/////////////////////////////////////////////////////////////////////////////

#define wxUSE_CHECKEDLISTCTRL 1

#ifndef _WX_CHECKEDLISTCTRL_H_
#define _WX_CHECKEDLISTCTRL_H_

// wxWidgets headers
//#include "wx/webupdatedef.h"		// for the WXDLLIMPEXP_WEBUPDATE macro
#include <wx/listctrl.h>
#include <wx/imaglist.h>

#if wxUSE_CHECKEDLISTCTRL

// image indexes (used internally by wxCheckedListCtrl)
#define wxCLC_UNCHECKED_IMGIDX				0		// unchecked & enabled
#define wxCLC_CHECKED_IMGIDX				1		// checked & enabled
#define wxCLC_DISABLED_UNCHECKED_IMGIDX		2		// unchecked & disabled
#define wxCLC_DISABLED_CHECKED_IMGIDX		3		// checked & disabled

// additional state flags (wx's defines should end at 0x0100; see listbase.h)
#define wxLIST_STATE_CHECKED				0x010000
#define wxLIST_STATE_ENABLED				0x100000

// additional wxCheckedListCtrl style flags
// (wx's defines should at 0x8000; see listbase.h)
#define wxCLC_CHECK_WHEN_SELECTING			0x10000


// -------------------------
// wxCheckedListCtrl events
// -------------------------

//DECLARE_EXPORTED_EVENT_TYPE(WXDLLIMPEXP_WEBUPDATE, wxEVT_COMMAND_LIST_ITEM_CHECKED, -1);
//DECLARE_EXPORTED_EVENT_TYPE(WXDLLIMPEXP_WEBUPDATE, wxEVT_COMMAND_LIST_ITEM_UNCHECKED, -1);

DECLARE_EXPORTED_EVENT_TYPE(WXEXPORT, wxEVT_COMMAND_LIST_ITEM_CHECKED, -1);
DECLARE_EXPORTED_EVENT_TYPE(WXEXPORT, wxEVT_COMMAND_LIST_ITEM_UNCHECKED, -1);


//DEFINE_EVENT_TYPE(wxEVT_COMMAND_LIST_ITEM_CHECKED);
//DEFINE_EVENT_TYPE(wxEVT_COMMAND_LIST_ITEM_UNCHECKED);

#ifndef EVT_LIST_ITEM_CHECKED
#define EVT_LIST_ITEM_CHECKED(id, fn) \
    DECLARE_EVENT_TABLE_ENTRY( \
        wxEVT_COMMAND_LIST_ITEM_CHECKED, id, -1, \
        (wxObjectEventFunction)(wxEventFunction)(wxListEventFunction)&fn, \
        (wxObject *) NULL \
    ),
#endif

#ifndef EVT_LIST_ITEM_UNCHECKED
#define EVT_LIST_ITEM_UNCHECKED(id, fn) \
    DECLARE_EVENT_TABLE_ENTRY( \
        wxEVT_COMMAND_LIST_ITEM_UNCHECKED, id, -1, \
        (wxObjectEventFunction)(wxEventFunction)(wxListEventFunction)&fn, \
        (wxObject *) NULL \
    ),
#endif


//! This is the class which performs all transactions with the server.
//! It uses the wxSocket facilities.
class wxCheckedListCtrl : public wxListCtrl
{
protected:

    // we have to keep a different array to keep track of the additional
    // states we support....
    wxArrayInt m_stateList;

    // our set of checkbox images...
    wxImageList m_imageList;

public:
    wxCheckedListCtrl()
        : wxListCtrl(), m_imageList(16, 16, TRUE) {}

    wxCheckedListCtrl(wxWindow *parent, wxWindowID id = -1,
                        const wxPoint& pt = wxDefaultPosition,
                        const wxSize& sz = wxDefaultSize,
                        long style = wxCLC_CHECK_WHEN_SELECTING,
                        const wxValidator& validator = wxDefaultValidator,
                        const wxString& name = wxListCtrlNameStr)
                        : wxListCtrl(), m_imageList(16, 16, TRUE)
        { Create(parent, id, pt, sz, style, validator, name); }

    bool Create(wxWindow *parent, wxWindowID id = -1,
                        const wxPoint& pt = wxDefaultPosition,
                        const wxSize& sz = wxDefaultSize,
                        long style = wxCLC_CHECK_WHEN_SELECTING,
                        const wxValidator& validator = wxDefaultValidator,
                        const wxString& name = wxListCtrlNameStr);

    virtual ~wxCheckedListCtrl() {}


public:			// utilities

    // core overloads (i.e. the most generic overloads)
    bool GetItem(wxListItem& info) const;
    bool SetItem(wxListItem& info);
    long InsertItem(wxListItem& info);
    bool DeleteItem(long item);
    bool DeleteAllItems()
        { m_stateList.Clear(); return wxListCtrl::DeleteAllItems(); }

    bool SortItems(wxListCtrlCompare, long)
        { wxASSERT_MSG(0, wxT("Not implemented yet ! sorry... ")); return FALSE; }

    // shortcuts to the SetItemState function
    void Check(long item, bool checked);
    void Enable(long item, bool enable);
    void CheckAll(bool checked = true);
    void EnableAll(bool enable = true);

    // this needs to be redeclared otherwise it's hidden by our other Enable() function.
    // However you should use #EnableAll instead of this function if you want to get
    // good graphics (try to understand)
    virtual bool Enable(bool enable = true)
        { return wxListCtrl::Enable(enable); }

    // shortcuts to the GetItemState function
    bool IsChecked(long item) const
        { return GetItemState(item, wxLIST_STATE_CHECKED) != 0; }
    bool IsEnabled(long item) const
        { return GetItemState(item, wxLIST_STATE_ENABLED) != 0; }

    // this needs to be redeclared otherwise it's hidden by our other IsEnabled() function.
    bool IsEnabled() const
        { return wxWindow::IsEnabled(); }

    //! Returns the number of checked items in the control.
    int GetCheckedItemCount() const;

    // we overload these so we are sure they will use our
    // #GetItem and #SetItem functions...
    bool SetItemState(long item, long state, long stateMask);
    int GetItemState(long item, long stateMask) const;
    long InsertItem( long index, const wxString& label, int imageIndex = -1);
    long SetItem(long index, int col, const wxString& label, int imageId = -1);

    // the image associated with an element is already in used by wxCheckedListCtrl
    // itself to show the checkbox and it cannot be handled by the user !
    bool SetItemImage(long, int)
        { wxASSERT_MSG(0, wxT("This function cannot be used with wxCheckedListCtrl !")); return FALSE; }

protected:		// event handlers

    void OnMouseEvent(wxMouseEvent& event);

protected:		// internal utilities

    static int GetItemImageFromAdditionalState(int addstate);
    static int GetAndRemoveAdditionalState(long *state, int statemask);
    wxColour GetBgColourFromAdditionalState(int additionalstate);

private:
    DECLARE_CLASS(wxCheckedListCtrl)
    DECLARE_EVENT_TABLE()
};


#endif	// wxUSE_CHECKEDLISTCTRL

#endif	// _WX_CHECKEDLISTCTRL_H_

