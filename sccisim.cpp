#include "sccisim.h"
#include <stdio.h>
#include <stdarg.h>

#define PROG_NAME "sccisim"
#define PROG_INI ".\\sccisim.ini"

FILE *fp = NULL;

unsigned char chmask[0x200];

#include "snddrv/snddrv.h"
#include "render.h"
#include "gmcdrv.h"

#include "log.h"

#define DEF_FREQ 44100
#define DEF_BIT 16
#define DEF_CH 2

#define PCM_BLOCK 8
#define PCM_BUFSIZE 1024
#define PCM_CH 2
#define PCM_SAMPLE_BYTES 2
#define PCM_SAMPLE_MAX (PCM_BLOCK * PCM_BUFSIZE)

#define DLL_VER 0x100
#define DLL_VERSTR "SCCISIM.DLL 161112"

#define SIM_MAXCHIP 8

// コンテキスト
static struct 
{
	int freq;
	int songno;
	int pos;
	int load;

  int debuglog;
  int pcmlog;
  int soundlog;
  int soundlog_start;
  int nlgmode;

  int use_fmgen;

  int maxchip;
  int chiptype[SIM_MAXCHIP];
  int chipclock[SIM_MAXCHIP];
  float chipvol[SIM_MAXCHIP];

  DWORD current_time;
  int wait_count;

  int test_flag;

  double pcm_sample_per_ms;
  int pcm_blocklen;

  int pcm_wait;
  int pcm_play_pos;
  int pcm_write_pos;
  int pcm_count;
  short pcm_buffer[(PCM_SAMPLE_MAX * PCM_CH) + 16];
  DWORD pcm_time;

  LOGCTX *logctx;

	SOUNDDEVICE *psd;
	SOUNDDEVICEPDI pdi;

  FILE *pcmfp;
} dllctx;

const char *chipnames[] = {
	"SIM_YM2151",
	"SIM_YM2610",
	"SIM_YM2203",
	"SIM_YM2612",
	"SIM_AY8910",
	"SIM_SN76489",
	"SIM_YM3812",
	"SIM_YMF262",
	"SIM_YM2413",
	"SIM_YM3526",
	"SIM_YMF288",
	"SIM_SCC",
	"SIM_SCCS",
	"SIM_Y8950",
	"SIM_YM2164",	
	"SIM_YM2414",	
	"SIM_AY8930",	
	"SIM_YM2149",	
	"SIM_YMZ294",	
	"SIM_SN76496",
	"SIM_YM2420",	
	"SIM_YMF281",	
	"SIM_YMF276",	
	"SIM_YM2610B",
	"SIM_YMF286",	
	"SIM_YM2602",	
	"SIM_UM3567",	
	"SIM_YMF274",	
	"SIM_YM3806",	
	"SIM_YM2163",	
	"SIM_YM7129",	
	"SIM_YMZ280",	
	"SIM_YMZ705",	
	"SIM_YMZ735",	
	"SIM_YM2423",	
	"SIM_SPC700",	
	"SIM_OTHER",	
	"SIM_UNKNOWN"
};

int SimDefaultChips[] = {
SC_TYPE_YM2608,
SC_TYPE_YM2151,
SC_TYPE_YM2151,
SC_TYPE_YM2413,
SC_TYPE_YM2413,
SC_TYPE_YMZ294,
SC_TYPE_YMZ294
};

void OutputLog(const char *format, ...);
void SimStop();
void CloseSoundLog();

//////////////////////////////////////////////////////
/// レンダラ

static void PushRenderBody(int samples) {

    int count = samples;

    // バッファ最大を超える？
    if (dllctx.pcm_write_pos + count > PCM_SAMPLE_MAX) {
      count = PCM_SAMPLE_MAX - dllctx.pcm_write_pos;
      // OutputLog("Pos:%d Samples:%d MaxCount:%d", dllctx.pcm_write_pos, samples, count);
    }

    if (count > 0) {
      DoRender(dllctx.pcm_buffer + (dllctx.pcm_write_pos * PCM_CH) , count);
      dllctx.pcm_write_pos += count;
    }
    
    // バッファを超えた部分のあまり
    count = samples - count;
    if (count > 0) {
      // OutputLog("Count:%d", count);
      DoRender(dllctx.pcm_buffer, count);
      dllctx.pcm_write_pos = count;
    }

    // 追加サンプル数加算
    dllctx.pcm_count += samples;
}

static void PushRender(int samples) {
    DWORD now_time = timeGetTime();

    // サンプル数を自動計算
    if (samples < 0) {     
      int ms = (now_time - dllctx.pcm_time);
      samples = (dllctx.pcm_sample_per_ms * ms);
    }
    dllctx.pcm_time = now_time;

    // 大きい場合は切り捨て
    if (samples > (PCM_SAMPLE_MAX - PCM_BUFSIZE)) 
      samples = PCM_SAMPLE_MAX - PCM_BUFSIZE;
    
    PushRenderBody(samples);
}

int LastSamples = 0;

// len = 必要バイト数
static void RenderWriteProc(void *lpargs, void *lpbuf, unsigned len) {
  // samples = 必要サンプル数
  int samples = len>>2;

  if (LastSamples != samples) {
    LastSamples = samples;
    OutputLog("samples:%d",samples);
  }
  
  if (dllctx.pcm_wait) {
    while(1) {
      Sleep(0);
      if (!dllctx.pcm_wait) {
        // OutputLog("Out from Wait:%d",dllctx.pcm_count); 
        break;
      }
    }
  }

  int count = dllctx.pcm_count;

  if (count < samples) {
    PushRender(samples - count);
  }

  // コピー
  memcpy(lpbuf, dllctx.pcm_buffer + (dllctx.pcm_play_pos * PCM_CH), len);
  
  // ローデータ出力
  if (dllctx.pcmfp) 
    fwrite(lpbuf,len,1,dllctx.pcmfp);

  // カウンタ減算
  if (dllctx.pcm_count - samples > 0)
  	dllctx.pcm_count -= samples;
  else 
    dllctx.pcm_count = 0;

  // 次の再生バッファへ
  if (dllctx.pcm_play_pos + samples < PCM_SAMPLE_MAX)
    dllctx.pcm_play_pos += samples;
  else
    dllctx.pcm_play_pos = 0;

}

static void RenderTermProc(void *lpargs) { }

void SimOpenPcm() {
  char name[1024];
  char path[1024];
  GetModuleFileName(NULL,path,1024);
  char *p = strrchr(path,'\\');

  MakeFilenameLOG(name,"pcmrec",".raw");
  if (p == NULL) strcpy(path,name);
  else strcpy(p+1, name);

  dllctx.pcmfp = fopen(path,"wb");
}

void SimClosePcm() {
  if (dllctx.pcmfp) fclose(dllctx.pcmfp);
  dllctx.pcmfp = NULL;
}

//////////////////////////////////////////////////////
/// ログ
void OpenDebugLog() {
  if (fp != NULL) return;
  fp = fopen("sccisim.log","a+");
}

void CloseDebugLog() {
  if (fp == NULL) return;
  fclose(fp);
  fp = NULL;
}

void OutputLog(const char *format, ...) {
    char buf[1024];
    va_list arg;
    va_start(arg, format);
    vsprintf(buf, format, arg);
    va_end(arg);
    strcat(buf,"\n");

    if (fp != NULL) 
        fputs(buf, fp);
    fputs(buf, stdout);
}

void OpenConsole() {
  AllocConsole();
  freopen("CONIN$", "r",stdin); 
  freopen("CONOUT$","w",stdout); 
  freopen("CONOUT$","w",stderr);  
}

//////////////////////////////////////////////////////
// 設定
void GetSettingString(const char *Key, char *Dest) {
  GetPrivateProfileString(PROG_NAME,Key,"",Dest, 256, PROG_INI);
}

void WriteSettingString(const char *Key, const char *Data) {
  WritePrivateProfileString(PROG_NAME,Key, Data, PROG_INI);
}

void WriteSettingInt(const char *Key, int Value) {
  char Buf[256];
  sprintf(Buf,"%d",Value);
  WriteSettingString(Key,Buf);
}

void WriteSettingIndexInt(const char *Key, int Index, int Value) {
  char KeyBuf[256];
  char Buf[256];
  sprintf(KeyBuf,"%s%d",Key,Index);
  sprintf(Buf,"%d",Value);
  WriteSettingString(KeyBuf,Buf);
}

void WriteSettingIndexFloat(const char *Key, int Index, float Value) {
  char KeyBuf[256];
  char Buf[256];
  sprintf(KeyBuf,"%s%d",Key,Index);
  sprintf(Buf,"%f",Value);
  WriteSettingString(KeyBuf,Buf);
}

int GetSettingInt(const char *Key) {
  int Result = 0;
  char Buf[256];
  GetPrivateProfileString(PROG_NAME,Key,"",Buf,256, PROG_INI);
  sscanf(Buf,"%d",&Result);
  return Result;
}

int GetSettingIndexInt(const char *Key,int Index) {
  int Result = 0;
  char KeyBuf[256];
  char Buf[256];
  sprintf(KeyBuf,"%s%d",Key,Index);
  GetPrivateProfileString(PROG_NAME,KeyBuf,"",Buf,256, PROG_INI);
  sscanf(Buf,"%d",&Result);
  return Result;
}

float GetSettingIndexFloat(const char *Key,int Index) {
  float Result = 0;
  char KeyBuf[256];
  char Buf[256];
  sprintf(KeyBuf,"%s%d",Key,Index);
  GetPrivateProfileString(PROG_NAME,KeyBuf,"",Buf,256, PROG_INI);
  sscanf(Buf,"%f",&Result);
  return Result;
}


void SaveDefaultSetting() {
  OutputLog("SaveDefaultSetting");

  WriteSettingInt("use_fmgen", 1);
  WriteSettingInt("debuglog", 0);
  WriteSettingInt("pcmlog", 0);
  WriteSettingInt("soundlog", 0);
  WriteSettingInt("pcm_blocklen", 0);
  WriteSettingInt("nlgmode", 0);
  WriteSettingInt("freq", 44100);
  int maxchip = sizeof(SimDefaultChips) / sizeof(int);
  WriteSettingInt("maxchip", maxchip);
  for(int i = 0; i < maxchip; i++) WriteSettingIndexInt("chiptype", i, SimDefaultChips[i]);
  for(int i = 0; i < maxchip; i++) WriteSettingIndexInt("chipclock", i, 0);  
  for(int i = 0; i < maxchip; i++) WriteSettingIndexFloat("chipvol", i, 1);
}

void LoadSetting(void) {
  char buf[256];
  OutputLog("LoadSetting");
  GetSettingString("debuglog", buf);
  if (buf[0] == 0) {
    SaveDefaultSetting();
  }

  dllctx.use_fmgen = GetSettingInt("use_fmgen");
  dllctx.debuglog = GetSettingInt("debuglog");
  dllctx.pcmlog = GetSettingInt("pcmlog");
  dllctx.soundlog = GetSettingInt("soundlog");
  dllctx.pcm_blocklen = GetSettingInt("pcm_blocklen");
  // 0 = デフォルトサイズ
  if (dllctx.pcm_blocklen <= 0) dllctx.pcm_blocklen = 0; 
  dllctx.nlgmode = GetSettingInt("nlgmode");
  dllctx.freq = GetSettingInt("freq");
  dllctx.maxchip = GetSettingInt("maxchip");
  if (dllctx.maxchip >= SIM_MAXCHIP) dllctx.maxchip = SIM_MAXCHIP;  

  OutputLog("MaxChip:%d",dllctx.maxchip);
  if (dllctx.maxchip == 0) {
      OutputLog("load default chips");
      int maxchip = sizeof(SimDefaultChips) / sizeof(int);
      dllctx.maxchip = maxchip;
      for(int i = 0; i < maxchip; i++) {
        dllctx.chiptype[i] = SimDefaultChips[i];
        dllctx.chipclock[i] = 0;
      }
  } else {
    for(int i = 0; i < dllctx.maxchip; i++) {
      int ct = GetSettingIndexInt("chiptype",i);
      int cc = GetSettingIndexInt("chipclock",i);
      float cv = GetSettingIndexFloat("chipvol",i);
      dllctx.chiptype[i] = ct;
      dllctx.chipclock[i] = cc;
      dllctx.chipvol[i] = cv;
      OutputLog("Type:%d Clock:%d Vol:%f",ct,cc,cv);
    }
  }
}

//////////////////////////////////////////////////////

void OpenSoundLog() {
  if (!dllctx.soundlog) return;
  if (dllctx.logctx) return;
  
  OutputLog("OpenSoundLog");
  char name[1024];
  char path[1024];
  GetModuleFileName(NULL,path,1024);
  OutputLog("Module:%s",path);
  char *p = strrchr(path,'\\');

  const char *log_ext = LOG_EXT_S98;
  if (dllctx.nlgmode) log_ext = LOG_EXT_NLG;

  dllctx.soundlog_start = 0;
  MakeFilenameLOG(name,"sound",log_ext);
  if (p == NULL) strcpy(path,name);
  else strcpy(p+1, name);
  MakeOutputFileLOG(path,path,log_ext);
  dllctx.logctx = CreateLOG(path, dllctx.nlgmode ? LOG_MODE_NLG : LOG_MODE_S98);

  OutputLog("File:%s",path);
  WriteLOG_Timing(dllctx.logctx, 1000);
  dllctx.current_time = timeGetTime();
}

void SimInit() {
  OutputLog("SimInit");

	memset(chmask, 1, sizeof(chmask));
	memset(&dllctx, 0, sizeof(dllctx));

  dllctx.current_time = timeGetTime();
	dllctx.freq = DEF_FREQ;
  LoadSetting();

  dllctx.pcm_sample_per_ms = (double)dllctx.freq / 1000;
  OutputLog("SamplePerMs:%lf",dllctx.pcm_sample_per_ms);

  if (dllctx.pcmlog) SimOpenPcm();
  if (dllctx.debuglog) OpenDebugLog();
	SimStop();

	c86x_init();

  InitRender();
  SetRenderFreq(dllctx.freq);
}

void SimStop() {
	SOUNDDEVICE *psd = dllctx.psd;
	if (psd) {
		psd->Term(psd);
		dllctx.psd = NULL;
	}
	return;
}

void CloseSoundLog() {
  OutputLog("CloseSoundLog");
  if (dllctx.logctx) {
    CloseLOG(dllctx.logctx);
    dllctx.logctx = NULL;
  }
}

void SimFree() {
  OutputLog("SimFree");
  SimClosePcm();
  SimStop();
  CloseDebugLog();
}

void SimPlay() {
  OutputLog("SimPlay");
	SOUNDDEVICEINITDATA sdid;

	SimStop();

	sdid.freq = dllctx.freq;
	sdid.bit = DEF_BIT;
	sdid.ch = DEF_CH;
	sdid.lpargs = 0;
	sdid.Write = RenderWriteProc;
	sdid.Term = RenderTermProc;
  sdid.blocklen = dllctx.pcm_blocklen;
  dllctx.pdi.hwnd = GetForegroundWindow();
	// if (hWnd) dllctx.pdi.hwnd = hWnd;
	sdid.ppdi = &dllctx.pdi;

	dllctx.pos = 0;
	dllctx.psd = CreateSoundDeviceDX(&sdid);
  // OutputLog("CreateSoundDeviceDx:%08x",dllctx.psd);
}

// 装置の追加
int SimAddDevice(int type, int bc, float vol) {
	RenderSetting rs;

	rs.type = type;
	rs.freq = dllctx.freq;
	rs.baseclock = bc;
	rs.use_gmc = 1;
  rs.vol = vol;
	return AddRender(&rs);
}

// 出力先の設定
void SimSetDevice(int id, int dev) {
	SetOutputRender(id, dev);
}

// 書き込み
void SimWriteDevice(int id, int addr, int data) {
	WriteDevice(id, addr, data);
}

///////////////////////////////////////////////
// 
class SIMCHIP : public SoundChip {
  int devid_;
  int sc_type_;
  int logid_;
  int clock_;

  BYTE Reg_[0x200];
  SCCI_SOUND_CHIP_INFO chipinfo_;

  public:

  SIMCHIP(int devid, int logid, int sc_type, int clock) {
    devid_ = devid;
    sc_type_ = sc_type;
    logid_ = logid;
    clock_ = clock;

    memset(&chipinfo_,0,sizeof(SCCI_SOUND_CHIP_INFO));
    if (sc_type < SC_TYPE_MAX)
      strcpy(chipinfo_.cSoundChipName, chipnames[sc_type]);

    chipinfo_.iSoundChip = sc_type;
    chipinfo_.dClock = clock;
  }

  int getDeviceId() {
    return devid_;
  }

  SCCI_SOUND_CHIP_INFO* __stdcall getSoundChipInfo() {
    OutputLog("SIMCHIP:getSoundChipInfo");
    return &chipinfo_;
  }

	// get sound chip type
  int __stdcall getSoundChipType() {
    OutputLog("SIMCHIP:getSoundChipType");
    return sc_type_;
  }
	// set Register data
  BOOL __stdcall setRegister(DWORD dAddr,DWORD dData) {

    dllctx.pcm_wait = 1; 
    Reg_[dAddr&0x1ff] = dData;
    DWORD now_time = timeGetTime();
    // OutputLog("SIMCHIP:setRegister");
    
    if (dllctx.logctx) {
      if (!dllctx.soundlog_start) {
        MapEndLOG(dllctx.logctx);
        dllctx.soundlog_start = 1;
        dllctx.current_time = now_time;
        dllctx.wait_count = 0;
      }
    }
     
    dllctx.wait_count += (now_time - dllctx.current_time);
    dllctx.current_time = now_time;
    
    while(dllctx.wait_count > 0) {
        if (dllctx.logctx) WriteLOG_SYNC(dllctx.logctx);
        dllctx.wait_count -= 1;
    }
    if (dllctx.logctx)
      WriteLOG_Data(dllctx.logctx, logid_, dAddr, dData);
      
    // PushRender(-1);

    
    SimWriteDevice(devid_,dAddr,dData);
    // OutputLog("Out:%02x %02x %02x",devid_, dAddr, dData);
    dllctx.pcm_wait = 0;
    return true;
  }
  DWORD __stdcall getRegister(DWORD dAddr) {
    // OutputLog("getRegister");
    return 0x00;
  }
  // initialize sound chip(clear registers)
  BOOL __stdcall init() {
    OutputLog("SIMCHIP:init");
    return true;
  }
  // get sound chip clock
  DWORD __stdcall getSoundChipClock() {
    // OutputLog("getSoundChipClock");
    return clock_;
  }
  // get writed register data
  DWORD __stdcall getWrittenRegisterData(DWORD addr) {
    return Reg_[addr&0x1ff];
  }
  // buffer check
  BOOL __stdcall isBufferEmpty() {
    OutputLog("SIMCHIP:isBufferEmpty");
    return true;
  }
};

///////////////////////////////////////////////
// 
class SimChipManager {
  SoundChip *chip_[SIM_MAXCHIP];

  public:
  SimChipManager() {
    for(int i=0; i<SIM_MAXCHIP; i++) chip_[i] = NULL;
  }

  ~SimChipManager() {
    int i = 0;
    for(i=0; i<SIM_MAXCHIP; i++) { 
      if (chip_[i] == NULL) continue;
      delete chip_[i]; chip_[i] = NULL; 
    }
  }

  int GetSoundChipCount() {
     return dllctx.maxchip;
  } 

  SoundChip* GetSoundChip(int index) {
    OutputLog("SimChipManager:GetSoundChip");

    if (index < 0 || index >= dllctx.maxchip) return NULL;
    int ct = dllctx.chiptype[index];
    int cc = dllctx.chipclock[index];
    float cv = dllctx.chipvol[index];
    chip_[index] = MakeSimChip(ct,cc,cv);
    return chip_[index];
  }

  SoundChip* GetSoundChipByType(int iSoundChipType) {
    OutputLog("SimChipManager:GetSoundChipByType");
    for(int i = 0; i < dllctx.maxchip; i++) {
      if (iSoundChipType != dllctx.chiptype[i] || chip_[i] != NULL) continue;
      return GetSoundChip(i);
    }
    return NULL;
  }


  SoundChip* MakeSimChip(int iSoundChipType,DWORD dClock, float vol) 
  {
    int id = -1;
    int clock = dClock;
    int prio = 1;
    int log_type = LOG_TYPE_OPNA;
    int render_type = RENDER_TYPE_OPNA;

    OutputLog("SimChipManager:MakeSimChip");

    switch(iSoundChipType) {
      case SC_TYPE_YM2151:
        if (clock == 0) clock = RENDER_BC_4M;
        if (dllctx.use_fmgen) 
          render_type = RENDER_TYPE_OPM_FMGEN;
        else 
          render_type = RENDER_TYPE_OPM;
        log_type = LOG_TYPE_OPM;
      break;
      case SC_TYPE_AY8910:
      case SC_TYPE_YMZ294:
        log_type = LOG_TYPE_SSG;
        prio = 0;
        if (clock == 0) clock = RENDER_BC_3M57;
        render_type = RENDER_TYPE_SSG;
      break;
      case SC_TYPE_YM2413:
        log_type = LOG_TYPE_OPLL;
        if (clock == 0) clock = RENDER_BC_3M57;
        render_type = RENDER_TYPE_OPLL;
      break;
      case SC_TYPE_YMF262:
        log_type = LOG_TYPE_OPL3;
        if (clock == 0) clock = RENDER_BC_14M3;      
        render_type = RENDER_TYPE_OPL3;
      break;
      case SC_TYPE_YM2203:
      case SC_TYPE_YM2608:
        log_type = LOG_TYPE_OPNA;
        if (clock == 0) clock = RENDER_BC_7M98;
        render_type = RENDER_TYPE_OPNA;
      break;
    }
    id = SimAddDevice(render_type,clock,vol);

    if (id >= 0) {
      int log_id = 0;
      OpenSoundLog();
      if (dllctx.logctx) log_id = AddMapLOG(dllctx.logctx, log_type, clock, prio);
      return new SIMCHIP(id, log_id, iSoundChipType, clock);
    }
    return NULL;
  }

  void ReleaseSimChip(SoundChip *pSoundChip) {
    if (pSoundChip == NULL) return;
    int id = ((SIMCHIP *)pSoundChip)->getDeviceId();
    DeleteRender(id);

    OutputLog("SimChipManager:ReleaseSimChip");

    for(int i=0; i < SIM_MAXCHIP; i++) {
      if (chip_[i] != pSoundChip) continue;
      delete pSoundChip;
      chip_[i] = NULL;
      break;
    }
    CloseSoundLog();
  }
};

SimChipManager *scman = NULL;

///////////////////////////////////////////////
//
class SIMSI : public SoundInterface {

	BOOL __stdcall isSupportLowLevelApi() {
    OutputLog("SIMSI:isSupportLowLevelApi");
    return true; 
  }
	BOOL __stdcall setData(BYTE *pData,DWORD dSendDataLen) {
    OutputLog("SIMSI:setData");
    return true;
  }
	DWORD __stdcall getData(BYTE *pData,DWORD dGetDataLen) {
    OutputLog("SIMSI:getData");
    return true;
  }
	BOOL __stdcall setDelay(DWORD dDelay) {
    OutputLog("SIMSI:setDelay");
    return true;
  }
	DWORD __stdcall getDelay() {
    OutputLog("SIMSI:getDelay");
    return true;
  }
	BOOL __stdcall reset() {
    OutputLog("SIMSI:reset");
    return true;
  }
	BOOL __stdcall init() {
    OutputLog("SIMSI:init");
    return true;
  }
	DWORD	__stdcall getSoundChipCount() {
    OutputLog("SIMSI:getSoundChipCount");
    return scman->GetSoundChipCount();
  }
	SoundChip* __stdcall getSoundChip(DWORD dNum) {
    return scman->GetSoundChip(dNum);
  }
};

///////////////////////////////////////////////
//
class SIMBody : public SoundInterfaceManager {
  SCCI_INTERFACE_INFO SIInfo;

  public:
  SIMBody() {
    memset(&SIInfo,0,sizeof(SCCI_INTERFACE_INFO));
    strcpy(SIInfo.cInterfaceName,"SCCISIM");
    SIInfo.iSoundChipCount = dllctx.maxchip;
  }

  ~SIMBody() {
  }

  int __stdcall getInterfaceCount() {
    OutputLog("SIMBody:getInterfaceCount");
    return 1;
  }

  SCCI_INTERFACE_INFO* __stdcall getInterfaceInfo(int iInterfaceNo) {
    OutputLog("SIMBody:getInterfaceInfo");
    return &SIInfo;
  }
  SoundInterface* __stdcall getInterface(int iInterfaceNo) {
    OutputLog("SIMBody:getInterface");
    return new SIMSI();
  }
  BOOL __stdcall releaseInterface(SoundInterface* pSoundInterface) {
    delete pSoundInterface;
    OutputLog("SIMBody:releaseInterface");
    return true;
  }
  BOOL __stdcall releaseAllInterface() {
    OutputLog("SIMBody:releaseAllInterface");
    return true;
  }
  SoundChip* __stdcall getSoundChip(int iSoundChipType,DWORD dClock) {
    OutputLog("SIMBody:getSoundChip:%d,%d",iSoundChipType, dClock);
    return scman->GetSoundChipByType(iSoundChipType);
  }
  BOOL __stdcall releaseSoundChip(SoundChip* pSoundChip) {
    OutputLog("SIMBody:releaseSoundChip");
    scman->ReleaseSimChip(pSoundChip);
    return true;
  }
  BOOL __stdcall releaseAllSoundChip() {
    OutputLog("SIMBody:releaseAllSoundChip");
    return true;
  }
  BOOL __stdcall setDelay(DWORD dMSec) {
    OutputLog("SIMBody:setDelay");
    return true;
  }
  DWORD __stdcall getDelay() {
    OutputLog("SIMBody:getDelay");
    return 0;
  }
  BOOL __stdcall reset() {
    OutputLog("SIMBody:reset");
    return true;
  }
  BOOL __stdcall init() {
    OutputLog("SIMBody:init");
    return true;
  }
  DLLDECL BOOL __stdcall initializeInstance() {
    OutputLog("SIMBody:initializeInstance");
    return true;
  }
  BOOL __stdcall releaseInstance() {
    OutputLog("SIMBody:releaseInstance");
    SimFree();
    return true;
  }
  BOOL __stdcall config() {
    OutputLog("SIMBody:config");
    return true;
  }
  DWORD __stdcall getVersion(DWORD *pMVersion) {
    OutputLog("SIMBody:getVersion");
    return 0;
  }
  BOOL __stdcall isValidLevelDisp() {
    OutputLog("SIMBody:isValidLevelDisp");
    return true;
  }
  BOOL __stdcall isLevelDisp() {
    OutputLog("SIMBody:isLevelDisp");
    return true;
  }
  void __stdcall setLevelDisp(BOOL bDisp) {
    OutputLog("SIMBody:setLevelDisp");
  }
  void __stdcall setMode(int iMode) {
    OutputLog("SIMBody:setMode:%d",iMode);
  }
  void __stdcall sendData() {
    // OutputLog("SIMBody:sendData");
  }
  void __stdcall clearBuff() {
    OutputLog("SIMBody:clearBuff");
  }
  void __stdcall setAcquisitionMode(int iMode) {
    OutputLog("SIMBody:setAcquisitionMode");
  }
  void __stdcall setAcquisitionModeClockRenge(DWORD dClock) {
    OutputLog("SIMBody:setAcquisitionModeClockRenge");
  }
  BOOL __stdcall setCommandBuffetSize(DWORD dBuffSize) {
    OutputLog("SIMBody:setCommandBuffetSize");
    return true;
  }
  BOOL __stdcall isBufferEmpty() {
    OutputLog("SIMBody:isBufferEmpty");
    return true;
  }
};

extern "C" DLLDECL SoundInterfaceManager* __cdecl getSoundInterfaceManager() {
  OpenConsole();
  OutputLog("-- %s --", DLL_VERSTR);
  OutputLog("getSoundInterfaceManager");

  SimInit();
  SimPlay();

  scman = new SimChipManager();

  SoundInterfaceManager *sim = new SIMBody();
  sim->initializeInstance();
  return sim;
}
