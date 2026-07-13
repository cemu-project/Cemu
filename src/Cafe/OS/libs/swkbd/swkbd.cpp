#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/coreinit/coreinit_FS.h"
#include "Cafe/OS/libs/gx2/GX2.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"

#include <imgui.h>
#include "imgui/imgui_extension.h"
#include "util/helpers/helpers.h"
#include "resource/IconsFontAwesome5.h"

#define SWKBD_FORM_STRING_MAX_LENGTH	(4096) // counted in 16-bit characters

#define SWKBD_STATE_BLANK				(0)	// not visible
#define SWKBD_STATE_APPEARING			(1)	// fade-in ?
#define SWKBD_STATE_DISPLAYED			(2)	// visible
#define SWKBD_STATE_DISAPPEARING		(3)	// fade-out ?

typedef struct  
{
	uint32 ukn00; // constructor?
	uint32 ukn04; // destructor?
	uint32 ukn08; // ?
	MEMPTR<void> changeString; // some function address
}SwkbdIEventReceiverVTable_t;

typedef struct  
{
	MEMPTR<SwkbdIEventReceiverVTable_t> vTable;
	// todo - more elements? (currently separated from this struct)
}SwkbdIEventReceiver_t;

struct swkbdReceiverArg_t
{
	MEMPTR<SwkbdIEventReceiver_t> IEventReceiver;
	MEMPTR<sint16be> stringBuf;
	sint32be stringBufSize;
	sint32be fixedCharLimit;
	sint32be cursorPos;
	sint32be selectFrom;
};

typedef struct  
{
	uint32 ukn000;
	uint32 controllerType;
	uint32 keyboardMode; // guessed
	uint32 ukn00C;
	uint32 ukn010;
	uint32 ukn014;
	uint32 ukn018;
	uint32 ukn01C; // ok string?
	uint32 ukn020[4];
	uint32 ukn030[4];
	uint32 ukn040[4];
	uint32 ukn050[4];
	uint32 ukn060[4];
	uint32 ukn070[4];
	uint32 ukn080[4];
	uint32 ukn090[4];
	uint32 ukn0A0;
	uint32 ukn0A4;
	//uint32 ukn0A8;
	//MEMPTR<SwkbdIEventReceiver_t> IEventReceiver;
	swkbdReceiverArg_t receiverArg;
}SwkbdKeyboardArg_t;

typedef struct  
{
	// this structure resides in PPC addressable memory space
	wchar_t formStringBuffer[SWKBD_FORM_STRING_MAX_LENGTH];
	sint32 formStringLength;
	// big endian version of the string buffer (converted whenever GetInputFormString is called)
	uint16be formStringBufferBE[SWKBD_FORM_STRING_MAX_LENGTH];
	bool isActive; // set when SwkbdAppearInputForm() is called
	//bool isDisplayed; // set when keyboard is rendering
	bool decideButtonWasPressed; // set to false when keyboard appears, and set to true when enter is pressed. Remains on true after the keyboard is disappeared (todo: Investigate how this really works)
	// keyboard only mode (no input form)
	bool keyboardOnlyMode;
	SwkbdKeyboardArg_t keyboardArg;
	// input form appear args
	sint32 maxTextLength;
	// imgui keyboard drawing stuff
	bool shiftActivated;
	bool returnState;
	bool cancelState;
}swkbdInternalState_t;

swkbdInternalState_t* swkbdInternalState = nullptr;
sint32 isNeedCalcSubThreadFont = 0;
sint32 isNeedCalcSubThreadPredict = 0;

void SwkbdCreate(uint8* workMemory, uint32 regionType, uint32 unk, coreinit::FSClient_t* fsClient)
{
	if( swkbdInternalState == nullptr )
	{
		MPTR swkbdInternalStateMPTR = coreinit_allocFromSysArea(sizeof(swkbdInternalState_t), 4);
		swkbdInternalState = (swkbdInternalState_t*)memory_getPointerFromVirtualOffset(swkbdInternalStateMPTR);
		memset(swkbdInternalState, 0x00, sizeof(swkbdInternalState_t));
	}
	coreinit::OSSleepTicks(ESPRESSO_TIMER_CLOCK); // simulate this function taking a bit of time to run. MH3U/MH3G requires this to not softlock
	// pretend that there is some work to do
	isNeedCalcSubThreadFont = 3;
	isNeedCalcSubThreadPredict = 3;
}

uint32 SwkbdGetStateKeyboard()
{
	uint32 r = SWKBD_STATE_BLANK;
	if( swkbdInternalState->isActive )
		r = SWKBD_STATE_DISPLAYED;
	return r;
}

uint32 SwkbdGetStateInputForm()
{
	uint32 r = SWKBD_STATE_BLANK;
	if( swkbdInternalState->isActive )
		r = SWKBD_STATE_DISPLAYED;
	return r;
}

//ReceiverArg:
//+0x00	IEventReceiver*
//+0x04	stringBuf
//+ 0x08	stringBufSize
//+ 0x0C	fixedCharNumLimit(-1)
//+ 0x10	cursorPos
//+ 0x14	selectFrom(-1)
//
//IEventReceiver:
//+0x00 IEventReceiver_vTable*
//+0x04 ?
//+0x08 ?
//+0x0C ?
//
//IEventReceiver_vTable :
//	+0x00 ?
//	+0x04 ?
//	+0x08 ?
//	+0x0C	functionPtr onDirtyString(const DirtyInfo& info) = 0; ->DirtyInfo is just two DWORDs.From and to ?
//	?

struct swkdbIEventReceiver_t
{
	MPTR vTable; // guessed
};

struct swkdbIEventReceiverVTable_t
{
	uint32 ukn00;
	uint32 ukn04;
	uint32 ukn08;
	MPTR   onDirtyString;
};

uint32 SwkbdSetReceiver(const swkbdReceiverArg_t* receiverArg)
{
	if(swkbdInternalState == nullptr)
		return 0;
	swkbdInternalState->keyboardArg.receiverArg = *receiverArg;
	return 0;
}

typedef struct  
{
	/* +0x00 */ uint32 ukn00;
	/* +0x04 */ uint32 ukn04;
	/* +0x08 */ uint32 ukn08;
	/* +0x0C */ uint32 ukn0C;
	/* +0x10 */ uint32 ukn10;
	/* +0x14 */ uint32 ukn14;
	/* +0x18 */ uint32 ukn18;
	/* +0x1C */ uint32 ukn1C; // pointer to OK string
	/* +0x20 */ uint32 ukn20[4];
	/* +0x30 */ uint32 ukn30[4];
	/* +0x40 */ uint32 ukn40[4];
	/* +0x50 */ uint32 ukn50[4];
	/* +0x60 */ uint32 ukn60[4];
	/* +0x70 */ uint32 ukn70[4];
	/* +0x80 */ uint32 ukn80[4];
	/* +0x90 */ uint32 ukn90[4];
	/* +0xA0 */ uint32 uknA0[4];
	/* +0xB0 */ uint32 uknB0[4];
	/* +0xC0 */ uint32 inputFormType;
	/* +0xC4 */ uint32be cursorIndex;
	/* +0xC8 */ MEMPTR<uint16be> initialText;
	/* +0xCC */ MEMPTR<uint16be> infoText;
	/* +0xD0 */ uint32be maxTextLength;
}swkbdAppearArg_t;

static_assert(offsetof(swkbdAppearArg_t, cursorIndex) == 0xC4, "appearArg.cursorIndex has invalid offset");

uint32 SwkbdAppearInputForm(const swkbdAppearArg_t* appearArg)
{
	swkbdInternalState->formStringLength = 0;
	swkbdInternalState->isActive = true;
	swkbdInternalState->decideButtonWasPressed = false;
	swkbdInternalState->keyboardOnlyMode = false;

	// setup max text length
	swkbdInternalState->maxTextLength = (sint32)(uint32)appearArg->maxTextLength;
	if (swkbdInternalState->maxTextLength <= 0)
		swkbdInternalState->maxTextLength = SWKBD_FORM_STRING_MAX_LENGTH - 1;
	else
		swkbdInternalState->maxTextLength = std::min(swkbdInternalState->maxTextLength, SWKBD_FORM_STRING_MAX_LENGTH - 1);
	// setup initial string
	uint16be* initialString = appearArg->initialText.GetPtr();
	if (initialString)
	{
		swkbdInternalState->formStringLength = 0;
		for (sint32 i = 0; i < swkbdInternalState->maxTextLength; i++)
		{
			wchar_t c = (uint16)initialString[i];
			if( c == '\0' )
				break;
			swkbdInternalState->formStringBuffer[i] = c;
			swkbdInternalState->formStringLength++;
		}
	}
	else
	{
		swkbdInternalState->formStringBuffer[0] = '\0';
		swkbdInternalState->formStringLength = 0;
	}
	return 1;
}

uint32 SwkbdAppearKeyboard(const SwkbdKeyboardArg_t* keyboardArg)
{
	// todo: Figure out what the difference between AppearInputForm and AppearKeyboard is?
	uint32 argPtr = MEMPTR(keyboardArg).GetMPTR();
	for(sint32 i=0; i<0x180; i += 4)
	{
		debug_printf("+0x%03x: 0x%08x\n", i, memory_readU32(argPtr+i));
	}

	swkbdInternalState->formStringLength = 0;
	swkbdInternalState->isActive = true;
	swkbdInternalState->keyboardOnlyMode = true;
	swkbdInternalState->decideButtonWasPressed = false;
	swkbdInternalState->formStringBuffer[0] = '\0';
	swkbdInternalState->formStringLength = 0;
	swkbdInternalState->keyboardArg = *keyboardArg;
	return 1;
}

uint32 SwkbdDisappearInputForm()
{
	debug_printf("SwkbdDisappearInputForm__3RplFv\n");
	swkbdInternalState->isActive = false;
	return 1;
}

uint32 SwkbdDisappearKeyboard()
{
	debug_printf("SwkbdDisappearKeyboard__3RplFv\n");
	swkbdInternalState->isActive = false;
	return 1;
}

uint16be* SwkbdGetInputFormString()
{
	for(sint32 i=0; i<swkbdInternalState->formStringLength; i++)
		swkbdInternalState->formStringBufferBE[i] = swkbdInternalState->formStringBuffer[i];
	cemu_assert(swkbdInternalState->formStringLength < SWKBD_FORM_STRING_MAX_LENGTH);
	swkbdInternalState->formStringBufferBE[swkbdInternalState->formStringLength] = 0;
	return swkbdInternalState->formStringBufferBE;
}

uint32 SwkbdIsDecideOkButton(uint8*)
{
	if (swkbdInternalState->decideButtonWasPressed)
		return 1;
	return 0;
}

typedef struct  
{
	uint32be ukn00;
	uint32be ukn04;
	uint32be ukn08;
	uint32be ukn0C;
	uint32be ukn10;
	uint32be ukn14;
	uint8 ukn18;
	// there might be padding here?
}SwkbdDrawStringInfo_t;

static_assert(sizeof(SwkbdDrawStringInfo_t) != 0x19, "SwkbdDrawStringInfo_t has invalid size");

uint32 SwkbdGetDrawStringInfo(SwkbdDrawStringInfo_t* drawStringInfo)
{
	cemuLog_logDebug(LogType::Force, "SwkbdGetDrawStringInfo(0x{:08x})", MEMPTR(drawStringInfo).GetMPTR());

	drawStringInfo->ukn00 = -1;
	drawStringInfo->ukn04 = -1;
	drawStringInfo->ukn08 = -1;
	drawStringInfo->ukn0C = -1;
	drawStringInfo->ukn10 = -1;
	drawStringInfo->ukn14 = -1;
	drawStringInfo->ukn18 = 0;

	return 0;
}

constexpr uint32 SWKBD_LEARN_DIC_SIZE = 0xA460;
constexpr uint32 SWKBD_LEARN_DIC_ENTRY_COUNT = 1000;
constexpr uint32 SWKBD_LEARN_DIC_ENTRY_STRIDE = 0x20;
constexpr uint32 SWKBD_LEARN_DIC_INDEX_COUNT = SWKBD_LEARN_DIC_ENTRY_COUNT + 1;
constexpr uint32 SWKBD_LEARN_DIC_MAGIC = 0x4E4A4443; // "NJDC"

struct SwkbdLearnDic
{
	/* 0x00 */ uint32be magic;
	/* 0x04 */ uint32be version;
	/* 0x08 */ uint32be format;
	/* 0x0C */ uint32be storageSize;
	/* 0x10 */ uint32be metadataSize;
	/* 0x14 */ uint32be maxWordLength;
	/* 0x18 */ uint32be maxReadingLength;
	/* 0x1C */ uint32be indexTableEnd;
	/* 0x20 */ uint32be entryBaseOffset;
	/* 0x24 */ uint32be reserved24;
	/* 0x28 */ uint32be entryCount;
	/* 0x2C */ uint32be entryStride;
	/* 0x30 */ uint32be reserved30;
	/* 0x34 */ uint32be reserved34;
	/* 0x38 */ uint32be reserved38;
	/* 0x3C */ uint32be firstIndexOffset;
	/* 0x40 */ uint32be secondIndexOffset;
	/* 0x44 */ uint32be entryAreaEnd;
	/* 0x48 */ uint8 payload[SWKBD_LEARN_DIC_SIZE - 0x48 - sizeof(uint32be)];
	/* 0xA45C */ uint32be endMagic;
};
static_assert(sizeof(SwkbdLearnDic) == SWKBD_LEARN_DIC_SIZE);

uint32 SwkbdInitLearnDic(SwkbdLearnDic* dictionary)
{
	cemuLog_logDebug(LogType::Force, "SwkbdInitLearnDic(0x{:08x})", MEMPTR(dictionary).GetMPTR());
	if (swkbdInternalState == nullptr || dictionary == nullptr)
		return 0;


	uint32 firstIndexOffset = 0x48;
	uint32 secondIndexOffset = firstIndexOffset + SWKBD_LEARN_DIC_INDEX_COUNT * sizeof(uint16be);
	uint32 indexTableEnd = secondIndexOffset + SWKBD_LEARN_DIC_INDEX_COUNT * sizeof(uint16be);
	uint32 entryAreaEnd = indexTableEnd + SWKBD_LEARN_DIC_ENTRY_COUNT * SWKBD_LEARN_DIC_ENTRY_STRIDE;

	memset(dictionary, 0, sizeof(SwkbdLearnDic));
	dictionary->magic = SWKBD_LEARN_DIC_MAGIC;
	dictionary->version = 0x30000;
	dictionary->format = 0x80020000;
	dictionary->storageSize = SWKBD_LEARN_DIC_SIZE - 0x48;
	dictionary->metadataSize = 0x2C;
	dictionary->maxWordLength = 0x6E;
	dictionary->maxReadingLength = 0x6E;
	dictionary->indexTableEnd = indexTableEnd;
	dictionary->entryCount = SWKBD_LEARN_DIC_ENTRY_COUNT;
	dictionary->entryStride = SWKBD_LEARN_DIC_ENTRY_STRIDE;
	dictionary->firstIndexOffset = firstIndexOffset;
	dictionary->secondIndexOffset = secondIndexOffset;
	dictionary->entryAreaEnd = entryAreaEnd;
	dictionary->endMagic = SWKBD_LEARN_DIC_MAGIC;

	return 1;
}

bool SwkbdIsNeedCalcSubThreadFont()
{
	return isNeedCalcSubThreadFont > 0;
}

bool SwkbdIsNeedCalcSubThreadPredict()
{
	return isNeedCalcSubThreadPredict > 0;
}

void SwkbdCalcSubThreadFont()
{
	if (isNeedCalcSubThreadFont == 0)
		return;
	isNeedCalcSubThreadFont--;
	coreinit::OSSleepTicks(ESPRESSO_TIMER_CLOCK / 200); // simulate 5ms of working time
}

void SwkbdCalcSubThreadPredict()
{
	if (isNeedCalcSubThreadPredict == 0)
		return;
	isNeedCalcSubThreadPredict--;
	coreinit::OSSleepTicks(ESPRESSO_TIMER_CLOCK / 200); // simulate 5ms of working time
}

void SwkbdCalc(void* controllerInfo)
{
	// no op for now
}

void swkbd_keyInput(uint32 keyCode);
void swkbd_render(bool mainWindow)
{
	// only render if active
	if( swkbdInternalState == NULL || swkbdInternalState->isActive == false)
		return;
	
	auto& io = ImGui::GetIO();
	const auto font = ImGui_GetFont(48.0f);
	const auto textFont = ImGui_GetFont(24.0f);
	if (!font || !textFont)
		return;

	const auto kPopupFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

	ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);

	ImGui::SetNextWindowPos({ 0,0 }, ImGuiCond_Always);
	ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0,0 });
	ImGui::SetNextWindowBgAlpha(0.8f);
	ImGui::Begin("Background overlay", nullptr, kPopupFlags | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);
	ImGui::End();
	ImGui::PopStyleVar(2);

	ImVec2 position = { io.DisplaySize.x / 2.0f, io.DisplaySize.y / 3.0f };
	ImVec2 pivot = { 0.5f, 0.5f };

	const auto button_len = font->GetCharAdvance('W');
	const float len = button_len * std::max(4, std::max(swkbdInternalState->maxTextLength, (sint32)swkbdInternalState->keyboardArg.receiverArg.stringBufSize));

	ImVec2 box_size = { std::min(io.DisplaySize.x * 0.9f, len + 90), 0 };
	ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);
	ImGui::SetNextWindowSize(box_size, ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.9f);
	ImGui::PushFont(font);
	if (ImGui::Begin("Keyboard Input", nullptr, kPopupFlags))
	{
		ImGui::Text("%s", _utf8WrapperPtr(ICON_FA_KEYBOARD));
		ImGui::SameLine(70);
		auto text = boost::nowide::narrow(fmt::format(L"{}", swkbdInternalState->formStringBuffer));

		static std::chrono::steady_clock::time_point s_last_tick = tick_cached();
		static bool s_blink_state = false;
		const auto now = tick_cached();

		if (std::chrono::duration_cast<std::chrono::milliseconds>(now - s_last_tick).count() >= 500)
		{
			s_blink_state = !s_blink_state;
			s_last_tick = now;
		}

		if (s_blink_state)
			text += "|";

		ImGui::PushTextWrapPos();
		ImGui::TextUnformatted(text.c_str(), text.c_str() + text.size());
		ImGui::PopTextWrapPos();

		position.y += ImGui::GetWindowSize().y + 100.0f;
	}
	ImGui::End();
	ImGui::PopFont();

	ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);
	//ImGui::SetNextWindowSize({ io.DisplaySize.x * 0.9f , 0.0f}, ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.9f);
	ImGui::PushFont(textFont);

	if (ImGui::Begin(fmt::format("Software keyboard##SoftwareKeyboard{}",mainWindow).c_str(), nullptr, kPopupFlags))
	{
		if(swkbdInternalState->shiftActivated)
		{
			const char* keys[] =
			{
				"#", "[", "]", "$", "%", "^", "&", "*", "(", ")", "_", _utf8WrapperPtr(ICON_FA_ARROW_CIRCLE_LEFT), "\n",
				"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "@", "\n",
				"A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "\"", "\n",
				"Z", "X", "C", "V", "B", "N", "M", "<", ">", "+", "=", "\n",
				_utf8WrapperPtr(ICON_FA_ARROW_UP), " ", _utf8WrapperPtr(ICON_FA_CHECK)
			};
			for (auto key : keys)
			{
				if (*key != '\n')
				{
					if (ImGui::Button(key, { *key == ' ' ? 537 : (button_len + 5), 0}))
					{
						if (strcmp(key, _utf8WrapperPtr(ICON_FA_ARROW_CIRCLE_LEFT)) == 0)
							swkbd_keyInput(8);
						else if (strcmp(key, _utf8WrapperPtr(ICON_FA_ARROW_UP)) == 0)
							swkbdInternalState->shiftActivated = !swkbdInternalState->shiftActivated;
						else if (strcmp(key, _utf8WrapperPtr(ICON_FA_CHECK)) == 0)
							swkbd_keyInput(13);
						else
							swkbd_keyInput(*key);
					}

					ImGui::SameLine();
				}
				else
					ImGui::NewLine();
			}
		}
		else
		{
			const char* keys[] =
			{
				"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", _utf8WrapperPtr(ICON_FA_ARROW_CIRCLE_LEFT), "\n",
				"q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "/", "\n",
				"a", "s", "d", "f", "g", "h", "j", "k", "l", ":", "'", "\n",
				"z", "x", "c", "v", "b", "n", "m", ",", ".", "?", "!", "\n",
				_utf8WrapperPtr(ICON_FA_ARROW_UP), " ", _utf8WrapperPtr(ICON_FA_CHECK)
			};
			for (auto key : keys)
			{
				if (*key != '\n')
				{
					if (ImGui::Button(key, { *key == ' ' ? 537 : (button_len + 5), 0 }))
					{
						if (strcmp(key, _utf8WrapperPtr(ICON_FA_ARROW_CIRCLE_LEFT)) == 0)
							swkbd_keyInput(8);
						else if (strcmp(key, _utf8WrapperPtr(ICON_FA_ARROW_UP)) == 0)
							swkbdInternalState->shiftActivated = !swkbdInternalState->shiftActivated;
						else if (strcmp(key, _utf8WrapperPtr(ICON_FA_CHECK)) == 0)
							swkbd_keyInput(13);
						else
							swkbd_keyInput(*key);
					}

					ImGui::SameLine();
				}
				else
					ImGui::NewLine();
			}
		}
		ImGui::NewLine();
	}
	ImGui::End();

	if (io.NavInputs[ImGuiNavInput_Cancel] > 0)
	{
		if(!swkbdInternalState->cancelState)
			swkbd_keyInput(8); // backspace
		swkbdInternalState->cancelState = true;
	}
	else
		swkbdInternalState->cancelState = false;

	if (io.NavInputs[ImGuiNavInput_Input] > 0)
	{
		if (!swkbdInternalState->returnState)
			swkbd_keyInput(13); // return
		swkbdInternalState->returnState = true;
	}
	else
		swkbdInternalState->returnState = false;

	ImGui::PopFont();
	ImGui::PopStyleColor();
}

bool swkbd_hasKeyboardInputHook()
{
	return swkbdInternalState != NULL && swkbdInternalState->isActive;
}

void swkbd_finishInput()
{
	swkbdInternalState->decideButtonWasPressed = true; // currently we always accept the input
}

typedef struct  
{
	uint32be beginIndex;
	uint32be endIndex;
}changeStringParam_t;

SysAllocator<changeStringParam_t> _changeStringParam;

void swkbd_inputStringChanged()
{
	if( true )//swkbdInternalState->keyboardOnlyMode )
	{
		// write changed string to application's string buffer
		uint32 stringBufferSize = swkbdInternalState->keyboardArg.receiverArg.stringBufSize; // in 2-byte words
		if( stringBufferSize > 1 )
		{
			stringBufferSize--; // don't count the null-termination character
			const auto stringBufferBE = swkbdInternalState->keyboardArg.receiverArg.stringBuf.GetPtr();
			sint32 copyLength = std::min((sint32)stringBufferSize, swkbdInternalState->formStringLength);
			for(sint32 i=0; i<copyLength; i++)
			{
				stringBufferBE[i] = swkbdInternalState->formStringBuffer[i];
			}
			stringBufferBE[copyLength] = '\0';

			//swkbdInternalState->keyboardArg.cursorPos = copyLength;
		}
		// IEventReceiver callback
		if (swkbdInternalState->keyboardArg.receiverArg.IEventReceiver)
		{
			SwkbdIEventReceiver_t* eventReceiver = swkbdInternalState->keyboardArg.receiverArg.IEventReceiver.GetPtr();
			MPTR cbChangeString = eventReceiver->vTable->changeString.GetMPTR();
			if (cbChangeString)
			{
				changeStringParam_t* changeStringParam = _changeStringParam.GetPtr();
				changeStringParam->beginIndex = 0;
				changeStringParam->endIndex = 0;
				coreinitAsyncCallback_add(cbChangeString, 2, memory_getVirtualOffsetFromPointer(eventReceiver), _changeStringParam.GetMPTR());
			}
		}
	}
}

void swkbd_keyInput(uint32 keyCode)
{
	if (keyCode == 8 || keyCode == 127) // backspace || backwards delete
	{
		if (swkbdInternalState->formStringLength > 0)
			swkbdInternalState->formStringLength--;
		swkbdInternalState->formStringBuffer[swkbdInternalState->formStringLength] = '\0';
		swkbd_inputStringChanged();
		return;
	}
	else if (keyCode == 13) // return
	{
		swkbd_finishInput();
		return;
	}
	// check if allowed character
	if ((keyCode >= 'a' && keyCode <= 'z') ||
		(keyCode >= 'A' && keyCode <= 'Z'))
	{
		// allowed
	}
	else if ((keyCode >= '0' && keyCode <= '9'))
	{
		// allowed
	}
	else if (keyCode == ' ' || keyCode == '.' || keyCode == '/' || keyCode == '?' || keyCode == '=')
	{
		// allowed
	}
	else if (keyCode < 32)
	{
		// control key
		return;
	}
	else
		;// return;
	// get max length
	sint32 maxLength = swkbdInternalState->maxTextLength;
	if (swkbdInternalState->keyboardOnlyMode)
	{
		uint32 stringBufferSize = swkbdInternalState->keyboardArg.receiverArg.stringBufSize;
		if (stringBufferSize > 1)
		{
			maxLength = stringBufferSize - 1; // don't count the null-termination character
		}
		else
			maxLength = 0;
	}
	// append character
	if (swkbdInternalState->formStringLength < maxLength)
	{
		swkbdInternalState->formStringBuffer[swkbdInternalState->formStringLength] = keyCode;
		swkbdInternalState->formStringLength++;
		swkbdInternalState->formStringBuffer[swkbdInternalState->formStringLength] = '\0';
		swkbd_inputStringChanged();
	}
}

namespace swkbd
{
	class : public COSModule
	{
		public:
		std::string_view GetName() override
		{
			return "swkbd";
		}

		void RPLMapped() override
		{
			cafeExportRegisterFunc(SwkbdCreate, "swkbd", "SwkbdCreate__3RplFPUcQ3_2nn5swkbd10RegionTypeUiP8FSClient", LogType::SWKBD);
			cafeExportRegisterFunc(SwkbdCalc, "swkbd", "SwkbdCalc__3RplFRCQ3_2nn5swkbd14ControllerInfo", LogType::SWKBD);
			cafeExportRegisterFunc(SwkbdGetStateKeyboard, "swkbd", "SwkbdGetStateKeyboard__3RplFv", LogType::SWKBD);
			cafeExportRegisterFunc(SwkbdGetStateInputForm, "swkbd", "SwkbdGetStateInputForm__3RplFv", LogType::SWKBD);
			cafeExportRegisterFunc(SwkbdSetReceiver, "swkbd", "SwkbdSetReceiver__3RplFRCQ3_2nn5swkbd11ReceiverArg", LogType::SWKBD);
			cafeExportRegisterFunc(SwkbdAppearInputForm, "swkbd", "SwkbdAppearInputForm__3RplFRCQ3_2nn5swkbd9AppearArg", LogType::SWKBD);
			cafeExportRegisterFunc(SwkbdDisappearInputForm, "swkbd", "SwkbdDisappearInputForm__3RplFv", LogType::SWKBD);
			cafeExportRegisterFunc(SwkbdDisappearKeyboard, "swkbd", "SwkbdDisappearKeyboard__3RplFv", LogType::SWKBD);
			cafeExportRegisterFunc(SwkbdAppearKeyboard, "swkbd", "SwkbdAppearKeyboard__3RplFRCQ3_2nn5swkbd11KeyboardArg", LogType::SWKBD);
			cafeExportRegisterFunc(SwkbdGetInputFormString, "swkbd", "SwkbdGetInputFormString__3RplFv", LogType::SWKBD);
			cafeExportRegisterFunc(SwkbdIsDecideOkButton, "swkbd", "SwkbdIsDecideOkButton__3RplFPb", LogType::SWKBD);
			cafeExportRegisterFunc(SwkbdInitLearnDic, "swkbd", "SwkbdInitLearnDic__3RplFPv", LogType::SWKBD);
			cafeExportRegisterFunc(SwkbdGetDrawStringInfo, "swkbd", "SwkbdGetDrawStringInfo__3RplFPQ3_2nn5swkbd14DrawStringInfo", LogType::SWKBD);
			cafeExportRegisterFunc(SwkbdCalcSubThreadFont, "swkbd", "SwkbdCalcSubThreadFont__3RplFv", LogType::SWKBD);
			cafeExportRegisterFunc(SwkbdCalcSubThreadPredict, "swkbd", "SwkbdCalcSubThreadPredict__3RplFv", LogType::SWKBD);
			cafeExportRegisterFunc(SwkbdIsNeedCalcSubThreadFont, "swkbd", "SwkbdIsNeedCalcSubThreadFont__3RplFv", LogType::SWKBD);
			cafeExportRegisterFunc(SwkbdIsNeedCalcSubThreadPredict, "swkbd", "SwkbdIsNeedCalcSubThreadPredict__3RplFv", LogType::SWKBD);
		};

		void rpl_entry(uint32 moduleHandle, coreinit::RplEntryReason reason) override
		{
			if (reason == coreinit::RplEntryReason::Loaded)
			{
				swkbdInternalState = nullptr;
			}
			else if (reason == coreinit::RplEntryReason::Unloaded)
			{
				// todo
			}
		}
	}s_COSswkbdModule;

	COSModule* GetModule()
	{
		return &s_COSswkbdModule;
	}
}
