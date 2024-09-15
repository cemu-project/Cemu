#include "LaunchSettings.h"

#include "util/helpers/helpers.h"
#include "Cafe/Account/Account.h"

#include "Cafe/OS/libs/coreinit/coreinit.h"

#include "boost/program_options.hpp"
#include <wx/msgdlg.h>

#include "config/ActiveSettings.h"
#include "config/NetworkSettings.h"
#include "util/crypto/aes128.h"

#include "Cafe/Filesystem/FST/FST.h"

void requireConsole();

bool LaunchSettings::HandleCommandline(const wchar_t* lpCmdLine)
{
	#if BOOST_OS_WINDOWS
	const std::vector<std::wstring> args = boost::program_options::split_winmain(lpCmdLine);
	return HandleCommandline(args);
	#else
	cemu_assert_unimplemented();
	return false;
	#endif
}

bool LaunchSettings::HandleCommandline(int argc, wchar_t* argv[])
{
	std::vector<std::wstring> args;
	args.reserve(argc);
	for(int i = 0; i < argc; ++i)
	{
		args.emplace_back(argv[i]);
	}
	
	return HandleCommandline(args);
}

bool LaunchSettings::HandleCommandline(int argc, char* argv[])
{
	std::vector<std::wstring> args;
	args.reserve(argc);
	for(int i = 0; i < argc; ++i)
	{
		args.emplace_back(boost::nowide::widen(argv[i]));
	}
	
	return HandleCommandline(args);
}

bool LaunchSettings::HandleCommandline(const std::vector<std::wstring>& args)
{
	namespace po = boost::program_options;
	po::options_description desc{ "Launch options" };
	desc.add_options()
		("help,h", "This help screen")
		("version,v", "Displays the version of Cemu")

		("game,g", po::wvalue<std::wstring>(), "Path of game to launch")
        ("title-id,t", po::value<std::string>(), "Title ID of the title to be launched (overridden by --game)")
		("mlc,m", po::wvalue<std::wstring>(), "Custom mlc folder location")

		("fullscreen,f", po::value<bool>()->implicit_value(true), "Launch games in fullscreen mode")
		("ud,u", po::value<bool>()->implicit_value(true), "Render output upside-down")

		("account,a", po::value<std::string>(), "Persistent id of account")

		("force-interpreter", po::value<bool>()->implicit_value(true), "Force interpreter CPU emulation, disables recompiler")
		("enable-gdbstub", po::value<bool>()->implicit_value(true), "Enable GDB stub to debug executables inside Cemu using an external debugger");

	po::options_description hidden{ "Hidden options" };
	hidden.add_options()
		("nsight", po::value<bool>()->implicit_value(true), "NSight debugging options")
		("legacy", po::value<bool>()->implicit_value(true), "Intel legacy graphic mode");

	po::options_description extractor{ "Extractor tool" };
	extractor.add_options()
		("extract,e", po::wvalue<std::wstring>(), "Path to WUD or WUX file for extraction")
		("path,p", po::value<std::string>(), "Path of file to extract (for example meta/meta.xml)")
		("output,o", po::wvalue<std::wstring>(), "Output path for extracted file.");
	
	po::options_description all;
	all.add(desc).add(hidden).add(extractor);

	po::options_description visible;
	visible.add(desc).add(extractor);

	try
	{
		po::wcommand_line_parser parser{ args };
		parser.allow_unregistered().options(all).style(po::command_line_style::allow_long | po::command_line_style::allow_short | po::command_line_style::allow_dash_for_short | po::command_line_style::case_insensitive |
			po::command_line_style::long_allow_next |
			po::command_line_style::short_allow_next |
			po::command_line_style::allow_long_disguise);

		const auto parsed_options = parser.run();

		po::variables_map vm;
		po::store(parsed_options, vm);
		notify(vm);

		if (vm.count("help"))
		{
			requireConsole();
			std::cout << visible << std::endl;
			return false; // exit in main
		}
		if (vm.count("version"))
		{
			requireConsole();
			std::string versionStr;
#if EMULATOR_VERSION_PATCH == 0
			versionStr = fmt::format("{}.{}{}", EMULATOR_VERSION_MAJOR, EMULATOR_VERSION_MINOR, EMULATOR_VERSION_SUFFIX);
#else
			versionStr = fmt::format("{}.{}-{}{}", EMULATOR_VERSION_MAJOR, EMULATOR_VERSION_MINOR, EMULATOR_VERSION_PATCH, EMULATOR_VERSION_SUFFIX);
#endif
			std::cout << versionStr << std::endl;
			return false; // exit in main
		}

		if (vm.count("game"))
		{
			std::wstring tmp = vm["game"].as<std::wstring>();
			// workaround for boost command_line_parser not trimming token for short name parameters despite short_allow_adjacent
			if (tmp.size() > 0 && tmp.front() == '=')
				tmp.erase(tmp.begin()+0);
			
			s_load_game_file = tmp;
		}
        if (vm.count("title-id"))
        {
            auto title_param = vm["title-id"].as<std::string>();
            try {

                if (title_param.starts_with('=')){
                    title_param.erase(title_param.begin());
                }
                s_load_title_id = std::stoull(title_param, nullptr, 16);
            }
            catch (std::invalid_argument const& e)
            {
                std::cerr << "Expected title_param ID as an unsigned 64-bit hexadecimal string\n";
            }
        }
			
		if (vm.count("mlc"))
		{
			std::wstring tmp = vm["mlc"].as<std::wstring>();
			// workaround for boost command_line_parser not trimming token for short name parameters despite short_allow_adjacent
			if (tmp.size() > 0 && tmp.front() == '=')
				tmp.erase(tmp.begin() + 0);

			s_mlc_path = tmp;
		}

		if (vm.count("account"))
		{
			const auto id = ConvertString<uint32>(vm["account"].as<std::string>(), 16);
			if (id >= Account::kMinPersistendId)
				s_persistent_id = id;
		}
		
		if (vm.count("fullscreen"))
			s_fullscreen = vm["fullscreen"].as<bool>();
		if (vm.count("ud"))
			s_render_upside_down = vm["ud"].as<bool>();
		
		if (vm.count("nsight"))
			s_nsight_mode = vm["nsight"].as<bool>();

		if(vm.count("force-interpreter"))
			s_force_interpreter = vm["force-interpreter"].as<bool>();
		
		if (vm.count("enable-gdbstub"))
			s_enable_gdbstub = vm["enable-gdbstub"].as<bool>();

		std::wstring extract_path, log_path;
		std::string output_path;
		if (vm.count("extract"))
			extract_path = vm["extract"].as<std::wstring>();
		if (vm.count("path"))
			output_path = vm["path"].as<std::string>();
		if (vm.count("output"))
			log_path = vm["output"].as<std::wstring>();

		if(!extract_path.empty())
		{
			ExtractorTool(extract_path, output_path, log_path);
			return false;
		}

		return true;
	}
	catch (const std::exception& ex)
	{
		std::string errorMsg;
		errorMsg.append("Error while trying to parse command line parameter:\n");
		errorMsg.append(ex.what());
		wxMessageBox(errorMsg, "Parameter error", wxICON_ERROR);
		return false;
	}
	
}

bool LaunchSettings::ExtractorTool(std::wstring_view wud_path, std::string_view output_path, std::wstring_view log_path)
{
	// extracting requires path of file
	if (output_path.empty())
	{
		requireConsole();
		puts("Cannot extract files because no source path was specified (-p)\n");
		return false;
	}
	// mount wud
	AES128_init();
	FSTVolume* srcVolume = FSTVolume::OpenFromDiscImage(fs::path(wud_path));
	if (!srcVolume)
	{
		requireConsole();
		puts(fmt::format("Unable to open \"%s\"\n", fs::path(wud_path).generic_string()).c_str());
		return false;
	}
	bool fileFound = false;
	std::vector<uint8> fileData = srcVolume->ExtractFile(output_path, &fileFound);
	delete srcVolume;
	if (!fileFound)
	{
		requireConsole();
		puts(fmt::format("Unable to read file \"%s\"\n", output_path).c_str());
		return false;
	}
	// output on console (if no output path was specified)
	if (!log_path.empty())
	{
		try
		{
			fs::path filename(std::wstring{ log_path });

			filename /= boost::nowide::widen(std::string(output_path));
			
			fs::path p = filename;
			p.remove_filename();
			
			fs::create_directories(p);
			std::ofstream file(filename, std::ios::out | std::ios::binary);
			file.write((const char*)fileData.data(), fileData.size());
			file.flush();
			file.close();
		}
		catch (const std::exception& ex)
		{
			cemuLog_log(LogType::Force, "can't write file: {}", ex.what());
			puts(fmt::format("can't write file: %s\n", ex.what()).c_str());
		}
	}
	else
	{
		// output to console
		requireConsole();
		printf("%.*s", (int)fileData.size(), fileData.data());
		fflush(stdout);
	}
	
	return true;
}
