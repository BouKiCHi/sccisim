// OPM for FMGEN

#include "kmsnddev.h"
#include "s_opm_gen.h"
#include "fmgen/opm.h"

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
	
	Uint8 opm_addr;
	char *mask;

    double adjvol;
    
    FM::OPM *opm_inst;
	
} OPM_FMGEN;


// #define VOL_OPM 128
#define VOL_OPM 256

static void sndsynth(void *p, Int32 *dest)
{
	OPM_FMGEN *sndp = (OPM_FMGEN *)(p);
	
    FM::Sample buf[2];
    
    buf[0] = 0;
    buf[1] = 0;
	
    sndp->opm_inst->Mix(buf, 1);
    buf[0] = (double)buf[0] * sndp->adjvol;
    buf[1] = (double)buf[1] * sndp->adjvol;
    
    dest[0] += (buf[0] * VOL_OPM);
    dest[1] += (buf[1] * VOL_OPM);
}

static void sndvolume(void *p, Int32 volume)
{
}

static void sndadjvol(void *p, double volume)
{
    OPM_FMGEN *sndp = (OPM_FMGEN *)(p);
    sndp->adjvol =volume;
}


static Uint32 sndread(void *p, Uint32 a)
{
	// printf("a:%04x\n",a);

	return 0;
}

static void sndwrite(void *p, Uint32 a, Uint32 v)
{
    OPM_FMGEN *sndp = (OPM_FMGEN *)(p);

    // int chip = (a / 0x10);
    
	switch(a & 0x1)
	{
		// OPM
		case 0x00:
			sndp->opm_addr = v;
		break;
		case 0x01:
			//if (nes_logfile)
			//	fprintf(nes_logfile,"OPM:%d:%02X:%02X\n",
			//		chip, sndp->opm_addr, v);
            
            sndp->opm_inst->SetReg(sndp->opm_addr, v);

		break;
	}
}

static void sndreset(void *p, Uint32 clock, Uint32 freq)
{
//	int bc = 4000000;
    OPM_FMGEN *sndp = (OPM_FMGEN *)(p);
	
	// printf("clk:%d freq:%d\n", clock, freq);
	
    if ( sndp->opm_inst )
    {
        delete sndp->opm_inst;
        sndp->opm_inst = NULL;
    }
    
    sndp->opm_inst = new FM::OPM();
    sndp->opm_inst->Init(clock, freq);
    sndp->opm_inst->Reset();
		
}

static void sndrelease(void *p)
{
    OPM_FMGEN *sndp = (OPM_FMGEN *)(p);
	
    if ( sndp->opm_inst )
    {
        delete sndp->opm_inst;
        sndp->opm_inst = NULL;
    }
    

	XFREE(sndp);
}

/*static void setmask(void *p, int dev, char *mask)
{
}*/

static void setinst(void *ctx, Uint32 n, void *p, Uint32 l){}

KMIF_SOUND_DEVICE *OPM_FMGen_SoundAlloc(void)
{
    OPM_FMGEN *sndp;
	sndp = (OPM_FMGEN *)(XMALLOC(sizeof(OPM_FMGEN)));
	if (!sndp) return 0;

	sndp = (OPM_FMGEN *)(XMEMSET(sndp,0,sizeof(OPM_FMGEN)));

	sndp->kmif.ctx = sndp;
	sndp->kmif.release = sndrelease;
	sndp->kmif.reset = sndreset;
	sndp->kmif.synth = sndsynth;
	sndp->kmif.volume = sndvolume;
    sndp->kmif.write = sndwrite;
	sndp->kmif.read = sndread;
	sndp->kmif.setinst = setinst;
    // sndp->kmif.setmask = setmask;
    
    sndp->kmif.adjust_volume = sndadjvol;
    sndp->adjvol = 1;
	
	return &sndp->kmif;
}
