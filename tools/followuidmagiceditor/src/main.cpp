// Usage: followuidmagiceditor 魔法特效_大火球 /home/anhong/mir2x/client/bin/Res/Texture/magic.zsdb

#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "dbcomid.hpp"
#include "fflerror.hpp"
#include "mainwindow.hpp"

namespace
{
    int runEditor(const std::string &magicName, const std::string &magicDBPath)
    {
        const auto magicID = DBCOM_MAGICID(magicName.c_str());
        const auto &record = DBCOM_MAGICRECORD(magicID);
        fflassert(record);

        MainWindow app(magicID, magicDBPath.c_str());
        return app.run();
    }

#ifdef _WIN32
    std::string toUTF8(const wchar_t *text)
    {
        if(!text){
            return {};
        }
        const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
        std::string result(size > 0 ? size : 0, '\0');
        if(size > 0){
            WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), size, nullptr, nullptr);
            result.pop_back();
        }
        return result;
    }
#endif
}

#ifdef _WIN32
int wmain(int argc, wchar_t *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    try{
        if(argc == 1){
            const auto dbPath = std::filesystem::path(argv[0]).parent_path() / "res" / "texture" / "magic.zsdb";
            return runEditor(reinterpret_cast<const char *>(u8"魔法特效_大火球"), dbPath.string());
        }
        fflassert(argc == 3);
#ifdef _WIN32
        return runEditor(toUTF8(argv[1]), toUTF8(argv[2]));
#else
        return runEditor(argv[1], argv[2]);
#endif
    }
    catch(const std::exception &e){
        std::cerr << "followuidmagiceditor: " << e.what() << std::endl;
        return 1;
    }
}
