
#include "kmsnddev.h"
#include "s_opl4.h"
#include "s_logtbl.h"

#include "mame/driver.h"
#include "mame/ymf262.h"

#include <stdio.h>

#ifdef USE_C86XDRV
#include "gmcdrv.h"
#endif

typedef struct
{
	KMIF_SOUND_DEVICE kmif;
    KMIF_LOGTABLE *logtbl;

	Uint8 type;
	
	int addr[2];
	
	void *chip_ctx;
	char *mask;
    Int32 volume;
    
    int use_gmc;
    int map_opl3;

} OPL3SOUND;


// #define VOL_OPM 128
#define VOL_CHIP 512

static void sndsynth(void *p, Int32 *dest)
{
	OPL3SOUND *sndp = (OPL3SOUND *)(p);
	
	OPL3SAMPLE *bufp[4];
	OPL3SAMPLE buf[4];
	
	bufp[0] = buf;
	bufp[1] = buf + 1;
    bufp[2] = buf + 2;
    bufp[3] = buf + 3;

    ymf262_update_one(sndp->chip_ctx, bufp, 1);
    
    Int32 lout = 0;
    Int32 rout = 0;
    
    
    lout += (buf[0] * VOL_CHIP);
    rout += (buf[1] * VOL_CHIP);
    
    // lout += (buf[2] * VOL_CHIP);
    // rout += (buf[3] * VOL_CHIP);
    
    dest[0] += lout;
    dest[1] += rout;
    

}

static void sndvolume(void *p, Int32 volume)
{
	OPL3SOUND *sndp = (OPL3SOUND *)(p);
	sndp->volume = (volume << (LOG_BITS - 8)) << 1;
}


static Uint32 sndread(void *p, Uint32 a)
{
	OPL3SOUND *sndp = (OPL3SOUND *)(p);
    
    return ymf262_read(sndp->chip_ctx, a);
}

static void sndwrite(void *p, Uint32 a, Uint32 v)
{
	OPL3SOUND *sndp = (OPL3SOUND *)(p);
    
    if (a & 1)
    {
    	int addr = 0x00;
    	if (a & 2)
    		addr = sndp->addr[1] | 0x100;
    	else
    		addr = sndp->addr[0];
    	
        // data
        if (sndp->kmif.logwrite)
            sndp->kmif.logwrite(
            	sndp->kmif.log_ctx, sndp->kmif.log_id, addr, v);
        
#ifdef USE_C86XDRV
        if (sndp->kmif.output_device & OUT_EXT)
            c86x_write(sndp->map_opl3, addr, v);
#endif
    }
    else
    {
        // address
        if (a & 2)
            sndp->addr[1] = v;
        else
            sndp->addr[0] = v;
    }

    if (sndp->kmif.output_device & OUT_INT)
        ymf262_write(sndp->chip_ctx, a & 3, v);
}

static void sndreset(void *p, Uint32 clock, Uint32 freq)
{
	int bc = BASECLOCK_OPL3;
    
	OPL3SOUND *sndp = (OPL3SOUND *)(p);
	
	// printf("clk:%d freq:%d\n", clock, freq);
	
    if ( sndp->chip_ctx )
    {
        ymf262_shutdown( sndp->chip_ctx );
        sndp->chip_ctx = NULL;
    }
    
    sndp->chip_ctx = ymf262_init(NULL, bc, freq);
    ymf262_reset_chip(sndp->chip_ctx);
    
#ifdef USE_C86XDRV
    if (sndp->use_gmc)
    {
        c86x_reset(sndp->map_opl3);
    }
#endif
		
    // if (sndp->mask)
    //    YM2151SetMask(sndp->chip_ctx, sndp->mask);
}

static void sndrelease(void *p)
{
	OPL3SOUND *sndp = (OPL3SOUND *)(p);
	
    if ( sndp && sndp->chip_ctx )
    {
        ymf262_shutdown( sndp->chip_ctx );
        sndp->chip_ctx = NULL;
    }
    if ( sndp )
    {
        if (sndp->logtbl) sndp->logtbl->release(sndp->logtbl->ctx);
    }


	XFREE(sndp);
}

/*static void setmask(void *p, int dev, char *mask)
{
    OPL3SOUND *sndp = (OPL3SOUND *)(p);
    
	if (sndp->chip_ctx)
	{
		YM2151SetMask(sndp->chip_ctx, mask);
	}
    
	sndp->mask = mask;
}
*/

static void setinst(void *ctx, Uint32 n, void *p, Uint32 l){}

KMIF_SOUND_DEVICE *OPL3SoundAlloc(void)
{
	OPL3SOUND *sndp;
	sndp = (OPL3SOUND *)(XMALLOC(sizeof(OPL3SOUND)));
	if (!sndp) return 0;

	sndp = (OPL3SOUND *)(XMEMSET(sndp,0,sizeof(OPL3SOUND)));

	sndp->kmif.ctx = sndp;
	sndp->kmif.release = sndrelease;
	sndp->kmif.reset = sndreset;
	sndp->kmif.synth = sndsynth;
	sndp->kmif.volume = sndvolume;
	sndp->kmif.write = sndwrite;
	sndp->kmif.read = sndread;
	sndp->kmif.setinst = setinst;
	
    sndp->logtbl = LogTableAddRef();
    if (!sndp->logtbl)
    {
        sndrelease(sndp);
        return 0;
    }
    
#ifdef USE_C86XDRV
    sndp->map_opl3 = c86x_getchip(C86XDRV_OPL3, 0);
#endif
    
	return &sndp->kmif;
}
