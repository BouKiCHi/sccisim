#include "kmsnddev.h"
#include "s_opm.h"
#include "s_logtbl.h"

#include "ym2151/ym2151.h"

#include <stdio.h>

#ifdef USE_C86XDRV
#include "gmcdrv.h"
#endif

#define CPS_SHIFT 18
#define CPS_ENVSHIFT 12

typedef struct
{
	KMIF_SOUND_DEVICE kmif;

	struct {
		Int32 mastervolume;
		Uint32 davolume;
		Uint32 envout;
		Uint8 daenable;
		Uint8 regs[1];
		Uint8 rngout;
		Uint8 adr;
	} common;
	Uint8 type;
	
    double adjvol;
	int opm_addr;
	void *opm_ctx;
	char *mask;
    
    int map_opm;
    
} OPMSOUND;


// #define VOL_OPM 128
#define VOL_OPM 256

static void sndsynth(void *p, Int32 *dest)
{
	OPMSOUND *sndp = (OPMSOUND *)(p);
	
	SAMP *bufp[2];
	SAMP buf[2];
	
	bufp[0] = buf;
	bufp[1] = buf+1;

    YM2151UpdateOne(sndp->opm_ctx, bufp, 1);
    
    buf[0] = (double)buf[0] * sndp->adjvol;
    buf[1] = (double)buf[1] * sndp->adjvol;
    
    dest[0] += (buf[0] * VOL_OPM);
    dest[1] += (buf[1] * VOL_OPM);
}

static void sndvolume(void *p, Int32 volume)
{
	OPMSOUND *sndp = (OPMSOUND *)(p);

	volume = (volume << (LOG_BITS - 8)) << 1;
	sndp->common.mastervolume = volume;
}

static void sndadjvolume(void *p, double volume)
{
    OPMSOUND *sndp = (OPMSOUND *)(p);
    sndp->adjvol = volume;
}

static Uint32 sndread(void *p, Uint32 a)
{
    OPMSOUND *sndp = (OPMSOUND *)(p);
	// printf("a:%04x\n",a);

	return YM2151ReadStatus(sndp->opm_ctx);
}

static void sndwrite(void *p, Uint32 a, Uint32 v)
{
	OPMSOUND *sndp = (OPMSOUND *)(p);
    
	switch(a & 0x1)
	{
		// OPM
		case 0x00:
			sndp->opm_addr = v;
		break;
		case 0x01:
            
            if (sndp->kmif.logwrite)
                sndp->kmif.logwrite(sndp->kmif.log_ctx, sndp->kmif.log_id, sndp->opm_addr, v);

#ifdef USE_C86XDRV
            if (sndp->kmif.output_device & OUT_EXT)
                c86x_write(sndp->map_opm, sndp->opm_addr, v);
#endif
            if (sndp->kmif.output_device & OUT_INT)
            	YM2151WriteReg(sndp->opm_ctx, sndp->opm_addr, v);
		break;
	}
}

static void sndreset(void *p, Uint32 clock, Uint32 freq)
{
	int bc = 4000000;
	OPMSOUND *sndp = (OPMSOUND *)(p);
	
	// printf("clk:%d freq:%d\n", clock, freq);
	
    if (sndp->opm_ctx)
    {
        YM2151Shutdown( sndp->opm_ctx );
        sndp->opm_ctx = NULL;
    }
    
    sndp->opm_ctx = YM2151Init(1, bc, freq);
    YM2151ResetChip(sndp->opm_ctx);
    
#ifdef USE_C86XDRV
    c86x_reset(sndp->map_opm);
    c86x_setPLL(sndp->map_opm, bc);
#endif

    if (sndp->mask)
        YM2151SetMask(sndp->opm_ctx, sndp->mask);
}

static void sndrelease(void *p)
{
	OPMSOUND *sndp = (OPMSOUND *)(p);
	
    if ( sndp->opm_ctx )
    {
        YM2151Shutdown( sndp->opm_ctx );
        sndp->opm_ctx = NULL;
    }

	XFREE(sndp);
}

/*static void setmask(void *p, int dev, char *mask)
{
    OPMSOUND *sndp = (OPMSOUND *)(p);
    
	if (sndp->opm_ctx)
	{
		YM2151SetMask(sndp->opm_ctx, mask);
	}
	sndp->mask = mask;
}
*/

static void setirq(void *p, void (*irq)(int))
{
    OPMSOUND *sndp = (OPMSOUND *)(p);
    
    YM2151SetIrqHandler(sndp->opm_ctx, irq);
}


static void setinst(void *ctx, Uint32 n, void *p, Uint32 l){}

KMIF_SOUND_DEVICE *OPMSoundAlloc(int count)
{
	OPMSOUND *sndp;
	sndp = (OPMSOUND *)(XMALLOC(sizeof(OPMSOUND)));
	if (!sndp) return 0;

	sndp = (OPMSOUND *)(XMEMSET(sndp, 0, sizeof(OPMSOUND)));


	sndp->kmif.ctx = sndp;
	sndp->kmif.release = sndrelease;
	sndp->kmif.reset = sndreset;
	sndp->kmif.synth = sndsynth;
	sndp->kmif.volume = sndvolume;
    sndp->kmif.write = sndwrite;
	sndp->kmif.read = sndread;
	sndp->kmif.setinst = setinst;
    sndp->kmif.setirq = setirq;
    
    sndp->kmif.adjust_volume = sndadjvolume;
    sndp->adjvol = 1;
    
    // sndp->kmif.setmask = setmask;
	
#ifdef USE_C86XDRV
    sndp->map_opm = c86x_getchip(C86XDRV_OPM, count);
#endif
    
	return &sndp->kmif;
}
