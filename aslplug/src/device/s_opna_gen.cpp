// OPNA for FMGEN

#include "kmsnddev.h"

#include "s_opna_gen.h"

#include "fmgen/opna.h"

#include <stdio.h>

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
	
	Uint32 addr;
	char *mask;
    
    FM::OPNA *opna_inst;
	
} OPNA_FMGEN;


// #define VOL_OPM 128
#define VOL_OPNA 512

static void sndsynth(void *p, Int32 *dest)
{
	OPNA_FMGEN *sndp = (OPNA_FMGEN *)(p);
	
    FM::Sample buf[2];
    
    buf[0] = 0;
    buf[1] = 0;
	
    sndp->opna_inst->Mix(buf, 1);

    dest[0] += (buf[0] * VOL_OPNA);
    dest[1] += (buf[1] * VOL_OPNA);
}

static void sndvolume(void *p, Int32 volume)
{
}


static Uint32 sndread(void *p, Uint32 a)
{
	// printf("a:%04x\n",a);

	return 0;
}

static void sndwrite(void *p, Uint32 a, Uint32 v)
{
    OPNA_FMGEN *sndp = (OPNA_FMGEN *)(p);

    // int chip = (a / 0x10);
    
	switch(a & 0x1)
	{
		case 0x00:
			sndp->addr = v;
		break;
		case 0x01:
			//if (nes_logfile)
			//	fprintf(nes_logfile,"OPNA:%d:%02X:%02X\n",
			//		chip, sndp->addr, v);
            
            sndp->opna_inst->SetReg(sndp->addr, v);

		break;
	}
}

static void sndreset(void *p, Uint32 clock, Uint32 freq)
{
//	int bc = 4000000;
    OPNA_FMGEN *sndp = (OPNA_FMGEN *)(p);
	
	// printf("clk:%d freq:%d\n", clock, freq);
	
    if ( sndp->opna_inst )
    {
        delete sndp->opna_inst;
        sndp->opna_inst = NULL;
    }
    
    sndp->opna_inst = new FM::OPNA();
    sndp->opna_inst->Init(clock, freq);
    sndp->opna_inst->Reset();
		
}

static void sndrelease(void *p)
{
    OPNA_FMGEN *sndp = (OPNA_FMGEN *)(p);
	
    if ( sndp->opna_inst )
    {
        delete sndp->opna_inst;
        sndp->opna_inst = NULL;
    }
    

	XFREE(sndp);
}

/*static void setmask(void *p, int dev, char *mask)
{
}*/

static void setinst(void *ctx, Uint32 n, void *p, Uint32 l){}

KMIF_SOUND_DEVICE *OPNA_FMGen_SoundAlloc(void)
{
    OPNA_FMGEN *sndp;
	sndp = (OPNA_FMGEN *)(XMALLOC(sizeof(OPNA_FMGEN)));
	if (!sndp) return 0;

	sndp = (OPNA_FMGEN *)(XMEMSET(sndp,0,sizeof(OPNA_FMGEN)));

	sndp->kmif.ctx = sndp;
	sndp->kmif.release = sndrelease;
	sndp->kmif.reset = sndreset;
	sndp->kmif.synth = sndsynth;
	sndp->kmif.volume = sndvolume;
	sndp->kmif.write = sndwrite;
	sndp->kmif.read = sndread;
	sndp->kmif.setinst = setinst;
    // sndp->kmif.setmask = setmask;
	
	return &sndp->kmif;
}
