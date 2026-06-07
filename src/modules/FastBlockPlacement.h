#pragma once

// マインクラフトの3次元整数ブロック座標を表す構造体
// 実装理由：hooks.cppとFastBlockPlacement.cppの双方でブロック座標（BlockPosポインタ）を正しくキャスト・操作するため。
struct BlockPos {
    int x;
    int y;
    int z;
};

// FastBlockPlacementの有効/無効状態を取得します。
// 実装理由：設定値の保持やメニュー上の表示色切り替えに使用するため。
bool GetFastBlockPlacementEnabled();

// FastBlockPlacementの有効/無効状態を設定します。
// 実装理由：メニューからの操作や起動時の設定ロード時に状態を同期させるため。
void SetFastBlockPlacementEnabled(bool enabled);

// ブロック設置イベント時に、ターゲット座標と設置方向（面）から実際の設置予定Y座標を保存します。
// 引数: gameMode - Gamemodeポインタ, pos - 注視しているブロック座標（BlockPosポインタ）, face - 設置面方向
// 実装理由：手動で置いた最初の1個目の高さ（Y座標）を検出し、その高さを固定値として保存するため。
void OnBlockPlaced(void* gameMode, void* pos, unsigned char face);

// 右クリック押し続け動作とプレイヤーの視線角度を検知し、指定されたY座標で前方6ブロックに自動設置し続けます。
// 実装理由：ゲームメインスレッド（CameraUpdate）から毎フレーム呼ばれ、フリーズやパケット過剰送信を防ぐため。
void UpdateFastPlacement();

// 現在自動設置中（右クリック長押し＆Y高度ロック中）であるかを取得します。
// 実装理由：フックハンドラでゲーム本来の手動設置要求を無効化すべきか判定するため。
bool IsFastPlacementActive();

// 自動設置処理のループが実行中（fnBuildBlock呼び出し中）であるかを取得します。
// 実装理由：自動設置処理自身によるbuildBlock呼び出しは無効化対象から除外するため。
bool IsFastPlacementRunning();



// FastBlockPlacementの設置モードを表す列挙型
// 実装理由：特定の軸(X, Y, Z)の平面を強制固定してブロックを設置するかを管理するため。(Autoモードは廃止されました)
enum PlacementMode {
    MODE_X_AXIS = 0,
    MODE_Y_AXIS = 1,
    MODE_Z_AXIS = 2
};

// 現在のFastBlockPlacementの設置モードを取得します。
// 実装理由：設定ファイルのセーブやコンソールメニューでの表示に用いるため。
PlacementMode GetFastPlacementMode();

// FastBlockPlacementの設置モードを設定します。
// 実装理由：コンソールメニューからの選択や起動時の設定ロード時にモード設定を反映するため。
void SetFastPlacementMode(PlacementMode mode);

// FastBlockPlacementの設置可能距離（何ブロック先まで置くか。1〜5）を取得します。
// 実装理由：設定ファイルのセーブやコンソールメニューでの表示、および設置処理に用いるため。
int GetFastPlacementDistance();

// FastBlockPlacementの設置可能距離（何ブロック先まで置くか。1〜5）を設定します。
// 実装理由：コンソールメニューからの選択や起動時の設定ロード時に設定値を反映するため。
void SetFastPlacementDistance(int distance);

