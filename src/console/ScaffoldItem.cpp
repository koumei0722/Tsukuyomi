#include "console/console.h"
#include "modules/Scaffold.h"
#include <fstream>
#include <nlohmann/json.hpp>

// Scaffold用の設定ファイルの絶対パスを取得します。
// 実装理由：他の設定ファイルから独立した専用のJSONファイルとして保存するため。
static std::wstring GetScaffoldConfigPath() {
    return GetConfigDir() + L"\\scaffold.json";
}

// Scaffoldの設定状態をJSONに保存します。
// 実装理由：ON/OFF状態は保存しないため、この処理では何も行いません（FreeCameraと同様）。
void SaveScaffoldConfig() {
}

// Scaffoldの設定状態をJSONからロードします。
// 実装理由：起動時は常に無効状態（OFF）で初期化するため、明示的に有効化フラグをfalseに設定します。
void LoadScaffoldConfig() {
    SetScaffoldEnabled(false);
}

// Scaffold用のトグル表示名（ON/OFF）を取得します。
// 実装理由：現在の有効状態をメニュー上で視覚的にわかりやすく（[ON]/[OFF]）表示するため。
std::wstring GetScaffoldMenuName() {
    return GetScaffoldEnabled() ? L"Toggle: [ON]" : L"Toggle: [OFF]";
}

// Scaffold用のキーバインド表示文字列を構築します。
// 実装理由：コンソール画面に現在のScaffoldキーバインドの割り当て状態を表示するため。
std::wstring GetScaffoldKeyMenuName() {
    if (g_scaffoldKeys.empty()) return L"Key: None";
    std::wstring name = L"Key: ";
    for (size_t i = 0; i < g_scaffoldKeys.size(); ++i) {
        if (i > 0) name += L" + ";
        name += GetKeyName(g_scaffoldKeys[i]);
    }
    return name;
}

// Scaffold用のトグルキーバインド確定処理を行います。
// 実装理由：キー設定待受完了時、Scaffold用のキー配列に登録し、キーバインドJSONを永続化するため。
void BindScaffoldKeys(const std::vector<int>& vks) {
    g_scaffoldKeys = vks;
    SaveKeybinds(g_openConsoleKeys);
    g_isWaitingForKeyBind = false;
    g_lastBindTime = GetTickCount(); // チャタリング防止用クールダウン設定
    
    if (g_scaffoldKeyNode) {
        g_scaffoldKeyNode->name = GetScaffoldKeyMenuName();
    }
    
    std::wstring keyNames = L"";
    for (size_t i = 0; i < vks.size(); ++i) {
        if (i > 0) keyNames += L" + ";
        keyNames += GetKeyName(vks[i]);
    }
    
    AddLog(L"[Tsukuyomi] Scaffold toggle key changed to: " + keyNames);
    UpdateMenuDisplay();
}
