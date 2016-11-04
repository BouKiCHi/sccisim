/* libnezp by Mamiya */

#ifndef KMSNDDEV_H__
#define KMSNDDEV_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "../nestypes.h"

typedef void (*FUNC_RELEASE)(void *ctx);
typedef void (*FUNC_RESET)(void *ctx, Uint32 clock, Uint32 freq);
typedef void (*FUNC_SYNTH)(void *ctx, Int32 *p);
typedef void (*FUNC_VOLUME)(void *ctx, Int32 v);
typedef void (*FUNC_ADJUST_VOLUME)(void *ctx, double v);
typedef void (*FUNC_WRITE)(void *ctx, Uint32 a, Uint32 v);
typedef Uint32 (*FUNC_READ)(void *ctx, Uint32 a);
typedef void (*FUNC_SETINST)(void *ctx, Uint32 n, void *p, Uint32 l);

typedef void (*FUNC_LOGWRITE)(void *ctx, int log_id, int a, int v);

typedef void (*FUNC_SETIRQ)(void *ctx, void (*irqfunc)(int));

typedef struct {
	void *ctx;
    FUNC_RELEASE release;
    FUNC_RESET reset;
    FUNC_SYNTH synth;
    FUNC_VOLUME volume;
    FUNC_ADJUST_VOLUME adjust_volume;
    FUNC_WRITE write;
    FUNC_READ read;
    FUNC_SETINST setinst;
    FUNC_LOGWRITE logwrite;
    
    FUNC_SETIRQ setirq;
    
    int output_device;
    
    int log_id;
    void *log_ctx;
    
} KMIF_SOUND_DEVICE;
    

// internal and/or external
#define OUT_INT (1<<0)
#define OUT_EXT (1<<1)

//チャンネルマスク用
enum{//順番を変えたら恐ろしいことになる
	DEV_2A03_SQ1, // 0
	DEV_2A03_SQ2,
	DEV_2A03_TR,
	DEV_2A03_NOISE,
	DEV_2A03_DPCM,

	DEV_FDS_CH1, // 5

	DEV_MMC5_SQ1, // 6
	DEV_MMC5_SQ2,
	DEV_MMC5_DA,

	DEV_VRC6_SQ1, // 9
	DEV_VRC6_SQ2,
	DEV_VRC6_SAW,

	DEV_N106_CH1, // 12
	DEV_N106_CH2,
	DEV_N106_CH3,
	DEV_N106_CH4,
	DEV_N106_CH5,
	DEV_N106_CH6,
	DEV_N106_CH7,
	DEV_N106_CH8,

	DEV_DMG_SQ1, // 20
	DEV_DMG_SQ2,
	DEV_DMG_WM,
	DEV_DMG_NOISE,

	DEV_HUC6230_CH1, // 24
	DEV_HUC6230_CH2,
	DEV_HUC6230_CH3,
	DEV_HUC6230_CH4,
	DEV_HUC6230_CH5,
	DEV_HUC6230_CH6,

	DEV_AY8910_CH1, // 30
	DEV_AY8910_CH2,
	DEV_AY8910_CH3,

	DEV_SN76489_SQ1, // 33
	DEV_SN76489_SQ2,
	DEV_SN76489_SQ3,
	DEV_SN76489_NOISE,

	DEV_SCC_CH1, // 37
	DEV_SCC_CH2,
	DEV_SCC_CH3,
	DEV_SCC_CH4,
	DEV_SCC_CH5,

	DEV_YM2413_CH1, // 42
	DEV_YM2413_CH2,
	DEV_YM2413_CH3,
	DEV_YM2413_CH4,
	DEV_YM2413_CH5,
	DEV_YM2413_CH6,
	DEV_YM2413_CH7,
	DEV_YM2413_CH8,
	DEV_YM2413_CH9,
	DEV_YM2413_BD, // 51
	DEV_YM2413_HH,
	DEV_YM2413_SD,
	DEV_YM2413_TOM,
	DEV_YM2413_TCY,

	DEV_ADPCM_CH1, // 56

	DEV_MSX_DA, // 57
    
    // OPL3
    DEV_OPL3_CH1, // 58
    DEV_OPL3_CH2,
    DEV_OPL3_CH3,
    DEV_OPL3_CH4,
    DEV_OPL3_CH5,
    DEV_OPL3_CH6,
    DEV_OPL3_CH7,
    DEV_OPL3_CH8,
    DEV_OPL3_CH9,
    DEV_OPL3_CH10, // 67
    DEV_OPL3_CH11,
    DEV_OPL3_CH12,
    DEV_OPL3_CH13,
    DEV_OPL3_CH14,
    DEV_OPL3_CH15,
    DEV_OPL3_CH16,
    DEV_OPL3_CH17,
    DEV_OPL3_CH18, // 75
    
	DEV_MAX,
};

    
extern Uint8 chmask[0x200];

#ifdef __cplusplus
}
#endif
#endif /* KMSNDDEV_H__ */


