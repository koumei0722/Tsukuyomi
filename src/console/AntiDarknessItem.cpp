#include "console/console.h"
#include "modules/AntiDarkness.h"
#include <fstream>
#include <nlohmann/json.hpp>

// AntiDarkness用の設定ファイルの絶対パスを取得します。
// 実装理由：キーバインド設定等から独立した専用の設定ファイルとしてロード/セーブを行うため。
static std::wstring GetAntiDarknessConfigPath() {
    return GetConfigDir() + L"\\AntiDarkness.json";
}

// AntiDarknessの設定状態をJSONに保存します。
void SaveAntiDarknessConfig() {
    std::wstring folderPath = GetConfigDir();
    CreateDirectoryW(folderPath.c_str(), NULL);

    nlohmann::json j;
    j["enabled"] = GetAntiDarknessEnabled();

    std::ofstream file(GetAntiDarknessConfigPath());
    if (file.is_open()) {
        file << j.dump(4);
        file.close();
    }
}

// AntiDarknessの設定状態をJSONからロードします。
void LoadAntiDarknessConfig() {
    std::wstring filePath = GetAntiDarknessConfigPath();
    std::ifstream file(filePath);
    if (file.is_open()) {
        try {
            nlohmann::json j;
            file >> j;
            if (j.contains("enabled") && j["enabled"].is_boolean()) {
                SetAntiDarknessEnabled(j["enabled"].get<bool>());
            }
            file.close();
        } catch (...) {}
    }
}

// AntiDarkness用のトグル表示名（ON/OFF）を取得します。
std::wstring GetAntiDarknessMenuName() {
    return GetAntiDarknessEnabled() ? L"Toggle: [ON]" : L"Toggle: [OFF]";
}
