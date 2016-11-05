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

#define DLL_VER 0x100
#define DLL_VERSTR "SCCISIM.DLL 161104"

// コンテキスト
static struct 
{
	int freq;
	int songno;
	int pos;
	int load;

  int debuglog;
  int soundlog;
  int soundlog_start;
  int nlgmode;

  DWORD current_time;
  int wait_count;

  LOGCTX *logctx;

	SOUNDDEVICE *psd;
	SOUNDDEVICEPDI pdi;
} dllctx;


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

int GetSettingInt(const char *Key) {
  int Result = 0;
  char Buf[256];
  GetPrivateProfileString(PROG_NAME,Key,"",Buf,256, PROG_INI);
  sscanf(Buf,"%d",&Result);
  return Result;
}

void SaveDefaultSetting() {
  OutputLog("SaveDefaultSetting");

  WriteSettingInt("debuglog", 0);
  WriteSettingInt("soundlog", 0);
  WriteSettingInt("nlgmode", 0);
  WriteSettingInt("freq", 44100);
}

void LoadSetting(void) {
  char buf[256];
  OutputLog("LoadSetting");
  GetSettingString("debuglog", buf);
  if (buf[0] == 0) {
    SaveDefaultSetting();
    return;
  }

  dllctx.debuglog = GetSettingInt("debuglog");
  dllctx.soundlog = GetSettingInt("soundlog");
  dllctx.nlgmode = GetSettingInt("nlgmode");
  dllctx.freq = GetSettingInt("freq");
}

/// レンダラ
static void RenderWriteProc(void *lpargs, void *lpbuf, unsigned len) {
	if (dllctx.pos >= 0) DoRender((short *)lpbuf, len>>2); 
  else memset(lpbuf, 0, len);

	dllctx.pos += (len >> 2);
}

static void RenderTermProc(void *lpargs) {
}

void Stop();
void CloseSoundLog();

void OpenSoundLog() {
  OutputLog("OpenSoundLog");
  if (!dllctx.soundlog) return;
  if (dllctx.logctx) return;
  
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

void Init() {
	memset(chmask, 1, sizeof(chmask));
	memset(&dllctx, 0, sizeof(dllctx));

	dllctx.freq = DEF_FREQ;
  LoadSetting();
  if (dllctx.debuglog) OpenDebugLog();
	Stop();

	c86x_init();

  InitRender();
  SetRenderFreq(dllctx.freq);
}

void Stop() {
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

void Free() {
  Stop();
  CloseDebugLog();
}


void Play() {
	SOUNDDEVICEINITDATA sdid;

	Stop();

	sdid.freq = dllctx.freq;
	sdid.bit = DEF_BIT;
	sdid.ch = DEF_CH;
	sdid.lpargs = 0;
	sdid.Write = RenderWriteProc;
	sdid.Term = RenderTermProc;
  dllctx.pdi.hwnd = GetForegroundWindow();
	// if (hWnd) dllctx.pdi.hwnd = hWnd;
	sdid.ppdi = &dllctx.pdi;

	dllctx.pos = 0;
	dllctx.psd = CreateSoundDeviceDX(&sdid);
  // OutputLog("CreateSoundDeviceDx:%08x",dllctx.psd);
}

// 装置の追加
int SimAddDevice(int type, int bc) {
	RenderSetting rs;

	rs.type = type;
	rs.freq = dllctx.freq;
	rs.baseclock = bc;
	rs.use_gmc = 1;

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

class SIMCHIP : public SoundChip {
  int devid_;
  int sc_type_;
  int logid_;
  int clock_;

  BYTE Reg[0x200];

  public:

  SIMCHIP(int devid, int logid, int sc_type, int clock) {
    devid_ = devid;
    sc_type_ = sc_type;
    logid_ = logid;
    clock_ = clock;
  }

  int getDeviceId() {
    return devid_;
  }

  SCCI_SOUND_CHIP_INFO* __stdcall getSoundChipInfo() {
    OutputLog("getSoundChipInfo");
    return NULL;
  }
	// get sound chip type
   int __stdcall getSoundChipType() {
     OutputLog("getSoundChipType");
     return sc_type_;
   }
	// set Register data
   BOOL __stdcall setRegister(DWORD dAddr,DWORD dData) {
     Reg[dAddr&0x1ff] = dData;
     if (dllctx.logctx) {
        DWORD now_time = timeGetTime();
        if (!dllctx.soundlog_start) {
          MapEndLOG(dllctx.logctx);
          dllctx.soundlog_start = 1;
          dllctx.current_time = now_time;
          dllctx.wait_count = 0;
        }
        dllctx.wait_count += (now_time - dllctx.current_time);
        dllctx.current_time = now_time;

        while(dllctx.wait_count > 0) {
          WriteLOG_SYNC(dllctx.logctx);
          dllctx.wait_count -= 1;
        }
        WriteLOG_Data(dllctx.logctx, logid_, dAddr, dData);
    }

     SimWriteDevice(devid_,dAddr,dData);
     // OutputLog("Out:%02x %02x %02x",devid_, dAddr, dData);
     return true;
   }
	// get Register data(It may not be supported)
   DWORD __stdcall getRegister(DWORD dAddr) {
     // OutputLog("getRegister");
     return 0x00;
   }
	// initialize sound chip(clear registers)
   BOOL __stdcall init() {
     OutputLog("init");
     return true;
   }
	// get sound chip clock
   DWORD __stdcall getSoundChipClock() {
     // OutputLog("getSoundChipClock");
     return clock_;
   }
	// get writed register data
   DWORD __stdcall getWrittenRegisterData(DWORD addr) {
     return Reg[addr&0x1ff];
   }
	// buffer check
   BOOL __stdcall isBufferEmpty() {
    OutputLog("isBufferEmpty");
     return true;
   }
};

class SIMBody : public SoundInterfaceManager {

  int __stdcall getInterfaceCount() {
    OutputLog("getInterfaceCount");
    return 2;
  }
  SCCI_INTERFACE_INFO* __stdcall getInterfaceInfo(int iInterfaceNo) {
    OutputLog("getInterfaceInfo");
    return NULL;
  }
  SoundInterface* __stdcall getInterface(int iInterfaceNo) {
    OutputLog("getInterface");
    return NULL;
  }
  BOOL __stdcall releaseInterface(SoundInterface* pSoundInterface) {
    OutputLog("releaseInterface");
    return true;
  }
  BOOL __stdcall releaseAllInterface() {
    OutputLog("releaseAllInterface");
    return true;
  }
  SoundChip* __stdcall getSoundChip(int iSoundChipType,DWORD dClock) {
    int id = -1;
    int clock = dClock;
    int prio = 1;
    int log_type = LOG_TYPE_OPNA;

    OutputLog("getSoundChip:%d,%d",iSoundChipType, dClock);
    switch(iSoundChipType) {
      case SC_TYPE_YM2151:
        if (clock == 0) clock = RENDER_BC_4M;        
        id = SimAddDevice(RENDER_TYPE_OPM_FMGEN,clock);
        log_type = LOG_TYPE_OPM;
      break;
      case SC_TYPE_AY8910:
      case SC_TYPE_YMZ294:
        log_type = LOG_TYPE_SSG;
        prio = 0;
        if (clock == 0) clock = RENDER_BC_3M57;
        id = SimAddDevice(RENDER_TYPE_SSG,clock);
      break;
      case SC_TYPE_YM2413:
        log_type = LOG_TYPE_OPLL;
        if (clock == 0) clock = RENDER_BC_3M57;      
        id = SimAddDevice(RENDER_TYPE_OPLL,clock);
      break;
      case SC_TYPE_YMF262:
        log_type = LOG_TYPE_OPL3;
        if (clock == 0) clock = RENDER_BC_14M3;      
        id = SimAddDevice(RENDER_TYPE_OPL3,clock);
      break;
      case SC_TYPE_YM2203:
      case SC_TYPE_YM2608:
        log_type = LOG_TYPE_OPNA;
        if (clock == 0) clock = RENDER_BC_7M98;      
        id = SimAddDevice(RENDER_TYPE_OPNA,clock);
      break;
    }
    if (id >= 0) {
      int log_id = 0;
      OpenSoundLog();
      if (dllctx.logctx) log_id = AddMapLOG(dllctx.logctx, log_type, clock, prio);
      return new SIMCHIP(id, log_id, iSoundChipType, clock);
    }
    return NULL;
  }
  BOOL __stdcall releaseSoundChip(SoundChip* pSoundChip) {
    OutputLog("releaseSoundChip");
    if (pSoundChip == NULL) return false;
    int id = ((SIMCHIP *)pSoundChip)->getDeviceId();
    DeleteRender(id);
    delete pSoundChip;
    CloseSoundLog();
    return true;
  }
  BOOL __stdcall releaseAllSoundChip() {
    OutputLog("releaseAllSoundChip");
    return true;
  }
  BOOL __stdcall setDelay(DWORD dMSec) {
    OutputLog("setDelay");
    return true;
  }
  DWORD __stdcall getDelay() {
    OutputLog("getDelay");
    return 0;
  }
  BOOL __stdcall reset() {
    OutputLog("reset");
    return true;
  }
  BOOL __stdcall init() {
    OutputLog("init");
    return true;
  }
  DLLDECL BOOL __stdcall initializeInstance() {
    OutputLog("initializeInstance");
    return true;
  }
  BOOL __stdcall releaseInstance() {
    OutputLog("releaseInstance");
    Free();
    return true;
  }
  BOOL __stdcall config() {
    OutputLog("config");
    return true;
  }
  DWORD __stdcall getVersion(DWORD *pMVersion) {
    OutputLog("getVersion");
    return 0;
  }
  BOOL __stdcall isValidLevelDisp() {
    OutputLog("isValidLevelDisp");
    return true;
  }
  BOOL __stdcall isLevelDisp() {
    OutputLog("isLevelDisp");
    return true;
  }
  void __stdcall setLevelDisp(BOOL bDisp) {
    OutputLog("setLevelDisp");
  }
  void __stdcall setMode(int iMode) {
    OutputLog("setMode:%d",iMode);
  }
  void __stdcall sendData() {
    // OutputLog("sendData");
  }
  void __stdcall clearBuff() {
    OutputLog("clearBuff");
  }
  void __stdcall setAcquisitionMode(int iMode) {
    OutputLog("setAcquisitionMode");
  }
  void __stdcall setAcquisitionModeClockRenge(DWORD dClock) {
    OutputLog("setAcquisitionModeClockRenge");
  }
  BOOL __stdcall setCommandBuffetSize(DWORD dBuffSize) {
    OutputLog("setCommandBuffetSize");
    return true;
  }
  BOOL __stdcall isBufferEmpty() {
    OutputLog("isBufferEmpty");
    return true;
  }
};

extern "C" DLLDECL SoundInterfaceManager* __cdecl getSoundInterfaceManager() {
  OpenConsole();
  OutputLog("-- Start --");
  OutputLog("getSoundInterfaceManager");

  Init();
  Play();

  SoundInterfaceManager *sim = new SIMBody();
  sim->initializeInstance();
  return sim;
}
