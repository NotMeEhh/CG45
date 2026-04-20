#include "GameAudio.h"
#include "GamePaths.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

namespace game {

    namespace {

        void LogUtf8Path(const char* prefix, const std::filesystem::path& p)
        {
            const std::wstring w = p.wstring();
            if (w.empty()) {
                std::cerr << prefix << '\n';
                return;
            }
            const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (n <= 0) {
                std::cerr << prefix << "(invalid path)\n";
                return;
            }
            std::string buf(static_cast<size_t>(n), '\0');
            WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, buf.data(), n, nullptr, nullptr);
            std::cerr << prefix << buf.c_str() << '\n';
        }

        std::filesystem::path ResolveBackgroundMusicPath()
        {
            wchar_t modulePath[MAX_PATH];
            if (!GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
                return {};

            const std::filesystem::path exeDir = std::filesystem::path(modulePath).parent_path();
            const std::filesystem::path projectMusicDir =
                exeDir.parent_path().parent_path() / L"music";

            std::vector<std::filesystem::path> candidates;
            if (!projectMusicDir.empty())
                candidates.push_back(projectMusicDir / paths::kBgMusicFile);
            candidates.push_back(exeDir / L"music" / paths::kBgMusicFile);
            try {
                candidates.push_back(std::filesystem::current_path() / L"music" / paths::kBgMusicFile);
            }
            catch (...) {
            }

            for (const auto& c : candidates) {
                std::error_code ec;
                if (std::filesystem::is_regular_file(c, ec))
                    return c;
            }
            return {};
        }
    }

    void BackgroundMusic::Start()
    {
        Stop();

        const std::filesystem::path mp3Path = ResolveBackgroundMusicPath();
        if (mp3Path.empty()) {
            std::cerr << "Background music not found. Tried (UTF-8):\n";
            wchar_t modulePath[MAX_PATH];
            if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH)) {
                const std::filesystem::path exeDir = std::filesystem::path(modulePath).parent_path();
                const auto projectMusicDir = exeDir.parent_path().parent_path() / L"music";
                if (!projectMusicDir.empty())
                    LogUtf8Path("  - ", projectMusicDir / paths::kBgMusicFile);
                LogUtf8Path("  - ", exeDir / L"music" / paths::kBgMusicFile);
                try {
                    LogUtf8Path("  - ", std::filesystem::current_path() / L"music" / paths::kBgMusicFile);
                }
                catch (...) {
                }
            }
            return;
        }

        std::error_code ec;
        const std::wstring fullPath = std::filesystem::absolute(mp3Path, ec).wstring();
        if (fullPath.empty()) {
            LogUtf8Path("Background music: empty absolute path for ", mp3Path);
            return;
        }

        std::wstring openCmd = L"open \"";
        openCmd += fullPath;
        openCmd += L"\" type mpegvideo alias bgm";
        if (mciSendStringW(openCmd.c_str(), nullptr, 0, nullptr) != 0) {
            std::cerr << "MCI: failed to open music file.\n";
            LogUtf8Path("  Path was: ", mp3Path);
            return;
        }
        playing_ = true;
        if (mciSendStringW(L"play bgm repeat", nullptr, 0, nullptr) != 0) {
            mciSendStringW(L"close bgm", nullptr, 0, nullptr);
            playing_ = false;
            std::cerr << "MCI: failed to start playback.\n";
        }
    }

    void BackgroundMusic::Stop()
    {
        if (!playing_)
            return;
        mciSendStringW(L"stop bgm", nullptr, 0, nullptr);
        mciSendStringW(L"close bgm", nullptr, 0, nullptr);
        playing_ = false;
    }
}
