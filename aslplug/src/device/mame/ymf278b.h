// license:BSD-3-Clause
// copyright-holders:R. Belmont, Olivier Galibert, hap
#pragma once

#ifndef __YMF278B_H__
#define __YMF278B_H__

#include "mametype.h"

class direct_read_data
{
public:
	UINT8 *_ptr;
	UINT64 _size;
	void setMemory(UINT8 *mem, UINT64 size)
	{
		_ptr = mem;
		_size = size;
		
	}
	
	UINT8 *getMemory(void)
	{
		return _ptr;
	}
	
	UINT8 read_byte(UINT64 adrs)
	{
		if (adrs >= _size)
			return 0xff;
		
		return _ptr[adrs];
	}
	void write_byte(UINT64 adrs, UINT8 data)
	{
		if (adrs >= _size)
			return;
		_ptr[adrs] = data;
	}	
};

// #include "emu.h"

#define YMF278B_STD_CLOCK (33868800)            /* standard clock for OPL4 */

class ymf278b_device
{
public:
	ymf278b_device(const char *tag, UINT32 clock);

	// DECLARE_READ8_MEMBER( read );
	// DECLARE_WRITE8_MEMBER( write );

	void write(int offset, int data);
	UINT8 read(int offset);

	void ymf262_update_request();

	void sound_stream_update(stream_sample_t **outputs, int samples);
	
	void load_rom(const char *filename);
	
protected:
	// device-level overrides
	void device_config_complete() ;
	void device_start();
	void device_reset() ;
	void device_stop();

private:
	struct YMF278BSlot
	{
		INT16 wave;     /* wavetable number */
		INT16 F_NUMBER; /* frequency */
		INT8 octave;    /* octave */
		INT8 preverb;   /* pseudo-reverb */
		INT8 DAMP;      /* damping */
		INT8 CH;        /* output channel */
		INT8 LD;        /* level direct */
		INT8 TL;        /* total level */
		INT8 pan;       /* panpot */
		INT8 LFO;       /* LFO */
		INT8 VIB;       /* vibrato */
		INT8 AM;        /* tremolo */

		INT8 AR;        /* attack rate */
		INT8 D1R;       /* decay 1 rate */
		INT8 DL;        /* decay level */
		INT8 D2R;       /* decay 2 rate */
		INT8 RC;        /* rate correction */
		INT8 RR;        /* release rate */

		UINT32 step;    /* fixed-point frequency step */
		UINT64 stepptr; /* fixed-point pointer into the sample */

		INT8 active;    /* channel is playing */
		INT8 KEY_ON;    /* slot keyed on */
		INT8 bits;      /* width of the samples */
		UINT32 startaddr;
		UINT32 loopaddr;
		UINT32 endaddr;

		int env_step;
		UINT32 env_vol;
		UINT32 env_vol_step;
		UINT32 env_vol_lim;
		INT8 env_preverb;

		int num;        /* slot number (for debug only) */
	};

	int compute_rate(YMF278BSlot *slot, int val);
	UINT32 compute_decay_env_vol_step(YMF278BSlot *slot, int val);
	void compute_freq_step(YMF278BSlot *slot);
	void compute_envelope(YMF278BSlot *slot);
	void irq_check();
	void A_w(UINT8 reg, UINT8 data);
	void B_w(UINT8 reg, UINT8 data);
	void retrigger_note(YMF278BSlot *slot);
	void C_w(UINT8 reg, UINT8 data);
	void timer_busy_start(int is_pcm);
	void precompute_rate_tables();
	void register_save_state();

	// internal state
	UINT8 m_pcmregs[256];
	YMF278BSlot m_slots[24];
	INT8 m_wavetblhdr;
	INT8 m_memmode;
	INT32 m_memadr;

	UINT8 m_status_busy, m_status_ld;
	UINT8 m_exp;

	INT32 m_fm_l, m_fm_r;
	INT32 m_pcm_l, m_pcm_r;

	UINT8 m_timer_a_count, m_timer_b_count;
	UINT8 m_enable, m_current_irq;
	int m_irq_line;

	UINT8 m_port_C, m_port_AB, m_lastport;

	// precomputed tables
	UINT32 m_lut_ar[64];              // attack rate
	UINT32 m_lut_dr[64];              // decay rate
	INT32 m_volume[256*4];            // precalculated attenuation values with some margin for envelope and pan levels
	int m_pan_left[16],m_pan_right[16]; // pan volume offsets
	INT32 m_mix_level[8];

	int m_clock;

	direct_read_data * m_direct;
	UINT8 m_last_fm_data;

	// ymf262
	void *m_ymf262;
};


#endif /* __YMF278B_H__ */
