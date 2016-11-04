#include "neserr.h"
#include "handler.h"
#include "audiosys.h"
#include "songinfo.h"

#include "m_kss.h"

#include "device/s_psg.h"
#include "device/s_scc.h"
#include "device/s_sng.h"
#include "device/opl/s_opl.h"
#include "device/s_opl4.h"
#include "device/s_opm.h"
#include "device/s_opm_gen.h"


#include "../cpu/kmz80/kmz80.h"

#include <stdio.h>

#include "log.h"

#define SHIFT_CPS 15
#define BASECYCLES       (3579545)
#define BASECYCLES_4M    (4000000)

#define EXTDEVICE_SNG		(1 << 1)

#define EXTDEVICE_FMUNIT		(1 << 0)
#define EXTDEVICE_GGSTEREO	(1 << 2)
#define EXTDEVICE_GGRAM		(1 << 3)

#define EXTDEVICE_MSXMUSIC	(1 << 0)
#define EXTDEVICE_MSXRAM	(1 << 2)
#define EXTDEVICE_MSXAUDIO	(1 << 3)
#define EXTDEVICE_MSXSTEREO	(1 << 4)

#define EXTDEVICE_USE_EXT2	(1 << 5)

#define EXTDEVICE_PAL		(1 << 6)
#define EXTDEVICE_EXRAM		(1 << 7)

#define EXT2_OPL3	(1 << 0)
#define EXT2_OPM	(1 << 1)
#define EXT2_4M 	(1 << 6)
#define EXT2_TURBO	(1 << 7)

#define SND_PSG 0
#define SND_SCC 1
#define SND_MSXMUSIC 2
#define SND_MSXAUDIO 3
#define SND_OPL3 4
#define SND_OPM  5
#define SND_OPM2 6
#define SND_MAX 7

#define SND_SNG 0
#define SND_FMUNIT 2

#define SND_VOLUME 0x800

#define CTC1_INT (1 << 0)
#define CTC3_INT (1 << 1)


typedef struct KSSSEQ_TAG KSSSEQ;

struct  KSSSEQ_TAG {
	KMZ80_CONTEXT ctx;
	KMIF_SOUND_DEVICE *sndp[SND_MAX];
	Uint8 vola[SND_MAX];
	KMEVENT kme;
	KMEVENT_ITEM_ID vsync_id;
	KMEVENT_ITEM_ID ctc3_id;
	KMEVENT_ITEM_ID ctc1_id;
    
  Uint8 ctc_const[4];
  Uint8 ctc_conf[4];
  Uint8 ctc_vect[4];
	Uint8 ctc_flags;

    
	Uint32 bus_vector;

	Uint8 *readmap[8];
	Uint8 *writemap[8];

	double	cps; // サンプルごとのサイクル数
	double	cpsrem; // サンプルごとのサイクル数のあまり
	double	cpf; // フレームごとのサイクル数
	double	cpfrem; // フレームごとのサイクル数のあまり
	
	double	cpu_usage; // CPU利用率
	int			cpu_ratio; // CPU倍数
	
	Uint32 cpu_cycle; // 1秒あたりのCPUクロック数
	Uint32 total_skip_cycles; // スキップクロック数(1秒以内)
	Uint32 total_cycles; // 実行クロック数(1秒以内)
	Uint32 total_sec;	// 再生時間(秒)

	Uint32 startsong;
	Uint32 maxsong;

	Uint32 initaddr;
	Uint32 playaddr;

	Uint8 *data;
	Uint32 dataaddr;
	Uint32 datasize;
	Uint8 *bankbase;
	Uint32 bankofs;
	Uint32 banknum;
	Uint32 banksize;
	enum
	{
		KSS_BANK_OFF,
		KSS_16K_MAPPER,
		KSS_8K_MAPPER
	} bankmode;
	enum
	{
		SYNTHMODE_SMS,
		SYNTHMODE_MSX,
		SYNTHMODE_MSXSTEREO
	} synthmode;
    
	Uint8 sccenable;
	Uint8 rammode;
	Uint8 extdevice;
    Uint8 ext2;
	Uint8 majutushimode;

	Uint8 ram[0x10000];
	Uint8 usertone_enable[2];
	Uint8 usertone[2][16 * 19];
    
    void *log_ctx;
};

Int32 MSXPSGType = 1;
Int32 MSXPSGVolume = 64;

static Uint32 GetWordLE(Uint8 *p)
{
	return p[0] | (p[1] << 8);
}

static Uint32 GetDwordLE(Uint8 *p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

#define GetDwordLEM(p) (Uint32)((((Uint8 *)p)[0] | (((Uint8 *)p)[1] << 8) | (((Uint8 *)p)[2] << 16) | (((Uint8 *)p)[3] << 24)))

/*
void OPLLSetTone(Uint8 *p, Uint32 type)
{
	if ((GetDwordLE(p) & 0xf0ffffff) == GetDwordLEM("ILL0"))
		XMEMCPY(usertone[type], p, 16 * 19);
	else
		XMEMCPY(usertone[type], p, 8 * 15);
	usertone_enable[type] = 1;
}
*/

static Int32 execute(KSSSEQ *THIS_)
{
	// サイクル数計算
	THIS_->cpsrem += THIS_->cps;
	Int32 cycles = (Int32)THIS_->cpsrem;
	THIS_->cpsrem -= kmz80_exec(&THIS_->ctx, cycles);
	
	// HALT時間、クロック数を加算
	THIS_->total_skip_cycles += THIS_->ctx.skip_cycle;
	THIS_->total_cycles += cycles;
	
	// CPU時間を計算する
	if (THIS_->total_cycles >= THIS_->cpu_cycle)
	{
		THIS_->total_skip_cycles = 0;
		
		// 秒数計算を行う(オーバーフロー対策)
		THIS_->total_sec++;
		THIS_->total_cycles -= THIS_->cpu_cycle;
	}
	
	return 0;
}

__inline static void synth(KSSSEQ *THIS_, Int32 *d)
{
	switch (THIS_->synthmode)
	{
		default:
			NEVER_REACH
		case SYNTHMODE_SMS:
			THIS_->sndp[SND_SNG]->synth(THIS_->sndp[SND_SNG]->ctx, d);
			if (THIS_->sndp[SND_FMUNIT]) THIS_->sndp[SND_FMUNIT]->synth(THIS_->sndp[SND_FMUNIT]->ctx, d);
			break;
		case SYNTHMODE_MSX:
			{
				Int32 b[2];
				b[0] = 0;
				THIS_->sndp[SND_PSG]->synth(THIS_->sndp[SND_PSG]->ctx, b);
				b[0] = b[0] * MSXPSGVolume / 64;
				d[0] += b[0];
				d[1] += b[0];
				THIS_->sndp[SND_SCC]->synth(THIS_->sndp[SND_SCC]->ctx, d);
				if (THIS_->sndp[SND_MSXMUSIC]) THIS_->sndp[SND_MSXMUSIC]->synth(THIS_->sndp[SND_MSXMUSIC]->ctx, d);
				if (THIS_->sndp[SND_MSXAUDIO]) THIS_->sndp[SND_MSXAUDIO]->synth(THIS_->sndp[SND_MSXAUDIO]->ctx, d);
                if (THIS_->sndp[SND_OPL3]) THIS_->sndp[SND_OPL3]->synth(THIS_->sndp[SND_OPL3]->ctx, d);
                if (THIS_->sndp[SND_OPM]) THIS_->sndp[SND_OPM]->synth(THIS_->sndp[SND_OPM]->ctx, d);
                if (THIS_->sndp[SND_OPM2]) THIS_->sndp[SND_OPM2]->synth(THIS_->sndp[SND_OPM2]->ctx, d);
			}
			break;
		case SYNTHMODE_MSXSTEREO:
			{
				Int32 b[3];
				b[0] = b[1] = b[2] = 0;
				THIS_->sndp[SND_PSG]->synth(THIS_->sndp[SND_PSG]->ctx, &b[0]);
				b[0] = b[1] = b[0] * MSXPSGVolume / 64;
				THIS_->sndp[SND_SCC]->synth(THIS_->sndp[SND_SCC]->ctx, &b[0]);
				if (THIS_->sndp[SND_MSXMUSIC]) THIS_->sndp[SND_MSXMUSIC]->synth(THIS_->sndp[SND_MSXMUSIC]->ctx, &b[0]);
				if (THIS_->sndp[SND_MSXAUDIO]) THIS_->sndp[SND_MSXAUDIO]->synth(THIS_->sndp[SND_MSXAUDIO]->ctx, &b[1]);

                if (THIS_->sndp[SND_OPL3]) THIS_->sndp[SND_OPL3]->synth(THIS_->sndp[SND_OPL3]->ctx, &b[0]);
                
                if (THIS_->sndp[SND_OPM])
                        THIS_->sndp[SND_OPM]->synth(THIS_->sndp[SND_OPM]->ctx, &b[0]);
                if (THIS_->sndp[SND_OPM2])
                        THIS_->sndp[SND_OPM2]->synth(THIS_->sndp[SND_OPM2]->ctx, &b[0]);


				d[0] += b[1];
				d[1] += b[2] + b[2];
			}
			break;
	}
}

__inline static void volume(KSSSEQ *THIS_, Uint32 v)
{
	v += 256;
	switch (THIS_->synthmode)
	{
		default:
			NEVER_REACH
		case SYNTHMODE_SMS:
			THIS_->sndp[SND_SNG]->volume(THIS_->sndp[SND_SNG]->ctx, v + (THIS_->vola[SND_SNG] << 4) - SND_VOLUME);
			if (THIS_->sndp[SND_FMUNIT]) THIS_->sndp[SND_FMUNIT]->volume(THIS_->sndp[SND_FMUNIT]->ctx, v + (THIS_->vola[SND_FMUNIT] << 4) - SND_VOLUME);
			break;
		case SYNTHMODE_MSX:
		case SYNTHMODE_MSXSTEREO:
			THIS_->sndp[SND_PSG]->volume(THIS_->sndp[SND_PSG]->ctx, v + (THIS_->vola[SND_PSG] << 4) - SND_VOLUME);
			THIS_->sndp[SND_SCC]->volume(THIS_->sndp[SND_SCC]->ctx, v + (THIS_->vola[SND_SCC] << 4) - SND_VOLUME);
			if (THIS_->sndp[SND_MSXMUSIC]) THIS_->sndp[SND_MSXMUSIC]->volume(THIS_->sndp[SND_MSXMUSIC]->ctx, v + (THIS_->vola[SND_MSXMUSIC] << 4) - SND_VOLUME);
			if (THIS_->sndp[SND_MSXAUDIO]) THIS_->sndp[SND_MSXAUDIO]->volume(THIS_->sndp[SND_MSXAUDIO]->ctx, v + (THIS_->vola[SND_MSXAUDIO] << 4) - SND_VOLUME);
			break;
	}
    // X1モード
    if (THIS_->sndp[SND_OPM]) {
        int vol = v + (THIS_->vola[SND_PSG] << 4) - SND_VOLUME;
        vol /= 2;
        THIS_->sndp[SND_PSG]->volume(THIS_->sndp[SND_PSG]->ctx, vol);
    }
    
}

static void vsync_setup(KSSSEQ *THIS_)
{
	Int32 cycles = 0;
	
	// 次のフレームまでの時間を算出
	THIS_->cpfrem += THIS_->cpf;
	cycles = (Int32)THIS_->cpfrem;
	THIS_->cpfrem -= cycles;

	
	kmevent_settimer(&THIS_->kme, THIS_->vsync_id, (Uint32)cycles);
}

static void ctc3_setup(KSSSEQ *THIS_)
{
    int c0 = THIS_->ctc_const[0];
    int c3 = THIS_->ctc_const[3];
    
    // 定数が0の場合は256になる
    if (c0 == 0) c0 = 256;
    if (c3 == 0) c3 = 256;
    
    Int32 cycles = (c0 * 256) * c3;
    
    if (THIS_->ext2 & EXT2_TURBO)
        cycles *= THIS_->cpu_ratio;
    
    kmevent_settimer(&THIS_->kme, THIS_->ctc3_id, (Uint32)cycles);
}

static void ctc1_setup(KSSSEQ *THIS_)
{
    Int32 cycles = 256 * 256;
    
    if (THIS_->ext2 & EXT2_TURBO)
        cycles *= THIS_->cpu_ratio;

    
    kmevent_settimer(&THIS_->kme, THIS_->ctc1_id, (Uint32)cycles);
}



static void play_setup_base(KSSSEQ *THIS_, Uint32 pc, Uint32 sp)
{
	Uint32 rp;
	THIS_->ram[--sp] = 0;

	THIS_->ram[--sp] = 0xfd; // -3 = HALT

	/*
	if (THIS_->ext2 & EXT2_OPM)
		THIS_->ram[--sp] = 0xfd; // -3
	else
		THIS_->ram[--sp] = 0xfe; // -2
	*/
	
	THIS_->ram[--sp] = 0x18;	/* JR */
	THIS_->ram[--sp] = 0x76;	/* HALT */
	rp = sp;
	THIS_->ram[--sp] = (Uint8)(rp >> 8);
	THIS_->ram[--sp] = (Uint8)(rp & 0xff);
	THIS_->ctx.sp = sp;
	THIS_->ctx.pc = pc;
	THIS_->ctx.regs8[REGID_HALTED] = 0;
}

static void play_setup(KSSSEQ *THIS_, Uint32 pc)
{
    if (THIS_->ext2 & EXT2_OPM)
        play_setup_base(THIS_, pc, 0x1000);
    else
        play_setup_base(THIS_, pc, 0xF380);
}

static Uint32 busread_event(void *ctx, Uint32 a)
{
    KSSSEQ *THIS_ = ctx;
 
    Uint32 vec = THIS_->bus_vector;
    
    // CTCの割り込み要因によってベクタが設定される
    if (THIS_->ctc_flags & CTC1_INT)
    {
        THIS_->ctc_flags &= ~CTC1_INT;
        vec = (THIS_->ctc_vect[0] + (1 * 2));
    }
    else
    if (THIS_->ctc_flags & CTC3_INT)
    {
        THIS_->ctc_flags &= ~CTC3_INT;
        vec = (THIS_->ctc_vect[0] + (3 * 2));
    }

    // 同時に割り込みが発生している
    if (THIS_->ctc_flags)
    {
        THIS_->ctx.regs8[REGID_INTREQ] = 1;
    }

    return vec;
}

static Uint32 read_event(void *ctx, Uint32 a)
{
	KSSSEQ *THIS_ = ctx;
	return THIS_->readmap[a >> 13][a];
}



static void KSSMaper8KWrite(KSSSEQ *THIS_, Uint32 a, Uint32 v)
{
	Uint32 page = a >> 13;
	v -= THIS_->bankofs;
	if (v < THIS_->banknum)
	{
		THIS_->readmap[page] = THIS_->bankbase + THIS_->banksize * v - a;
	}
	else
	{
		THIS_->readmap[page] = THIS_->ram;
	}
}

static void KSSMaper16KWrite(KSSSEQ *THIS_, Uint32 a, Uint32 v)
{
	Uint32 page = a >> 13;
	v -= THIS_->bankofs;
	if (v < THIS_->banknum)
	{
		THIS_->readmap[page] = THIS_->readmap[page + 1] = THIS_->bankbase + THIS_->banksize * v - a;
		if (!(THIS_->extdevice & EXTDEVICE_SNG)) THIS_->sccenable = 1;
	}
	else
	{
		THIS_->readmap[page] = THIS_->readmap[page + 1] = THIS_->ram;
		if (!(THIS_->extdevice & EXTDEVICE_SNG)) THIS_->sccenable = !THIS_->rammode;
	}
}

static void write_event(void *ctx, Uint32 a, Uint32 v)
{
	KSSSEQ *THIS_ = ctx;
	Uint32 page = a >> 13;
	if (THIS_->writemap[page])
	{
		THIS_->writemap[page][a] = (Uint8)(v & 0xff);
	}
	else if (THIS_->bankmode == KSS_8K_MAPPER)
	{
		if (a == 0x9000)
			KSSMaper8KWrite(THIS_, 0x8000, v);
		else if (a == 0xb000)
			KSSMaper8KWrite(THIS_, 0xa000, v);
		else if (THIS_->sccenable)
			THIS_->sndp[SND_SCC]->write(THIS_->sndp[SND_SCC]->ctx, a, v);
	}
	else
	{
		if (THIS_->sccenable)
			THIS_->sndp[SND_SCC]->write(THIS_->sndp[SND_SCC]->ctx, a, v);
		else if (THIS_->rammode)
			THIS_->ram[a] = (Uint8)(v & 0xff);
	}
}

static Uint32 ioread_eventSMS(void *ctx, Uint32 a)
{
	return 0xff;
}

static Uint32 ioread_eventX1(void *ctx, Uint32 a)
{
    KSSSEQ *THIS_ = ctx;

    // PSG
    if ((a & 0xF800) == 0x1800)
    {
        return 0x00;
    }
    
    // printf("Read A:%04X \n");
    
    // OPM
    if ((a & 0xFFF4) == 0x700)
    {
        return 0x00;
    }
    
    // Z80 CTC 0x704 or 0x70C
    if (a == 0x704 || a == 0x70c)
    {
        return  (THIS_->ctc_const[0]) & 0xFF;
    }
    return 0xff;
}


static Uint32 ioread_eventMSX(void *ctx, Uint32 a)
{
	KSSSEQ *THIS_ = ctx;
    
    if (THIS_->ext2 & EXT2_OPM)
    {
        return ioread_eventX1(ctx, a);
    }
    
    // 8bit mask
	a &= 0xff;
    
    // OPL3
    if ((a & 0xfc) == 0xc4)
        return 0x00;
    
	if (a == 0xa2)
		return THIS_->sndp[SND_PSG]->read(THIS_->sndp[SND_PSG]->ctx, 0);
	if (THIS_->sndp[SND_MSXAUDIO] && (a & 0xfe) == 0xc0)
		return THIS_->sndp[SND_MSXAUDIO]->read(THIS_->sndp[SND_MSXAUDIO]->ctx, a & 1);
	return 0xff;
}

static void iowrite_eventSMS(void *ctx, Uint32 a, Uint32 v)
{
	KSSSEQ *THIS_ = ctx;
    
    a &= 0xff;
    
	if ((a & 0xfe) == 0x7e)
		THIS_->sndp[SND_SNG]->write(THIS_->sndp[SND_SNG]->ctx, 0, v);
	else if (a == 0x06)
		THIS_->sndp[SND_SNG]->write(THIS_->sndp[SND_SNG]->ctx, 1, v);
	else if (THIS_->sndp[SND_FMUNIT] && (a & 0xfe) == 0xf0)
		THIS_->sndp[SND_FMUNIT]->write(THIS_->sndp[SND_FMUNIT]->ctx, a & 1, v);
	else if (a == 0xfe && THIS_->bankmode == KSS_16K_MAPPER)
		KSSMaper16KWrite(THIS_, 0x8000, v);
}

static void iowrite_eventX1(void *ctx, Uint32 a, Uint32 v)
{
    KSSSEQ *THIS_ = ctx;
    // PSG
    if ((a & 0xF800) == 0x1800)
    {
        a = ((a>>8) & 1);
        v &= 0xff;
        
        THIS_->sndp[SND_PSG]->write(THIS_->sndp[SND_PSG]->ctx, a, v);
        
        return;
    }
    
    // OPM
    if ((a & 0xFFF4) == 0x700)
    {
        int chip = (a & 0x08 ? 1 : 0);
        v &= 0xff;
        
        int opm_id = chip ? SND_OPM2 : SND_OPM;
        
        if (THIS_->sndp[opm_id])
            THIS_->sndp[opm_id]->
            write(THIS_->sndp[opm_id]->ctx, a & 0x01, v);
        
        return;
    }
    
    // Z80 CTC
    if ((a & 0xFFF4) == 0x704)
    {
        int ch = a & 0x03;
        v &= 0xff;
        
        // bit: 76543210
        // bit7 = 割り込みフラグ(1で許可)
        // bit6 = カウンタモード(0でタイマー)
        // bit5 = プリスケーラ(1で1/256、0で1/16)
        // bit4 = トリガ極性(1で立ち上がり)
        // bit3 = トリガ(タイマー時に1で外部トリガ)
        // bit2 = 定数設定(1で定数を書き換える)
        // bit1 = リセット(1で動作停止)
        // bit0 = 設定モード(1で設定、0でベクタ書き換え)
        
        if (THIS_->ctc_conf[ch] & 4)
        {
            // 定数設定モード
            THIS_->ctc_const[ch] = v;
            
            THIS_->ctc_conf[ch] &= ~4;
            
            // 割り込み変更
            if (ch == 1)
                ctc1_setup(THIS_);
            
            if (ch == 0 || ch == 3)
                ctc3_setup(THIS_);
        }
        else
        {
            // ベクタ設定
            if (!(v & 1))
            {
                THIS_->ctc_vect[ch] = v;
                
                // printf("CTC Vect:%02x\n",v);
            }
            else
            {
                THIS_->ctc_conf[ch] = v;
            }
        }
        return;
    }
}

static void iowrite_eventMSX(void *ctx, Uint32 a, Uint32 v)
{
	KSSSEQ *THIS_ = ctx;
    
    if (THIS_->ext2 & EXT2_OPM)
    {
        iowrite_eventX1(ctx, a, v);
        return;
    }
    
    // 8bit
    a &= 0xff;

    // OPL3
    if ((a & 0xfc) == 0xc4)
    {
        if (THIS_->sndp[SND_OPL3])
            THIS_->sndp[SND_OPL3]->write(THIS_->sndp[SND_OPL3]->ctx, a, v);
        return;
    }
    
	if ((a & 0xfe) == 0xa0)
		THIS_->sndp[SND_PSG]->write(THIS_->sndp[SND_PSG]->ctx, a & 1, v);
	else if (a == 0xab)
		THIS_->sndp[SND_PSG]->write(THIS_->sndp[SND_PSG]->ctx, 2, v & 1);
	else if (THIS_->sndp[SND_MSXMUSIC] && (a & 0xfe) == 0x7c)
		THIS_->sndp[SND_MSXMUSIC]->write(THIS_->sndp[SND_MSXMUSIC]->ctx, a & 1, v);
	else if (THIS_->sndp[SND_MSXAUDIO] && (a & 0xfe) == 0xc0)
		THIS_->sndp[SND_MSXAUDIO]->write(THIS_->sndp[SND_MSXAUDIO]->ctx, a & 1, v);
	else if (a == 0xfe && THIS_->bankmode == KSS_16K_MAPPER)
		KSSMaper16KWrite(THIS_, 0x8000, v);
}

static void vsync_event(KMEVENT *event, KMEVENT_ITEM_ID curid, KSSSEQ *THIS_)
{
    if (THIS_->log_ctx)
    {
        WriteLOG_SYNC(THIS_->log_ctx);
    }
    
	vsync_setup(THIS_);
	
	// X1モードではない
  if (!(THIS_->ext2 & EXT2_OPM))
        if (THIS_->ctx.regs8[REGID_HALTED]) play_setup(THIS_, THIS_->playaddr);
}

// CTC1はエフェクト
static void ctc1_event(KMEVENT *event, KMEVENT_ITEM_ID curid, KSSSEQ *THIS_)
{
    if (THIS_->ctc_conf[1] & 0x80)
    {
        THIS_->ctx.regs8[REGID_INTREQ] = 1;
        THIS_->ctc_flags |= CTC1_INT;
        ctc1_setup(THIS_);
    }
}

// CTC3はテンポ
static void ctc3_event(KMEVENT *event, KMEVENT_ITEM_ID curid, KSSSEQ *THIS_)
{
    if (THIS_->ctc_conf[3] & 0x80)
    {
        THIS_->ctx.regs8[REGID_INTREQ] = 1;
        THIS_->ctc_flags |= CTC3_INT;
        ctc3_setup(THIS_);

    }
}


// 現在のコンテキスト
static KSSSEQ* kss_context;



//ここからメモリービュアー設定
extern Uint32 (*memview_memread)(Uint32 a);
extern int MEM_MAX,MEM_IO,MEM_RAM,MEM_ROM;
Uint32 memview_memread_kss(Uint32 a){
	return read_event(kss_context,a);
}
//ここまでメモリービュアー設定

//ここからダンプ設定
// static NEZ_PLAY *pNezPlayDump;
extern Uint32 (*dump_MEM_MSX)(Uint32 a,unsigned char* mem);
static Uint32 dump_MEM_MSX_bf(Uint32 menu,unsigned char* mem){
	int i;
	switch(menu){
	case 1://Memory
		for(i=0;i<0x10000;i++)
			mem[i] = memview_memread_kss(i);
		return i;
	}
	return -2;
}
//----------
extern Uint32 (*ioview_ioread_DEV_AY8910)(Uint32 a);

extern Uint32 (*dump_DEV_AY8910)(Uint32 a,unsigned char* mem);
static Uint32 dump_DEV_AY8910_bf(Uint32 menu,unsigned char* mem){
	int i;
	switch(menu){
	case 1://Register
		for(i=0;i<0x0e;i++)
			mem[i] = ioview_ioread_DEV_AY8910(i);
		return i;

	}
	return -2;
}
//----------
extern Uint32 (*ioview_ioread_DEV_SN76489)(Uint32 a);

Uint32 (*dump_DEV_SN76489)(Uint32 a,unsigned char* mem);
static Uint32 dump_DEV_SN76489_bf(Uint32 menu,unsigned char* mem){
	int i;
	switch(menu){
	case 1://Register
		for(i=0;i<0x10;i++)
			mem[i] = ioview_ioread_DEV_SN76489(i);
		return i;

	}
	return -2;
}
static Uint32 dump_DEV_SN76489_bf2(Uint32 menu,unsigned char* mem){
	int i;
	switch(menu){
	case 1://Register
		for(i=0;i<0x11;i++)
			mem[i] = ioview_ioread_DEV_SN76489(i);
		return i;

	}
	return -2;
}
//----------
extern Uint32 (*ioview_ioread_DEV_SCC)(Uint32 a);

Uint32 (*dump_DEV_SCC)(Uint32 a,unsigned char* mem);
static Uint32 dump_DEV_SCC_bf(Uint32 menu,unsigned char* mem){
	int i;
	switch(menu){
	case 1://Register
		for(i=0;i<0x10;i++)
			mem[i] = ioview_ioread_DEV_SCC(i+0xb0);
		return i;
	case 2://Wave Data - CH1
	case 3://Wave Data - CH2
	case 4://Wave Data - CH3
	case 5://Wave Data - CH4
	case 6://Wave Data - CH5
		for(i=0;i<0x20;i++)
			mem[i] = ioview_ioread_DEV_SCC(i+(menu-2)*0x20);
		return i;

	}
	return -2;
}
//----------
extern Uint32 (*ioview_ioread_DEV_OPL)(Uint32 a);

Uint32 (*dump_DEV_OPL)(Uint32 a,unsigned char* mem);
static Uint32 dump_DEV_OPL_bf(Uint32 menu,unsigned char* mem){
	int i;
	switch(menu){
	case 1://Register
		for(i=0;i<0x100;i++)
			mem[i] = ioview_ioread_DEV_OPL(i);
		return i;

	}
	return -2;
}
//----------
extern Uint32 (*ioview_ioread_DEV_OPLL)(Uint32 a);

extern Uint32 (*dump_DEV_OPLL)(Uint32 a,unsigned char* mem);
static Uint32 dump_DEV_OPLL_bf(Uint32 menu,unsigned char* mem){
	int i;
	switch(menu){
	case 1://Register
		for(i=0;i<0x40;i++)
			mem[i] = ioview_ioread_DEV_OPLL(i);
		return i;

	}
	return -2;
}
//----------
extern Uint32 (*ioview_ioread_DEV_ADPCM)(Uint32 a);
extern Uint32 (*ioview_ioread_DEV_ADPCM2)(Uint32 a);

extern Uint32 (*dump_DEV_ADPCM)(Uint32 a,unsigned char* mem);
static Uint32 dump_DEV_ADPCM_bf(Uint32 menu,unsigned char* mem){
	int i;
	switch(menu){
	case 1://Register
		for(i=0;i<0xc;i++)
			mem[i] = ioview_ioread_DEV_ADPCM(i);
		return i;
	case 3://Memory
		for(i=0;i<0x10000;i++){
			if(ioview_ioread_DEV_ADPCM2(i)==0x100)break;
			mem[i] = ioview_ioread_DEV_ADPCM2(i);
		}
		return i;

	}
	return -2;
}
//----------

// CPU使用率ハンドラ
extern double (*get_cpuusage)(void);

static double kss_calc_cpuusage(void)
{
	double cpu_usage = 0;
	
	cpu_usage = 1 - ((double)kss_context->total_skip_cycles / kss_context->total_cycles);
	cpu_usage *= 100;

	return cpu_usage;
}

static double kss_cpuusage(void)
{
	return kss_calc_cpuusage();
}


// リセット
static void reset(NEZ_PLAY *pNezPlay)
{
	KSSSEQ *THIS_ = pNezPlay->kssseq;
	Uint32 i, freq, song;
    
    // 基本周波数の設定
    int baseclock = BASECYCLES;
    
    if (THIS_->ext2 & EXT2_4M)
        baseclock = BASECYCLES_4M;

	freq = NESAudioFrequencyGet(pNezPlay);
	song = SONGINFO_GetSongNo(pNezPlay->song) - 1;
	if (song >= THIS_->maxsong) song = THIS_->startsong - 1;
    
    /* sound reset */
	if (THIS_->extdevice & EXTDEVICE_SNG)
	{
		if (THIS_->sndp[SND_FMUNIT] && THIS_->usertone_enable[1])
            THIS_->sndp[SND_FMUNIT]->setinst(THIS_->sndp[SND_FMUNIT]->ctx, 0, THIS_->usertone[1], 16 * 19);
	}
	else
	{
		if (THIS_->sndp[SND_MSXMUSIC] && THIS_->usertone_enable[0])
            THIS_->sndp[SND_MSXMUSIC]->setinst(THIS_->sndp[SND_MSXMUSIC]->ctx, 0, THIS_->usertone[0], 16 * 19);
	}
	for (i = 0; i < SND_MAX; i++)
	{
		if (THIS_->sndp[i])
        {
            THIS_->sndp[i]->reset(THIS_->sndp[i]->ctx, baseclock, freq);
        }
	}

	/* memory reset */
	XMEMSET(THIS_->ram, 0xC9, 0x4000);
	XMEMCPY(THIS_->ram + 0x93, "\xC3\x01\x00\xC3\x09\x00", 6);
	XMEMCPY(THIS_->ram + 0x01, "\xD3\xA0\xF5\x7B\xD3\xA1\xF1\xC9", 8);
	XMEMCPY(THIS_->ram + 0x09, "\xD3\xA0\xDB\xA2\xC9", 5);
	XMEMSET(THIS_->ram + 0x4000, 0, 0xC000);
	if (THIS_->datasize)
	{
		if (THIS_->dataaddr + THIS_->datasize > 0x10000)
			XMEMCPY(THIS_->ram + THIS_->dataaddr, THIS_->data, 0x10000 - THIS_->dataaddr);
		else
			XMEMCPY(THIS_->ram + THIS_->dataaddr, THIS_->data, THIS_->datasize);
	}
	for (i = 0; i < 8; i++)
	{
		THIS_->readmap[i] = THIS_->ram;
		THIS_->writemap[i] = THIS_->ram;
	}
	THIS_->writemap[4] = THIS_->writemap[5] = 0;
	if (THIS_->majutushimode)
	{
		THIS_->writemap[2] = 0;
		THIS_->writemap[3] = 0;
	}
	if (!(THIS_->extdevice & EXTDEVICE_SNG)) THIS_->sccenable = !THIS_->rammode;

	/* cpu reset */
	kmz80_reset(&THIS_->ctx);
	THIS_->ctx.user = THIS_;
	THIS_->ctx.kmevent = &THIS_->kme;
	THIS_->ctx.memread = read_event;
	THIS_->ctx.memwrite = write_event;
	if (THIS_->extdevice & EXTDEVICE_SNG)
	{
		THIS_->ctx.ioread = ioread_eventSMS;
		THIS_->ctx.iowrite = iowrite_eventSMS;
		THIS_->ctx.regs8[REGID_M1CYCLE] = 1;
	}
	else
	{
		THIS_->ctx.ioread = ioread_eventMSX;
		THIS_->ctx.iowrite = iowrite_eventMSX;
		THIS_->ctx.regs8[REGID_M1CYCLE] = 2;
	}
	THIS_->ctx.busread = busread_event;

	THIS_->ctx.regs8[REGID_A] = (Uint8)((song >> 0) & 0xff);
	THIS_->ctx.regs8[REGID_H] = (Uint8)((song >> 8) & 0xff);
	play_setup(THIS_, THIS_->initaddr);

	THIS_->ctx.exflag = 3;	/* ICE/ACI */
	THIS_->ctx.regs8[REGID_IFF1] = THIS_->ctx.regs8[REGID_IFF2] = 0;
	THIS_->ctx.regs8[REGID_INTREQ] = 0;
	THIS_->ctx.regs8[REGID_IMODE] = 1;

	/* vsync reset */
	kmevent_init(&THIS_->kme);
	THIS_->vsync_id = kmevent_alloc(&THIS_->kme);
	kmevent_setevent(&THIS_->kme, THIS_->vsync_id, vsync_event, THIS_);
	// THIS_->cpsgap = THIS_->cpsrem  = 0;
	THIS_->cpfrem  = 0;

    // 割り込みベクタ
    THIS_->bus_vector = 0x38;
    
    /* CTC */
    if (THIS_->ext2 & EXT2_OPM)
    {
        THIS_->ctc1_id = kmevent_alloc(&THIS_->kme);
        THIS_->ctc3_id = kmevent_alloc(&THIS_->kme);
        kmevent_setevent(&THIS_->kme, THIS_->ctc1_id, ctc1_event, THIS_);
        kmevent_setevent(&THIS_->kme, THIS_->ctc3_id, ctc3_event, THIS_);
    }
    
    // CPUを高速に設定
		THIS_->cpu_ratio = 1;
    if (THIS_->ext2 & EXT2_TURBO)
        THIS_->cpu_ratio = 4;
    
    // 1秒間のCPUクロック数
    THIS_->cpu_cycle = baseclock * THIS_->cpu_ratio;

	// 1サンプルあたりのクロック数
    THIS_->cps = (double)THIS_->cpu_cycle / freq;
    
    // THIS_->cps = DivFix(cpu_cycle, freq, SHIFT_CPS);

	// 1フレームあたりのクロック数
	if (THIS_->extdevice & EXTDEVICE_PAL)
        THIS_->cpf = ((4 * 342 * 313 / 6) * THIS_->cpu_ratio);
	else
        THIS_->cpf = ((4 * 342 * 262 / 6) * THIS_->cpu_ratio);

    // THIS_->cpf = THIS_->cpf << SHIFT_CPS;
    
	{
		// ログ出力
		if (pNezPlay->log_ctx)
		{
				THIS_->log_ctx = pNezPlay->log_ctx;
				
				if (THIS_->ext2 & EXT2_OPM)
				{
						THIS_->cpf = (THIS_->cpu_cycle / 100);
						double vsync = (double)THIS_->cpu_cycle / (THIS_->cpf);
						double vsync_us = ((double)1*1000*1000) / vsync;

						WriteLOG_Timing(pNezPlay->log_ctx, (int)vsync_us);
				}
				else
				{
						double vsync = (double)THIS_->cpu_cycle / (THIS_->cpf);
						double vsync_us = ((double)1*1000*1000) / vsync;
						// if 60Hz, vsync_us = 16666.666us
						// 16666 / 64 exceed 256
						WriteLOG_Timing(pNezPlay->log_ctx, (int)vsync_us);
				}
		}
		
		/* request execute */
		Uint32 initbreak = 5 << 8; /* 5sec */
		
		// リクエスト実行
		while (!THIS_->ctx.regs8[REGID_HALTED] && --initbreak)
			kmz80_exec(&THIS_->ctx, THIS_->cpu_cycle >> 8);
	}
	
	// 垂直割り込みのセットアップ
	vsync_setup(THIS_);
	
	// HALTしているので、再生アドレスに戻す
	if (THIS_->ctx.regs8[REGID_HALTED])
	{
		play_setup(THIS_, THIS_->playaddr);
	}
	
	//　初期化
	THIS_->cpu_usage = 0;
	THIS_->total_sec = 0;
	THIS_->total_cycles = 0;
	THIS_->total_skip_cycles = 0;
    
	//ここからメモリービュアー設定
	kss_context = THIS_;
	MEM_MAX=0xffff;
	MEM_IO =0x0000;
	MEM_RAM=0xC000;
	MEM_ROM=0x8000;
	memview_memread = memview_memread_kss;
	//ここまでメモリービュアー設定

	// CPU使用率ハンドラ
	get_cpuusage = kss_cpuusage;

}

static void terminate(KSSSEQ *THIS_)
{
	Uint32 i;
	// CPU使用率ハンドラ
	get_cpuusage = NULL;

	//ここからダンプ設定
	dump_MEM_MSX     = NULL;
	dump_DEV_AY8910  = NULL;
	dump_DEV_SN76489 = NULL;
	dump_DEV_SCC     = NULL;
	dump_DEV_OPL     = NULL;
	dump_DEV_OPLL    = NULL;
	dump_DEV_ADPCM   = NULL;
	//ここまでダンプ設定
	for (i = 0; i < SND_MAX; i++)
	{
		if (THIS_->sndp[i]) THIS_->sndp[i]->release(THIS_->sndp[i]->ctx);
	}
	if (THIS_->data) XFREE(THIS_->data);
	XFREE(THIS_);
}


static Uint32 load(NEZ_PLAY *pNezPlay, KSSSEQ *THIS_, Uint8 *pData, Uint32 uSize)
{
	Uint32 i, headersize;
	XMEMSET(THIS_, 0, sizeof(KSSSEQ));
	for (i = 0; i < SND_MAX; i++) THIS_->sndp[i] = 0;
	for (i = 0; i < SND_MAX; i++) THIS_->vola[i] = 0x80;
	THIS_->data = 0;

	if (GetDwordLE(pData) == GetDwordLEM("KSCC"))
	{
		headersize = 0x10;
		THIS_->startsong = 1;
		THIS_->maxsong = 256;
	}
	else if (GetDwordLE(pData) == GetDwordLEM("KSSX"))
	{
		headersize = 0x10 + pData[0x0e];
		if (pData[0x0e] >= 0x0c)
		{
			THIS_->startsong = GetWordLE(pData + 0x18) + 1;
			THIS_->maxsong = GetWordLE(pData + 0x1a) + 1;
		}
		else
		{
			THIS_->startsong = 1;
			THIS_->maxsong = 256;
		}
		if (pData[0x0e] >= 0x10)
		{
			THIS_->vola[SND_PSG] = 0x100 - (pData[0x1c] ^ 0x80);
			THIS_->vola[SND_SCC] = 0x100 - (pData[0x1d] ^ 0x80);
			THIS_->vola[SND_MSXMUSIC] = 0x100 - (pData[0x1e] ^ 0x80);
			THIS_->vola[SND_MSXAUDIO] = 0x100 - (pData[0x1f] ^ 0x80);
		}
	}
	else
	{
		return NESERR_FORMAT;
	}
	THIS_->dataaddr = GetWordLE(pData + 0x04);
	THIS_->datasize = GetWordLE(pData + 0x06);
	THIS_->initaddr = GetWordLE(pData + 0x08);
	THIS_->playaddr = GetWordLE(pData + 0x0A);
	THIS_->bankofs = pData[0x0C];
	THIS_->banknum = pData[0x0D];
    THIS_->ext2 = pData[0x0E];
	THIS_->extdevice = pData[0x0F];
	SONGINFO_SetStartSongNo(pNezPlay->song, THIS_->startsong);
	SONGINFO_SetMaxSongNo(pNezPlay->song, THIS_->maxsong);
	SONGINFO_SetInitAddress(pNezPlay->song, THIS_->initaddr);
	SONGINFO_SetPlayAddress(pNezPlay->song, THIS_->playaddr);
	SONGINFO_SetExtendDevice(pNezPlay->song, THIS_->extdevice << 8);
    
    if (pNezPlay->turbo)
        THIS_->ext2 |= EXT2_TURBO;

	sprintf(songinfodata.detail,
"Type         : KS%c%c\r\n"
"Load Address : %04XH\r\n"
"Load Size    : %04XH\r\n"
"Init Address : %04XH\r\n"
"Play Address : %04XH\r\n"
"Extra Device : %s%s%s%s%s"
		,pData[0x02],pData[0x03]
		,THIS_->dataaddr,THIS_->datasize,THIS_->initaddr,THIS_->playaddr
		,pData[0x0F]&0x01 ? (pData[0x0F]&0x02 ? "FMUNIT " : "FMPAC ") : ""
		,pData[0x0F]&0x02 ? "SN76489 " : ""
		,pData[0x0F]&0x04 ? (pData[0x0F]&0x02 ? "GGstereo " : "RAM ") : ""
		,pData[0x0F]&0x08 ? (pData[0x0F]&0x02 ? "RAM " : "MSX-AUDIO ") : ""
		,pData[0x0F] ? "" : "None"
	);

	if (THIS_->banknum & 0x80)
	{
		THIS_->banknum &= 0x7f;
		THIS_->bankmode = KSS_8K_MAPPER;
		THIS_->banksize = 0x2000;
	}
	else if (THIS_->banknum)
	{
		THIS_->bankmode = KSS_16K_MAPPER;
		THIS_->banksize = 0x4000;
	}
	else
	{
		THIS_->bankmode = KSS_BANK_OFF;
	}
	THIS_->data = XMALLOC(THIS_->datasize + THIS_->banksize * THIS_->banknum + 8);
	if (!THIS_->data) return NESERR_SHORTOFMEMORY;
	THIS_->bankbase = THIS_->data + THIS_->datasize;
	if (uSize > 0x10 && THIS_->datasize)
	{
		Uint32 size;
		XMEMSET(THIS_->data, 0, THIS_->datasize);
		if (THIS_->datasize < uSize - headersize)
			size = THIS_->datasize;
		else
			size = uSize - headersize;
		XMEMCPY(THIS_->data, pData + headersize, size);
	}
	else
	{
		THIS_->datasize = 0;
	}
	if (uSize > (headersize + THIS_->datasize) && THIS_->bankmode != KSS_BANK_OFF)
	{
		Uint32 size;
		XMEMSET(THIS_->bankbase, 0, THIS_->banksize * THIS_->banknum);
		if (THIS_->banksize * THIS_->banknum < uSize - (headersize + THIS_->datasize))
			size = THIS_->banksize * THIS_->banknum;
		else
			size = uSize - (headersize + THIS_->datasize);
		XMEMCPY(THIS_->bankbase, pData + (headersize + THIS_->datasize), size);
	}
	else
	{
		THIS_->banknum = 0;
		THIS_->bankmode = KSS_BANK_OFF;
	}

	//ここからダンプ設定
	dump_MEM_MSX     = dump_MEM_MSX_bf;
	//ここまでダンプ設定
    
    // 独自拡張を使用するか
    if (!(THIS_->extdevice & EXTDEVICE_USE_EXT2))
        THIS_->ext2 = 0;
        

    // 基本周波数
    int baseclock = BASECYCLES;

    if (THIS_->ext2 & EXT2_4M)
        baseclock = BASECYCLES_4M;
    

	THIS_->majutushimode = 0;
	if (THIS_->extdevice & EXTDEVICE_SNG)
	{
		THIS_->synthmode = SYNTHMODE_SMS;
		if (THIS_->extdevice & EXTDEVICE_GGSTEREO)
		{
			THIS_->sndp[SND_SNG] = SNGSoundAlloc(SNG_TYPE_GAMEGEAR);
			SONGINFO_SetChannel(pNezPlay->song, 2);
			//ここからダンプ設定
			dump_DEV_SN76489 = dump_DEV_SN76489_bf2;
			//ここまでダンプ設定
		}
		else
		{
			THIS_->sndp[SND_SNG] = SNGSoundAlloc(SNG_TYPE_SEGAMKIII);
			SONGINFO_SetChannel(pNezPlay->song, 1);
			//ここからダンプ設定
			dump_DEV_SN76489 = dump_DEV_SN76489_bf;
			//ここまでダンプ設定
		}
		if (!THIS_->sndp[SND_SNG]) return NESERR_SHORTOFMEMORY;
		if (THIS_->extdevice & EXTDEVICE_FMUNIT)
		{
			THIS_->sndp[SND_FMUNIT] = OPLSoundAlloc(OPL_TYPE_SMSFMUNIT);
			if (!THIS_->sndp[SND_FMUNIT]) return NESERR_SHORTOFMEMORY;
			//ここからダンプ設定
			dump_DEV_OPLL = dump_DEV_OPLL_bf;
			//ここまでダンプ設定
		}
		if (THIS_->extdevice & (EXTDEVICE_EXRAM | EXTDEVICE_GGRAM))
		{
			THIS_->rammode = 1;
		}
		else
		{
			THIS_->rammode = 0;
		}
		THIS_->sccenable = 0;
	}
	else
	{
		if (THIS_->extdevice & EXTDEVICE_MSXSTEREO)
		{
			if (THIS_->extdevice & EXTDEVICE_MSXAUDIO)
			{
				SONGINFO_SetChannel(pNezPlay->song, 2);
				THIS_->synthmode = SYNTHMODE_MSXSTEREO;
			}
			else
			{
				SONGINFO_SetChannel(pNezPlay->song, 1);
				THIS_->synthmode = SYNTHMODE_MSX;
				THIS_->majutushimode = 1;
			}
		}
		else
		{
			SONGINFO_SetChannel(pNezPlay->song, 1);
			THIS_->synthmode = SYNTHMODE_MSX;
		}
		if(MSXPSGType){
			THIS_->sndp[SND_PSG] = PSGSoundAlloc(PSG_TYPE_YM2149);
		}else{
			THIS_->sndp[SND_PSG] = PSGSoundAlloc(PSG_TYPE_AY_3_8910);
		}
        
		if (!THIS_->sndp[SND_PSG])
            return NESERR_SHORTOFMEMORY;
        
		//ここからダンプ設定
		dump_DEV_AY8910 = dump_DEV_AY8910_bf;
		//ここまでダンプ設定
		if (THIS_->extdevice & EXTDEVICE_MSXMUSIC)
		{
			THIS_->sndp[SND_MSXMUSIC] = OPLSoundAlloc(OPL_TYPE_MSXMUSIC);
			if (!THIS_->sndp[SND_MSXMUSIC]) return NESERR_SHORTOFMEMORY;
			//ここからダンプ設定
			dump_DEV_OPLL = dump_DEV_OPLL_bf;
			//ここまでダンプ設定
		}
		if (THIS_->extdevice & EXTDEVICE_MSXAUDIO)
		{
			//THIS_->sndp[SND_MSXAUDIO] = OPLSoundAlloc(OPL_TYPE_MSXAUDIO);
			THIS_->sndp[SND_MSXAUDIO] = OPLSoundAlloc(OPL_TYPE_OPL2);
			if (!THIS_->sndp[SND_MSXAUDIO]) return NESERR_SHORTOFMEMORY;
			//ここからダンプ設定
			dump_DEV_OPL = dump_DEV_OPL_bf;
			dump_DEV_ADPCM = dump_DEV_ADPCM_bf;
			//ここまでダンプ設定
		}
        
        // OPL3
        if (THIS_->ext2 & EXT2_OPL3)
        {
#ifdef USE_OPL3
            THIS_->sndp[SND_OPL3] = OPL3SoundAlloc();
            if (!THIS_->sndp[SND_OPL3]) return NESERR_SHORTOFMEMORY;
#else
            printf("Warning: USE_OPL3 is not defined\n");
#endif
        }
        // OPM
        if (THIS_->ext2 & EXT2_OPM)
        {
#ifdef USE_FMGEN
            if (!pNezPlay->use_gmc && pNezPlay->use_fmgen)
            {
                THIS_->sndp[SND_OPM] = OPM_FMGen_SoundAlloc();
                if (!THIS_->sndp[SND_OPM]) return NESERR_SHORTOFMEMORY;
                THIS_->sndp[SND_OPM2] = OPM_FMGen_SoundAlloc();
                if (!THIS_->sndp[SND_OPM2]) return NESERR_SHORTOFMEMORY;
            }
            else
#endif
            {
#ifdef USE_OPM
                THIS_->sndp[SND_OPM] = OPMSoundAlloc(0);
                if (!THIS_->sndp[SND_OPM]) return NESERR_SHORTOFMEMORY;
                THIS_->sndp[SND_OPM2] = OPMSoundAlloc(1);
                if (!THIS_->sndp[SND_OPM2]) return NESERR_SHORTOFMEMORY;
#endif
            }
                
#if !defined(USE_FMGEN) && !defined(USE_OPM)
            printf("Warning: NO OPM Sound Generator\n");
#endif

        }

        int flags = OUT_INT;
        if (pNezPlay->use_gmc)
            flags |= OUT_EXT;
        
        // デバイス出力フラグ設定
        if (THIS_->sndp[SND_PSG])
            THIS_->sndp[SND_PSG]->output_device = flags;

        if (THIS_->sndp[SND_OPL3])
            THIS_->sndp[SND_OPL3]->output_device = flags;

        if (THIS_->sndp[SND_OPM])
            THIS_->sndp[SND_OPM]->output_device = flags;

        if (THIS_->sndp[SND_OPM2])
            THIS_->sndp[SND_OPM2]->output_device = flags;

        
        // ログ出力
        if (pNezPlay->log_ctx)
        {
            int type = LOG_TYPE_PSG;
            
            // ログ出力関数
            FUNC_LOGWRITE func_logwrite = (FUNC_LOGWRITE)WriteLOG_Data;
            
            // SSGの場合もある
            if (MSXPSGType)
                type = LOG_TYPE_SSG;
            
            
            
            // PSG or SSG
            THIS_->sndp[SND_PSG]->log_id =
                AddMapLOG(pNezPlay->log_ctx, type, baseclock, LOG_PRIO_PSG);
            THIS_->sndp[SND_PSG]->log_ctx = pNezPlay->log_ctx;
            THIS_->sndp[SND_PSG]->logwrite = func_logwrite;
            
            // MSXMUSIC(YM2413)
            if (THIS_->sndp[SND_MSXMUSIC])
            {
                THIS_->sndp[SND_MSXMUSIC]->log_id =
                    AddMapLOG(pNezPlay->log_ctx, LOG_TYPE_OPLL, baseclock, LOG_PRIO_OPLL);
                THIS_->sndp[SND_MSXMUSIC]->log_ctx = pNezPlay->log_ctx;
                THIS_->sndp[SND_MSXMUSIC]->logwrite = func_logwrite;
            }
            
            // MSXAUDIO(Y8950)
            if (THIS_->sndp[SND_MSXAUDIO])
            {
                // Y8950 is not defined in S98V3
                THIS_->sndp[SND_MSXAUDIO]->log_id =
                    AddMapLOG(pNezPlay->log_ctx, LOG_TYPE_OPLL, baseclock, LOG_PRIO_OPLL);
                THIS_->sndp[SND_MSXAUDIO]->log_ctx = pNezPlay->log_ctx;
                THIS_->sndp[SND_MSXAUDIO]->logwrite = func_logwrite;
            }
            
            // OPL3(MoonSound FM)
            if (THIS_->sndp[SND_OPL3])
            {
                THIS_->sndp[SND_OPL3]->log_id =
                    AddMapLOG(pNezPlay->log_ctx, LOG_TYPE_OPL3, BASECLOCK_OPL3, LOG_PRIO_OPL3);
                THIS_->sndp[SND_OPL3]->log_ctx = pNezPlay->log_ctx;
                THIS_->sndp[SND_OPL3]->logwrite = func_logwrite;
            }
            
            // OPM(X1 FM)
            if (THIS_->sndp[SND_OPM])
            {
                THIS_->sndp[SND_OPM]->log_id =
                    AddMapLOG(pNezPlay->log_ctx, LOG_TYPE_OPM, baseclock, LOG_PRIO_OPM);
            
                THIS_->sndp[SND_OPM]->log_ctx = pNezPlay->log_ctx;
                THIS_->sndp[SND_OPM]->logwrite = func_logwrite;

                // THIS_->sndp[SND_OPM]->log_id = -1;
            }

            // OPM(X1 2nd FM)
            if (THIS_->sndp[SND_OPM2])
            {
                THIS_->sndp[SND_OPM2]->log_id =
                    AddMapLOG(pNezPlay->log_ctx, LOG_TYPE_OPM, baseclock, LOG_PRIO_OPM);
                THIS_->sndp[SND_OPM2]->log_ctx = pNezPlay->log_ctx;
                THIS_->sndp[SND_OPM2]->logwrite = func_logwrite;
                
                // THIS_->sndp[SND_OPM2]->log_id = -1;
            }
            
            MapEndLOG(pNezPlay->log_ctx);

            // calc with 4MHz(default)
            //WriteNLG_SetBaseClock(pNezPlay->log_ctx, BASECYCLES);
        }
        
        
		if (THIS_->extdevice & EXTDEVICE_EXRAM)
		{
			THIS_->rammode = 1;
			THIS_->sccenable = 0;
		}
		else
		{
			THIS_->sndp[SND_SCC] = SCCSoundAlloc();
			if (!THIS_->sndp[SND_SCC]) return NESERR_SHORTOFMEMORY;
			//ここからダンプ設定
			dump_DEV_SCC = dump_DEV_SCC_bf;
			//ここまでダンプ設定
			THIS_->rammode = (THIS_->extdevice & EXTDEVICE_MSXRAM) != 0;
			THIS_->sccenable = !THIS_->rammode;
		}
	}
	return NESERR_NOERROR;
}

static void KSSSEQAdjVolume(void *p,int device, double v) {
    NEZ_PLAY *pNezPlay = (NEZ_PLAY *)p;
    KSSSEQ *THIS_ = pNezPlay->kssseq;
    switch(device) {
        case ADJ_VOL_FM1:
            THIS_->sndp[SND_OPM]->adjust_volume(THIS_->sndp[SND_OPM]->ctx,v);
            break;
        case ADJ_VOL_FM2:
            THIS_->sndp[SND_OPM2]->adjust_volume(THIS_->sndp[SND_OPM2]->ctx,v);
            break;
        case ADJ_VOL_PSG:
            THIS_->sndp[SND_PSG]->adjust_volume(THIS_->sndp[SND_PSG]->ctx,v);
            break;
    }
}

static Int32 __fastcall KSSSEQExecuteZ80CPU(void *pNezPlay)
{
	execute(((NEZ_PLAY*)pNezPlay)->kssseq);
	return 0;
}

static void __fastcall KSSSEQSoundRenderStereo(void *pNezPlay, Int32 *d)
{
	synth(((NEZ_PLAY*)pNezPlay)->kssseq, d);
}

static Int32 __fastcall KSSSEQSoundRenderMono(void *pNezPlay)
{
	Int32 d[2] = { 0, 0 };
	synth(((NEZ_PLAY*)pNezPlay)->kssseq, d);
#if (((-1) >> 1) == -1)
	return (d[0] + d[1]) >> 1;
#else
	return (d[0] + d[1]) / 2;
#endif
}

const static NES_AUDIO_HANDLER kssseq_audio_handler[] = {
	{ 0, KSSSEQExecuteZ80CPU, 0, },
	{ 3, KSSSEQSoundRenderMono, KSSSEQSoundRenderStereo },
	{ 0, 0, 0, },
};

static void __fastcall KSSSEQVolume(void *pNezPlay, Uint32 v)
{
	if (((NEZ_PLAY*)pNezPlay)->kssseq)
	{
		volume(((NEZ_PLAY*)pNezPlay)->kssseq, v);
	}
}

const static NES_VOLUME_HANDLER kssseq_volume_handler[] = {
	{ KSSSEQVolume, }, 
	{ 0, }, 
};

static void __fastcall KSSSEQReset(void *pNezPlay)
{
	if (((NEZ_PLAY*)pNezPlay)->kssseq) reset((NEZ_PLAY*)pNezPlay);
}

const static NES_RESET_HANDLER kssseq_reset_handler[] = {
	{ NES_RESET_SYS_LAST, KSSSEQReset, },
	{ 0,                  0, },
};

static void __fastcall KSSSEQTerminate(void *pNezPlay)
{
	if (((NEZ_PLAY*)pNezPlay)->kssseq)
	{
		terminate(((NEZ_PLAY*)pNezPlay)->kssseq);
		((NEZ_PLAY*)pNezPlay)->kssseq = 0;
	}
}

const static NES_TERMINATE_HANDLER kssseq_terminate_handler[] = {
	{ KSSSEQTerminate, },
	{ 0, },
};

Uint32 KSSLoad(NEZ_PLAY *pNezPlay, Uint8 *pData, Uint32 uSize)
{
	Uint32 ret;
	KSSSEQ *THIS_;
	if (pNezPlay->kssseq) ASSERT(0);	/* ASSERT */
	THIS_ = XMALLOC(sizeof(KSSSEQ));
	if (!THIS_) return NESERR_SHORTOFMEMORY;
    memset(THIS_, 0, sizeof(KSSSEQ));
    
	ret = load(pNezPlay, THIS_, pData, uSize);
	if (ret)
	{
		terminate(THIS_);
		return ret;
	}
	pNezPlay->kssseq = THIS_;
	NESAudioHandlerInstall(pNezPlay, kssseq_audio_handler);
	NESVolumeHandlerInstall(pNezPlay, kssseq_volume_handler);
	NESResetHandlerInstall(pNezPlay->nrh, kssseq_reset_handler);
	NESTerminateHandlerInstall(&pNezPlay->nth, kssseq_terminate_handler);
    pNezPlay->adj_dev_volume = KSSSEQAdjVolume;
	return ret;
}
