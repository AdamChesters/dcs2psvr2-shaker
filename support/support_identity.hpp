#pragma once
#include <functional>
#include <string>
namespace shaker {
struct SupportIdentity {
    std::string appName;
    void* appLogo;
    std::string appVersion;
    std::string appId;
    std::function<void()> checkForUpdates;
    std::function<void*(const std::string& icon)> socialIcon; // texture for a socials[].icon asset
};
}
