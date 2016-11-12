#ifndef __RENDER_H__
#define __RENDER_H__

#define MODE_OPM 0
#define MODE_OPLL 1
#define MODE_OPM_FMGEN 2

// BaseClock for OPM/OPLL/PSG
#define RENDER_BC_4M 4000000
#define RENDER_BC_3M57 3579545

// BaseClock for OPNA
#define RENDER_BC_7M98 7987200

// BaseClock for OPL3
#define RENDER_BC_14M3 14318180

enum
{
	RENDER_TYPE_NONE = 0,
    RENDER_TYPE_PSG = 1,
    RENDER_TYPE_SSG,
    RENDER_TYPE_OPM,
    RENDER_TYPE_OPLL,
    RENDER_TYPE_OPNA,
    RENDER_TYPE_OPM_FMGEN,
    RENDER_TYPE_OPL3,
};

enum
{
    RENDER_OUTPUT_NONE = 0,
    RENDER_OUTPUT_INT = 1,
    RENDER_OUTPUT_EXT = 2,
};

typedef struct
{
	int type;
	int baseclock;
	int freq;
	int use_gmc;
	int opm_count;
    float vol;
} RenderSetting;

#ifdef __cplusplus
extern "C" {
#endif

// 初期化と開放
void InitRender(void);
void ReleaseRender(void);

// レンダラのフィルタ等の周波数を設定
void SetRenderFreq(int freq);

// レンダラを追加する
int  AddRender(RenderSetting *rs);

// レンダラのリセット
void ResetRender(int id);

// レンダラの削除
void DeleteRender(int id);

// ボリューム設定
void VolumeRender(int id, int volume);

// 出力先設定
void SetOutputRender(int id, int outdev);


// デバイスへの書き込み
void WriteDevice(int id, int addr, int data);

// デバイスから読み出し
int ReadDevice(int id, int addr);

// 割り込み設定
void SetIrqRender(int id, void (*func)(int));


// バッファに生成する
void DoRender(short *outp, int len);

#ifdef __cplusplus
}
#endif


#endif

