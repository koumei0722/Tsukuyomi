#include "console/console.h"
#include "modules/SchematicControl.h"
#include <fstream>
#include <nlohmann/json.hpp>

// SchematicControl用の設定ファイルの絶対パスを取得します。
// 実装理由：回路操作モジュール専用の独立した設定JSONを管理するため。
static std::wstring GetSchematicControlConfigPath() {
    return GetConfigDir() + L"\\SchematicControl.json";
}

// SchematicControlの設定状態をJSONに保存します。
void SaveSchematicControlConfig() {
    std::wstring folderPath = GetConfigDir();
    CreateDirectoryW(folderPath.c_str(), NULL);

    nlohmann::json j;
    j["enabled"] = GetSchematicControlEnabled();

    std::ofstream file(GetSchematicControlConfigPath());
    if (file.is_open()) {
        file << j.dump(4);
        file.close();
    }
}

// SchematicControlの設定状態をJSONからロードします。
void LoadSchematicControlConfig() {
    std::wstring filePath = GetSchematicControlConfigPath();
    std::ifstream file(filePath);
    if (file.is_open()) {
        try {
            nlohmann::json j;
            file >> j;
            if (j.contains("enabled") && j["enabled"].is_boolean()) {
                SetSchematicControlEnabled(j["enabled"].get<bool>());
            }
            file.close();
        } catch (...) {}
    }
}

// SchematicControl用のトグル表示名（ON/OFF）を取得します。
std::wstring GetSchematicControlMenuName() {
    return GetSchematicControlEnabled() ? L"Toggle: [ON]" : L"Toggle: [OFF]";
}
