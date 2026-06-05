#include "console.h"
#include <vector>
#include <fstream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>
#include "modules/FreeCamera.h"
#include "modules/creativeNoClip.h"
#include "modules/AntiDarkness.h"
#include "modules/SchematicControl.h"
#include <richedit.h>

#define WM_USER_ADD_LOG (WM_USER + 100)

HWND g_hMainWnd = NULL;
HWND g_hLogEdit = NULL;
HWND g_hMenuStatic = NULL;
HBRUSH g_hBlackBrush = NULL;

WNDPROC g_fnOldLogProc = NULL;

std::vector<int> g_openConsoleKeys = { 'Y' }; // 初期値は 'Y' キー
std::vector<int> g_freeCameraKeys = { VK_F5 };  // デフォルトのFreeCameraトグルキー
bool g_isWaitingForKeyBind = false;
KeyBindTarget g_keyBindTarget = KeyBindTarget::Console;

bool g_isWaitingForSpeedInput = false; // 速度の数値入力中か
std::wstring g_speedInputBuffer = L""; // 入力中の速度文字列
DWORD g_lastSpeedInputStartTime = 0;   // 速度入力開始時のタイムスタンプ。実装理由：メッセージキューの残存Enterキーによる即時確定を防止するため。
bool g_isFirstSpeedInputChar = false;  // 速度入力開始後, 最初の文字入力かどうか。実装理由：現在の速度を表示しつつ, 数字入力時に自動でクリアして上書きできるようにするため。

// キーバインド設定完了時のタイムスタンプを保持する変数。
// 実装理由：設定完了時にキーを離した瞬間の残存メッセージによるメニュー再選択の誤動作を防ぐため。
DWORD g_lastBindTime = 0;

struct MenuNode {
    std::wstring name;
    std::vector<MenuNode> children;
};

MenuNode g_menuTree;
std::vector<MenuNode*> g_menuStack;
int g_selectedIndex = 0;
std::vector<int> g_menuIndexStack; // メニュー階層移動時に元のカーソル位置を復帰させるためのインデックス履歴スタック。実装理由：「..」で親階層に戻った際、サブメニューに入る前のカーソル位置を正確に復元するため。

MenuNode* g_keyBindNode = nullptr;          // Consoleキーのメニューノードへの参照
MenuNode* g_freeCameraKeyNode = nullptr;    // FreeCameraキーのメニューノードへの参照
MenuNode* g_freeCameraSpeedNode = nullptr;  // FreeCamera速度のメニューノードへの参照
MenuNode* g_creativeNoClipNode = nullptr;   // creativeNoClipのメニューノードへの参照
MenuNode* g_antiDarknessNode = nullptr;     // AntiDarknessのメニューノードへの参照
MenuNode* g_schematicControlNode = nullptr; // SchematicControlのメニューノードへの参照

// 仮想キーコードのテキスト変換
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

// 現在のキーバインド設定から表示用の文字列を構築する関数
std::wstring GetKeybindMenuName() {
    if (g_openConsoleKeys.empty()) return L"Key: None";
    std::wstring name = L"Key: ";
    for (size_t i = 0; i < g_openConsoleKeys.size(); ++i) {
        if (i > 0) name += L" + ";
        name += GetKeyName(g_openConsoleKeys[i]);
    }
    return name;
}

// FreeCamera用のキーバインド表示文字列を構築する関数
std::wstring GetFreeCameraKeyMenuName() {
    if (g_freeCameraKeys.empty()) return L"Key: None";
    std::wstring name = L"Key: ";
    for (size_t i = 0; i < g_freeCameraKeys.size(); ++i) {
        if (i > 0) name += L" + ";
        name += GetKeyName(g_freeCameraKeys[i]);
    }
    return name;
}

std::wstring GetFreeCameraSpeedMenuName() {
    // 現在のFreeCameraの移動速度を文字列表記にして取得します。
    wchar_t buf[64];
    swprintf_s(buf, L"Speed: %.5f", GetFreeCameraSpeed());
    return buf;
}

std::wstring GetCreativeNoClipMenuName() {
    return GetCreativeNoClipEnabled() ? L"Toggle: [ON]" : L"Toggle: [OFF]";
}

std::wstring GetConfigDir() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring exeDir = exePath;
    size_t lastSlash = exeDir.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        exeDir = exeDir.substr(0, lastSlash);
    }
    return exeDir + L"\\Tsukuyomi";
}

std::wstring GetKeybindConfigPath() {
    return GetConfigDir() + L"\\Keybinds.json";
}

std::wstring GetFreeCameraConfigPath() {
    return GetConfigDir() + L"\\FreeCamera.json";
}

// creativeNoClip用の設定ファイルの保存先パスを取得します。
// 実装理由：キーバインド設定等とは独立した専用の設定ファイルとして保存するため。
std::wstring GetCreativeNoClipConfigPath() {
    return GetConfigDir() + L"\\creativeNoClip.json";
}

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

// AntiDarkness用の設定ファイルの保存先パスを取得します。
// 実装理由：キーバインド設定等とは独立した専用の設定ファイルとして保存するため。
std::wstring GetAntiDarknessConfigPath() {
    return GetConfigDir() + L"\\AntiDarkness.json";
}

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

// AntiDarkness用のメニュー項目名を取得します。
std::wstring GetAntiDarknessMenuName() {
    return GetAntiDarknessEnabled() ? L"Toggle: [ON]" : L"Toggle: [OFF]";
}

// SchematicControl用の設定ファイルの保存先パスを取得します。
// 実装理由：キーバインド設定等とは独立した専用の設定ファイルとして保存するため。
std::wstring GetSchematicControlConfigPath() {
    return GetConfigDir() + L"\\SchematicControl.json";
}

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

// SchematicControl用のメニュー項目名を取得します。
std::wstring GetSchematicControlMenuName() {
    return GetSchematicControlEnabled() ? L"Toggle: [ON]" : L"Toggle: [OFF]";
}

void SaveKeybinds(const std::vector<int>& vks) {
    std::wstring folderPath = GetConfigDir();
    CreateDirectoryW(folderPath.c_str(), NULL);

    // 1. Keybinds.json の保存
    // 実装理由：コンソールおよびFreeCameraのホットキーバインド設定を簡略化されたキー名で保存するため。
    nlohmann::json kbJson;
    kbJson["console"] = vks;
    kbJson["freecamera"] = g_freeCameraKeys;

    std::ofstream kbFile(GetKeybindConfigPath());
    if (kbFile.is_open()) {
        kbFile << kbJson.dump(4);
        kbFile.close();
    }

    // 2. FreeCamera.json の保存
    // 実装理由：FreeCameraの速度設定をキーバインド設定と分離し、単体のモジュール用設定として保存するため。
    nlohmann::json fcJson;
    fcJson["speed"] = GetFreeCameraSpeed();

    std::ofstream fcFile(GetFreeCameraConfigPath());
    if (fcFile.is_open()) {
        fcFile << fcJson.dump(4);
        fcFile.close();
    }
}

void LoadKeybinds() {
    std::wstring kbPath = GetKeybindConfigPath();
    std::ifstream kbFile(kbPath);
    bool needsMigration = false;

    if (kbFile.is_open()) {
        try {
            nlohmann::json j;
            kbFile >> j;

            // 互換性マイグレーション：古いキー名（"open_console_keys"等）がある場合
            // 実装理由：ユーザーの既存の設定を引き継ぎつつ、新しいフォーマットへ自動移行するため。
            if (j.contains("open_console_keys") || j.contains("freecamera_toggle_keys") || j.contains("freecamera_speed")) {
                needsMigration = true;
                if (j.contains("open_console_keys") && j["open_console_keys"].is_array()) {
                    g_openConsoleKeys = j["open_console_keys"].get<std::vector<int>>();
                }
                if (j.contains("freecamera_toggle_keys") && j["freecamera_toggle_keys"].is_array()) {
                    g_freeCameraKeys = j["freecamera_toggle_keys"].get<std::vector<int>>();
                }
                if (j.contains("freecamera_speed") && j["freecamera_speed"].is_number()) {
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
            }
            kbFile.close();
        } catch (...) {}
    }

    // 2. 新しい FreeCamera.json のロード (マイグレーションが行われない場合のみロード)
    if (!needsMigration) {
        std::wstring fcPath = GetFreeCameraConfigPath();
        std::ifstream fcFile(fcPath);
        if (fcFile.is_open()) {
            try {
                nlohmann::json j;
                fcFile >> j;
                if (j.contains("speed") && j["speed"].is_number()) {
                    SetFreeCameraSpeed(j["speed"].get<float>());
                }
                fcFile.close();
            } catch (...) {}
        }
    }

    // マイグレーションが必要だった場合、新しいファイル構成で保存し直して移行を完了します
    if (needsMigration) {
        SaveKeybinds(g_openConsoleKeys);
    }
}

void UpdateMenuDisplay() {
    if (!g_hMenuStatic) return;

    // 描画更新時の一時的なちらつき（フリッカー）を防止するため、描画制御を無効化します。
    // 実装理由：RichEditへのテキストクリアと複数回の挿入がリアルタイムで画面に描画されるのを防ぎ、一括で描画を更新するため。
    SendMessageW(g_hMenuStatic, WM_SETREDRAW, FALSE, 0);

    // 一旦全クリア
    SetWindowTextW(g_hMenuStatic, L"");

    CHARFORMAT2W cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;

    // ヘルパーラムダ式：テキストを末尾に指定の色で挿入する
    // 実装理由：行ごとに異なる色（ONなら緑、OFFなら赤、選択肢ならシアン等）を設定してRichEditへ順次追記するため。
    auto AppendText = [&](const std::wstring& str, COLORREF color) {
        int len = GetWindowTextLengthW(g_hMenuStatic);
        SendMessageW(g_hMenuStatic, EM_SETSEL, len, len);
        cf.crTextColor = color;
        SendMessageW(g_hMenuStatic, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        SendMessageW(g_hMenuStatic, EM_REPLACESEL, FALSE, (LPARAM)str.c_str());
    };

    if (g_isWaitingForKeyBind) {
        AppendText(L"\r\n  --- Keybind ---\r\n\r\n  [ Press any key to bind... ]", RGB(0, 255, 255));
        
        SendMessageW(g_hMenuStatic, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(g_hMenuStatic, NULL, TRUE);
        return;
    }

    if (g_isWaitingForSpeedInput) {
        AppendText(L"\r\n  --- Speed Input ---\r\n\r\n  Enter Speed: ", RGB(0, 255, 255));
        AppendText(g_speedInputBuffer + L"_", RGB(255, 255, 255)); // 入力中の数値は白で目立たせる
        
        SendMessageW(g_hMenuStatic, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(g_hMenuStatic, NULL, TRUE);
        return;
    }

    MenuNode* current = g_menuStack.back();

    if (current != &g_menuTree) {
        AppendText(L"--- " + current->name + L" ---\r\n", RGB(0, 255, 255));
    } else {
        AppendText(L"--- Main Menu ---\r\n", RGB(0, 255, 255));
    }

    for (size_t i = 0; i < current->children.size(); ++i) {
        std::wstring prefix = ((int)i == g_selectedIndex) ? L" >" : L"  ";
        std::wstring name = current->children[i].name;

        // デフォルトの色（シアン）
        COLORREF itemColor = RGB(0, 255, 255);

        // トグル状態に応じたカラーの適用
        // 実装理由：ON状態なら#00FF00(緑)、OFF状態なら#FF0000(赤)で視覚的に区別するため。
        if (current == &g_menuTree) {
            // Main Menuの項目についてのカラー判定
            if (name == L"FreeCamera") {
                itemColor = GetFreeCameraEnabled() ? RGB(0, 255, 0) : RGB(255, 0, 0);
            } else if (name == L"CreativeNoClip") {
                itemColor = GetCreativeNoClipEnabled() ? RGB(0, 255, 0) : RGB(255, 0, 0);
            } else if (name == L"AntiDarkness") {
                itemColor = GetAntiDarknessEnabled() ? RGB(0, 255, 0) : RGB(255, 0, 0);
            } else if (name == L"SchematicControl") {
                itemColor = GetSchematicControlEnabled() ? RGB(0, 255, 0) : RGB(255, 0, 0);
            }
        } else {
            // サブメニュー項目についてもON/OFFを緑/赤で着色
            if (name.find(L"[ON]") != std::wstring::npos) {
                itemColor = RGB(0, 255, 0);
            } else if (name.find(L"[OFF]") != std::wstring::npos) {
                itemColor = RGB(255, 0, 0);
            }
        }

        AppendText(prefix + name + L"\r\n", itemColor);
    }

    // 描画を再度有効化し、全体を再描画します
    SendMessageW(g_hMenuStatic, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_hMenuStatic, NULL, TRUE);
}

void InitMenu() {
    LoadKeybinds();
    LoadCreativeNoClipConfig(); // creativeNoClip設定のロード。実装理由：起動時に前回のON/OFF設定を引き継ぐため。
    LoadAntiDarknessConfig();   // AntiDarkness設定のロード。実装理由：起動時に前回のON/OFF設定を引き継ぐため。
    LoadSchematicControlConfig(); // SchematicControl設定のロード。実装理由：起動時に前回のON/OFF設定を引き継ぐため。

    g_menuTree = { L"ROOT", {
        { L"Console", { { L"..", {} }, { GetKeybindMenuName(), {} } } },
        { L"FreeCamera", { { L"..", {} }, { GetFreeCameraKeyMenuName(), {} }, { GetFreeCameraSpeedMenuName(), {} } } },
        { L"CreativeNoClip", { { L"..", {} }, { GetCreativeNoClipMenuName(), {} } } },
        { L"AntiDarkness", { { L"..", {} }, { GetAntiDarknessMenuName(), {} } } },
        { L"SchematicControl", { { L"..", {} }, { GetSchematicControlMenuName(), {} } } }
    }};
    
    // メインメニューの各項目名（Console, FreeCamera, CreativeNoClip, AntiDarkness, SchematicControl）をアルファベット昇順（A-Z）で並べ替えます。
    // 実装理由：ユーザー要求「Main Menuの項目の並びはアルファベット順になるようにして」を満たし、項目のアクセス順を整理するため。
    std::sort(g_menuTree.children.begin(), g_menuTree.children.end(), [](const MenuNode& a, const MenuNode& b) {
        return a.name < b.name;
    });

    g_menuStack.clear();
    g_menuStack.push_back(&g_menuTree);
    g_menuIndexStack.clear(); // 履歴スタックの初期化。実装理由：メニューリセット時に過去の不要な移動履歴をクリアするため。
    g_selectedIndex = 0;
    
    // ソートにより子要素 of g_menuTree.children の配列インデックスが動的に変化するため、名前（name）からマッチする項目を検索して参照ポインタをセットします。
    // 実装理由：決め打ちインデックス（[0]等）での参照取得によるバグを防ぎ、ソート順の変化に依存しない堅牢なバインド関係を保証するため。
    for (auto& child : g_menuTree.children) {
        if (child.name == L"Console") {
            g_keyBindNode = &child.children[1];
        } else if (child.name == L"FreeCamera") {
            g_freeCameraKeyNode = &child.children[1];
            g_freeCameraSpeedNode = &child.children[2];
        } else if (child.name == L"CreativeNoClip") {
            g_creativeNoClipNode = &child.children[1];
        } else if (child.name == L"AntiDarkness") {
            g_antiDarknessNode = &child.children[1];
        } else if (child.name == L"SchematicControl") {
            g_schematicControlNode = &child.children[1];
        }
    }
}

void MenuUp() {
    if (g_isWaitingForKeyBind || g_isWaitingForSpeedInput) return;
    MenuNode* current = g_menuStack.back();
    if (current->children.empty()) return;
    g_selectedIndex = (g_selectedIndex - 1 + current->children.size()) % current->children.size();
    UpdateMenuDisplay();
}

void MenuDown() {
    if (g_isWaitingForKeyBind || g_isWaitingForSpeedInput) return;
    MenuNode* current = g_menuStack.back();
    if (current->children.empty()) return;
    g_selectedIndex = (g_selectedIndex + 1) % current->children.size();
    UpdateMenuDisplay();
}

// トグル項目（CreativeNoClipやAntiDarkness）のON/OFF切り替えと設定の保存、およびログ出力を行います。
// 実装理由：サブメニューから選択された場合と、メインメニューから直接選択された場合のトグル処理を共通化し、コードの重複を防ぐため。
void ToggleMenuNode(const std::wstring& parentName, MenuNode* toggleNode) {
    if (parentName == L"CreativeNoClip") {
        bool nextState = !GetCreativeNoClipEnabled();
        SetCreativeNoClipEnabled(nextState);
        AddLog(nextState ? L"[CreativeNoClip] Enabled" : L"[CreativeNoClip] Disabled");
        if (toggleNode) {
            toggleNode->name = GetCreativeNoClipMenuName();
        }
        SaveCreativeNoClipConfig();
    } else if (parentName == L"AntiDarkness") {
        bool nextState = !GetAntiDarknessEnabled();
        SetAntiDarknessEnabled(nextState);
        AddLog(nextState ? L"[AntiDarkness] Enabled" : L"[AntiDarkness] Disabled");
        if (toggleNode) {
            toggleNode->name = GetAntiDarknessMenuName();
        }
        SaveAntiDarknessConfig();
    } else if (parentName == L"SchematicControl") {
        bool nextState = !GetSchematicControlEnabled();
        SetSchematicControlEnabled(nextState);
        AddLog(nextState ? L"[SchematicControl] Enabled" : L"[SchematicControl] Disabled");
        if (toggleNode) {
            toggleNode->name = GetSchematicControlMenuName();
        }
        SaveSchematicControlConfig();
    }
}

void MenuSelect() {
    if (g_isWaitingForKeyBind || g_isWaitingForSpeedInput) return;
    MenuNode* current = g_menuStack.back();
    if (g_selectedIndex < 0 || g_selectedIndex >= (int)current->children.size()) return;

    MenuNode* selected = &current->children[g_selectedIndex];

    if (selected->name == L"..") {
        if (g_menuStack.size() > 1 && !g_menuIndexStack.empty()) {
            g_menuStack.pop_back();
            g_selectedIndex = g_menuIndexStack.back(); // 遷移前のカーソル位置を復元
            g_menuIndexStack.pop_back();
        }
    } else if (selected->name.rfind(L"Key: ", 0) == 0) {
        // 決定キー（Space/Enter）が離されるのを待機して誤チャタリングバインドを防止
        while ((GetAsyncKeyState(VK_SPACE) & 0x8000) || (GetAsyncKeyState(VK_RETURN) & 0x8000)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // バインド対象の振り分け
        // 実装理由：Console用とFreeCamera用のキー設定項目で、同一の待受フラグを共有しつつ
        // バインド先のキー配列変数を正しく切り替えて書き込むため。
        if (current->name == L"Console") {
            g_keyBindTarget = KeyBindTarget::Console;
        } else {
            g_keyBindTarget = KeyBindTarget::FreeCamera;
        }
        g_isWaitingForKeyBind = true;

    } else if (selected->name.rfind(L"Speed: ", 0) == 0) {
        // Space / Enter が離されるまで待機してチャタリングを防止
        while ((GetAsyncKeyState(VK_SPACE) & 0x8000) || (GetAsyncKeyState(VK_RETURN) & 0x8000)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        g_isWaitingForSpeedInput = true;
        g_lastSpeedInputStartTime = GetTickCount(); // 入力開始時刻を記録。実装理由：メッセージキューの残存キー誤動作を防ぐため。
        g_isFirstSpeedInputChar = true;             // 最初の入力を待ち受ける状態にする。実装理由：数値キー入力時に現在の設定値をクリアするため。
        // 初期値として現在の速度をセットします
        wchar_t buf[64];
        swprintf_s(buf, L"%.5f", GetFreeCameraSpeed());
        g_speedInputBuffer = buf;
    } else if (selected->name.rfind(L"Toggle: ", 0) == 0) {
        ToggleMenuNode(current->name, selected);

    } else if (!selected->children.empty()) {
        // 「中身が Toggle:[ON/OFF] だけのもの」を判定します。
        // 実装理由：余分なサブメニュー遷移を省き、親メニューから直接ON/OFFを切り替えられるようにして操作性を向上させるため。
        bool isDirectToggle = false;
        MenuNode* toggleNode = nullptr;
        if (selected->children.size() == 2) {
            MenuNode* child0 = &selected->children[0];
            MenuNode* child1 = &selected->children[1];
            if (child0->name == L".." && child1->name.rfind(L"Toggle: ", 0) == 0) {
                isDirectToggle = true;
                toggleNode = child1;
            } else if (child1->name == L".." && child0->name.rfind(L"Toggle: ", 0) == 0) {
                isDirectToggle = true;
                toggleNode = child0;
            }
        }

        if (isDirectToggle && toggleNode) {
            // 子階層に潜らず、その場で直接トグル処理を実行します。
            ToggleMenuNode(selected->name, toggleNode);
        } else {
            // 通常のサブメニュー遷移
            g_menuIndexStack.push_back(g_selectedIndex); // 遷移元の選択位置を保存。実装理由：サブメニューから親階層へ戻る際、遷移前の位置に戻れるようにするため。
            g_menuStack.push_back(selected);
            if (selected->children.size() > 1 && selected->children[0].name == L"..") {
                g_selectedIndex = 1;
            } else {
                g_selectedIndex = 0;
            }
        }
    } else {
        AddLog(L"Action: " + selected->name + L" selected.");
    }
    UpdateMenuDisplay();
}

// 複数キーコンボを新キーバインドとして登録・保存する関数 (Console用)
void BindNewKeys(const std::vector<int>& vks) {
    g_openConsoleKeys = vks;
    SaveKeybinds(vks);
    g_isWaitingForKeyBind = false;
    // キーバインド設定完了時の時刻（ミリ秒）を記録します。
    // 実装理由：決定キーが離された際の余波メッセージがメニュー選択を再度トリガーするのを防ぐクールダウン監視用。
    g_lastBindTime = GetTickCount();
    
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

// FreeCamera用のトグルキーバインド確定処理
// 実装理由：ホットキーバインド設定待受完了時、FreeCamera用のキー配列変数に登録し永続化するため。
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

void AddLog(const std::wstring& text) {
    if (!g_hLogEdit) return;

    // 現在のスレッドがUIスレッド（コンソールウィンドウを作成したスレッド）であるか判定します。
    // 実装理由：ゲームのメイン/フックスレッドから直接 UI メッセージ（SendMessage）を送ると、UIの描画完了までゲーム側が同期ブロックされて一瞬画面が固まるため、非同期の PostMessage を使用して転送します。
    DWORD uiThreadId = GetWindowThreadProcessId(g_hMainWnd, NULL);
    if (GetCurrentThreadId() != uiThreadId) {
        std::wstring* pText = new std::wstring(text);
        PostMessageW(g_hMainWnd, WM_USER_ADD_LOG, 0, reinterpret_cast<LPARAM>(pText));
        return;
    }

    // 1. テキスト挿入前に、現在のスクロール位置と「一番下かどうか」を取得します。
    // 実装理由：ユーザーが上部にスクロールして過去ログを読んでいる最中に、新しいログで勝手に下部へスクロールされるのを防ぐため。
    POINT scrollPt;
    SendMessageW(g_hLogEdit, EM_GETSCROLLPOS, 0, (LPARAM)&scrollPt);

    SCROLLINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
    bool isAtBottom = false;
    if (GetScrollInfo(g_hLogEdit, SB_VERT, &si)) {
        // 現在位置 + ページの高さ が 最大スクロール範囲（の付近）に達しているか
        // 実装理由：スクロールバーのつまみが完全に下部にあるか、極めて近い位置にあるかを頑健に判定するため。
        isAtBottom = (si.nPos + (int)si.nPage >= si.nMax - 15);
    } else {
        // スクロールバーが不要（テキストが少ない）なら常に一番下とみなす
        isAtBottom = true;
    }

    COLORREF color = RGB(0, 255, 255); // デフォルトの文字色：シアン

    // メッセージ内容に "Enabled" または "Disabled" が含まれているか確認します。
    // 実装理由：ユーザーがモジュールのトグル状態（有効/無効）を一目で判別しやすくするため、色をそれぞれ緑/赤にします。
    if (text.find(L"Enabled") != std::wstring::npos) {
        color = RGB(0, 255, 0); // #00FF00 (緑)
    } else if (text.find(L"Disabled") != std::wstring::npos) {
        color = RGB(255, 0, 0); // #FF0000 (赤)
    }

    int len = GetWindowTextLengthW(g_hLogEdit);
    SendMessageW(g_hLogEdit, EM_SETSEL, len, len);

    // 文字の書式（カラー）を設定します
    CHARFORMAT2W cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = color;
    SendMessageW(g_hLogEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

    std::wstring formatText = text + L"\r\n";
    SendMessageW(g_hLogEdit, EM_REPLACESEL, FALSE, (LPARAM)formatText.c_str());

    // 次回のプレーンテキスト挿入時に最後の書式が引き継がれるのを防ぐため、末尾の書式をデフォルト（シアン）にリセットします。
    // 実装理由：RichEditは挿入時のフォーマットを引き継ぐため、後続の通常のログまで緑や赤になるのを防ぐため。
    int newLen = GetWindowTextLengthW(g_hLogEdit);
    SendMessageW(g_hLogEdit, EM_SETSEL, newLen, newLen);
    cf.crTextColor = RGB(0, 255, 255);
    SendMessageW(g_hLogEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

    // 2. ログ挿入後のスクロール制御
    if (isAtBottom) {
        // 新しいログが流れてきた時に自動スクロールします。
        // 実装理由：最新の出力をリアルタイムに追跡したいユーザーのためにスクロールを同期するため。
        SendMessageW(g_hLogEdit, WM_VSCROLL, SB_BOTTOM, 0);
    } else {
        // 過去のログを読んでいる場合は、スクロール座標を完全に維持します。
        // 実装理由：過去の記録を確認する作業を新しいメッセージの流入によって阻害しないため。
        SendMessageW(g_hLogEdit, EM_SETSCROLLPOS, 0, (LPARAM)&scrollPt);
    }
}

// ログエディットコントロールの内容をクリアし、初期起動時の案内メッセージを再出力します。
// 実装理由：ウィンドウが閉じられた際（WM_CLOSE）や初期作成時にログをクリーンな初期状態にリセットするため。
void ClearLog() {
    if (g_hLogEdit) {
        SetWindowTextW(g_hLogEdit, L"");
        AddLog(L"[Tsukuyomi] Mod Injected Successfully!");
        AddLog(L"Use W/S or Up/Down keys to navigate menu.");
        AddLog(L"Press 'Space' to select item.");
        AddLog(L"Press 'Esc' to hide window.");
    }
}

LRESULT CALLBACK LogSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN) {
        SendMessageW(GetParent(hwnd), uMsg, wParam, lParam);
        return 0;
    }
    return CallWindowProcW(g_fnOldLogProc, hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_USER_ADD_LOG: {
        // 他のスレッド（ゲームのメインスレッドなど）から非同期で送られてきたログ表示用の文字列を処理します。
        // 実装理由：別スレッドでnewされたstd::wstringポインタからテキストを取り出してAddLogを呼び、安全にdeleteしてメモリリークを防ぐため。
        if (lParam) {
            std::wstring* pText = reinterpret_cast<std::wstring*>(lParam);
            AddLog(*pText);
            delete pText;
        }
        break;
    }
    case WM_CREATE: {
        g_hBlackBrush = CreateSolidBrush(RGB(0, 0, 0));
        HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");

        g_hLogEdit = CreateWindowExW(
            0, L"RichEdit20W", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            10, 10, 565, 200,
            hWnd, (HMENU)1, GetModuleHandle(NULL), NULL
        );
        SendMessageW(g_hLogEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

        // RichEditの背景色を黒、デフォルトの文字色をシアン（RGB(0, 255, 255)）に設定します。
        // 実装理由：標準の EDIT コントロールから RichEdit コントロールに変更したため、背景・前景色の設定方法を RichEdit 専用のメッセージ通信（EM_SETBKGNDCOLOR, EM_SETCHARFORMAT）に変更する必要があるため。
        SendMessageW(g_hLogEdit, EM_SETBKGNDCOLOR, FALSE, RGB(0, 0, 0));
        CHARFORMAT2W cf;
        ZeroMemory(&cf, sizeof(cf));
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = RGB(0, 255, 255);
        SendMessageW(g_hLogEdit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

        g_fnOldLogProc = (WNDPROC)SetWindowLongPtrW(g_hLogEdit, GWLP_WNDPROC, (LONG_PTR)LogSubclassProc);

        g_hMenuStatic = CreateWindowExW(
            0, L"RichEdit20W", L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY,
            10, 220, 565, 130,
            hWnd, (HMENU)2, GetModuleHandle(NULL), NULL
        );
        SendMessageW(g_hMenuStatic, WM_SETFONT, (WPARAM)hFont, TRUE);

        // RichEditの背景色を黒、デフォルトの文字色をシアン（RGB(0, 255, 255)）に設定します。
        // 実装理由：g_hMenuStaticをSTATICからRichEditに変更したため、背景とデフォルトの書式を黒とシアンに初期設定する必要があるため。
        SendMessageW(g_hMenuStatic, EM_SETBKGNDCOLOR, FALSE, RGB(0, 0, 0));
        CHARFORMAT2W cfMenu;
        ZeroMemory(&cfMenu, sizeof(cfMenu));
        cfMenu.cbSize = sizeof(cfMenu);
        cfMenu.dwMask = CFM_COLOR;
        cfMenu.crTextColor = RGB(0, 255, 255);
        SendMessageW(g_hMenuStatic, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cfMenu);

        InitMenu();
        UpdateMenuDisplay();

        ClearLog();
        break;
    }
    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        if (g_hLogEdit) MoveWindow(g_hLogEdit, 10, 10, width - 20, height - 160, TRUE);
        if (g_hMenuStatic) MoveWindow(g_hMenuStatic, 10, height - 140, width - 20, 130, TRUE);
        break;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        // --- 数値入力モード時の処理 ---
        // 実装理由：エディットボックスやポップアップ入力なしに、メニュー上の表示領域だけで
        // 直感的に小数のタイピングを受け付け、確定・破棄できるようにするため。
        if (g_isWaitingForSpeedInput) {
            if (wParam >= '0' && wParam <= '9') {
                if (g_isFirstSpeedInputChar) {
                    g_speedInputBuffer = L"";
                    g_isFirstSpeedInputChar = false;
                }
                g_speedInputBuffer += (wchar_t)wParam;
            }
            else if (wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9) {
                if (g_isFirstSpeedInputChar) {
                    g_speedInputBuffer = L"";
                    g_isFirstSpeedInputChar = false;
                }
                g_speedInputBuffer += (wchar_t)('0' + (wParam - VK_NUMPAD0));
            }
            else if (wParam == VK_OEM_PERIOD || wParam == VK_DECIMAL) {
                if (g_isFirstSpeedInputChar) {
                    g_speedInputBuffer = L"";
                    g_isFirstSpeedInputChar = false;
                }
                // 小数点がまだ入力されていなければ追加可能にします
                if (g_speedInputBuffer.find(L'.') == std::wstring::npos) {
                    g_speedInputBuffer += L'.';
                }
            }
            else if (wParam == VK_BACK) {
                if (g_isFirstSpeedInputChar) {
                    // 最初に入力されたBackspaceは、現在の設定値を全消去して空にします
                    g_speedInputBuffer = L"";
                    g_isFirstSpeedInputChar = false;
                } else if (!g_speedInputBuffer.empty()) {
                    g_speedInputBuffer.pop_back();
                }
            }
            else if (wParam == VK_RETURN) {
                // 入力開始直後（300ms以内）の Enter キーは、メッセージキューに残存した決定キーの誤動作を防ぐため無視します。
                if (GetTickCount() - g_lastSpeedInputStartTime < 300) {
                    break;
                }
                if (!g_speedInputBuffer.empty()) {
                    try {
                        float speed = std::stof(g_speedInputBuffer);
                        if (speed < 0.0f) speed = 0.0f;
                        SetFreeCameraSpeed(speed);
                        AddLog(L"[FreeCamera] Speed set to: " + g_speedInputBuffer);
                    } catch (...) {
                        AddLog(L"[Error] Invalid speed value.");
                    }
                }
                g_isWaitingForSpeedInput = false;
                g_lastBindTime = GetTickCount(); // 確定後にメニューへ戻った際の残存キー誤動作を防ぐクールダウン。
                if (g_freeCameraSpeedNode) {
                    g_freeCameraSpeedNode->name = GetFreeCameraSpeedMenuName();
                }
                SaveKeybinds(g_openConsoleKeys);
                UpdateMenuDisplay();
            }
            else if (wParam == VK_ESCAPE) {
                g_isWaitingForSpeedInput = false;
                g_lastBindTime = GetTickCount(); // キャンセル後にメニューへ戻った際の残存キー誤動作を防ぐクールダウン。
                UpdateMenuDisplay();
            }
            // 入力バッファが更新されたため、毎キー入力後に画面表示を再描画します。
            // 実装理由：ユーザーがタイピングした数値がリアルタイムにメニュー表示に反映されるようにするため。
            UpdateMenuDisplay();
            break;
        }

        // キーバインド待ち状態の時は、WndProcではなくMainThread側の非同期ポーリングに処理を任せます。
        if (g_isWaitingForKeyBind) {
            break;
        }

        // キーバインド設定完了直後は、メッセージキューに残存したキーメッセージによる誤動作を防ぐため、300msのクールダウンを設けます。
        if (GetTickCount() - g_lastBindTime < 300) {
            break;
        }

        if (wParam == 'W' || wParam == VK_UP) {
            MenuUp();
        }
        else if (wParam == 'S' || wParam == VK_DOWN) {
            MenuDown();
        }
        else if (wParam == VK_SPACE || wParam == VK_RETURN) {
            MenuSelect();
        }
        else if (wParam == VK_ESCAPE) {
            ShowWindow(hWnd, SW_HIDE);
        }
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(0, 255, 255));
        SetBkColor(hdc, RGB(0, 0, 0));
        return (INT_PTR)g_hBlackBrush;
    }
    case WM_CLOSE:
        ClearLog();
        ShowWindow(hWnd, SW_HIDE);
        return 0;
    case WM_DESTROY:
        if (g_hBlackBrush) DeleteObject(g_hBlackBrush);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}

void CreateCustomConsole(HINSTANCE hInst) {
    WNDCLASSEXW wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInst;
    wcex.hIcon = NULL;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"TsukuyomiConsoleClass";
    RegisterClassExW(&wcex);

    g_hMainWnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        L"TsukuyomiConsoleClass",
        L"Tsukuyomi",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 400,
        NULL, NULL, hInst, NULL
    );

    ShowWindow(g_hMainWnd, SW_SHOW);
    UpdateWindow(g_hMainWnd);
}

void DestroyCustomConsole(HINSTANCE hInst) {
    if (g_hMainWnd) {
        DestroyWindow(g_hMainWnd);
    }
    UnregisterClassW(L"TsukuyomiConsoleClass", hInst);
}
