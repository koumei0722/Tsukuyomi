#include "hooks.h"
#include <windows.h>
#include "scans/scans.h"
#include "console/console.h"
#include "modules/FreeCamera.h"
#include "modules/FastBlockPlacement.h"
#include "modules/Scaffold.h"
#include <MinHook.h>
#include <cmath>

// 元の関数のポインタ定義。
// フックハンドラ内からオリジナルのゲーム処理を呼び出すために使用します。
typedef void(__fastcall* CameraUpdate_t)(void* rcx, void* rdx, void* r8);
static CameraUpdate_t OrigCameraUpdate = nullptr;

typedef void(__fastcall* YawUpdate_t)(void* rcx, void* rdx, void* r8, void* r9);
static YawUpdate_t OrigYawUpdate = nullptr;

typedef void(__fastcall* PacketSend_t)(void* _this, void* packet);
static PacketSend_t OrigPacketSend = nullptr;

// buildBlock フックのオリジナルポインタ
// 実装理由：他モジュール（Scaffoldなど）からフックをバイパスしてオリジナルのブロック設置処理を呼び出せるようにグローバル公開します。
typedef bool(__fastcall* BuildBlockHook_t)(void* rcx, void* rdx, unsigned char r8, unsigned char r9);
BuildBlockHook_t g_origBuildBlock = nullptr;

// プレイヤー位置更新フックのオリジナルポインタ
// 実装理由：Microsoft x64 呼出規約により、フック元が4つの引数を渡すため、シグネチャを正確に合わせてレジスタの破損を防ぎます。
typedef void(__fastcall* PlayerPositionUpdate_t)(void* rcx, void* rdx, void* r8, void* r9);
static PlayerPositionUpdate_t OrigPlayerPositionUpdate = nullptr;

// プレイヤーの現在座標を保持するグローバル変数
// 実装理由：座標更新フックで常に最新値に保ち、高速設置時の原点座標として使用するため。
int g_playerBlockX = 0;
int g_playerBlockY = 0;
int g_playerBlockZ = 0;

// プレイヤー移動に関連する`パケットIDの列挙
enum class PacketID : int {
    MovePlayer      = 0x13,
    PlayerAuthInput = 0x90,
};

// パケットオブジェクトからIDを取得するヘルパー関数
// 実装理由：C++の仮想関数テーブル（vtable）の構造に依存して、安全にパケットIDを取り出すため。
static int getPacketId(void* packet) {
    auto vtable = *reinterpret_cast<void***>(packet);
    auto getId = reinterpret_cast<int(__fastcall*)(void*)>(vtable[1]);
    return getId(packet);
}

// ---------------------------------------------------------
// フックハンドラ
// ---------------------------------------------------------

// カメラ座標の更新処理のフック
// 実装理由：フリーカメラ有効時のみ、ゲーム内カメラの座標更新処理を割り込み、独自の自由移動座標を反映するため。
static void __fastcall hk_CameraUpdate(void* rcx, void* rdx, void* r8) {
    OrigCameraUpdate(rcx, rdx, r8);
    if (GetFreeCameraEnabled()) {
        UpdateFreeCameraPosition(rdx);
    }
}

// プレイヤーの視線（Yaw）更新処理のフック
// 実装理由：カメラの進行方向の算出に必要なプレイヤーの水平視線角度（Yaw）のメモリアドレスを特定・取得するため。
static void __fastcall hk_YawUpdate(void* rcx, void* rdx, void* r8, void* r9) {
    OrigYawUpdate(rcx, rdx, r8, r9);
    // rdxの指す領域にプレイヤーの視点情報（座標、Pitch、Yaw）が存在するため、
    // ポインタをPlayerView型にキャストしてモジュールへ登録します。
    // 実装理由：FreeCameraやFastBlockPlacementの計算で、視線情報と始点座標を同期的に参照するため。
    SetPlayerViewPtr(reinterpret_cast<PlayerView*>(rdx));
}

// パケット送信処理のフック
// 実装理由：フリーカメラが有効な間、プレイヤーの移動情報をサーバーに送信するのを防ぎ、
// サーバー側との同期ずれ（キャラクターが突然ワープするなどの不自然な同期）を抑止するため。
static void __fastcall hk_PacketSend(void* _this, void* packet) {
    if (GetFreeCameraEnabled()) {
        int id = getPacketId(packet);
        if (id == (int)PacketID::PlayerAuthInput || id == (int)PacketID::MovePlayer) {
            return; // 該当パケットの送信をスキップ（遮断）
        }
    }
    OrigPacketSend(_this, packet);
}

static void* g_lastGameMode = nullptr;
static ULONGLONG g_lastPositionTick = 0; // 最終座標更新時刻
bool g_isPlacingBlock = false; // グローバル再入ガードフラグ

// buildBlock のフックハンドラ
// 実装理由：設置時のGamemodeポインタを動的に取得・保存し、FastBlockPlacement有効時には
// 周囲への高速一括設置要求をキュー（遅延処理）に登録するため。
static bool __fastcall hk_BuildBlock(void* rcx, void* rdx, unsigned char r8, unsigned char r9) {
    g_lastGameMode = rcx; // Gamemodeポインタを保存します

    // 呼び出し時点での自動設置アクティブ状態を判定して保存します。
    // 実装理由：OnBlockPlaced で g_hasSavedY が true に更新されるため、
    // 手動での最初のブロック設置（g_hasSavedY が false から始まる回）が
    // 誤って自動設置の無効化ガード（(0,0,0)への書き換え）に巻き込まれないように保護するため。
    bool wasActive = IsFastPlacementActive();

    if (GetFastBlockPlacementEnabled()) {
        // 最初のブロック設置イベント時に、実際設置Y座標を特定・保存します。
        OnBlockPlaced(rcx, rdx, r8);
    }

    // 自動設置中（右クリック長押しでY高度固定設置中）である場合、
    // ゲーム本来の手動設置処理を無効化するため、オリジナル処理を呼び出さずに早期リターンします。
    // 実装理由：手動による重複設置やパケット競合を無効化し、自動設置（Scaffold）の整合性を保つため。
    // プログラムによる自動設置は g_origBuildBlock をバイパスして直接呼び出すため、ここを通過しません。
    // したがって、IsFastPlacementRunning() による呼び出し元チェックは不要となり、wasActive のみで安全にガードできます。
    if (wasActive) {
        return false;
    }

    // 再入ガードを設定してオリジナル関数を呼び出します。
    // 実装理由：手動設置処理の実行中に、ゲーム内の位置同期によって再帰的に Scaffold 等の自動設置が
    // 呼び出されてクラッシュする（関数の再入バグ）のを防ぐため。
    g_isPlacingBlock = true;
    bool result = g_origBuildBlock(rcx, rdx, r8, r9);
    g_isPlacingBlock = false;
    return result;
}

// プレイヤー座標更新関数のフックハンドラ
// 実装理由：ゲーム内のプレイヤー現在座標(rdxが指すメモリ)からX, Y, Zの整数座標を読み出し、
// グローバル変数にリアルタイム保存しておくため。シグネチャを完全に合致させてレジスタR8, R9の破壊を防ぎます。
static void __fastcall hk_PlayerPositionUpdate(void* rcx, void* rdx, void* r8, void* r9) {
    // プレイヤーの座標取得（位置同期フック）が1秒以上動作しなかった場合のみ、保存されているGameModeポインタをクリアします。
    // 実装理由：ワールド退出時やローディング時にのみ安全にポインタをリセットし、ダングリングを防止するため。
    const ULONGLONG now = GetTickCount64();
    if (g_lastPositionTick != 0 && now - g_lastPositionTick >= 1000) {
        g_lastGameMode = nullptr;
    }
    g_lastPositionTick = now;

    if (rdx) {
        float* pPos = reinterpret_cast<float*>(rdx);
        g_playerBlockX = static_cast<int>(std::floor(pPos[0]));
        g_playerBlockY = static_cast<int>(std::floor(pPos[1]));
        g_playerBlockZ = static_cast<int>(std::floor(pPos[2]));

        // オリジナルの位置更新処理を実行する前に、自動設置のトリガーを実行します。
        // 実装理由：OrigPlayerPositionUpdateを実行するとそのフレームのTickが進み、手動設置で利用された
        // コンテキストポインタが破棄されるため、まだ有効な状態（前処理）のポインタを用いて安全に設置処理を呼び出すため。
        if (!g_isPlacingBlock) {
            // Scaffoldの設置判定と処理を実行します。
            UpdateScaffold(g_lastGameMode);

            // 安全なシミュレーションTickのタイミングでFastBlockPlacementの遅延自動設置を更新します。
            UpdateFastPlacement();
        }
    }

    // 最後にオリジナルの位置更新を呼び出します。
    OrigPlayerPositionUpdate(rcx, rdx, r8, r9);
}

// ---------------------------------------------------------
// フック制御API
// ---------------------------------------------------------

bool InitializeHooks() {
    if (MH_Initialize() != MH_OK) {
        AddLog(L"[Error] Failed to initialize MinHook.");
        return false;
    }

    // 1. CameraUpdate フック作成
    if (MH_CreateHook(g_addrCameraUpdate, &hk_CameraUpdate, reinterpret_cast<void**>(&OrigCameraUpdate)) != MH_OK ||
        MH_EnableHook(g_addrCameraUpdate) != MH_OK) {
        AddLog(L"[Error] Failed to hook CameraUpdate.");
        return false;
    }

    // 2. YawUpdate フック作成
    if (MH_CreateHook(g_addrYawUpdate, &hk_YawUpdate, reinterpret_cast<void**>(&OrigYawUpdate)) != MH_OK ||
        MH_EnableHook(g_addrYawUpdate) != MH_OK) {
        AddLog(L"[Error] Failed to hook YawUpdate.");
        return false;
    }

    // 3. PacketSend フック作成
    if (MH_CreateHook(g_addrPacketSend, &hk_PacketSend, reinterpret_cast<void**>(&OrigPacketSend)) != MH_OK ||
        MH_EnableHook(g_addrPacketSend) != MH_OK) {
        AddLog(L"[Error] Failed to hook PacketSend.");
        return false;
    }

    // 4. buildBlock フック作成
    if (MH_CreateHook(g_addrBuildBlock, &hk_BuildBlock, reinterpret_cast<void**>(&g_origBuildBlock)) != MH_OK ||
        MH_EnableHook(g_addrBuildBlock) != MH_OK) {
        AddLog(L"[Error] Failed to hook buildBlock.");
        return false;
    }

    // 5. PlayerPositionUpdate フック作成
    if (MH_CreateHook(g_addrPlayerPositionUpdate, &hk_PlayerPositionUpdate, reinterpret_cast<void**>(&OrigPlayerPositionUpdate)) != MH_OK ||
        MH_EnableHook(g_addrPlayerPositionUpdate) != MH_OK) {
        AddLog(L"[Error] Failed to hook PlayerPositionUpdate.");
        return false;
    }

    AddLog(L"[Tsukuyomi] All hooks initialized successfully.");
    return true;
}

void CleanupHooks() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}

