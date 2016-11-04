#ifndef NEZPLUG_H__
#define NEZPLUG_H__

#ifdef __ANDROID__
#include "output_log.h"
#endif

#include "nestypes.h"
#include "format/songinfo.h"
#include "format/handler.h"
#include "format/audiohandler.h"

#ifdef __cplusplus
extern "C" {
#endif
	
// global(extern)
typedef struct {
		char* title;
		char* artist;
		char* copyright;
		char detail[1024];
} songinfodata_t;
	
extern songinfodata_t songinfodata;

// 音量調節
enum {
    ADJ_VOL_FM1,
    ADJ_VOL_FM2,
    ADJ_VOL_PSG
};
    
typedef void (*ADJUST_DEVICE_VOLUME)(void *,int device, double v);
    
    
// CPU使用率
extern double (*get_cpuusage)(void);

extern Uint32 (*memview_memread)(Uint32 a);
extern int MEM_MAX, MEM_IO, MEM_RAM, MEM_ROM;


// tag
typedef struct NEZPLAY_TAG {
	SONG_INFO *song;
	Uint volume;
	Uint frequency;
	Uint channel;
	NES_AUDIO_HANDLER *nah;
	NES_VOLUME_HANDLER *nvh;
	NES_RESET_HANDLER *(nrh[0x10]);
	NES_TERMINATE_HANDLER *nth;
    ADJUST_DEVICE_VOLUME adj_dev_volume;
	Uint naf_type;
	Uint32 naf_prev[2];
	void *nsf;
	void *gbrdmg;
	void *heshes;
	void *zxay;
	void *kssseq;
	void *sgcseq;
	void *nsdp;

    void *log_ctx;
    
    int turbo;
    int use_fmgen;
    int use_gmc;

    Int32 output2[2];
    Int32 audio_filter;
    
    int LowPassFilterLevel;
    int lowlevel;
    
} NEZ_PLAY;

/* NEZ player */
NEZ_PLAY* NEZNew();
void NEZDelete(NEZ_PLAY*);

Uint NEZLoad(NEZ_PLAY*, Uint8*, Uint);
void NEZSetSongNo(NEZ_PLAY*, Uint uSongNo);
void NEZSetFrequency(NEZ_PLAY*, Uint freq);
void NEZSetChannel(NEZ_PLAY*, Uint ch);
void NEZReset(NEZ_PLAY*);
void NEZSetFilter(NEZ_PLAY *, Uint filter);
void NEZVolume(NEZ_PLAY*, Uint uVolume);
void NEZAdjVolume(NEZ_PLAY*, int device, double v);
    
void NEZAPUVolume(NEZ_PLAY*, Int32 uVolume);
void NEZDPCMVolume(NEZ_PLAY*, Int32 uVolume);
void NEZRender(NEZ_PLAY*, void *bufp, Uint buflen);

Uint NEZGetSongNo(NEZ_PLAY*);
Uint NEZGetSongStart(NEZ_PLAY*);
Uint NEZGetSongMax(NEZ_PLAY*);
Uint NEZGetChannel(NEZ_PLAY*);
Uint NEZGetFrequency(NEZ_PLAY*);
void NEZGetFileInfo(char **p1, char **p2, char **p3, char **p4);

	
double NEZGetCPUUsage(NEZ_PLAY *pNezPlay);

#ifdef __cplusplus
}
#endif

#endif /* NEZPLUG_H__ */
