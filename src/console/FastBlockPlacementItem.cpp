#include "console/console.h"
#include "modules/FastBlockPlacement.h"
#include <fstream>
#include <nlohmann/json.hpp>

// FastBlockPlacement用の設定ファイルの絶対パスを取得します。
// 実装理由：自動設置モジュール用の個別設定JSONファイルを管理するため。
static std::wstring GetFastBlockPlacementConfigPath() {
    return GetConfigDir() + L"\\FastBlockPlacement.json";
}

// FastBlockPlacementの設定状態をJSONに保存します。
// 実装理由：トグル状態（ON/OFF）とディレイ設定は永続化しないため、軸固定モード、設置可能距離のみを保存します（FreeCameraと同様）。
void SaveFastBlockPlacementConfig() {
    std::wstring folderPath = GetConfigDir();
    CreateDirectoryW(folderPath.c_str(), NULL);

    nlohmann::json j;
    j["mode"] = static_cast<int>(GetFastPlacementMode());
    j["distance"] = GetFastPlacementDistance();

    std::ofstream file(GetFastBlockPlacementConfigPath());
    if (file.is_open()) {
        file << j.dump(4);
        file.close();
    }
}

// FastBlockPlacementの設定状態をJSONからロードします。
// 実装理由：ゲーム起動時に、以前保存された軸固定モード、設置可能距離を復元し、
// 有効化フラグは常に無効状態（OFF）で初期化するため。
void LoadFastBlockPlacementConfig() {
    SetFastBlockPlacementEnabled(false);

    std::wstring filePath = GetFastBlockPlacementConfigPath();
    std::ifstream file(filePath);
    if (file.is_open()) {
        try {
            nlohmann::json j;
            file >> j;
            if (j.contains("mode") && j["mode"].is_number_integer()) {
                int modeVal = j["mode"].get<int>();
                if (modeVal >= 0 && modeVal <= 2) {
                    SetFastPlacementMode(static_cast<PlacementMode>(modeVal));
                }
            }
            if (j.contains("distance") && j["distance"].is_number_integer()) {
                SetFastPlacementDistance(j["distance"].get<int>());
            }
            file.close();
        } catch (...) {}
    }
}

// FastBlockPlacement用のトグル表示名（ON/OFF）を取得します。
std::wstring GetFastBlockPlacementMenuName() {
    return GetFastBlockPlacementEnabled() ? L"Toggle: [ON]" : L"Toggle: [OFF]";
}

// FastBlockPlacementの設置モードメニュー表示名を取得します。
// 実装理由：手動軸固定のどの設定（X, Y, Z）になっているかをメニューUIに表示するため。
std::wstring GetFastPlacementModeMenuName() {
    PlacementMode mode = GetFastPlacementMode();
    switch (mode) {
    case MODE_X_AXIS: return L"Mode: X-Axis";
    case MODE_Y_AXIS: return L"Mode: Y-Axis";
    case MODE_Z_AXIS: return L"Mode: Z-Axis";
    default:          return L"Mode: Y-Axis";
    }
}

// FastBlockPlacementの設置可能距離メニュー表示名を取得します。
// 実装理由：設定されている現在の設置可能距離（1〜5ブロック先）をメニューUIに表示するため。
std::wstring GetFastPlacementDistanceMenuName() {
    wchar_t buf[64];
    swprintf_s(buf, L"Range: %d blocks", GetFastPlacementDistance());
    return buf;
}

// FastBlockPlacement用のキーバインド表示文字列を構築します。
// 実装理由：コンソール画面に現在のFastBlockPlacementキーバインドの割り当て状態を表示するため。
std::wstring GetFastBlockPlacementKeyMenuName() {
    if (g_fastBlockPlacementKeys.empty()) return L"Key: None";
    std::wstring name = L"Key: ";
    for (size_t i = 0; i < g_fastBlockPlacementKeys.size(); ++i) {
        if (i > 0) name += L" + ";
        name += GetKeyName(g_fastBlockPlacementKeys[i]);
    }
    return name;
}

// FastBlockPlacement用のトグルキーバインド確定処理を行います。
// 実装理由：キー設定待受完了時、FastBlockPlacement用のキー配列に登録し、キーバインドJSONを永続化するため。
void BindFastBlockPlacementKeys(const std::vector<int>& vks) {
    g_fastBlockPlacementKeys = vks;
    SaveKeybinds(g_openConsoleKeys);
    g_isWaitingForKeyBind = false;
    g_lastBindTime = GetTickCount(); // チャタリング防止用クールダウン設定
    
    if (g_fastBlockPlacementKeyNode) {
        g_fastBlockPlacementKeyNode->name = GetFastBlockPlacementKeyMenuName();
    }
    
    std::wstring keyNames = L"";
    for (size_t i = 0; i < vks.size(); ++i) {
        if (i > 0) keyNames += L" + ";
        keyNames += GetKeyName(vks[i]);
    }
    
    AddLog(L"[Tsukuyomi] FastBlockPlacement toggle key changed to: " + keyNames);
    UpdateMenuDisplay();
}
