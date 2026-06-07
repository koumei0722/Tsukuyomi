#include "console/console.h"
#include "modules/creativeNoClip.h"
#include <fstream>
#include <nlohmann/json.hpp>

// creativeNoClip用の設定ファイルの絶対パスを取得します。
// 実装理由：他の設定ファイルから独立した専用のJSONファイルとして保存するため。
static std::wstring GetCreativeNoClipConfigPath() {
    return GetConfigDir() + L"\\creativeNoClip.json";
}

// creativeNoClipの設定状態をJSONに保存します。
void SaveCreativeNoClipConfig() {
    std::wstring folderPath = GetConfigDir();
    CreateDirectoryW(folderPath.c_str(), NULL);

    nlohmann::json j;
    j["enabled"] = GetCreativeNoClipEnabled();

    std::ofstream file(GetCreativeNoClipConfigPath());
    if (file.is_open()) {
        file << j.dump(4);
        file.close();
    }
}

// creativeNoClipの設定状態をJSONからロードします。
void LoadCreativeNoClipConfig() {
    std::wstring filePath = GetCreativeNoClipConfigPath();
    std::ifstream file(filePath);
    if (file.is_open()) {
        try {
            nlohmann::json j;
            file >> j;
            if (j.contains("enabled") && j["enabled"].is_boolean()) {
                SetCreativeNoClipEnabled(j["enabled"].get<bool>());
            }
            file.close();
        } catch (...) {}
    }
}

// creativeNoClip用のトグル表示名（ON/OFF）を取得します。
std::wstring GetCreativeNoClipMenuName() {
    return GetCreativeNoClipEnabled() ? L"Toggle: [ON]" : L"Toggle: [OFF]";
}
