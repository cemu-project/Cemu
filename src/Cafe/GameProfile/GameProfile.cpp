#include "Cafe/GameProfile/GameProfile.h"
#include "util/helpers/helpers.h"

#include "boost/nowide/convert.hpp"

#include "config/ActiveSettings.h"
#include "Common/FileStream.h"
#include "util/IniParser/IniParser.h"
#include "util/helpers/StringHelpers.h"
#include "Cafe/CafeSystem.h"

std::unique_ptr<GameProfile> g_current_game_profile = std::make_unique<GameProfile>();

struct gameProfileBooleanOption_t
{
	bool isPresent = false;
	bool value;
};

/*
* Attempts to load a boolean option
* If the option exists, true is returned.
* The boolean is stored in *optionValue
*/
bool gameProfile_loadBooleanOption(IniParser* iniParser, char* optionName, gameProfileBooleanOption_t* option)
{
	auto option_value = iniParser->FindOption(optionName);
	option->isPresent = false;
	option->value = false;
	if (!option_value)
		return false;
	// parse option
	if (boost::iequals(*option_value, "false") || boost::iequals(*option_value, "0"))
	{
		option->isPresent = true;
		option->value = false;
		return true;
	}
	else if (boost::iequals(*option_value, "true") || boost::iequals(*option_value, "1"))
	{
		option->isPresent = true;
		option->value = true;
		return true;
	}
	else
		cemuLog_log(LogType::Force, "Unknown value '{}' for option '{}' in game profile", *option_value, optionName);
	return false;
}


bool gameProfile_loadBooleanOption2(IniParser& iniParser, const char* optionName, bool& option)
{
	auto option_value = iniParser.FindOption(optionName);
	if (!option_value)
		return false;
	if (boost::iequals(*option_value, "1") || boost::iequals(*option_value, "true"))
	{
		option = true;
		return true;
	}
	else if (boost::iequals(*option_value, "0") || boost::iequals(*option_value, "false"))
	{
		option = false;
		return true;
	}
	else
		cemuLog_log(LogType::Force, "Unknown value '{}' for option '{}' in game profile", *option_value, optionName);
	return false;
}

bool gameProfile_loadBooleanOption2(IniParser& iniParser, const char* optionName, std::optional<bool>& option)
{
	bool tmp;
	const auto result = gameProfile_loadBooleanOption2(iniParser, optionName, tmp);
	if(result)
		option = tmp;
	return result;
}

/*
* Attempts to load a integer option
* Allows to specify min and max value (error is logged if out of range and default value is picked)
*/
bool gameProfile_loadIntegerOption(IniParser* iniParser, const char* optionName, gameProfileIntegerOption_t* option, sint32 defaultValue, sint32 minVal, sint32 maxVal)
{
	auto option_value = iniParser->FindOption(optionName);
	option->isPresent = false;
	if (!option_value)
	{
		option->value = defaultValue;
		return false;
	}
	// parse option
	sint32 val = StringHelpers::ToInt(*option_value, defaultValue);
	if (val < minVal || val > maxVal)
	{
		cemuLog_log(LogType::Force, "Value '{}' is out of range for option '{}' in game profile", *option_value, optionName);
		option->value = defaultValue;
		return false;
	}
	option->isPresent = true;
	option->value = val;
	return true;
}

template <typename T>
bool gameProfile_loadIntegerOption(IniParser& iniParser, const char* optionName, T& option, T minVal, T maxVal)
{
	static_assert(std::is_integral<T>::value);
	auto option_value = iniParser.FindOption(optionName);
	if (!option_value)
		return false;
	// parse option
	try
	{
		T val = ConvertString<T>(*option_value);
		if (val < minVal || val > maxVal)
		{
			cemuLog_log(LogType::Force, "Value '{}' is out of range for option '{}' in game profile", *option_value, optionName);
			return false;
		}

		option = val;
		return true;
	}
	catch(std::exception&)
	{
		cemuLog_log(LogType::Force, "Value '{}' is out of range for option '{}' in game profile", *option_value, optionName);
		return false;
	}	
}

template<typename T>
bool gameProfile_loadEnumOption(IniParser& iniParser, const char* optionName, T& option)
{
	static_assert(std::is_enum<T>::value);
	auto option_value = iniParser.FindOption(optionName);
	if (!option_value)
		return false;
	for(const T& v : T())
	{
		// test integer option
		if (boost::iequals(fmt::format("{}", static_cast<typename std::underlying_type<T>::type>(v)), *option_value))
		{
			option = v;
			return true;
		}

		// test enum name
		if(boost::iequals(fmt::format("{}", v), *option_value))
		{
			option = v;
			return true;
		}
	}
	return false;
}

template<typename T>
bool gameProfile_loadEnumOption(IniParser& iniParser, const char* optionName, std::optional<T>& option)
{
	T tmp;
	const auto result = gameProfile_loadEnumOption(iniParser, optionName, tmp);
	if(result)
		option = tmp;
	return result;
}

void gameProfile_load()
{
	g_current_game_profile->ResetOptional(); // reset with global values as optional
	g_current_game_profile->Load(CafeSystem::GetForegroundTitleId());

	// apply some settings immediately
	ppcThreadQuantum = g_current_game_profile->GetThreadQuantum();

	if (ppcThreadQuantum != GameProfile::kThreadQuantumDefault)
		cemuLog_log(LogType::Force, "Thread quantum set to {}", ppcThreadQuantum);
}

bool GameProfile::Load(uint64_t title_id)
{
	auto gameProfilePath = ActiveSettings::GetConfigPath("gameProfiles/{:016x}.ini", title_id);

	std::optional<std::vector<uint8>> profileContents = FileStream::LoadIntoMemory(gameProfilePath);
	if (!profileContents)
	{
		gameProfilePath = ActiveSettings::GetDataPath("gameProfiles/default/{:016x}.ini", title_id);
		profileContents = FileStream::LoadIntoMemory(gameProfilePath);
		if (!profileContents)
			return false;
		m_is_default = true;
	}
	else
		m_is_default = false;

	m_is_loaded = true;
	// most official gameprofiles start with "# gamename"
	std::vector<char> game_name;
	if (profileContents->size() > 0 && profileContents->data()[0] == '#')
	{
		char c;
		size_t idx = 1;
		while (idx < profileContents->size() && (c = profileContents->data()[idx]) != '\n' && idx < 128)
		{
			game_name.emplace_back(c);
			idx++;
		}
		m_gameName = std::string(game_name.begin(), game_name.end());
		trim(m_gameName.value());
	}
	IniParser iniParser(*profileContents, _pathToUtf8(gameProfilePath));
	// parse ini
	while (iniParser.NextSection())
	{
		if (boost::iequals(iniParser.GetCurrentSectionName(), "General"))
		{
			gameProfile_loadBooleanOption2(iniParser, "loadSharedLibraries", m_loadSharedLibraries);
			gameProfile_loadBooleanOption2(iniParser, "startWithPadView", m_startWithPadView);
		}
		else if (boost::iequals(iniParser.GetCurrentSectionName(), "Graphics"))
		{
			gameProfileIntegerOption_t graphicsApi;
			gameProfile_loadIntegerOption(&iniParser, "graphics_api", &graphicsApi, -1, 0, 1);
			if (graphicsApi.value != -1)
				m_graphics_api = (GraphicAPI)graphicsApi.value;
			
			gameProfile_loadEnumOption(iniParser, "accurateShaderMul", m_accurateShaderMul);

			// legacy support
			auto option_precompiledShaders = iniParser.FindOption("precompiledShaders");
			if (option_precompiledShaders)
			{
				if (boost::iequals(*option_precompiledShaders, "1") || boost::iequals(*option_precompiledShaders, "true"))
					m_precompiledShaders = PrecompiledShaderOption::Enable;
				else if (boost::iequals(*option_precompiledShaders, "0") || boost::iequals(*option_precompiledShaders, "false"))
					m_precompiledShaders = PrecompiledShaderOption::Disable;
				else
					m_precompiledShaders = PrecompiledShaderOption::Auto;
			}
			else
				m_precompiledShaders = PrecompiledShaderOption::Auto;
		}
		else if (boost::iequals(iniParser.GetCurrentSectionName(), "Audio"))
		{
			gameProfile_loadBooleanOption2(iniParser, "disableAudio", m_disableAudio);
		}
		else if (boost::iequals(iniParser.GetCurrentSectionName(), "CPU"))
		{
			gameProfile_loadIntegerOption(iniParser, "threadQuantum", m_threadQuantum, 1000U, 536870912U);
			if (!gameProfile_loadEnumOption(iniParser, "cpuMode", m_cpuMode))
			{
				// try to load the old enum value strings
				std::optional<CPUModeLegacy> cpu_mode_legacy;
				if (gameProfile_loadEnumOption(iniParser, "cpuMode", cpu_mode_legacy) && cpu_mode_legacy.has_value())
				{
					m_cpuMode = (CPUMode)cpu_mode_legacy.value();
					if (m_cpuMode == CPUMode::DualcoreRecompiler)
						m_cpuMode = CPUMode::MulticoreRecompiler;
				}
			}
		}
		else if (boost::iequals(iniParser.GetCurrentSectionName(), "Controller"))
		{
			for (int i = 0; i < 8; ++i)
			{
				auto option_value = iniParser.FindOption(fmt::format("controller{}", (i + 1)));
				if (option_value)
					m_controllerProfile[i] = std::string(*option_value);
			}

		}
	}
	return true;
}

void GameProfile::Save(uint64_t title_id)
{
	auto gameProfileDir = ActiveSettings::GetConfigPath("gameProfiles");
	if (std::error_code ex_ec; !fs::exists(gameProfileDir, ex_ec)) 
		fs::create_directories(gameProfileDir, ex_ec);
	auto gameProfilePath = gameProfileDir / fmt::format("{:016x}.ini", title_id);
	FileStream* fs = FileStream::createFile2(gameProfilePath);
	if (!fs)
	{
		cemuLog_log(LogType::Force, "Failed to write game profile");
		return;
	}

	if (m_gameName)
		fs->writeLine(fmt::format("# {}\n", m_gameName.value()).c_str());

#define WRITE_OPTIONAL_ENTRY(__NAME) if (m_##__NAME) fs->writeLine(fmt::format("{} = {}", #__NAME, m_##__NAME.value()).c_str());
#define WRITE_ENTRY(__NAME) fs->writeLine(fmt::format("{} = {}", #__NAME, m_##__NAME).c_str());

	fs->writeLine("[General]");
	WRITE_OPTIONAL_ENTRY(loadSharedLibraries);
	WRITE_ENTRY(startWithPadView);

	fs->writeLine("");


	fs->writeLine("[CPU]");
	WRITE_OPTIONAL_ENTRY(cpuMode);
	WRITE_ENTRY(threadQuantum);

	fs->writeLine("");

	fs->writeLine("[Graphics]");
	WRITE_ENTRY(accurateShaderMul);
	WRITE_OPTIONAL_ENTRY(precompiledShaders);
	WRITE_OPTIONAL_ENTRY(graphics_api);
	fs->writeLine("");

	fs->writeLine("[Controller]");
	for (int i = 0; i < 8; ++i)
	{
		if (m_controllerProfile[i])
			fs->writeLine(fmt::format("controller{} = {}", (i + 1), m_controllerProfile[i].value()).c_str());
	}

	fs->writeLine("");

#undef WRITE_OPTIONAL_ENTRY
#undef WRITE_ENTRY

	delete fs;
}

void GameProfile::ResetOptional()
{
	m_gameName.reset();

	// general settings
	m_loadSharedLibraries.reset(); // true;
	m_startWithPadView = false;

	// graphic settings
	m_accurateShaderMul = AccurateShaderMulOption::True;
	// cpu settings
	m_threadQuantum = kThreadQuantumDefault;
	m_cpuMode.reset(); // CPUModeOption::kSingleCoreRecompiler;
	// audio
	m_disableAudio = false;
	// controller settings
	for (auto& profile : m_controllerProfile)
		profile.reset();
}

void GameProfile::Reset()
{
	m_gameName.reset();

	// general settings
	m_loadSharedLibraries = true;
	m_startWithPadView = false;
	
	// graphic settings
	m_accurateShaderMul = AccurateShaderMulOption::True;
	m_precompiledShaders = PrecompiledShaderOption::Auto;
	// cpu settings
	m_threadQuantum = kThreadQuantumDefault;
	m_cpuMode = CPUMode::Auto;
	// audio
	m_disableAudio = false;
	// controller settings
	for (auto& profile : m_controllerProfile)
		profile.reset();
}