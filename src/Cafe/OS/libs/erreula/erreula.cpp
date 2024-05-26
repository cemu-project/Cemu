#include "Cafe/OS/common/OSCommon.h"
#include "erreula.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "util/helpers/helpers.h"

#include <imgui.h>
#include "imgui/imgui_extension.h"

#include <wx/msgdlg.h>

#include "Cafe/OS/libs/coreinit/coreinit_FS.h"
#include "Cafe/OS/libs/vpad/vpad.h"

namespace nn
{
namespace erreula
{
#define RESULTTYPE_NONE		0
#define RESULTTYPE_FINISH	1
#define RESULTTYPE_NEXT		2
#define RESULTTYPE_JUMP		3
#define RESULTTYPE_PASSWORD 4

#define ERRORTYPE_CODE 0
#define ERRORTYPE_TEXT 1
#define ERRORTYPE_TEXT_ONE_BUTTON 2
#define ERRORTYPE_TEXT_TWO_BUTTON 3

#define ERREULA_STATE_HIDDEN		0
#define ERREULA_STATE_APPEARING		1
#define ERREULA_STATE_VISIBLE		2
#define ERREULA_STATE_DISAPPEARING	3

	struct AppearArg_t
	{
		AppearArg_t() = default;
		AppearArg_t(const AppearArg_t& o)
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

		uint32be errorType;
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

	static_assert(sizeof(AppearArg_t) == 0x2C); // maybe larger

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

	struct ErrEula_t
	{
		SysAllocator<coreinit::OSMutex> mutex;
		uint32 regionType;
		uint32 langType;
		MEMPTR<coreinit::FSClient_t> fsClient;

		AppearArg_t currentDialog;
		uint32 state;
		bool buttonPressed;
		bool rightButtonPressed;

		bool homeNixSignVisible;

		std::chrono::steady_clock::time_point stateTimer{};
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
	

	void export_ErrEulaCreate(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(thisptr, uint8, 0);
		ppcDefineParamU32(regionType, 1);
		ppcDefineParamU32(langType, 2);
		ppcDefineParamMEMPTR(fsClient, coreinit::FSClient_t, 3);

		coreinit::OSLockMutex(&g_errEula.mutex);

		g_errEula.regionType = regionType;
		g_errEula.langType = langType;
		g_errEula.fsClient = fsClient;

		coreinit::OSUnlockMutex(&g_errEula.mutex);

		osLib_returnFromFunction(hCPU, 0);
	}

	void export_AppearHomeNixSign(PPCInterpreter_t* hCPU)
	{
		g_errEula.homeNixSignVisible = TRUE;
		osLib_returnFromFunction(hCPU, 0);
	}

	void export_AppearError(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(arg, AppearArg_t, 0);

		g_errEula.currentDialog = *arg.GetPtr();
		g_errEula.state = ERREULA_STATE_APPEARING;
		g_errEula.buttonPressed = false;
		g_errEula.rightButtonPressed = false;

		g_errEula.stateTimer = tick_cached();

		osLib_returnFromFunction(hCPU, 0);
	}

	void export_GetStateErrorViewer(PPCInterpreter_t* hCPU)
	{
		osLib_returnFromFunction(hCPU, g_errEula.state);
	}
	void export_DisappearError(PPCInterpreter_t* hCPU)
	{
		g_errEula.state = ERREULA_STATE_HIDDEN;
		osLib_returnFromFunction(hCPU, 0);
	}

	void export_ChangeLang(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(langType, 0);
		g_errEula.langType = langType;
		osLib_returnFromFunction(hCPU, 0);
	}

	void export_IsDecideSelectButtonError(PPCInterpreter_t* hCPU)
	{
		if (g_errEula.buttonPressed)
			cemuLog_logDebug(LogType::Force, "IsDecideSelectButtonError: TRUE");
		osLib_returnFromFunction(hCPU, g_errEula.buttonPressed);
	}

	void export_IsDecideSelectLeftButtonError(PPCInterpreter_t* hCPU)
	{
		if (g_errEula.buttonPressed)
			cemuLog_logDebug(LogType::Force, "IsDecideSelectLeftButtonError: TRUE");
		osLib_returnFromFunction(hCPU, g_errEula.buttonPressed);
	}

	void export_IsDecideSelectRightButtonError(PPCInterpreter_t* hCPU)
	{
		if (g_errEula.rightButtonPressed)
			cemuLog_logDebug(LogType::Force, "IsDecideSelectRightButtonError: TRUE");
		osLib_returnFromFunction(hCPU, g_errEula.rightButtonPressed);
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

	void export_GetResultType(PPCInterpreter_t* hCPU)
	{
		uint32 result = RESULTTYPE_NONE;
		if (g_errEula.buttonPressed || g_errEula.rightButtonPressed)
		{
			cemuLog_logDebug(LogType::Force, "GetResultType: FINISH");
			result = RESULTTYPE_FINISH;
		}

		osLib_returnFromFunction(hCPU, result);
	}

	void export_Calc(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(controllerInfo, ControllerInfo_t, 0);
		// TODO: check controller buttons bla to accept dialog?
		osLib_returnFromFunction(hCPU, 0);
	}

	void render(bool mainWindow)
	{
		if(g_errEula.state == ERREULA_STATE_HIDDEN)
			return;

		if(g_errEula.state == ERREULA_STATE_APPEARING)
		{
			if(std::chrono::duration_cast<std::chrono::milliseconds>(tick_cached() - g_errEula.stateTimer).count() <= 1000)
			{
				return;
			}

			g_errEula.state = ERREULA_STATE_VISIBLE;
			g_errEula.stateTimer = tick_cached();
		}
		/*else if(g_errEula.state == STATE_VISIBLE)
		{
			if (std::chrono::duration_cast<std::chrono::milliseconds>(tick_cached() - g_errEula.stateTimer).count() >= 1000)
			{
				g_errEula.state = STATE_DISAPPEARING;
				g_errEula.stateTimer = tick_cached();
				return;
			}
		}*/
		else if(g_errEula.state == ERREULA_STATE_DISAPPEARING)
		{
			if (std::chrono::duration_cast<std::chrono::milliseconds>(tick_cached() - g_errEula.stateTimer).count() >= 2000)
			{
				g_errEula.state = ERREULA_STATE_HIDDEN;
				g_errEula.stateTimer = tick_cached();
			}

			return;
		}
		
		const AppearArg_t& appearArg = g_errEula.currentDialog;
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
		if(title.empty()) // ImGui doesn't allow empty titles, so set one if appearArg.title is not set or empty
			title = "ErrEula";
		if (ImGui::Begin(title.c_str(), nullptr, kPopupFlags))
		{
			const float startx = ImGui::GetWindowSize().x / 2.0f;

			switch ((uint32)appearArg.errorType)
			{
			default:
			{
				// TODO layout based on error code
				ImGui::TextUnformatted(text.c_str(), text.c_str() + text.size());
				ImGui::Spacing();
				ImGui::SetCursorPosX(startx - 50);
				g_errEula.buttonPressed |= ImGui::Button("OK", {100, 0});
				
				break;
			}
			case ERRORTYPE_TEXT:
			{
				std::string txtTmp = "Unknown Error";
				if (appearArg.text)
					txtTmp = boost::nowide::narrow(GetText(appearArg.text.GetPtr()));

				text += txtTmp;
				ImGui::TextUnformatted(text.c_str(), text.c_str() + text.size());
				ImGui::Spacing();
					
				ImGui::SetCursorPosX(startx - 50);
				g_errEula.buttonPressed |= ImGui::Button("OK", { 100, 0 });
				break;
			}
			case ERRORTYPE_TEXT_ONE_BUTTON:
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
				g_errEula.buttonPressed |= ImGui::Button(button1.c_str(), { width, 0 });
				break;
			}
			case ERRORTYPE_TEXT_TWO_BUTTON:
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
					
				g_errEula.buttonPressed |= ImGui::Button(button1.c_str(), { width1, 0 });
				ImGui::SameLine();
					
				g_errEula.rightButtonPressed |= ImGui::Button(button2.c_str(), { width2, 0 });
				break;
			}
			}
		}
		ImGui::End();
		ImGui::PopFont();

		if(g_errEula.buttonPressed || g_errEula.rightButtonPressed)
		{
			g_errEula.state = ERREULA_STATE_DISAPPEARING;
			g_errEula.stateTimer = tick_cached();
		}
	}

	void load()
	{
		OSInitMutexEx(&g_errEula.mutex, nullptr);

		//osLib_addFunction("erreula", "ErrEulaCreate__3RplFPUcQ3_2nn7erreula10", export_ErrEulaCreate); // copy ctor?
		osLib_addFunction("erreula", "ErrEulaCreate__3RplFPUcQ3_2nn7erreula10RegionTypeQ3_2nn7erreula8LangTypeP8FSClient", export_ErrEulaCreate);
		osLib_addFunction("erreula", "ErrEulaAppearHomeNixSign__3RplFRCQ3_2nn7erreula14HomeNixSignArg", export_AppearHomeNixSign);
		osLib_addFunction("erreula", "ErrEulaAppearError__3RplFRCQ3_2nn7erreula9AppearArg", export_AppearError);
		osLib_addFunction("erreula", "ErrEulaGetStateErrorViewer__3RplFv", export_GetStateErrorViewer);
		osLib_addFunction("erreula", "ErrEulaChangeLang__3RplFQ3_2nn7erreula8LangType", export_ChangeLang);
		osLib_addFunction("erreula", "ErrEulaIsDecideSelectButtonError__3RplFv", export_IsDecideSelectButtonError);
		osLib_addFunction("erreula", "ErrEulaCalc__3RplFRCQ3_2nn7erreula14ControllerInfo", export_Calc);
		osLib_addFunction("erreula", "ErrEulaIsDecideSelectLeftButtonError__3RplFv", export_IsDecideSelectLeftButtonError);
		osLib_addFunction("erreula", "ErrEulaIsDecideSelectRightButtonError__3RplFv", export_IsDecideSelectRightButtonError);
		osLib_addFunction("erreula", "ErrEulaIsAppearHomeNixSign__3RplFv", export_IsAppearHomeNixSign);
		osLib_addFunction("erreula", "ErrEulaDisappearHomeNixSign__3RplFv", export_DisappearHomeNixSign);
		osLib_addFunction("erreula", "ErrEulaGetResultType__3RplFv", export_GetResultType);
		osLib_addFunction("erreula", "ErrEulaDisappearError__3RplFv", export_DisappearError);
	}
}
}
 
