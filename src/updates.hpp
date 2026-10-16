#pragma once
#include "core.hpp"
#include "platform.hpp"
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>

namespace shaker {
inline constexpr char releasesUrl[] = "https://github.com/AdamChesters/dcs2psvr2-shaker/releases";
inline constexpr char releasesApi[] = "https://api.github.com/repos/AdamChesters/dcs2psvr2-shaker/releases?per_page=100";
struct Release {
    std::string tag,version,page,installerUrl,installerName,digest;
    uint64_t size=0;
    bool installable()const;
};
// SemVer precedence, including Alpha/Beta releases. Invalid versions are rejected.
int compareVersions(const std::string& left,const std::string& right);
bool shouldShowChangelog(const std::string& lastShown,const std::string& current,bool existingSettings);
std::string latestChangelog(const std::string& markdown);
std::optional<Release> newestRelease(const Json& releases);
bool allowedUpdateUrl(const std::string& url,bool assetRedirect=false);
void verifyInstaller(const fs::path& path,const Release& release);
using DownloadProgress=std::function<void(uint64_t)>;
Release checkReleases(std::stop_token stop={});
fs::path downloadInstaller(const Release& release,const fs::path& directory,std::stop_token stop={},DownloadProgress progress={});
bool launchInstaller(const fs::path& path,const fs::path& destination);

enum class UpdateState {Checking,Current,Available,Downloading,Ready,Failed};
struct UpdateStatus {
    UpdateState state=UpdateState::Checking;
    std::optional<Release> release;
    std::string message;
    fs::path installer;
    uint64_t downloaded=0;
};
class UpdateClient {
    mutable std::mutex mutex_;
    UpdateStatus status_;
    std::jthread worker_;
    void fail(const std::string& message);
public:
    UpdateClient();
    ~UpdateClient();
    UpdateStatus status()const;
    void check();
    void download();
    void cancel();
};
}
