/*********
 s98x.c by BouKiCHi 2014
 2015-02-25 Added priority settings.
 2014-12-29

 This file is no warranty, but free to use.
 ********/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "s98x.h"

typedef unsigned char  byte;
typedef unsigned short word;
typedef unsigned long  dword;


static void WriteHeaderS98(S98CTX *ctx);
static void WriteTagS98(S98CTX *ctx);

/*
 // 変数書き出し(WORD)
static void WriteWORD(byte *p, word val)
{
    p[0] = (val & 0xff);
    p[1] = ((val>>8) & 0xff);
}

// 変数読み出し(WORD)
static word ReadWORD(byte *p)
{
	return
 ((word)p[0]) |
 ((word)p[1])<<8;
}

*/

// 変数読み出し(DWORD)
static dword ReadDWORD(byte *p)
{
	return
		((dword)p[0]) |
		((dword)p[1])<<8 |
		((dword)p[2])<<16 |
		((dword)p[3])<<24;
}

// ファイルを開く
S98CTX *OpenS98(const char *file)
{
	int i;
	byte hdr[0x80];

    S98CTX *ctx = (S98CTX *)malloc(sizeof(S98CTX));

    if (!ctx)
    {
        printf("Failed to malloc!");
        return NULL;
    }
    memset(ctx, 0, sizeof(S98CTX));

	ctx->file = fopen(file, "rb");

    if (!ctx->file)
	{
		printf("File open error:%s\n", file);
        free(ctx);
		return NULL;
	}

    ctx->mode = S98_READMODE;

	fread(hdr, 0x20, 1, ctx->file);

	// フォーマット確認
	if (memcmp(hdr, "S98", 3) != 0)
	{
		printf("Invalid S98 header!!\n");
		fclose(ctx->file);

        free(ctx);
		return NULL;
	}

	// バージョン確認
	ctx->version = 1;
	if (hdr[3] == '3')
		ctx->version = 3;

	// タイミング
	ctx->time_nume = (int)ReadDWORD(hdr + 0x04);
	ctx->time_denom = (int)ReadDWORD(hdr + 0x08);

	// デフォルト値の設定
	if (ctx->time_nume == 0)
		ctx->time_nume = 10;

	if (ctx->time_denom == 0)
		ctx->time_denom = 1000;

	// ウェイトをus単位に変換する
	ctx->time_us = (int)(((double)ctx->time_nume * S98_US) / ctx->time_denom);

	// データ位置の取得
	ctx->dump_ptr = (int)ReadDWORD(hdr + 0x14);
	ctx->dump_loop = (int)ReadDWORD(hdr + 0x18);

	// デバイスカウント
	ctx->dev_count = (int)ReadDWORD(hdr + 0x1c);

	// デバイス情報の取得
	for(i = 0; i < ctx->dev_count && i < MAX_S98DEV; i++)
	{
		fread(hdr, 0x10, 1, ctx->file);
		ctx->dev_type[i] = (int)ReadDWORD(hdr + 0x00);
		ctx->dev_freq[i] = (int)ReadDWORD(hdr + 0x04);
	}
	// データ先頭までシーク
	fseek(ctx->file, ctx->dump_ptr, SEEK_SET);

	return ctx;
}

// ファイルを閉じる
void CloseS98(S98CTX *ctx)
{
    if (!ctx)
        return;

    if (ctx->file)
    {
#ifndef S98_READONLY
        if (ctx->mode != S98_READMODE)
        {
            // 終了マーカーを出力
            fputc(S98_CMD_END, ctx->file);

            // タグ情報がある
            if (ctx->tag_info)
            {
                ctx->tag_pos = TellS98(ctx);
                WriteTagS98(ctx);

            }

            WriteHeaderS98(ctx);
        }
#endif
        fclose(ctx->file);
        ctx->file = NULL;
    }

    // タグ
    if (ctx->tag_info)
        free(ctx->tag_info);

    free(ctx);
}


#ifndef S98_READONLY

/////////////////////////////////// S98書き込みルーチン
//
// 変数書き出し(DWORD)
static void WriteDWORD(byte *p, dword val)
{
    p[0] = (val & 0xff);
    p[1] = ((val>>8) & 0xff);
    p[2] = ((val>>16) & 0xff);
    p[3] = ((val>>24) & 0xff);
}

// ヘッダの作成
static void WriteHeaderS98(S98CTX *ctx)
{
	int i;
    byte hdr[0x80];

    memset(hdr, 0, 0x80);
    memcpy(hdr, "S983", 4);

    // 分子
    WriteDWORD(hdr + 0x04, ctx->time_nume);

    // 分母
    WriteDWORD(hdr + 0x08, ctx->time_denom);

    // タグ位置
    WriteDWORD(hdr + 0x10, ctx->tag_pos);


    // ヘッダサイズの計算
    ctx->dump_ptr = (0x20 + (0x10 * ctx->dev_count));

    // ヘッダの先頭
	WriteDWORD(hdr + 0x14, ctx->dump_ptr);
    // ループポインタ
	WriteDWORD(hdr + 0x18, ctx->dump_loop);
    // デバイス数
	WriteDWORD(hdr + 0x1c, ctx->dev_count);

    // ヘッダ書き込み
	fseek(ctx->file, 0, SEEK_SET);
	fwrite(hdr, 0x20, 1, ctx->file);

    // ポインタ
    struct prio_list *ptr = ctx->prio_top;

    // デバイスがない
    if (!ptr)
        return;

    // デバイスマップの作成
    // printf("-- devmap -- \n");

    // 初期化
    for(i = 0; i < MAX_S98DEV; i++)
        ctx->dev_map[i] = -1;

    for(i = 0; i < MAX_S98DEV; i++)
    {
        int idx = ptr->index;

        // printf("log_id:%d -> device:%d\n", idx, i);

        // ptr->index = device_id
        ctx->dev_map[idx] = i;

        // 次が無ければ終了
        if (!ptr->right)
            break;

        ptr = ptr->right;
    }

     ptr = ctx->prio_top;

    // デバイスリスト書き込み
	for(i = 0; i < ctx->dev_count; i++)
	{
		memset(hdr, 0, 0x10);

        int idx = -1;

        if (ptr)
        {
            idx = ptr->index;

            WriteDWORD(hdr + 0x00, ctx->dev_type[idx]);
            WriteDWORD(hdr + 0x04, ctx->dev_freq[idx]);
        }

		fwrite(hdr, 0x10, 1, ctx->file);

        if (ptr)
            ptr = ptr->right;
	}
}

// タグを出力
static void WriteTagS98(S98CTX *ctx)
{
    if (!ctx || !ctx->tag_info)
        return;

    S98CTX_TAG *tag = (S98CTX_TAG *)ctx->tag_info;

    fprintf(ctx->file,"[S98]");
    fprintf(ctx->file,"title=%s", tag->title);
    fputc(0x0a, ctx->file);
    fprintf(ctx->file,"artist=%s", tag->artist);
    fputc(0x0a, ctx->file);
    fprintf(ctx->file,"game=%s", tag->game);
    fputc(0x0a, ctx->file);

    fputc(0x00, ctx->file);

}

// ファイルを開く
S98CTX *CreateS98(const char *file)
{

    S98CTX *ctx = (S98CTX *)malloc(sizeof(S98CTX));

    if (!ctx)
    {
        printf("Failed to malloc!");
        return NULL;
    }

    memset(ctx, 0, sizeof(S98CTX));

    ctx->file = fopen(file, "wb");

    if (!ctx->file)
    {
        printf("File open error:%s\n", file);
        free(ctx);
        return NULL;
    }

    ctx->mode = S98_WRITEMODE;
    ctx->dev_count = 0;
    ctx->def_denom = 1000;

    return ctx;
}

// マップ終了
void MapEndS98(S98CTX *ctx)
{
    WriteHeaderS98(ctx);
}

// ソートマップを作る
void AddSortMapS98(S98CTX *ctx, int id, int prio)
{
    struct prio_list *obj = &ctx->prio_map[ctx->dev_count];

    // 現在のオブジェクトを初期化
    obj->left = NULL;
    obj->right = NULL;
    obj->prio = prio;
    obj->index = id;

    // printf("prio:%d idx:%d\n", prio, id);

    // ポインタの作成
    struct prio_list *ptr = ctx->prio_top;
    struct prio_list *prev = NULL;

    // まだマップなし
    if (!ptr)
    {
        // printf("top\n");
        ctx->prio_top = obj;
        return;
    }

    // 下にたどる
    while(ptr)
    {
        // 優先順位がptrよりも上の場合
        if (prio > ptr->prio)
            break;

        // 次に進める
        prev = ptr;
        ptr = ptr->right;
    }

    /* if (ptr)
        printf("next is %d\n", ptr->index);
    else
        printf("this is bottom");
    */

    // ptrより前に挿入する
    // obj > ptr

    obj->right = ptr;
    obj->left = prev;

    if (prev)
    {
        // printf("prev is %d\n", prev->index);
        prev->right = obj;
    }

    //　これが最後でない場合
    if (ptr)
    {
        ptr->left = obj;
    }

    // トップを更新する
    if (!prev)
    {
        // printf("tops is %d\n", obj->index);
        ctx->prio_top = obj;
    }
}


// デバイスマッピング
int AddMapS98(S98CTX *ctx, int type, int freq, int prio)
{
    if (!ctx)
        return -1;

    int idx = ctx->dev_count;

    // printf("type:%d freq:%d\n", type, freq);

    ctx->dev_type[idx] = type;
    ctx->dev_freq[idx] = freq;

    AddSortMapS98(ctx, idx, prio);

    ctx->dev_count++;

    return idx;

}

#define COPY_STR(dest, str) { strncpy(dest, str, 255); dest[255] = 0; }


// 文字列書き込み
void WriteStringS98(S98CTX *ctx, int type, const char *str)
{
    if (!ctx)
        return;

    // タグ構造体の取得
    if (!ctx->tag_info)
    {
        ctx->tag_info = malloc(sizeof(S98CTX_TAG));

        if (!ctx->tag_info)
            return;

        memset(ctx->tag_info, 0, sizeof(S98CTX_TAG));
    }


    S98CTX_TAG *tag = (S98CTX_TAG *)ctx->tag_info;

    switch (type) {
        case S98_STR_ARTIST:
            COPY_STR(tag->artist, str);
            break;
        case S98_STR_TITLE:
            COPY_STR(tag->title, str);
            break;
        case S98_STR_GAME:
            COPY_STR(tag->game, str);
            break;
    }
}

// データ書き込み
void WriteDataS98(S98CTX *ctx, int id, int addr, int data)
{
    if (id < 0 || !ctx)
        return;

    id = ctx->dev_map[id];

    // レジスタは表と裏が想定されている
    int cmd = id * 2;

    if (addr > 0xff)
        cmd += 1;

    fputc(cmd, ctx->file);
    fputc(addr & 0xff, ctx->file);
    fputc(data & 0xff, ctx->file);
}

// シンク出力
void WriteSyncS98(S98CTX *ctx)
{
    if (!ctx)
        return;

    ctx->sys_time += ctx->sys_step;

    // 積算された時間が単位時間よりも上
    while(ctx->sys_time > ctx->s98_step)
    {
        ctx->sys_time -= ctx->s98_step;
        fputc(S98_CMD_SYNC, ctx->file);
    }
}

// 分母設定
void SetDenomS98(S98CTX *ctx,int denom)
{
    ctx->def_denom = denom;
}

// タイミング設定
void SetTimingS98(S98CTX *ctx, int us)
{
    if (!ctx)
        return;

    // 1秒単位に変換する
    double step_sec = ((double)us / S98_US);

    int denom = ctx->def_denom;

    int nume = (int)(step_sec * denom);

    // S98設定値
    ctx->time_nume = nume;
    ctx->time_denom = denom;

    // 積算用数値
    ctx->sys_step = step_sec;
    ctx->s98_step = ((double)nume/denom);
}


#endif

// データの読み出し
int ReadS98(S98CTX *ctx)
{
	return fgetc(ctx->file);
}

// ファイルポインタの位置を取得
long TellS98(S98CTX *ctx)
{
	return ftell(ctx->file);
}

// ティックの読み出し
int GetTickS98(S98CTX *ctx)
{
    return ctx->time_us;
}

// ティックの読み出し(マイクロ秒)
int GetTickUsS98(S98CTX *ctx)
{
    return ctx->time_us;
}



// デバイス数の読み出し
int GetDeviceCountS98(S98CTX *ctx)
{
    return ctx->dev_count;
}


// デバイスタイプの読み出し
int GetDeviceTypeS98(S98CTX *ctx, int idx)
{
    return ctx->dev_type[idx];
}

// デバイス周波数の読み出し
int GetDeviceFreqS98(S98CTX *ctx, int idx)
{
    return ctx->dev_freq[idx];
}


// 可変長データの読み出し
int ReadS98VV(S98CTX *ctx)
{
	int out = 0;
	int v = 0;
	int s = 0;

	do
	{
		v = fgetc(ctx->file);
		out |= (v & 0x7f) << s;
		s += 7;
	}while(v & 0x80);

	return out + 2;
}
