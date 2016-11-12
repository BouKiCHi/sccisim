//
// render.c by BouKiCHi 
// a part from nezplug++ 
//

#include <stdio.h>

#include "neserr.h"
#include "handler.h"
#include "audiosys.h"
#include "songinfo.h"

#include "device/s_psg.h"
#include "device/s_opm.h"
#include "device/opl/s_opl.h"

#ifdef USE_OPL3
#include "device/mame/driver.h"
#include "device/mame/ymf262.h"
#include "device/s_opl4.h"
#endif

#ifdef USE_FMGEN

#include "s_opm_gen.h"
#include "s_opna_gen.h"

#endif

#include "render.h"

#define SHIFT_BITS 8

#define DEV_MAX 8


#define FILTER_LPF 0
#define FILTER_WEIGHTED 1

static Uint naf_type = FILTER_LPF;
static Uint32 naf_prev[2] = { 0x8000, 0x8000 };

Int32 output2[2];
Int32 audio_filter;
int LowPassFilterLevel=8,lowlevel;
int audio_freq;

static struct {
	KMIF_SOUND_DEVICE *kmif;
	int bc;
	int freq;
	float vol;
} dev[DEV_MAX];

// static KMIF_SOUND_DEVICE *dev[DEV_MAX];
static int dev_count = 0;

void WriteDevice(int id, int addr, int data)
{
    if (id < 0 || !dev[id].kmif)
        return;

#ifdef LOG_DUMP
    printf("id:%d addr:%03x data:%02x\n", id, addr, data);
#endif
    
	if (addr < 0x100)
	{
	    dev[id].kmif->write(dev[id].kmif->ctx, 0, addr);
		dev[id].kmif->write(dev[id].kmif->ctx, 1, data);
	}
	else
	{
	    dev[id].kmif->write(dev[id].kmif->ctx, 2, addr);
		dev[id].kmif->write(dev[id].kmif->ctx, 3, data);
	}
}

int ReadDevice(int id, int addr)
{
    if (id < 0 || !dev[id].kmif)
        return 0;
    
    return dev[id].kmif->read(dev[id].kmif->ctx, addr);
}


void DoRender(short *outp, int len)
{
	int i,j,ch;
	Int32 accum[2] = { 0, 0 };
	// Int32 b[3] = { 0, 0, 0 };

	Uint32 output[2];

	for(i = 0; i < len; i++)
	{
		accum[0] = accum[1] = 0;

		for(j = 0; j < dev_count; j++) {
			Int32 tmp[2] = { 0, 0 };

			if (dev[j].kmif)
				dev[j].kmif->synth(dev[j].kmif->ctx, tmp);

			accum[0] += (dev[j].vol * tmp[0]);
			accum[1] += (dev[j].vol * tmp[1]);
		}

		for(ch=0; ch < 2; ch++)
		{
			accum[ch] += (0x10000 << SHIFT_BITS);
			if (accum[ch] < 0)
				output[ch] = 0;
			else if (accum[ch] > (0x20000 << SHIFT_BITS) - 1)
				output[ch] = (0x20000 << SHIFT_BITS) - 1;
			else
				output[ch] = accum[ch];
			output[ch] >>= SHIFT_BITS;
			
			if(!(naf_type&4)){
					Int32 buffer;
					if (output2[ch] == 0x7fffffff){
						output2[ch] = ((Int32)output[ch]<<14) - 0x40000000;
						//output2[ch] *= -1;
					}
					output2[ch] -= (output2[ch] - (((Int32)output[ch]<<14) - 0x40000000))/(64*audio_filter);
					buffer =  output[ch]/2 - output2[ch]/0x8000;

					if (buffer < 0)
						output[ch] = 0;
					else if (buffer > 0xffff)
						output[ch] = 0xffff;
					else
						output[ch] = buffer;
			}else{
					output[ch] >>= 1;
			}
        
            switch (naf_type)
            {
                case FILTER_LPF:
                {
                	Uint32 prev = naf_prev[ch];
					//output[ch] = (output[ch] + prev) >> 1;
					output[ch] = (output[ch] * lowlevel + prev * audio_filter) / (lowlevel+audio_filter);
					naf_prev[ch] = output[ch];
				}
                break;
                case FILTER_WEIGHTED:
                {
							Uint32 prev = naf_prev[ch];
							naf_prev[ch] = output[ch];
							output[ch] = (output[ch] + output[ch] + output[ch] + prev) >> 2;
				}
                break;
            }
			*outp++ = (Int16)(((Int32)output[ch]) - 0x8000);
		}
	}
}


void SetFilterRender(int filter)
{
	naf_type = filter;
	naf_prev[0] = 0x8000;
	naf_prev[1] = 0x8000;
	output2[0] = 0x7fffffff;
	output2[1] = 0x7fffffff;
}

void InitRender(void)
{
	int i;
	for(i = 0; i < DEV_MAX; i++)
		dev[i].kmif = NULL;
    
    dev_count = 0;
    
    SetFilterRender(FILTER_LPF);
}

void SetRenderFreq(int freq)
{
	audio_freq = freq;
	audio_filter = freq / 3000;
	if(!audio_filter) audio_filter=1;

	lowlevel = 33-LowPassFilterLevel;
}

int AddRender(RenderSetting *rs)
{
	const char *gen[] = 
	{
		"NONE",
		"PSG",
		"SSG",
		"OPM",
		"OPLL",
		"OPNA",
		"OPM_FMGEN",
		"OPL3"
	};

    if (dev_count >= DEV_MAX)
    {
        printf("over request!\n");
        return -1;
    }
	
	// ID確保
	int id = -1;
	for(int i = 0; i < dev_count; i++) {
		if (!dev[i].kmif) {
			id = i;
			break;
		}
	}

	if (id < 0) {
		id = dev_count;
		dev_count++;
	}
    
    printf("id:%d chip:[%s] bc:%d freq:%d\n", 
    	id, gen[rs->type], rs->baseclock, rs->freq);

	// RTモード時はタイプを変換する
	if (rs->use_gmc && rs->type == RENDER_TYPE_OPM_FMGEN)
		rs->type = RENDER_TYPE_OPM;

	KMIF_SOUND_DEVICE *kmif = NULL;
    switch (rs->type)
    {
        case RENDER_TYPE_PSG:
            kmif = PSGSoundAlloc(PSG_TYPE_AY_3_8910);
            break;
        case RENDER_TYPE_SSG:
            kmif = PSGSoundAlloc(PSG_TYPE_YM2149);
            break;
        case RENDER_TYPE_OPM:
            kmif = OPMSoundAlloc(rs->opm_count++);
            break;
        case RENDER_TYPE_OPLL:
            kmif = OPLSoundAlloc(OPL_TYPE_OPLL);
            break;
#ifdef USE_OPL3
        case RENDER_TYPE_OPL3:
        	kmif = OPL3SoundAlloc();
        	break;
#endif
#ifdef  USE_FMGEN
        case RENDER_TYPE_OPNA:
            kmif = OPNA_FMGen_SoundAlloc();
            break;
        case RENDER_TYPE_OPM_FMGEN:
            kmif = OPM_FMGen_SoundAlloc();
            break;
#endif
        default:
        	// デバイスが存在しない
        	return -1;
            break;
    }
    
    
    if (kmif)
    {
        kmif->output_device = OUT_INT;
        if (rs->use_gmc)
            kmif->output_device |= OUT_EXT;


        dev[id].kmif = kmif;

        dev[id].vol = rs->vol;
        dev[id].bc = rs->baseclock;
        dev[id].freq = rs->freq;
        ResetRender(id);
    }
    else // 確保失敗
    	id = -1;
    
    return id;
}

// レンダラのリセット
void ResetRender(int id)
{
    if (id < 0 || !dev[id].kmif)
    	return;

    dev[id].kmif->reset(dev[id].kmif->ctx, dev[id].bc, dev[id].freq);
}

// 出力先設定
void SetOutputRender(int id, int outdev)
{
    if (id < 0 || !dev[id].kmif)
        return;
    
    int flags = 0;
    
    if (outdev & RENDER_OUTPUT_INT)
        flags |= OUT_INT;
    
    if (outdev & RENDER_OUTPUT_EXT)
        flags |= OUT_EXT;
    
    dev[id].kmif->output_device = flags;
}


// レンダラのボリューム設定
void VolumeRender(int id, int volume)
{
    if (id < 0 || !dev[id].kmif)
    	return;

	dev[id].kmif->volume(dev[id].kmif->ctx, volume);
}

// レンダラの削除
void DeleteRender(int id)
{
    if (!dev[id].kmif)
        return;
    
    dev[id].kmif->release(dev[id].kmif->ctx);
    dev[id].kmif = NULL;
}

// 割り込み設定
void SetIrqRender(int id, void (*func)(int))
{
    if (!dev[id].kmif || !dev[id].kmif->setirq)
        return;
    
    dev[id].kmif->setirq(dev[id].kmif->ctx, func);
}

// レンダラの開放
void ReleaseRender(void)
{
	int i;
	for(i = 0; i < DEV_MAX; i++) {
        DeleteRender(i);
	}
	
	dev_count = 0;
}


