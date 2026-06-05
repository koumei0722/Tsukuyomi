#pragma once

// creativeNoClipが有効化されているかどうかを取得します。
bool GetCreativeNoClipEnabled();

// creativeNoClipの有効/無効を切り替えます。
// 実装理由：ゲーム内のNoClip判定バイトを書き換えることで、壁抜けができる状態をトグルするため。
void SetCreativeNoClipEnabled(bool enabled);
