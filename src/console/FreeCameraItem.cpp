#include "console/console.h"
#include "modules/FreeCamera.h"

// FreeCameraのトグル表示名（ON/OFF）を取得します。
// 実装理由：現在の有効状態をメニュー上で視覚的にわかりやすく（[ON]/[OFF]）表示するため。
std::wstring GetFreeCameraMenuName() {
    return GetFreeCameraEnabled() ? L"Toggle: [ON]" : L"Toggle: [OFF]";
}

// FreeCamera用のキーバインド表示文字列を構築します。
std::wstring GetFreeCameraKeyMenuName() {
    if (g_freeCameraKeys.empty()) return L"Key: None";
    std::wstring name = L"Key: ";
    for (size_t i = 0; i < g_freeCameraKeys.size(); ++i) {
        if (i > 0) name += L" + ";
        name += GetKeyName(g_freeCameraKeys[i]);
    }
    return name;
}

// FreeCamera用の移動速度の表示文字列を構築します。
std::wstring GetFreeCameraSpeedMenuName() {
    wchar_t buf[64];
    swprintf_s(buf, L"Speed: %.5f", GetFreeCameraSpeed());
    return buf;
}

// FreeCamera用のトグルキーバインド確定処理を行います。
// 実装理由：キー設定待受完了時、FreeCamera用のキー配列に登録し、設定JSONを永続化するため。
void BindFreeCameraKeys(const std::vector<int>& vks) {
    g_freeCameraKeys = vks;
    SaveKeybinds(g_openConsoleKeys);
    g_isWaitingForKeyBind = false;
    g_lastBindTime = GetTickCount(); // クールダウン設定
    
    if (g_freeCameraKeyNode) {
        g_freeCameraKeyNode->name = GetFreeCameraKeyMenuName();
    }
    
    std::wstring keyNames = L"";
    for (size_t i = 0; i < vks.size(); ++i) {
        if (i > 0) keyNames += L" + ";
        keyNames += GetKeyName(vks[i]);
    }
    
    AddLog(L"[Tsukuyomi] FreeCamera toggle key changed to: " + keyNames);
    UpdateMenuDisplay();
}
