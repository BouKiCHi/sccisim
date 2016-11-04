/*********
 s98x.h by BouKiCHi 2014
 2014-12-29

 This file is no warranty, but free to use.
 ********/

#ifndef __S98X_H__
#define __S98X_H__

#include <stdio.h>

// #define S98_READONLY

#ifdef _WIN32
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

#define MAX_S98DEV 8

// 優先順位マップ
struct prio_list
{
    struct prio_list *left;
    struct prio_list *right;

    int prio;
    int index;
};

typedef struct {
    FILE *file;
    int version;
    int mode;

    int time_nume; // 分子(デフォルトは10)
    int time_denom; // 分母(デフォルトは1000)
    int time_us; // us単位での1SYNC

    long tag_pos; // タグ位置
    void *tag_info; // タグ情報

    int def_denom; // デフォルト分母

    double s98_step; // S98の単位時間
    double sys_time; // 実際の時間
    double sys_step; // 実際の単位時間


    int dump_ptr; // データ先頭
    int dump_loop; // ループ位置

    int dev_count;
    int dev_type[MAX_S98DEV];
    int dev_freq[MAX_S98DEV];
    int dev_map[MAX_S98DEV];

    struct prio_list prio_map[MAX_S98DEV];
    struct prio_list *prio_top;

} S98CTX;


#define S98_READMODE 0
#define S98_WRITEMODE 1

// 10^6
#define S98_US 1000000

enum S98_DEVID
{
	S98_NONE = 0,
	S98_SSG = 1, // YM2149
	S98_OPN,    // YM2203
	S98_OPN2,   // YM2612
	S98_OPNA,   // YM2608
	S98_OPM,    // YM2151
	S98_OPLL,   // YM2413
	S98_OPL,    // YM3526
	S98_OPL2,   // YM3812
	S98_OPL3,   // YMF262
	S98_PSG,    // AY-3-8910
	S98_DCSG,   // DCSG
};

enum
{
    S98_STR_TITLE = 0,
    S98_STR_ARTIST,
    S98_STR_GAME
};

S98CTX *OpenS98(const char *file);
void CloseS98(S98CTX *ctx);
int ReadS98(S98CTX *ctx);
long TellS98(S98CTX *ctx);
int GetTickS98(S98CTX *ctx);
int GetTickUsS98(S98CTX *ctx);
int GetDeviceCountS98(S98CTX *ctx);
int GetDeviceTypeS98(S98CTX *ctx, int idx);
int GetDeviceFreqS98(S98CTX *ctx, int idx);

int ReadS98VV(S98CTX *ctx);


#ifndef S98_READONLY

typedef struct {
    char title[256];
    char artist[256];
    char game[256];
} S98CTX_TAG;


S98CTX *CreateS98(const char *file);
int AddMapS98(S98CTX *ctx, int type, int freq, int prio);
void MapEndS98(S98CTX *ctx);
void WriteDataS98(S98CTX *ctx, int id, int addr, int data);
void WriteSyncS98(S98CTX *ctx);
void SetTimingS98(S98CTX *ctx, int us);
void SetDenomS98(S98CTX *ctx,int denom);

void WriteStringS98(S98CTX *ctx, int type, const char *str);

#endif


#define S98_CMD_SYNC 0xff
#define S98_CMD_NSYNC 0xfe
#define S98_CMD_END 0xfd


#endif
