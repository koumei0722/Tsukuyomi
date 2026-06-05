#define NOMINMAX
#include <Windows.h>
#include <cstdlib>
#include <thread>
#include <chrono>

#include <libhat/scanner.hpp>
#include <libhat/signature.hpp>
#include <MinHook.h>
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")

#include "SchematicControl.h"
#include "console/console.h"
#include "utils/window_utils.h"

static bool g_enabled = false;
static std::byte* g_addrSchematic = nullptr;
static bool g_isScanning = false;
static std::thread g_scanThread;
static bool g_hookCreated = false;

static bool Pressed = false;

static std::byte* ScanAllModules(const auto& signature) {
    HANDLE hProcess = GetCurrentProcess();
    HMODULE hModules[1024];
    DWORD cbNeeded;

    if (!EnumProcessModules(hProcess, hModules, sizeof(hModules), &cbNeeded))
        return nullptr;

    DWORD moduleCount = cbNeeded / sizeof(HMODULE);

    for (DWORD i = 0; i < moduleCount; i++) {
        MODULEINFO modInfo{};
        if (!GetModuleInformation(hProcess, hModules[i], &modInfo, sizeof(modInfo)))
            continue;

        auto* start = reinterpret_cast<std::byte*>(modInfo.lpBaseOfDll);
        auto* end   = start + modInfo.SizeOfImage;

        auto result = hat::find_pattern(start, end, signature);
        if (result.get())
            return result.get();
    }

    return nullptr;
}

typedef void(__fastcall* Schematica_t)(void* rcx, void* rdx, void* r8);
static Schematica_t OrigSchematica = nullptr;

static void __fastcall hk_Schematica(void* rcx, void* rdx, void* r8) {
    auto base = reinterpret_cast<uintptr_t>(rcx);

    // ゲームがフォーカスされている場合のみ、矢印キーによるOffset Yの変更を処理します。
    // 実装理由：他ウィンドウやコンソールを操作中に、意図せずOffset Yを変更してしまうのを防止するため。
    if (IsGameFocused() && (GetAsyncKeyState(VK_UP) & 0x8000)) {
        if (!Pressed) {
            *reinterpret_cast<int*>(base + 0x58C) = *reinterpret_cast<int*>(base + 0x58C) + 1;
            Pressed = true;

            // 変更後のOffset Yを取得してログに出力します。
            // 実装理由：ユーザーが矢印キーでOffset Yを変更した際に、現在の値をコンソールでリアルタイムに視認できるようにするため。
            int newOffsetY = *reinterpret_cast<int*>(base + 0x58C);
            AddLog(L"[SchematicControl] Offset Y changed to: " + std::to_wstring(newOffsetY));
        }
    } else if (IsGameFocused() && (GetAsyncKeyState(VK_DOWN) & 0x8000)) {
        if (!Pressed) {
            *reinterpret_cast<int*>(base + 0x58C) = *reinterpret_cast<int*>(base + 0x58C) - 1;
            Pressed = true;

            // 変更後のOffset Yを取得してログに出力します。
            // 実装理由：ユーザーが矢印キーでOffset Yを変更した際に、現在の値をコンソールでリアルタイムに視認できるようにするため。
            int newOffsetY = *reinterpret_cast<int*>(base + 0x58C);
            AddLog(L"[SchematicControl] Offset Y changed to: " + std::to_wstring(newOffsetY));
        }
    } else {
        Pressed = false;
    }

    OrigSchematica(rcx, rdx, r8);
}

static void ScanLoop() {
    constexpr auto signature = hat::compile_signature<"48 8B C4 55 53 56 57 41 54 41 55 41 56 41 57 48 8D A8 ?? ?? ?? ?? 48 81 EC E8 05 00 00 0F 29 70 A8 0F 29 78 98 44 0F 29 40 88 44 0F 29 88 ?? ?? ?? ?? 44 0F 29 90 ?? ?? ?? ?? 44 0F 29 98 ?? ?? ?? ?? 44 0F 29 A0 ?? ?? ?? ?? 44 0F 29 A8 ?? ?? ?? ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 50 04 00 00">();

    while (g_enabled && !g_addrSchematic) {
        g_addrSchematic = ScanAllModules(signature);
        if (g_addrSchematic) {
            AddLog(L"[SchematicControl] Signature found!");
            // フックの作成と有効化を行います。
            // 実装理由：シグネチャスキャンで見つかったアドレスに対して、プレイヤーの回路操作イベントを取得するフックを登録するため。
            if (!g_hookCreated) {
                if (MH_CreateHook(g_addrSchematic, &hk_Schematica, reinterpret_cast<void**>(&OrigSchematica)) == MH_OK) {
                    g_hookCreated = true;
                } else {
                    AddLog(L"[Error] Failed to create SchematicControl hook.");
                    g_addrSchematic = nullptr;
                }
            }
            if (g_hookCreated) {
                MH_EnableHook(g_addrSchematic);
            }
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    g_isScanning = false;
}

bool GetSchematicControlEnabled() {
    return g_enabled;
}

void SetSchematicControlEnabled(bool enabled) {
    g_enabled = enabled;
    if (enabled) {
        if (g_addrSchematic) {
            if (g_hookCreated) {
                MH_EnableHook(g_addrSchematic);
            }
        } else {
            // スキャン用バックグラウンドスレッドを起動します。
            // 実装理由：ONの時のみに限定してシグネチャスキャンを非同期で開始し、起動中のハングや無駄なCPU使用を避けるため。
            if (!g_isScanning) {
                g_isScanning = true;
                if (g_scanThread.joinable()) {
                    g_scanThread.join();
                }
                g_scanThread = std::thread(ScanLoop);
            }
        }
    } else {
        if (g_addrSchematic && g_hookCreated) {
            MH_DisableHook(g_addrSchematic);
        }
    }
}

void CleanupSchematicControl() {
    g_enabled = false;
    if (g_scanThread.joinable()) {
        g_scanThread.join();
    }
    if (g_addrSchematic && g_hookCreated) {
        MH_DisableHook(g_addrSchematic);
        MH_RemoveHook(g_addrSchematic);
    }
}