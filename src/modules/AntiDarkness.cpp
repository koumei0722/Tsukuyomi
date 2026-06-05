#include "AntiDarkness.h"
#include "../scans/scans.h"
#include <windows.h>

static bool g_enabled = false;

bool GetAntiDarknessEnabled() {
    return g_enabled;
}

void SetAntiDarknessEnabled(bool enabled) {
    g_enabled = enabled; // まず状態変数を確実に更新します。

    if (!g_addrAntiDarkness) return; // アドレス特定前であれば、実際のメモリ書き換えを行わずに戻ります。

    DWORD oldProtect;
    // メモリ保護設定を変更し、値を書き換え可能にします。
    // 実装理由：暗闇処理の命令コード（2バイト）を書き換えることで、暗闇エフェクトをバイパスさせるため。
    VirtualProtect(g_addrAntiDarkness, 2, PAGE_EXECUTE_READWRITE, &oldProtect);

    if (enabled) {
        // 元の命令 "8B 02" (mov eax, [rdx]) を NOP ("90 90") に書き換えて無効化します。
        g_addrAntiDarkness[0] = static_cast<std::byte>(0x90);
        g_addrAntiDarkness[1] = static_cast<std::byte>(0x90);
    } else {
        // 元の命令 "8B 02" に戻して通常の暗闇処理を復元します。
        g_addrAntiDarkness[0] = static_cast<std::byte>(0x8B);
        g_addrAntiDarkness[1] = static_cast<std::byte>(0x02);
    }

    VirtualProtect(g_addrAntiDarkness, 2, oldProtect, &oldProtect);
}