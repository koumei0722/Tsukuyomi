#pragma once
#include <windows.h>
#include <string>
#include <vector>

// ---------------------------------------------------------
// 共通データ構造
// ---------------------------------------------------------

// コンソールメニューの各階層や項目を表すツリーノード構造体
// 実装理由：console.cppおよび各Itemファイル間でメニューツリー情報を共有し、項目名や親子関係を操作できるようにするため。
struct MenuNode {
    std::wstring name;
    std::vector<MenuNode> children;
};

// ---------------------------------------------------------
// 外部モジュールから参照・操作するためのグローバル変数
// ---------------------------------------------------------
extern HWND g_hMainWnd;       // メインのコンソールウィンドウハンドル
extern bool shouldExit;       // スレッドおよびDLLの終了シグナル

extern std::vector<int> g_openConsoleKeys;  // コンソールを開くキー（仮想キーコード配列）
extern std::vector<int> g_freeCameraKeys;   // FreeCameraの有効/無効を切り替えるトグルキー配列
extern std::vector<int> g_fastBlockPlacementKeys; // FastBlockPlacementのトグルキー配列
extern std::vector<int> g_scaffoldKeys;     // Scaffoldのトグルキー配列

extern bool g_isWaitingForKeyBind;  // キーバインド入力待ちフラグ

// キーバインド変更時に、どの機能のキーを変更しようとしているかを区別する列挙型。
// 実装理由：各モジュールのトグルキー設定要求を共通の入力待ちループで処理するため。
enum class KeyBindTarget {
    Console,
    FreeCamera,
    FastBlockPlacement,
    Scaffold
};
extern KeyBindTarget g_keyBindTarget; // 現在待受中のキーバインド対象

// 数値入力待受の対象を判別する列挙型。
// 実装理由：FreeCameraの速度（小数）と自動設置のディレイ（自然数）の双方の数値入力を1つの入力画面で汎用的に処理するため。
enum class NumberInputTarget {
    None,
    FreeCameraSpeed
};
extern NumberInputTarget g_numberInputTarget; // 数値の入力対象
extern std::wstring g_numberInputBuffer;       // 入力中の数値文字列
extern DWORD g_lastNumberInputStartTime;       // 数値入力開始時のタイムスタンプ
extern bool g_isFirstNumberInputChar;          // 数値入力開始後、最初の文字入力か

// キーバインド設定完了時のタイムスタンプを保持する変数。
// 実装理由：設定完了後にメニューへ戻った際の残存キーメッセージによる誤選択を防ぐため。
extern DWORD g_lastBindTime;

// ---------------------------------------------------------
// 各項目ノードへの参照ポインタ (extern)
// 実装理由：各Itemファイルから自身の関連ノードの名前（name）を動的に書き換えて表示をリアルタイム更新するため。
// ---------------------------------------------------------
extern MenuNode* g_keyBindNode;
extern MenuNode* g_freeCameraNode;
extern MenuNode* g_freeCameraKeyNode;
extern MenuNode* g_freeCameraSpeedNode;
extern MenuNode* g_creativeNoClipNode;
extern MenuNode* g_antiDarknessNode;
extern MenuNode* g_autoToolNode;            // AutoToolのメニューノードへの参照。実装理由：メニュー表示名を動的に書き換えるため。
extern MenuNode* g_schematicControlNode;
extern MenuNode* g_fastBlockPlacementNode;
extern MenuNode* g_fastBlockPlacementKeyNode;
extern MenuNode* g_fastPlacementModeNode;
extern MenuNode* g_fastPlacementDistanceNode;
extern MenuNode* g_scaffoldNode;
extern MenuNode* g_scaffoldKeyNode;


// ---------------------------------------------------------
// コンソール制御・表示・ユーティリティAPI
// ---------------------------------------------------------
void AddLog(const std::wstring& text);
void CreateCustomConsole(HINSTANCE hInst);
void DestroyCustomConsole(HINSTANCE hInst);

// 共通設定保存フォルダの絶対パスを取得します。
// 実装理由：各Itemファイルの個別設定JSONを同一の「Tsukuyomi」フォルダ配下に保存するため。
std::wstring GetConfigDir();

// 仮想キーコードを人間が読める文字列に変換します（例: VK_SPACE -> "SPACE"）。
// 実装理由：ConsoleItemやFreeCameraItemで現在登録されているキーコンボを表示するために使用。
std::wstring GetKeyName(int vk);

// メニュー操作用API
void MenuUp();
void MenuDown();
void MenuLeft();
void MenuRight();
void MenuSelect();
void UpdateMenuDisplay(); // メニューの描画更新API。実装理由：ホットキー操作時などに別スレッドからメニュー画面のトグル状態（文字色）をリアルタイムに再描画させるため。

// ---------------------------------------------------------
// 各項目のセーブ/ロード/表示名取得API（各Itemファイルに実体を配置）
// ---------------------------------------------------------

// Console項目 (ConsoleItem.cpp)
std::wstring GetKeybindMenuName();
void SaveKeybinds(const std::vector<int>& vks);
void LoadKeybinds();
void BindNewKeys(const std::vector<int>& vks);
void CancelKeyBind();

// FreeCamera項目 (FreeCameraItem.cpp)
std::wstring GetFreeCameraMenuName();
std::wstring GetFreeCameraKeyMenuName();
std::wstring GetFreeCameraSpeedMenuName();
void BindFreeCameraKeys(const std::vector<int>& vks);

// CreativeNoClip項目 (CreativeNoClipItem.cpp)
std::wstring GetCreativeNoClipMenuName();
void SaveCreativeNoClipConfig();
void LoadCreativeNoClipConfig();

// AntiDarkness項目 (AntiDarknessItem.cpp)
std::wstring GetAntiDarknessMenuName();
void SaveAntiDarknessConfig();
void LoadAntiDarknessConfig();

// AutoTool項目 (AutoToolItem.cpp)
std::wstring GetAutoToolMenuName();
void SaveAutoToolConfig();
void LoadAutoToolConfig();

// SchematicControl項目 (SchematicControlItem.cpp)
std::wstring GetSchematicControlMenuName();
void SaveSchematicControlConfig();
void LoadSchematicControlConfig();

// FastBlockPlacement項目 (FastBlockPlacementItem.cpp)
std::wstring GetFastBlockPlacementMenuName();
std::wstring GetFastBlockPlacementKeyMenuName();
std::wstring GetFastPlacementModeMenuName();
std::wstring GetFastPlacementDistanceMenuName();
void SaveFastBlockPlacementConfig();
void LoadFastBlockPlacementConfig();
void BindFastBlockPlacementKeys(const std::vector<int>& vks);

// Scaffold項目 (ScaffoldItem.cpp)
std::wstring GetScaffoldMenuName();
std::wstring GetScaffoldKeyMenuName();
void SaveScaffoldConfig();
void LoadScaffoldConfig();
void BindScaffoldKeys(const std::vector<int>& vks);


