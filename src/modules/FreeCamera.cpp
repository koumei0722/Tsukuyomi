#include "FreeCamera.h"
#include "../scans/scans.h"
#include "utils/window_utils.h"
#include <windows.h>
#include <glm/glm.hpp>
#include <algorithm>

static float g_camX = 0.0f;
static float g_camY = 0.0f;
static float g_camZ = 0.0f;
static float g_speed = 0.015625f; // デフォルト移動速度
static PlayerView* g_playerView = nullptr;

static bool g_initialized = false;
static bool g_enabled = false;

static std::byte g_originalBytes[15];
static bool g_originalBytesSaved = false;
extern HWND g_hMainWnd; // console.cpp で定義されているコンソールウィンドウのハンドル

// キー入力に応じた移動方向のYawオフセットを計算します。
// W/S/A/Dの組み合わせに基づいてオフセット（度）を返します。入力がない場合は 360.0f を返します。
static float getYawOffset() {
    bool w = GetAsyncKeyState('W') & 0x8000;
    bool s = GetAsyncKeyState('S') & 0x8000;
    bool a = GetAsyncKeyState('A') & 0x8000;
    bool d = GetAsyncKeyState('D') & 0x8000;

    if (!w && !s && !a && !d) return 360.0f;
    if (w && a) return -45.0f;
    if (w && d) return  45.0f;
    if (s && a) return -135.0f;
    if (s && d) return  135.0f;
    if (w)      return  0.0f;
    if (s)      return  180.0f;
    if (a)      return -90.0f;
    if (d)      return  90.0f;
    return 360.0f;
}

bool GetFreeCameraEnabled() {
    return g_enabled;
}

void SetFreeCameraEnabled(bool enabled) {
    if (!g_addrCameraUpdate) return;

    DWORD oldProtect;
    // CameraUpdate関数のオフセット11バイト目から15バイト分のメモリ領域の保護を変更し、書き換えを可能にします。
    // 実装理由：ゲーム内のカメラ位置更新処理の一部をNOPで無効化し、独自座標での描画を可能にするため。
    VirtualProtect(g_addrCameraUpdate + 11, 15, PAGE_EXECUTE_READWRITE, &oldProtect);

    // 元のバイト列の保存（初回の無効化時のみ実行）
    if (!g_originalBytesSaved) {
        memcpy(g_originalBytes, g_addrCameraUpdate + 11, 15);
        g_originalBytesSaved = true;
    }

    if (enabled) {
        // カメラ座標がプレイヤー座標に引き戻される命令を NOP (0x90) で無効化します
        memset(g_addrCameraUpdate + 11, 0x90, 15);
        g_initialized = false;
        g_enabled = true;
    } else {
        g_enabled = false;
        // 元のバイト列を復元します
        memcpy(g_addrCameraUpdate + 11, g_originalBytes, 15);
    }
    
    VirtualProtect(g_addrCameraUpdate + 11, 15, oldProtect, &oldProtect);
}

void SetPlayerViewPtr(PlayerView* viewPtr) {
    g_playerView = viewPtr;
}

void UpdateFreeCameraPosition(void* cameraBase) {
    auto base = reinterpret_cast<uintptr_t>(cameraBase);

    // 初期有効化時、現在のカメラ位置を初期位置として同期
    if (!g_initialized) {
        g_camX = *reinterpret_cast<float*>(base + 0x40);
        g_camY = *reinterpret_cast<float*>(base + 0x44);
        g_camZ = *reinterpret_cast<float*>(base + 0x48);
        g_initialized = true;
    }

    // ゲームがアクティブかつコンソールが開かれていない場合のみキー入力を受け付け、移動を処理します。
    // 実装理由：他ウィンドウやコンソールを操作中に、意図せずカメラが移動してしまうのを防止するため。
    if (IsGameFocused()) {
        // Ctrlキーが押されている間は移動速度を2倍にします
        float currentSpeed = (GetAsyncKeyState(VK_LCONTROL) & 0x8000) ? g_speed * 2.0f : g_speed;

        // 視線方向（Yaw）を考慮した水平移動の計算
        if (g_playerView) {
            float yaw = g_playerView->yaw;
            float yawOffset = getYawOffset();

            if (yawOffset < 360.0f) {
                float calcYaw = glm::radians(yaw + yawOffset + 90.0f);
                g_camX += glm::cos(calcYaw) * currentSpeed;
                g_camZ += glm::sin(calcYaw) * currentSpeed;
            }
        }

        // Spaceキーで上昇、LSHIFTキーで下降
        if (GetAsyncKeyState(VK_SPACE)   & 0x8000) g_camY += currentSpeed;
        if (GetAsyncKeyState(VK_LSHIFT) & 0x8000) g_camY -= currentSpeed;
    }

    // 計算したカメラ位置をゲームメモリに書き戻します
    *reinterpret_cast<float*>(base + 0x40) = g_camX;
    *reinterpret_cast<float*>(base + 0x44) = g_camY;
    *reinterpret_cast<float*>(base + 0x48) = g_camZ;
}

float GetFreeCameraSpeed() {
    return g_speed;
}

void SetFreeCameraSpeed(float speed) {
    g_speed = speed;
}

PlayerView* GetPlayerViewPtr() {
    return g_playerView;
}