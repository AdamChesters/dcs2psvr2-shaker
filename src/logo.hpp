#pragma once
#include <d3d11.h>
#include <string>
#include <utility>
#include <vector>
namespace shaker {
ID3D11ShaderResourceView* createLogoTexture(ID3D11Device* device);
// Support panel social button logos, keyed by content.json socials[].icon.
std::vector<std::pair<std::string,ID3D11ShaderResourceView*>> createSocialTextures(ID3D11Device* device);
}
