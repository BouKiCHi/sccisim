#ifndef __LOG_H__
#define __LOG_H__

// 拡張子
#define LOG_EXT_NLG ".NLG"
#define LOG_EXT_S98 ".S98"

#ifdef __cplusplus
extern "C" {
#endif

// ログのモード(CreateLogで使用)
enum
{
    LOG_MODE_NLG = 0,
    LOG_MODE_S98,
};

// デバイスのタイプ。数値はS98と同じ(AddMapLOGで使用)
enum
{
    LOG_TYPE_NONE = 0,
    LOG_TYPE_SSG = 1, // YM2149
    LOG_TYPE_OPN,    // YM2203
    LOG_TYPE_OPN2,   // YM2612
    LOG_TYPE_OPNA,   // YM2608
    LOG_TYPE_OPM,    // YM2151
    LOG_TYPE_OPLL,   // YM2413
    LOG_TYPE_OPL,    // YM3526
    LOG_TYPE_OPL2,   // YM3812
    LOG_TYPE_OPL3,   // YMF262
    LOG_TYPE_PSG,    // AY-3-8910
    LOG_TYPE_DCSG,   // DCSG
};

// 優先順位。値が高いほど最初にマップされる。AddMapLOGで使用。
enum
{
  LOG_PRIO_NORM = 0,
  LOG_PRIO_PSG,
  LOG_PRIO_FM,
  LOG_PRIO_OPN,
  LOG_PRIO_OPM,
  LOG_PRIO_OPLL,
  LOG_PRIO_OPL3,
};

enum
{
    LOG_CTC0 = 0,
    LOG_CTC3,
};

enum
{
    LOG_STR_TITLE = 0,
    LOG_STR_ARTIST,
    LOG_STR_GAME,
};


#define LOG_MAXMAP 8

typedef struct {
    int mode;
    void *log_ctx;

    int fm_count;
    int device_count;
    char map[LOG_MAXMAP];
    int freq[LOG_MAXMAP];
} LOGCTX;

// ログの作成
LOGCTX *CreateLOG(const char *file, int mode);

// ログのクローズ
void CloseLOG(LOGCTX *ctx);


// デバイスの追加(返り値はデバイスID)
int AddMapLOG(LOGCTX *ctx, int type, int freq, int prio);

// デバイス追加の終了
void MapEndLOG(LOGCTX *ctx);

// データ書き込み
void WriteLOG_Data(LOGCTX *ctx, int device, int addr, int data);

// 文字列書き込み
void WriteLOG_SetTitle(LOGCTX *ctx, int type, const char *str);


// CTC出力
void WriteLOG_CTC(LOGCTX *ctx, int type, int value);

// 時間出力
void WriteLOG_Timing(LOGCTX *ctx, int us);

// シンク出力
void WriteLOG_SYNC(LOGCTX *ctx);

// ラフモード出力
void SetRoughModeLOG(LOGCTX *ctx, int denom);

// 分母設定
void SetDenomLOG(LOGCTX *ctx, int denom);

// ファイル存在確認
// 1 = 存在
int IsExistLOG(const char *path);

// ファイル名の作成(dest <- name without ext + ext)
void MakeFilenameLOG(char *dest, const char *name, const char *ext);

// 出力ファイル名の作成(dest <- name without ext + ext)
// 0 = 成功, -1 = 失敗
int MakeOutputFileLOG(char *dest, const char *name, const char *ext);

#ifdef __cplusplus
}
#endif


#endif
