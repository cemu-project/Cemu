#pragma once

#include <wx/event.h>
#include <wx/progdlg.h>
#include <wx/thread.h>
#include "util/helpers/Semaphore.h"

wxDEFINE_EVENT(wxEVT_CREATE_PROGRESS_DIALOG, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_DESTROY_PROGRESS_DIALOG, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_UPDATE_PROGRESS_DIALOG, wxThreadEvent);

// wrapper for wxGenericProgressDialog which can be used from any thread
class wxProgressDialogManager : public wxEvtHandler
{
public:
    wxProgressDialogManager(wxWindow* parent) : m_parent(parent), m_dialog(nullptr)
    {
        Bind(wxEVT_CREATE_PROGRESS_DIALOG, &wxProgressDialogManager::OnCreateProgressDialog, this);
        Bind(wxEVT_DESTROY_PROGRESS_DIALOG, &wxProgressDialogManager::OnDestroyProgressDialog, this);
        Bind(wxEVT_UPDATE_PROGRESS_DIALOG, &wxProgressDialogManager::OnUpdateProgressDialog, this);
    }

    ~wxProgressDialogManager()
    {
        if (m_dialog)
            Destroy();
    }

    void Create(const wxString& title, const wxString& message, int maximum, int style = wxPD_APP_MODAL | wxPD_ELAPSED_TIME | wxPD_REMAINING_TIME)
    {
        m_instanceSemaphore.increment();
        m_isCancelled = false;
        m_isSkipped = false;
        wxThreadEvent event(wxEVT_CREATE_PROGRESS_DIALOG);
        event.SetString(title);
        event.SetInt(maximum);
        event.SetExtraLong(style);
        wxQueueEvent(this, event.Clone());
    }

    void Destroy()
    {
        wxThreadEvent event(wxEVT_DESTROY_PROGRESS_DIALOG);
        wxQueueEvent(this, event.Clone());
        m_instanceSemaphore.waitUntilZero(); // wait until destruction is complete
    }

    // this also updates the cancel and skip state
    void Update(int value, const wxString& newmsg = wxEmptyString)
    {
        wxThreadEvent event(wxEVT_UPDATE_PROGRESS_DIALOG);
        event.SetInt(value);
        event.SetString(newmsg);
        wxQueueEvent(this, event.Clone());
    }

    bool IsCancelled() const
    {
        return m_isCancelled;
    }

    bool IsSkipped() const
    {
        return m_isSkipped;
    }

    bool IsCancelledOrSkipped() const
    {
        return m_isCancelled || m_isSkipped;
    }

private:
    void OnCreateProgressDialog(wxThreadEvent& event)
    {
        if (m_dialog)
        {
            m_dialog->Destroy();
            m_instanceSemaphore.waitUntilZero();
        }
        m_maximum = event.GetInt();
        m_dialog = new wxGenericProgressDialog(event.GetString(), "Please wait...", m_maximum, m_parent, event.GetExtraLong());
    }

    void OnDestroyProgressDialog(wxThreadEvent& event)
    {
        if (m_dialog)
        {
            m_dialog->Destroy();
            m_dialog = nullptr;
            m_instanceSemaphore.decrement();
        }
    }

    void OnUpdateProgressDialog(wxThreadEvent& event)
    {
        if (m_dialog)
        {
            // make sure that progress is never >= maximum
            // because wxGenericProgressDialog seems to become crashy on destruction otherwise
            int progress = event.GetInt();
            if(progress >= m_maximum)
                progress = m_maximum - 1;
            bool wasSkipped = false;
            bool r = m_dialog->Update(progress, event.GetString(), &wasSkipped);
            if(!r)
                m_isCancelled = true;
            if(wasSkipped)
                m_isSkipped = true;
        }
    }

    wxWindow* m_parent;
    wxGenericProgressDialog* m_dialog;
    bool m_isCancelled{false};
    bool m_isSkipped{false};
    int m_maximum{0};
    CounterSemaphore m_instanceSemaphore; // used to synchronize destruction of the dialog
};