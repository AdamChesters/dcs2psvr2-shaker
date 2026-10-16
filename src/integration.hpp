#pragma once
#include "platform.hpp"
namespace shaker {
bool integrationInstalled(const fs::path& profile);
void installIntegration(const fs::path& profile,const fs::path& source);
void removeIntegration(const fs::path& profile);
std::string withoutHook(const std::string& original);
}
