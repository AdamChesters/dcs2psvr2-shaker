#pragma once
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <string>
#include <vector>

namespace shaker {
namespace fs=std::filesystem;
enum class Presence { Unknown, Absent, Present };
struct SystemStatus {
    Presence headset=Presence::Unknown,steamVR=Presence::Unknown,dcs=Presence::Unknown;
};
// Read-only OS inventory. Does not open a headset or initialize SteamVR.
SystemStatus readSystemStatus();
std::string utf8(const std::wstring& text);
std::wstring wide(const std::string& text);
fs::path appDirectory();
fs::path dataDirectory();
std::string readText(const fs::path& file);
void writeTextAtomic(const fs::path& file,const std::string& text);
std::string sha256(const fs::path& file);
std::vector<fs::path> dcsProfiles();
fs::path dcsInstall();
fs::path toolkitFile();
bool spawnHidden(const std::wstring& args,HANDLE* process=nullptr);
void openPath(const fs::path& path);
}
