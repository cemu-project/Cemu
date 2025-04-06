#include "Cafe/OS/common/OSCommon.h"
#include "erreula.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "util/helpers/helpers.h"

#include <imgui.h>
#include "imgui/imgui_extension.h"

#include <wx/msgdlg.h>

#include "Cafe/OS/libs/coreinit/coreinit_FS.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"
#include "Cafe/OS/libs/vpad/vpad.h"

namespace nn
{
namespace erreula
{

	enum class ErrorDialogType : uint32
	{
		Code = 0,
		Text = 1,
		TextOneButton = 2,
		TextTwoButton = 3
	};

	static const sint32 FADE_TIME = 80;

	enum class ErrEulaState : uint32
	{
		Hidden = 0,
		Appearing = 1,
		Visible = 2,
		Disappearing = 3
	};

	enum class ResultType : uint32
	{
		None = 0,
		Finish = 1,
		Next = 2,
		Jump = 3,
		Password = 4
	};

	struct AppearError
	{
		AppearError() = default;
		AppearError(const AppearError& o)
		{
			errorType = o.errorType;
			screenType = o.screenType;
			controllerType = o.controllerType;
			holdType = o.holdType;
			errorCode = o.errorCode;
			framerate = o.framerate;
			text = o.text;
			button1Text = o.button1Text;
			button2Text = o.button2Text;
			title = o.title;
			drawCursor = o.drawCursor;
		}

		betype<ErrorDialogType> errorType;
		uint32be screenType;
		uint32be controllerType;
		uint32be holdType;
		uint32be errorCode;
		uint32be framerate;
		MEMPTR<uint16be> text;
		MEMPTR<uint16be> button1Text;
		MEMPTR<uint16be> button2Text;
		MEMPTR<uint16be> title;
		uint8 padding[3];
		bool drawCursor{};
	};

	using AppearArg = AppearError;

	static_assert(sizeof(AppearError) == 0x2C); // maybe larger

	struct HomeNixSignArg_t
	{
		uint32be framerate;
	};

	static_assert(sizeof(HomeNixSignArg_t) == 0x4); // maybe larger

	struct ControllerInfo_t
	{
		MEMPTR<VPADStatus_t> vpadStatus;
		MEMPTR<KPADStatus_t> kpadStatus[4]; // or 7 now like KPAD_MAX_CONTROLLERS?
	};

	static_assert(sizeof(ControllerInfo_t) == 0x14); // maybe larger

	class ErrEulaInstance
	{
	  public:
		enum class BUTTON_SELECTION : uint32
		{
			NONE = 0xFFFFFFFF,
			LEFT = 0,
			RIGHT = 1,
		};

		void Init()
		{
			m_buttonSelection = BUTTON_SELECTION::NONE;
			m_resultCode = -1;
			m_resultCodeForLeftButton = 0;
			m_resultCodeForRightButton = 0;
			SetState(ErrEulaState::Hidden);
		}

		void DoAppearError(AppearArg* arg)
		{
			m_buttonSelection = BUTTON_SELECTION::NONE;
			m_resultCode = -1;
			m_resultCodeForLeftButton = -1;
			m_resultCodeForRightButton = -1;
			// for standard dialog its 0 and 1?
			m_resultCodeForLeftButton = 0;
			m_resultCodeForRightButton = 1;
			SetState(ErrEulaState::Appearing);
		}

		void DoDisappearError()
		{
			if(m_state != ErrEulaState::Visible)
				return;
			SetState(ErrEulaState::Disappearing);
		}

		void DoCalc()
		{
			// appearing and disappearing state will automatically advance after some time
			if (m_state == ErrEulaState::Appearing || m_state == ErrEulaState::Disappearing)
			{
				uint32 elapsedTick = coreinit::OSGetTime() - m_lastStateChange;
				if (elapsedTick > coreinit::EspressoTime::ConvertMsToTimerTicks(FADE_TIME))
				{
					SetState(m_state == ErrEulaState::Appearing ? ErrEulaState::Visible : ErrEulaState::Hidden);
				}
			}
		}

		bool IsDecideSelectButtonError() const
		{
			return m_buttonSelection != BUTTON_SELECTION::NONE;
		}

		bool IsDecideSelectLeftButtonError() const
		{
			return m_buttonSelection != BUTTON_SELECTION::LEFT;
		}

		bool IsDecideSelectRightButtonError() const
		{
			return m_buttonSelection != BUTTON_SELECTION::RIGHT;
		}

		void SetButtonSelection(BUTTON_SELECTION selection)
		{
			cemu_assert_debug(m_buttonSelection == BUTTON_SELECTION::NONE);
			m_buttonSelection = selection;
			cemu_assert_debug(selection == BUTTON_SELECTION::LEFT || selection == BUTTON_SELECTION::RIGHT);
			m_resultCode = selection == BUTTON_SELECTION::LEFT ? m_resultCodeForLeftButton : m_resultCodeForRightButton;
		}

		ErrEulaState GetState() const
		{
			return m_state;
		}

		sint32 GetResultCode() const
		{
			return m_resultCode;
		}

		ResultType GetResultType() const
		{
		  if(m_resultCode == -1)
			  return ResultType::None;
		  if(m_resultCode < 10)
			  return ResultType::Finish;
		  if(m_resultCode >= 9999)
			  return ResultType::Next;
		  if(m_resultCode == 40)
			  return ResultType::Password;
		  return ResultType::Jump;
		}

		float GetFadeTransparency() const
		{
			if(m_state == ErrEulaState::Appearing || m_state == ErrEulaState::Disappearing)
			{
				uint32 elapsedTick = coreinit::OSGetTime() - m_lastStateChange;
				if(m_state == ErrEulaState::Appearing)
					return std::min<float>(1.0f, (float)elapsedTick / (float)coreinit::EspressoTime::ConvertMsToTimerTicks(FADE_TIME));
				else
					return std::max<float>(0.0f, 1.0f - (float)elapsedTick / (float)coreinit::EspressoTime::ConvertMsToTimerTicks(FADE_TIME));
			}
			return 1.0f;
		}

	  private:
		void SetState(ErrEulaState state)
		{
			m_state = state;
			m_lastStateChange = coreinit::OSGetTime();
		}

		ErrEulaState m_state;
		uint32 m_lastStateChange;

		/* +0x30 */ betype<sint32> m_resultCode;
		/* +0x239C */ betype<BUTTON_SELECTION> m_buttonSelection;
		/* +0x23A0 */ betype<sint32> m_resultCodeForLeftButton;
		/* +0x23A4 */ betype<sint32> m_resultCodeForRightButton;
	};

	struct ErrEula_t
	{
		SysAllocator<coreinit::OSMutex> mutex;
		uint32 regionType;
		uint32 langType;
		MEMPTR<coreinit::FSClient_t> fsClient;

		std::unique_ptr<ErrEulaInstance> errEulaInstance;

		AppearError currentDialog;
		bool homeNixSignVisible;
	} g_errEula = {};
	
	std::wstring GetText(uint16be* text)
	{
		std::wstringstream result;
		while(*text != 0)
		{
			auto c = (uint16)*text;
			result << static_cast<wchar_t>(c);
			text++;
		}

		return result.str();
	}
	

	void ErrEulaCreate(void* workmem, uint32 regionType, uint32 langType, coreinit::FSClient_t* fsClient)
	{
		coreinit::OSLockMutex(&g_errEula.mutex);

		g_errEula.regionType = regionType;
		g_errEula.langType = langType;
		g_errEula.fsClient = fsClient;
		cemu_assert_debug(!g_errEula.errEulaInstance);
		g_errEula.errEulaInstance = std::make_unique<ErrEulaInstance>();
		g_errEula.errEulaInstance->Init();

		coreinit::OSUnlockMutex(&g_errEula.mutex);
	}

	void ErrEulaDestroy()
	{
		g_errEula.errEulaInstance.reset();
	}

	// check if any dialog button was selected
	bool IsDecideSelectButtonError()
	{
		if(!g_errEula.errEulaInstance)
			return false;
		return g_errEula.errEulaInstance->IsDecideSelectButtonError();
	}

	// check if left dialog button was selected
	bool IsDecideSelectLeftButtonError()
	{
		if(!g_errEula.errEulaInstance)
			return false;
		return g_errEula.errEulaInstance->IsDecideSelectLeftButtonError();
	}

	// check if right dialog button was selected
	bool IsDecideSelectRightButtonError()
	{
		if(!g_errEula.errEulaInstance)
			return false;
		return g_errEula.errEulaInstance->IsDecideSelectRightButtonError();
	}

	sint32 GetResultCode()
	{
		if(!g_errEula.errEulaInstance)
			return -1;
		return g_errEula.errEulaInstance->GetResultCode();
	}

	ResultType GetResultType()
	{
		if(!g_errEula.errEulaInstance)
			return ResultType::None;
		return g_errEula.errEulaInstance->GetResultType();
	}

	void export_AppearHomeNixSign(PPCInterpreter_t* hCPU)
	{
		g_errEula.homeNixSignVisible = TRUE;
		osLib_returnFromFunction(hCPU, 0);
	}

	void ErrEulaAppearError(AppearArg* arg)
	{
		g_errEula.currentDialog = *arg;
		if(g_errEula.errEulaInstance)
			g_errEula.errEulaInstance->DoAppearError(arg);
	}

	void ErrEulaDisappearError()
	{
		if(g_errEula.errEulaInstance)
			g_errEula.errEulaInstance->DoDisappearError();
	}

	ErrEulaState ErrEulaGetStateErrorViewer()
	{
		if(!g_errEula.errEulaInstance)
			return ErrEulaState::Hidden;
		return g_errEula.errEulaInstance->GetState();
	}

	void export_ChangeLang(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(langType, 0);
		g_errEula.langType = langType;
		osLib_returnFromFunction(hCPU, 0);
	}

	void export_IsAppearHomeNixSign(PPCInterpreter_t* hCPU)
	{
		osLib_returnFromFunction(hCPU, g_errEula.homeNixSignVisible);
	}

	void export_DisappearHomeNixSign(PPCInterpreter_t* hCPU)
	{
		g_errEula.homeNixSignVisible = FALSE;
		osLib_returnFromFunction(hCPU, 0);
	}

	void ErrEulaCalc(ControllerInfo_t* controllerInfo)
	{
		if(g_errEula.errEulaInstance)
			g_errEula.errEulaInstance->DoCalc();
	}

	void render(bool mainWindow)
	{
		if(!g_errEula.errEulaInstance)
			return;
		if(g_errEula.errEulaInstance->GetState() != ErrEulaState::Visible && g_errEula.errEulaInstance->GetState() != ErrEulaState::Appearing && g_errEula.errEulaInstance->GetState() != ErrEulaState::Disappearing)
			return;
		const AppearError& appearArg = g_errEula.currentDialog;
		std::string text;
		const uint32 errorCode = (uint32)appearArg.errorCode;
		if (errorCode != 0)
		{
			const uint32 errorCodeHigh = errorCode / 10000;
			const uint32 errorCodeLow = errorCode % 10000;
			text = fmt::format("Error-Code: {:03}-{:04}\n", errorCodeHigh, errorCodeLow);
		}

		auto font = ImGui_GetFont(32.0f);
		if (!font)
			return;

		const auto kPopupFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

		auto& io = ImGui::GetIO();
		ImVec2 position = { io.DisplaySize.x / 2.0f, io.DisplaySize.y / 2.0f };
		ImVec2 pivot = { 0.5f, 0.5f };
		ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);
		ImGui::SetNextWindowBgAlpha(0.9f);
		ImGui::PushFont(font);

		std::string title;
		if (appearArg.title)
			title = boost::nowide::narrow(GetText(appearArg.title.GetPtr()));
		if (title.empty()) // ImGui doesn't allow empty titles, so set one if appearArg.title is not set or empty
			title = "ErrEula";

		float fadeTransparency = 1.0f;
		if (g_errEula.errEulaInstance->GetState() == ErrEulaState::Appearing || g_errEula.errEulaInstance->GetState() == ErrEulaState::Disappearing)
		{
			fadeTransparency = g_errEula.errEulaInstance->GetFadeTransparency();
		}

		float originalAlpha = ImGui::GetStyle().Alpha;
		ImGui::GetStyle().Alpha = fadeTransparency;
		ImGui::SetNextWindowBgAlpha(0.9f * fadeTransparency);
		if (ImGui::Begin(title.c_str(), nullptr, kPopupFlags))
		{
			const float startx = ImGui::GetWindowSize().x / 2.0f;
			bool hasLeftButtonPressed = false, hasRightButtonPressed = false;

			switch (appearArg.errorType)
			{
			default:
			{
				// TODO layout based on error code
				ImGui::TextUnformatted(text.c_str(), text.c_str() + text.size());
				ImGui::Spacing();
				ImGui::SetCursorPosX(startx - 50);
				hasLeftButtonPressed = ImGui::Button("OK", {100, 0});
				break;
			}
			case ErrorDialogType::Text:
			{
				std::string txtTmp = "Unknown Error";
				if (appearArg.text)
					txtTmp = boost::nowide::narrow(GetText(appearArg.text.GetPtr()));

				text += txtTmp;
				ImGui::TextUnformatted(text.c_str(), text.c_str() + text.size());
				ImGui::Spacing();
					
				ImGui::SetCursorPosX(startx - 50);
				hasLeftButtonPressed = ImGui::Button("OK", { 100, 0 });
				break;
			}
			case ErrorDialogType::TextOneButton:
			{
				std::string txtTmp = "Unknown Error";
				if (appearArg.text)
					txtTmp = boost::nowide::narrow(GetText(appearArg.text.GetPtr()));

				text += txtTmp;
				ImGui::TextUnformatted(text.c_str(), text.c_str() + text.size());
				ImGui::Spacing();

				std::string button1 = "Yes";
				if (appearArg.button1Text)
					button1 = boost::nowide::narrow(GetText(appearArg.button1Text.GetPtr()));

				float width = std::max(100.0f, ImGui::CalcTextSize(button1.c_str()).x + 10.0f);
				ImGui::SetCursorPosX(startx - (width / 2.0f));
				hasLeftButtonPressed = ImGui::Button(button1.c_str(), { width, 0 });
				break;
			}
			case ErrorDialogType::TextTwoButton:
			{
				std::string txtTmp = "Unknown Error";
				if (appearArg.text)
					txtTmp = boost::nowide::narrow(GetText(appearArg.text.GetPtr()));

				text += txtTmp;
				ImGui::TextUnformatted(text.c_str(), text.c_str() + text.size());
				ImGui::Spacing();

				std::string button1 = "Yes";
				if (appearArg.button1Text)
					button1 = boost::nowide::narrow(GetText(appearArg.button1Text.GetPtr()));
				std::string button2 = "No";
				if (appearArg.button2Text)
					button2 = boost::nowide::narrow(GetText(appearArg.button2Text.GetPtr()));

				float width1 = std::max(100.0f, ImGui::CalcTextSize(button1.c_str()).x + 10.0f);
				float width2 = std::max(100.0f, ImGui::CalcTextSize(button2.c_str()).x + 10.0f);
				ImGui::SetCursorPosX(startx - (width1 / 2.0f) - (width2 / 2.0f) - 10);
					
				hasLeftButtonPressed = ImGui::Button(button1.c_str(), { width1, 0 });
				ImGui::SameLine();
					
				hasRightButtonPressed = ImGui::Button(button2.c_str(), { width2, 0 });
				break;
			}
			}
			if (!g_errEula.errEulaInstance->IsDecideSelectButtonError())
			{
				if (hasLeftButtonPressed)
					g_errEula.errEulaInstance->SetButtonSelection(ErrEulaInstance::BUTTON_SELECTION::LEFT);
				if (hasRightButtonPressed)
					g_errEula.errEulaInstance->SetButtonSelection(ErrEulaInstance::BUTTON_SELECTION::RIGHT);
			}
		}
		ImGui::End();
		ImGui::PopFont();
		ImGui::GetStyle().Alpha = originalAlpha;
	}

	void load()
	{
		g_errEula.errEulaInstance.reset();

		OSInitMutexEx(&g_errEula.mutex, nullptr);

		cafeExportRegisterFunc(ErrEulaCreate, "erreula", "ErrEulaCreate__3RplFPUcQ3_2nn7erreula10RegionTypeQ3_2nn7erreula8LangTypeP8FSClient", LogType::Placeholder);
		cafeExportRegisterFunc(ErrEulaDestroy, "erreula", "ErrEulaDestroy__3RplFv", LogType::Placeholder);

		cafeExportRegisterFunc(IsDecideSelectButtonError, "erreula", "ErrEulaIsDecideSelectButtonError__3RplFv", LogType::Placeholder);
		cafeExportRegisterFunc(IsDecideSelectLeftButtonError, "erreula", "ErrEulaIsDecideSelectLeftButtonError__3RplFv", LogType::Placeholder);
		cafeExportRegisterFunc(IsDecideSelectRightButtonError, "erreula", "ErrEulaIsDecideSelectRightButtonError__3RplFv", LogType::Placeholder);

		cafeExportRegisterFunc(GetResultCode, "erreula", "ErrEulaGetResultCode__3RplFv", LogType::Placeholder);
		cafeExportRegisterFunc(GetResultType, "erreula", "ErrEulaGetResultType__3RplFv", LogType::Placeholder);

		cafeExportRegisterFunc(ErrEulaAppearError, "erreula", "ErrEulaAppearError__3RplFRCQ3_2nn7erreula9AppearArg", LogType::Placeholder);
		cafeExportRegisterFunc(ErrEulaDisappearError, "erreula", "ErrEulaDisappearError__3RplFv", LogType::Placeholder);
		cafeExportRegisterFunc(ErrEulaGetStateErrorViewer, "erreula", "ErrEulaGetStateErrorViewer__3RplFv", LogType::Placeholder);

		cafeExportRegisterFunc(ErrEulaCalc, "erreula", "ErrEulaCalc__3RplFRCQ3_2nn7erreula14ControllerInfo", LogType::Placeholder);

		osLib_addFunction("erreula", "ErrEulaAppearHomeNixSign__3RplFRCQ3_2nn7erreula14HomeNixSignArg", export_AppearHomeNixSign);
		osLib_addFunction("erreula", "ErrEulaChangeLang__3RplFQ3_2nn7erreula8LangType", export_ChangeLang);
		osLib_addFunction("erreula", "ErrEulaIsAppearHomeNixSign__3RplFv", export_IsAppearHomeNixSign);
		osLib_addFunction("erreula", "ErrEulaDisappearHomeNixSign__3RplFv", export_DisappearHomeNixSign);
	}
}
}
 
