#include "Utils.h"
#include "config/ActiveSettings.h"
#include "Cemu/ncrypto/ncrypto.h"

void createCemuDirectories()
{
    std::wstring mlc = ActiveSettings::GetMlcPath().generic_wstring();

    // create sys/usr folder in mlc01
    try {
        const auto sysFolder = fs::path(mlc).append(L"sys");
        fs::create_directories(sysFolder);

        const auto usrFolder = fs::path(mlc).append(L"usr");
        fs::create_directories(usrFolder);
        fs::create_directories(fs::path(usrFolder).append("title/00050000"));  // base
        fs::create_directories(fs::path(usrFolder).append("title/0005000c"));  // dlc
        fs::create_directories(fs::path(usrFolder).append("title/0005000e"));  // update

        // Mii Maker save folders {0x500101004A000, 0x500101004A100, 0x500101004A200},
        fs::create_directories(fs::path(mlc).append(L"usr/save/00050010/1004a000/user/common/db"));
        fs::create_directories(fs::path(mlc).append(L"usr/save/00050010/1004a100/user/common/db"));
        fs::create_directories(fs::path(mlc).append(L"usr/save/00050010/1004a200/user/common/db"));

        // lang files
        auto langDir = fs::path(mlc).append(L"sys/title/0005001b/1005c000/content");
        fs::create_directories(langDir);

        auto langFile = fs::path(langDir).append("language.txt");
        if (!fs::exists(langFile)) {
            std::ofstream file(langFile);
            if (file.is_open()) {
                const char *langStrings[] = {"ja", "en", "fr", "de", "it", "es", "zh", "ko", "nl",
                                             "pt", "ru", "zh"};
                for (const char *lang: langStrings)
                    file << fmt::format(R"("{}",)", lang) << std::endl;

                file.flush();
                file.close();
            }
        }

        auto countryFile = fs::path(langDir).append("country.txt");
        if (!fs::exists(countryFile)) {
            std::ofstream file(countryFile);
            for (sint32 i = 0; i < 201; i++) {
                const char *countryCode = NCrypto::GetCountryAsString(i);
                if (boost::iequals(countryCode, "NN"))
                    file << "NULL," << std::endl;
                else
                    file << fmt::format(R"("{}",)", countryCode) << std::endl;
            }
            file.flush();
            file.close();
        }
    }
    catch (const std::exception &ex) {
        // std::stringstream errorMsg;
        // errorMsg << fmt::format(fmt::runtime(_("Couldn't create a required mlc01 subfolder or file!\n\nError: {0}\nTarget path:\n{1}").ToStdString()), ex.what(), boost::nowide::narrow(mlc));
        exit(0);
    }

    // cemu directories
    try {
        const auto controllerProfileFolder = ActiveSettings::GetConfigPath(
                L"controllerProfiles").generic_wstring();
        if (!fs::exists(controllerProfileFolder))
            fs::create_directories(controllerProfileFolder);

        const auto memorySearcherFolder = ActiveSettings::GetUserDataPath(
                L"memorySearcher").generic_wstring();
        if (!fs::exists(memorySearcherFolder))
            fs::create_directories(memorySearcherFolder);
    }
    catch (const std::exception &ex) {
        std::stringstream errorMsg;
        // errorMsg << fmt::format(fmt::runtime(_("Couldn't create a required cemu directory or file!\n\nError: {0}").ToStdString()), ex.what());
        exit(0);
    }
}