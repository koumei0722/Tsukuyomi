#include "console/console.h"
#include <fstream>
#include <algorithm>
#include <nlohmann/json.hpp>

// キーバインド設定ファイルの絶対パスを取得します。
// 実装理由：設定データ（Keybinds.json）を共通設定ディレクトリに保存・取得するため。
static std::wstring GetKeybindConfigPath() {
    return GetConfigDir() + L"\\Keybinds.json";
}

// FreeCamera設定ファイルの絶対パスを取得します。
// 実装理由：FreeCameraの速度設定JSON（FreeCamera.json）をバインドセーブ時に合わせて保存するため。
static std::wstring GetFreeCameraConfigPath() {
    return GetConfigDir() + L"\\FreeCamera.json";
}

// 仮想キーコードを人間が読める文字列に変換します。
// 実装理由：コンソール上での現在キー割り当て表示やホットキー変更時のUIテキスト生成のため。
std::wstring GetKeyName(int vk) {
    switch (vk) {
    case VK_SPACE: return L"SPACE";
    case VK_RETURN: return L"ENTER";
    case VK_ESCAPE: return L"ESC";
    case VK_INSERT: return L"INSERT";
    case VK_DELETE: return L"DELETE";
    case VK_END: return L"END";
    case VK_HOME: return L"HOME";
    case VK_PRIOR: return L"PAGE UP";
    case VK_NEXT: return L"PAGE DOWN";
    case VK_UP: return L"UP";
    case VK_DOWN: return L"DOWN";
    case VK_LEFT: return L"LEFT";
    case VK_RIGHT: return L"RIGHT";
    case VK_LCONTROL: case VK_RCONTROL: case VK_CONTROL: return L"CTRL";
    case VK_LSHIFT: case VK_RSHIFT: case VK_SHIFT: return L"SHIFT";
    case VK_LMENU: case VK_RMENU: case VK_MENU: return L"ALT";
    }

    if (vk >= VK_F1 && vk <= VK_F24) {
        return L"F" + std::to_wstring(vk - VK_F1 + 1);
    }

    wchar_t name[128] = { 0 };
    UINT scanCode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    LONG lParamValue = scanCode << 16;

    switch (vk) {
    case VK_INSERT: case VK_DELETE: case VK_HOME: case VK_END:
    case VK_PRIOR: case VK_NEXT: case VK_UP: case VK_DOWN:
    case VK_LEFT: case VK_RIGHT: case VK_RMENU: case VK_RCONTROL:
        lParamValue |= 0x01000000;
        break;
    }

    if (GetKeyNameTextW(lParamValue, name, 128) > 0) {
        return name;
    }

    if (vk >= 'A' && vk <= 'Z') {
        return std::wstring(1, (wchar_t)vk);
    }
    if (vk >= '0' && vk <= '9') {
        return std::wstring(1, (wchar_t)vk);
    }

    return L"VK_" + std::to_wstring(vk);
}

// 現在のキーバインド設定から表示用の文字列を構築します。
std::wstring GetKeybindMenuName() {
    if (g_openConsoleKeys.empty()) return L"Key: None";
    std::wstring name = L"Key: ";
    for (size_t i = 0; i < g_openConsoleKeys.size(); ++i) {
        if (i > 0) name += L" + ";
        name += GetKeyName(g_openConsoleKeys[i]);
    }
    return name;
}

// 複数キーコンボを新キーバインドとして登録・保存します。
void BindNewKeys(const std::vector<int>& vks) {
    g_openConsoleKeys = vks;
    SaveKeybinds(vks);
    g_isWaitingForKeyBind = false;
    g_lastBindTime = GetTickCount(); // チャタリング防止クールダウン
    
    if (g_keyBindNode) {
        g_keyBindNode->name = GetKeybindMenuName();
    }
    
    std::wstring keyNames = L"";
    for (size_t i = 0; i < vks.size(); ++i) {
        if (i > 0) keyNames += L" + ";
        keyNames += GetKeyName(vks[i]);
    }
    
    AddLog(L"[Tsukuyomi] Console keybind changed to: " + keyNames);
    UpdateMenuDisplay();
}

// キーバインド変更を適用せずに設定をキャンセルします。
void CancelKeyBind() {
    g_isWaitingForKeyBind = false;
    g_lastBindTime = GetTickCount(); // チャタリング防止クールダウン
    UpdateMenuDisplay();
}

// 全てのキーバインド設定をJSON形式で保存します。
// 実装理由：コンソール、FreeCamera、FastBlockPlacement、およびScaffoldのホットキーバインド設定と
// FreeCameraの速度設定を一貫して保存するため。
void SaveKeybinds(const std::vector<int>& vks) {
    std::wstring folderPath = GetConfigDir();
    CreateDirectoryW(folderPath.c_str(), NULL);

    // 1. Keybinds.json の保存
    nlohmann::json kbJson;
    kbJson["console"] = vks;
    kbJson["freecamera"] = g_freeCameraKeys;
    kbJson["fastblockplacement"] = g_fastBlockPlacementKeys;
    kbJson["scaffold"] = g_scaffoldKeys;

    std::ofstream kbFile(GetKeybindConfigPath());
    if (kbFile.is_open()) {
        kbFile << kbJson.dump(4);
        kbFile.close();
    }

    // 2. FreeCamera.json の保存
    extern float GetFreeCameraSpeed();
    nlohmann::json fcJson;
    fcJson["speed"] = GetFreeCameraSpeed();

    std::ofstream fcFile(GetFreeCameraConfigPath());
    if (fcFile.is_open()) {
        fcFile << fcJson.dump(4);
        fcFile.close();
    }
}

// 全てのキーバインド設定をロードします。
// 実装理由：起動時に前回のバインド設定（古い形式からの移行処理含む）を復元して適用するため。
void LoadKeybinds() {
    std::wstring kbPath = GetKeybindConfigPath();
    std::ifstream kbFile(kbPath);
    bool needsMigration = false;

    if (kbFile.is_open()) {
        try {
            nlohmann::json j;
            kbFile >> j;

            // 互換性マイグレーション：古いキー名（"open_console_keys"等）がある場合
            if (j.contains("open_console_keys") || j.contains("freecamera_toggle_keys") || j.contains("freecamera_speed")) {
                needsMigration = true;
                if (j.contains("open_console_keys") && j["open_console_keys"].is_array()) {
                    g_openConsoleKeys = j["open_console_keys"].get<std::vector<int>>();
                }
                if (j.contains("freecamera_toggle_keys") && j["freecamera_toggle_keys"].is_array()) {
                    g_freeCameraKeys = j["freecamera_toggle_keys"].get<std::vector<int>>();
                }
                if (j.contains("freecamera_speed") && j["freecamera_speed"].is_number()) {
                    extern void SetFreeCameraSpeed(float);
                    SetFreeCameraSpeed(j["freecamera_speed"].get<float>());
                }
            } else {
                // 新しいキー名のロード
                if (j.contains("console") && j["console"].is_array()) {
                    g_openConsoleKeys = j["console"].get<std::vector<int>>();
                }
                if (j.contains("freecamera") && j["freecamera"].is_array()) {
                    g_freeCameraKeys = j["freecamera"].get<std::vector<int>>();
                }
                if (j.contains("fastblockplacement") && j["fastblockplacement"].is_array()) {
                    g_fastBlockPlacementKeys = j["fastblockplacement"].get<std::vector<int>>();
                }
                if (j.contains("scaffold") && j["scaffold"].is_array()) {
                    g_scaffoldKeys = j["scaffold"].get<std::vector<int>>();
                }
            }
            kbFile.close();
        } catch (...) {}
    }

    // FreeCamera用のロード
    if (!needsMigration) {
        std::wstring fcPath = GetFreeCameraConfigPath();
        std::ifstream fcFile(fcPath);
        if (fcFile.is_open()) {
            try {
                nlohmann::json j;
                fcFile >> j;
                if (j.contains("speed") && j["speed"].is_number()) {
                    extern void SetFreeCameraSpeed(float);
                    SetFreeCameraSpeed(j["speed"].get<float>());
                }
                fcFile.close();
            } catch (...) {}
        }
    }

    if (needsMigration) {
        SaveKeybinds(g_openConsoleKeys);
    }
}
