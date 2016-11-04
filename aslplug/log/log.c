/*********
 LOG.C by BouKiCHi 2015
 2015-02-25
 ********/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "log.h"
#include "nlg.h"
#include "s98x.h"

// ログの作成
LOGCTX *CreateLOG(const char *file, int mode)
{
    LOGCTX *ctx = (LOGCTX *)malloc(sizeof(LOGCTX));

    if (!ctx)
    {
        printf("Failed to malloc!");
        return NULL;
    }
    memset(ctx, 0 ,sizeof(LOGCTX));

    // ログのモードを切り替える
    switch (mode)
    {
        case LOG_MODE_NLG:
            ctx->log_ctx = (void *)CreateNLG(file);
            break;

        default:
            ctx->log_ctx = (void *)CreateS98(file);
            break;
    }

    if (!ctx->log_ctx)
    {
        free(ctx);
        printf("failed to create log file!\n");
        return NULL;
    }

    ctx->mode = mode;

    return ctx;
}

// ログのクローズ
void CloseLOG(LOGCTX *ctx)
{
    if (!ctx)
        return;

    switch (ctx->mode)
    {
        case LOG_MODE_NLG:
            CloseNLG((NLGCTX *)ctx->log_ctx);
            break;

        default:
        case LOG_MODE_S98:
            CloseS98(ctx->log_ctx);
            break;
    }
}

// S98のIDに変換する
static int convS98Type(int type)
{
    switch(type)
    {
        case LOG_TYPE_SSG:
            return S98_SSG;
        case LOG_TYPE_OPN:
            return S98_OPN;
        case LOG_TYPE_OPN2:
            return S98_OPN2;
        case LOG_TYPE_OPNA:
            return S98_OPNA;
        case LOG_TYPE_OPM:
            return S98_OPM;
        case LOG_TYPE_OPLL:
            return S98_OPLL;
        case LOG_TYPE_OPL:
            return S98_OPL;
        case LOG_TYPE_OPL2:
            return S98_OPL2;
        case LOG_TYPE_OPL3:
            return S98_OPL3;
        case LOG_TYPE_PSG:
            return S98_PSG;
        case LOG_TYPE_DCSG:
            return S98_DCSG;
        default:
            return S98_NONE;
    }

}

// デバイスの追加(返り値はデバイスID)
int AddMapLOG(LOGCTX *ctx, int type, int freq, int prio)
{
    int idx = 0;

    if (!ctx)
        return -1;

    idx = ctx->device_count;

    switch (ctx->mode)
    {
        case LOG_MODE_NLG:
            if (type == LOG_TYPE_PSG || type == LOG_TYPE_SSG)
            {
                idx = CMD_PSG;
                ctx->device_count++;
            }
            else
            {
                idx = CMD_FM1 + ctx->fm_count;
                ctx->fm_count++;
                ctx->device_count++;
            }
        break;
        case LOG_MODE_S98:
            idx = AddMapS98(ctx->log_ctx, convS98Type(type), freq, prio);
        break;
    }

    return idx;
}


// マップ終了
void MapEndLOG(LOGCTX *ctx)
{
    if (!ctx)
        return;

    switch (ctx->mode)
    {
        case LOG_MODE_NLG:
            break;
        case LOG_MODE_S98:
            MapEndS98(ctx->log_ctx);
            break;
    }


}


// データ書き込み
void WriteLOG_Data(LOGCTX *ctx, int device, int addr, int data)
{
    if (!ctx)
        return;

#ifdef LOG_DUMP
    printf("id:%d addr:%03x data:%02x\n", device, addr, data);
#endif

    switch (ctx->mode)
    {
        case LOG_MODE_NLG:
            WriteNLG_Data(ctx->log_ctx, device, addr, data);
            break;
        case LOG_MODE_S98:
            WriteDataS98(ctx->log_ctx, device, addr, data);
            break;
    }
}

//
int convTAGtype(int type)
{
    switch (type) {
        case LOG_STR_ARTIST:
            return S98_STR_ARTIST;
        case LOG_STR_TITLE:
            return S98_STR_TITLE;
        case LOG_STR_GAME:
            return S98_STR_GAME;
    }
    return -1;
}

// データ書き込み
void WriteLOG_SetTitle(LOGCTX *ctx, int type, const char *str)
{
    if (!ctx)
        return;

    switch (ctx->mode)
    {
        case LOG_MODE_NLG:
            break;
        case LOG_MODE_S98:
            WriteStringS98(ctx->log_ctx, convTAGtype(type), str);
            break;
    }
}

// CTC出力
void WriteLOG_CTC(LOGCTX *ctx, int type, int value)
{
    if (!ctx)
        return;

    switch (ctx->mode)
    {
        case LOG_MODE_NLG:
            if (type == LOG_CTC0)
                WriteNLG_CTC(ctx->log_ctx, CMD_CTC0, value);
            else
                WriteNLG_CTC(ctx->log_ctx, CMD_CTC3, value);
            break;
        case LOG_MODE_S98:
            // TODO:ctcの値をベースにusに変換
            break;
    }
}

// ログティック設定(マイクロ秒単位)
void WriteLOG_Timing(LOGCTX *ctx, int us)
{
    if (!ctx)
        return;

    switch (ctx->mode)
    {
        case LOG_MODE_NLG:
            WriteNLG_CTC(ctx->log_ctx, CMD_CTC0, (int)(us / 128));
            WriteNLG_CTC(ctx->log_ctx, CMD_CTC3, 2);
            break;
        case LOG_MODE_S98:
            SetTimingS98(ctx->log_ctx, us);
            break;
        default:
            break;
    }

}

// シンク出力
void WriteLOG_SYNC(LOGCTX *ctx)
{
    if (!ctx)
        return;

    switch (ctx->mode)
    {
        case LOG_MODE_NLG:
            WriteNLG_IRQ(ctx->log_ctx);
            break;
        case LOG_MODE_S98:
            WriteSyncS98(ctx->log_ctx);
            break;
    }

}


// ラフモード出力
void SetRoughModeLOG(LOGCTX *ctx, int denom)
{
    if (!ctx)
        return;

    switch (ctx->mode)
    {
        case LOG_MODE_NLG:
            break;
        case LOG_MODE_S98:
            SetDenomS98(ctx->log_ctx, denom);
            break;
    }
}

// 分母設定
void SetDenomLOG(LOGCTX *ctx, int denom)
{
    if (!ctx)
      return;

    switch (ctx->mode)
    {
      case LOG_MODE_NLG:
        break;
      case LOG_MODE_S98:
        SetDenomS98(ctx->log_ctx, denom);
        break;
    }
}

// ファイル存在確認
// 1 = 存在
int IsExistLOG(const char *path)
{
  FILE *fp = fopen(path, "rb");

  if (fp)
  {
    fclose(fp);
    return 1;
  }

  return 0;
}

// ファイル名の作成(dest <- name without ext + ext)
void MakeFilenameLOG(char *dest, const char *name, const char *ext)
{
    strcpy(dest, name);
    char *p = strrchr(dest, '.');

    if (p)
      strcpy(p, ext);
    else
      strcat(dest, ext);
}

// 出力ファイル名の作成(dest <- name without ext + ext)
// 0 = 成功, -1 = 失敗
int MakeOutputFileLOG(char *dest, const char *name, const char *ext)
{
  int i;
  char temp[512];

  strcpy(temp, name);
  char *p = strrchr(temp, '.');

  if (p)
    *p = 0;

  sprintf(dest, "%s%s", temp, ext);

  if (!IsExistLOG(dest))
    return 0;

  for(i = 0; i < 1000; i++)
  {
    sprintf(dest, "%s.%03d%s", temp, i, ext);
    if (!IsExistLOG(dest))
      return 0;
  }

  dest[0] = 0;

  return -1;
}
