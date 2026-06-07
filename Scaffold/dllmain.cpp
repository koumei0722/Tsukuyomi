#define NOMINMAX
#include <Windows.h>
#include <atomic>
#include <cmath>
#include <cstdint>

#include <libhat/scanner.hpp>
#include <libhat/signature.hpp>
#include <MinHook.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

std::byte* Position = nullptr;
std::byte* BuildBlock = nullptr;

struct BlockPos {
    int32_t x;
    int32_t y;
    int32_t z;
};

std::atomic<int32_t> PositionX = 0;
std::atomic<int32_t> PositionY = 0;
std::atomic<int32_t> PositionZ = 0;
void* LastGameMode = nullptr;
ULONGLONG LastPositionTick = 0;

typedef void(__fastcall* Position_t)(void* rcx, void* rdx, void* r8, void* r9);
Position_t OrigPosition = nullptr;

typedef bool(__fastcall* BuildBlock_t)(void* rcx, BlockPos* pos, unsigned char face, bool isSneaking);
BuildBlock_t OrigBuildBlock = nullptr;

bool __fastcall hk_BuildBlock(void* rcx, BlockPos* pos, unsigned char face, bool isSneaking) {
    LastGameMode = rcx;
    return OrigBuildBlock(rcx, pos, face, isSneaking);
}

void TryPlaceBlockBelow() {
    if (OrigBuildBlock == nullptr || LastGameMode == nullptr) {
        return;
    }

    const int32_t x = PositionX.load(std::memory_order_relaxed);
    const int32_t y = PositionY.load(std::memory_order_relaxed) - 2;
    const int32_t z = PositionZ.load(std::memory_order_relaxed);

    BlockPos pos{x, y, z};

    if (GetKeyState(VK_CAPITAL) & 0x0001) {
        for (unsigned char face = 1; face <= 5; ++face) {
            if (face == 1) {
                BlockPos offsetPos{x, y, z + 1};
                OrigBuildBlock(LastGameMode, &offsetPos, 2, false);
                continue;
            }

            OrigBuildBlock(LastGameMode, &pos, face, false);
        }
    }
}

void __fastcall hk_Position(void* rcx, void* rdx, void* r8, void* r9) {
    const ULONGLONG now = GetTickCount64();
    if (LastPositionTick != 0 && now - LastPositionTick >= 1000) {
        LastGameMode = nullptr;
    }
    LastPositionTick = now;

    auto base = reinterpret_cast<uintptr_t>(rdx);
    PositionX = static_cast<int32_t>(std::floor(*reinterpret_cast<float*>(base)));
    PositionY = static_cast<int32_t>(std::floor(*reinterpret_cast<float*>(base + 0x4)));
    PositionZ = static_cast<int32_t>(std::floor(*reinterpret_cast<float*>(base + 0x8)));

    TryPlaceBlockBelow();

    OrigPosition(rcx, rdx, r8, r9);
}

static DWORD WINAPI startup(LPVOID dll) {
    constexpr auto Positionsignature = hat::compile_signature<"F2 41 0F 10 00 33 C9 F2 0F 11 02 41 8B 40 08 89 42 08 F2 41 0F 10 40 0C F2 0F 11 42 0C 41 8B 40 14 89 42 14 41 8B 40 18 89 42 18">();
    hat::scan_result PositionScanResult = hat::find_pattern(Positionsignature, ".text");
    Position = PositionScanResult.get();

    constexpr auto BuildBlockSignature = hat::compile_signature<"48 89 5C 24 10 48 89 74 24 18 55 57 41 54 41 56 41 57 48 8D 6C 24 90 48 81 EC 70 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 60 41 0F B6 F1 45 0F B6 F0 4C 8B FA 48 8B F9">();
    hat::scan_result BuildBlockScanResult = hat::find_pattern(BuildBlockSignature, ".text");
    BuildBlock = BuildBlockScanResult.get();

    MH_Initialize();
    if (Position != nullptr) {
        MH_CreateHook(Position, &hk_Position, reinterpret_cast<void**>(&OrigPosition));
        MH_EnableHook(Position);
    }

    if (BuildBlock != nullptr) {
        MH_CreateHook(BuildBlock, &hk_BuildBlock, reinterpret_cast<void**>(&OrigBuildBlock));
        MH_EnableHook(BuildBlock);
    }

    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            CreateThread(nullptr, 0, &startup, hinstDLL, 0, nullptr);
            break;

        case DLL_PROCESS_DETACH:

        default:
            break;
    }

    return TRUE;
}
