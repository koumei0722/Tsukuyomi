#include "hooks.h"
#include "scans/scans.h"
#include "console/console.h"
#include "modules/FreeCamera.h"
#include <MinHook.h>

// 元の関数のポインタ定義。
// フックハンドラ内からオリジナルのゲーム処理を呼び出すために使用します。
typedef void(__fastcall* CameraUpdate_t)(void* rcx, void* rdx, void* r8);
static CameraUpdate_t OrigCameraUpdate = nullptr;

typedef void(__fastcall* YawUpdate_t)(void* rcx, void* rdx, void* r8, void* r9);
static YawUpdate_t OrigYawUpdate = nullptr;

typedef void(__fastcall* PacketSend_t)(void* _this, void* packet);
static PacketSend_t OrigPacketSend = nullptr;

// プレイヤー移動に関連するパケットIDの列挙
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
    auto base = reinterpret_cast<uintptr_t>(rdx);
    SetFreeCameraYawPtr(reinterpret_cast<float*>(base + 0x10));
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

    AddLog(L"[Tsukuyomi] All hooks initialized successfully.");
    return true;
}

void CleanupHooks() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}
