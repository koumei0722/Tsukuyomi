#include "console/console.h"
#include "modules/AutoTool.h"
#include <fstream>
#include <nlohmann/json.hpp>

// AutoTool用の設定ファイルの絶対パスを取得します。
// 実装理由：キーバインド設定等から独立した専用の設定ファイルとしてロード/セーブを行うため。
static std::wstring GetAutoToolConfigPath() {
    return GetConfigDir() + L"\\AutoTool.json";
}

// AutoToolの設定状態をJSONに保存します。
// 実装理由：メニューからの設定変更時に、状態（有効/無効）をローカルに永続化するため。
void SaveAutoToolConfig() {
    std::wstring folderPath = GetConfigDir();
    CreateDirectoryW(folderPath.c_str(), NULL);

    nlohmann::json j;
    j["enabled"] = GetAutoToolEnabled();

    std::ofstream file(GetAutoToolConfigPath());
    if (file.is_open()) {
        file << j.dump(4);
        file.close();
    }
}

// AutoToolの設定状態をJSONからロードします。
// 実装理由：起動時に前回のON/OFF設定を引き継ぐため。
void LoadAutoToolConfig() {
    std::wstring filePath = GetAutoToolConfigPath();
    std::ifstream file(filePath);
    if (file.is_open()) {
        try {
            nlohmann::json j;
            file >> j;
            if (j.contains("enabled") && j["enabled"].is_boolean()) {
                SetAutoToolEnabled(j["enabled"].get<bool>());
            }
            file.close();
        } catch (...) {}
    }
}

// AutoTool用のトグル表示名（ON/OFF）を取得します。
// 実装理由：メニュー画面に現在のトグル状態を [ON] / [OFF] 形式で表示するため。
std::wstring GetAutoToolMenuName() {
    return GetAutoToolEnabled() ? L"Toggle: [ON]" : L"Toggle: [OFF]";
}
