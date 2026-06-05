#include "creativeNoClip.h"
#include "../scans/scans.h"
#include <windows.h>

static bool g_enabled = false;

bool GetCreativeNoClipEnabled() {
    return g_enabled;
}

void SetCreativeNoClipEnabled(bool enabled) {
    g_enabled = enabled; // まず状態変数を確実に更新します。

    if (!g_addrCreativeNoClip) return; // アドレス特定前であれば、実際のメモリ書き換えを行わずに戻ります。

    DWORD oldProtect;
    // メモリ保護設定を変更し、値を書き換え可能にします。
    // 実装理由：ゲーム内のNoClip判定フラグ（1バイト）を書き換え、壁抜け状態をON/OFFするため。
    VirtualProtect(g_addrCreativeNoClip, 1, PAGE_EXECUTE_READWRITE, &oldProtect);

    if (enabled) {
        // 壁抜けフラグを有効（0x01）にします
        memset(g_addrCreativeNoClip, 0x01, 1);
    } else {
        // 壁抜けフラグを無効（0x00）に戻します
        memset(g_addrCreativeNoClip, 0x00, 1);
    }

    VirtualProtect(g_addrCreativeNoClip, 1, oldProtect, &oldProtect);
}