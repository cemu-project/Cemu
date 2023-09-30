#include "gui/wxgui.h"
#include "audioDebuggerWindow.h"

#include "Cafe/OS/libs/snd_core/ax.h"
#include "Cafe/OS/libs/snd_core/ax_internal.h"

enum
{
	// options
	REFRESH_ID,
	CLOSE_ID,
	VOICELIST_ID,
	REFRESH_TIMER_ID,
};

wxBEGIN_EVENT_TABLE(AudioDebuggerWindow, wxFrame)
EVT_BUTTON(CLOSE_ID, AudioDebuggerWindow::OnCloseButton)
EVT_BUTTON(REFRESH_ID, AudioDebuggerWindow::OnRefreshButton)
EVT_TIMER(REFRESH_TIMER_ID, AudioDebuggerWindow::OnRefreshTimer)

EVT_CLOSE(AudioDebuggerWindow::OnClose)
wxEND_EVENT_TABLE()

AudioDebuggerWindow::AudioDebuggerWindow(wxFrame& parent)
	: wxFrame(&parent, wxID_ANY, _("AX voice viewer"), wxDefaultPosition, wxSize(1126, 580), wxCLOSE_BOX | wxCLIP_CHILDREN | wxCAPTION | wxRESIZE_BORDER)
{
	
	wxPanel* mainPane = new wxPanel(this);
	wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
	
	voiceListbox = new wxListCtrl(mainPane, VOICELIST_ID, wxPoint(0, 0), wxSize(1126, 570), wxLC_REPORT);
	voiceListbox->SetFont(wxFont(8, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, "Courier New"));
	// add columns
	wxListItem col0;
	col0.SetId(0);
	col0.SetText("idx");
	col0.SetWidth(40);
	voiceListbox->InsertColumn(0, col0);
	wxListItem col1;
	col1.SetId(1);
	col1.SetText("state");
	col1.SetWidth(48);
	voiceListbox->InsertColumn(1, col1);
	//wxListItem col2;
	// format
	col1.SetId(2);
	col1.SetText("fmt");
	col1.SetWidth(52);
	voiceListbox->InsertColumn(2, col1);
	// sample base addr
	col1.SetId(3);
	col1.SetText("base");
	col1.SetWidth(70);
	voiceListbox->InsertColumn(3, col1);
	// current offset
	col1.SetId(4);
	col1.SetText("current");
	col1.SetWidth(70);
	voiceListbox->InsertColumn(4, col1);
	// loop offset
	col1.SetId(5);
	col1.SetText("loop");
	col1.SetWidth(70);
	voiceListbox->InsertColumn(5, col1);
	// end offset
	col1.SetId(6);
	col1.SetText("end");
	col1.SetWidth(70);
	voiceListbox->InsertColumn(6, col1);
	// volume
	col1.SetId(7);
	col1.SetText("vol");
	col1.SetWidth(46);
	voiceListbox->InsertColumn(7, col1);
	// volume delta
	col1.SetId(8);
	col1.SetText("volD");
	col1.SetWidth(46);
	voiceListbox->InsertColumn(8, col1);
	// src
	col1.SetId(9);
	col1.SetText("src");
	col1.SetWidth(70);
	voiceListbox->InsertColumn(9, col1);
	// low-pass filter coef a0
	col1.SetId(10);
	col1.SetText("lpa0");
	col1.SetWidth(46);
	voiceListbox->InsertColumn(10, col1);
	// low-pass filter coef b0
	col1.SetId(11);
	col1.SetText("lpb0");
	col1.SetWidth(46);
	voiceListbox->InsertColumn(11, col1);
	// biquad filter coef b0
	col1.SetId(12);
	col1.SetText("bqb0");
	col1.SetWidth(46);
	voiceListbox->InsertColumn(12, col1);
	// biquad filter coef b0
	col1.SetId(13);
	col1.SetText("bqb1");
	col1.SetWidth(46);
	voiceListbox->InsertColumn(13, col1);
	// biquad filter coef b0
	col1.SetId(14);
	col1.SetText("bqb2");
	col1.SetWidth(46);
	voiceListbox->InsertColumn(14, col1);
	// biquad filter coef a0
	col1.SetId(15);
	col1.SetText("bqa1");
	col1.SetWidth(46);
	voiceListbox->InsertColumn(15, col1);
	// biquad filter coef a1
	col1.SetId(16);
	col1.SetText("bqa2");
	col1.SetWidth(46);
	voiceListbox->InsertColumn(16, col1);
	// device mix
	col1.SetId(17);
	col1.SetText("deviceMix");
	col1.SetWidth(186);
	voiceListbox->InsertColumn(17, col1);

	sizer->Add(voiceListbox, 1, wxEXPAND | wxBOTTOM, 0);

	voiceListbox->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(AudioDebuggerWindow::OnVoiceListRightClick), NULL, this);

	mainPane->SetSizer(sizer);

	// add empty entries
	for (sint32 i = 0; i < snd_core::AX_MAX_VOICES; i++)
	{
		wxListItem item;
		item.SetId(i);
		char tempStr[32];
		sprintf(tempStr, "%d", snd_core::AX_MAX_VOICES-i-1);
		item.SetText(wxString(tempStr));
		voiceListbox->InsertItem(item);
	}
	RefreshVoiceList();

	wxFrame::SetBackgroundColour(*wxWHITE);

	// start refresh timer
	static const int INTERVAL = 100; // milliseconds
	refreshTimer = new wxTimer(this, REFRESH_TIMER_ID);
	refreshTimer->Start(INTERVAL);
}

void AudioDebuggerWindow::OnRefreshTimer(wxTimerEvent& event)
{
	RefreshVoiceList();
}

void AudioDebuggerWindow::OnCloseButton(wxCommandEvent& event)
{
	Close();
}

void AudioDebuggerWindow::OnRefreshButton(wxCommandEvent& event)
{
	RefreshVoiceList();
}

void AudioDebuggerWindow::OnClose(wxCloseEvent& event)
{
	Close();
}

#define _r(__idx) _swapEndianU32(threadItrBE->context.gpr[__idx])

void AudioDebuggerWindow::RefreshVoiceList_sndgeneric()
{
	if (snd_core::__AXVPBInternalVoiceArray == nullptr || snd_core::__AXVPBArrayPtr == nullptr)
		return;

	snd_core::AXVPB tempVoiceArray[snd_core::AX_MAX_VOICES];
	memcpy(tempVoiceArray, snd_core::__AXVPBArrayPtr, sizeof(snd_core::AXVPB)*snd_core::AX_MAX_VOICES);

	voiceListbox->Freeze();

	char tempStr[64];
	for (sint32 i = 0; i < snd_core::AX_MAX_VOICES; i++)
	{
		sint32 voiceIndex = snd_core::AX_MAX_VOICES - 1 - i;
		snd_core::AXVPBInternal_t* internal = snd_core::__AXVPBInternalVoiceArray + voiceIndex;
		// index
		sprintf(tempStr, "%d", (sint32)tempVoiceArray[voiceIndex].index);
		voiceListbox->SetItem(i, 0, tempStr);
		// state
		uint16 playbackState = _swapEndianU16(internal->playbackState);
		if (playbackState)
			strcpy(tempStr, "on");
		else
			strcpy(tempStr, "off");
		voiceListbox->SetItem(i, 1, tempStr);
		// if voice index is invalid then stop updating here to prevent crashes
		if (voiceIndex < 0 || playbackState == 0)
		{
			voiceListbox->SetItem(i, 0, "");
			voiceListbox->SetItem(i, 1, "");
			voiceListbox->SetItem(i, 2, "");
			voiceListbox->SetItem(i, 3, "");
			voiceListbox->SetItem(i, 4, "");
			voiceListbox->SetItem(i, 5, "");
			voiceListbox->SetItem(i, 6, "");
			voiceListbox->SetItem(i, 7, "");
			voiceListbox->SetItem(i, 8, "");
			voiceListbox->SetItem(i, 9, "");
			voiceListbox->SetItem(i, 10, "");
			voiceListbox->SetItem(i, 11, "");
			voiceListbox->SetItem(i, 12, "");
			voiceListbox->SetItem(i, 13, "");
			voiceListbox->SetItem(i, 14, "");
			voiceListbox->SetItem(i, 15, "");
			voiceListbox->SetItem(i, 16, "");
			voiceListbox->SetItem(i, 17, "");
			continue;
		}
		// format
		if (tempVoiceArray[voiceIndex].offsets.format == _swapEndianU16(snd_core::AX_FORMAT_ADPCM))
			strcpy(tempStr, "adpcm");
		else if (tempVoiceArray[voiceIndex].offsets.format == _swapEndianU16(snd_core::AX_FORMAT_PCM16))
			strcpy(tempStr, "pcm16");
		else if (tempVoiceArray[voiceIndex].offsets.format == _swapEndianU16(snd_core::AX_FORMAT_PCM8))
			strcpy(tempStr, "pcm8");
		else
			strcpy(tempStr, "ukn");
		voiceListbox->SetItem(i, 2, tempStr);
		// update offsets
		snd_core::AXPBOFFSET_t tempOffsets;
		snd_core::AXGetVoiceOffsets(tempVoiceArray + voiceIndex, &tempOffsets);
		// sample base
		sprintf(tempStr, "%08x", _swapEndianU32(tempOffsets.samples));
		voiceListbox->SetItem(i, 3, tempStr);
		// current offset
		sprintf(tempStr, "%08x", _swapEndianU32(tempOffsets.currentOffset));
		voiceListbox->SetItem(i, 4, tempStr);
		// loop offset
		if (tempOffsets.loopFlag)
			sprintf(tempStr, "%08x", _swapEndianU32(tempOffsets.loopOffset));
		else
			sprintf(tempStr, "");
		voiceListbox->SetItem(i, 5, tempStr);
		// end offset
		sprintf(tempStr, "%08x", _swapEndianU32(tempOffsets.endOffset));
		voiceListbox->SetItem(i, 6, tempStr);
		// volume
		sprintf(tempStr, "%04x", (uint16)internal->veVolume);
		voiceListbox->SetItem(i, 7, tempStr);
		// volume delta
		sprintf(tempStr, "%04x", (uint16)internal->veDelta);
		voiceListbox->SetItem(i, 8, tempStr);
		// src
		sprintf(tempStr, "%04x%04x", _swapEndianU16(internal->src.ratioHigh), _swapEndianU16(internal->src.ratioLow));
		voiceListbox->SetItem(i, 9, tempStr);
		// lpf
		if (internal->lpf.on)
		{
			sprintf(tempStr, "%04x", _swapEndianU16(internal->lpf.a0));
			voiceListbox->SetItem(i, 10, tempStr);
			sprintf(tempStr, "%04x", _swapEndianU16(internal->lpf.b0));
			voiceListbox->SetItem(i, 11, tempStr);
		}
		else
		{
			voiceListbox->SetItem(i, 10, "");
			voiceListbox->SetItem(i, 11, "");
		}
		// biquad
		if (internal->biquad.on)
		{
			sprintf(tempStr, "%04x", _swapEndianU16(internal->biquad.b0));
			voiceListbox->SetItem(i, 12, tempStr);
			sprintf(tempStr, "%04x", _swapEndianU16(internal->biquad.b1));
			voiceListbox->SetItem(i, 13, tempStr);
			sprintf(tempStr, "%04x", _swapEndianU16(internal->biquad.b2));
			voiceListbox->SetItem(i, 14, tempStr);
			sprintf(tempStr, "%04x", _swapEndianU16(internal->biquad.a1));
			voiceListbox->SetItem(i, 15, tempStr);
			sprintf(tempStr, "%04x", _swapEndianU16(internal->biquad.a2));
			voiceListbox->SetItem(i, 16, tempStr);
		}
		else
		{
			voiceListbox->SetItem(i, 12, "");
			voiceListbox->SetItem(i, 13, "");
			voiceListbox->SetItem(i, 14, "");
			voiceListbox->SetItem(i, 15, "");
			voiceListbox->SetItem(i, 16, "");
		}
		// device mix
		for (uint32 f = 0; f < snd_core::AX_TV_CHANNEL_COUNT*snd_core::AX_MAX_NUM_BUS; f++)
		{
			sint32 busIndex = f% snd_core::AX_MAX_NUM_BUS;
			sint32 channelIndex = f / snd_core::AX_MAX_NUM_BUS;

			//debug_printf("DeviceMix TV Voice %08x b%02d/c%02d vol %04x delta %04x\n", hCPU->gpr[3], busIndex, channelIndex, _swapEndianU16(mixArrayBE[f].vol), _swapEndianU16(mixArrayBE[f].volDelta));
			uint32 mixVol = internal->deviceMixTV[channelIndex * 4 + busIndex].vol;
			mixVol = (mixVol + 0x0FFF) >> (12);
			sprintf(tempStr + f, "%x", mixVol);
			//ax.voiceInternal[voiceIndex].deviceMixTVChannel[channelIndex].bus[busIndex].vol = _swapEndianU16(mixArrayBE[f].vol);
		}
		voiceListbox->SetItem(i, 17, tempStr);
	}
	voiceListbox->Thaw();
}

void AudioDebuggerWindow::RefreshVoiceList()
{
	RefreshVoiceList_sndgeneric();
}

void AudioDebuggerWindow::OnVoiceListPopupClick(wxCommandEvent &evt)
{

}

void AudioDebuggerWindow::OnVoiceListRightClick(wxMouseEvent& event)
{

}

void AudioDebuggerWindow::Close()
{
	// this->MakeModal(false);
	refreshTimer->Stop();
	this->Destroy();
}
