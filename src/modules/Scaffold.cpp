#include "Scaffold.h"
#include "FastBlockPlacement.h" // BlockPos 構造体を利用するため
#include <windows.h>
#include <cstddef>

static bool g_enabled = false;

// buildBlock のトランポリン関数型定義
typedef bool(__fastcall* BuildBlockHook_t)(void* rcx, void* rdx, unsigned char r8, unsigned char r9);

// 外部モジュールで特定済みのアドレス、状態変数、およびオリジナルトランポリンを参照します。
extern BuildBlockHook_t g_origBuildBlock;
extern int g_playerBlockX;
extern int g_playerBlockY;
extern int g_playerBlockZ;
extern bool g_isPlacingBlock; // hooks.cppで定義されているグローバル再入ガードフラグ

bool GetScaffoldEnabled() {
    return g_enabled;
}

void SetScaffoldEnabled(bool enabled) {
    g_enabled = enabled;
}

void UpdateScaffold(void* gameMode) {
    // 既にブロック設置処理（手動または別の自動設置）が実行中の場合は、再入クラッシュを防ぐためスキップします。
    if (!g_enabled || !g_origBuildBlock || !gameMode || g_isPlacingBlock) {
        return;
    }

    // 再入ガードを設定して自動設置処理を実行します。
    // 実装理由：Scaffoldの連続設置処理中に発生する位置同期イベントから再帰的にScaffoldが呼ばれて
    // スタックやゲーム内部バッファが破壊されてクラッシュするのを防ぐため。
    g_isPlacingBlock = true;

    // プレイヤーの足元の座標（プレイヤーがいる座標の1ブロック下＝Y-2）
    // 実装理由：プレイヤーの移動に追従して、常に足元（落下防止となる高さ）を足場ブロックで埋め続けるため。
    int x = g_playerBlockX;
    int y = g_playerBlockY - 2;
    int z = g_playerBlockZ;

    BlockPos pos{ x, y, z };

    // g_origBuildBlock（フックバイパス用のオリジナルトランポリン）を直接呼び出すように変更します。
    // 実装理由：フックが仕込まれた元のg_addrBuildBlockを直接呼ぶと、フックハンドラを再入してしまい、
    // レジスタの破壊や他モジュールによる座標書き換えが発生してクラッシュするため、安全にバイパスさせます。
    for (unsigned char face = 1; face <= 5; ++face) {
        if (face == 1) {
            BlockPos offsetPos{ x, y, z + 1 };
            g_origBuildBlock(gameMode, &offsetPos, 2, false);
            continue;
        }
        g_origBuildBlock(gameMode, &pos, face, false);
    }

    g_isPlacingBlock = false; // 処理完了後にフラグを解除します
}
