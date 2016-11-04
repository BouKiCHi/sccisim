#ifndef SCCISIM_DLL_H
#define SCCISIM_DLL_H

#include <windows.h>

typedef struct {
	char	cInterfaceName[64];
	int		iSoundChipCount;
} SCCI_INTERFACE_INFO;

typedef struct {
	char	cSoundChipName[64];
	int		iSoundChip;
	int		iCompatibleSoundChip[2];
	DWORD	dClock;
	DWORD	dCompatibleClock[2];
	BOOL	bIsUsed;
	DWORD	dBusID;						
	DWORD	dSoundLocation;
} SCCI_SOUND_CHIP_INFO;

class	SoundInterfaceManager;
class	SoundInterface;
class	SoundChip;

#ifdef DLLMAKE
#define DLLDECL __declspec(dllexport)
#else
#define DLLDECL __declspec(dllimport)
#endif

class	SoundInterfaceManager{
public:
	virtual int __stdcall getInterfaceCount() = 0;
	virtual SCCI_INTERFACE_INFO* __stdcall getInterfaceInfo(int iInterfaceNo) = 0;
	virtual SoundInterface* __stdcall getInterface(int iInterfaceNo) = 0;
	virtual BOOL __stdcall releaseInterface(SoundInterface* pSoundInterface) = 0;
	virtual BOOL __stdcall releaseAllInterface() = 0;
	virtual SoundChip* __stdcall getSoundChip(int iSoundChipType,DWORD dClock) = 0;
	virtual BOOL __stdcall releaseSoundChip(SoundChip* pSoundChip) = 0;
	virtual BOOL __stdcall releaseAllSoundChip() = 0;
	virtual BOOL __stdcall setDelay(DWORD dMSec) = 0;
	virtual DWORD __stdcall getDelay() = 0;
	virtual BOOL __stdcall reset() = 0;
	virtual BOOL __stdcall init() = 0;
	virtual	BOOL __stdcall initializeInstance() = 0;
	virtual BOOL __stdcall releaseInstance() = 0;
	virtual BOOL __stdcall config() = 0;
	virtual DWORD __stdcall getVersion(DWORD *pMVersion = NULL) = 0;
	virtual BOOL __stdcall isValidLevelDisp() = 0;
	virtual BOOL __stdcall isLevelDisp() = 0;
	virtual void __stdcall setLevelDisp(BOOL bDisp) = 0;
	virtual void __stdcall setMode(int iMode) = 0;
	virtual void __stdcall sendData() = 0;
	virtual void __stdcall clearBuff() = 0;
	virtual void __stdcall setAcquisitionMode(int iMode) = 0;
	virtual void __stdcall setAcquisitionModeClockRenge(DWORD dClock) = 0;
	virtual BOOL __stdcall setCommandBuffetSize(DWORD dBuffSize) = 0;
	virtual BOOL __stdcall isBufferEmpty() = 0;
};

class	SoundInterface{
public:
	virtual BOOL __stdcall isSupportLowLevelApi() = 0;
	virtual BOOL __stdcall setData(BYTE *pData,DWORD dSendDataLen) = 0;
	virtual DWORD __stdcall getData(BYTE *pData,DWORD dGetDataLen) = 0;
	virtual	BOOL __stdcall setDelay(DWORD dDelay) = 0;
	virtual DWORD __stdcall getDelay() = 0;
	virtual BOOL __stdcall reset() = 0;
	virtual BOOL __stdcall init() = 0;
	virtual DWORD	__stdcall getSoundChipCount() = 0;
	virtual	SoundChip* __stdcall getSoundChip(DWORD dNum) = 0;
};

class	SoundChip{
public:
	virtual SCCI_SOUND_CHIP_INFO* __stdcall getSoundChipInfo() = 0;
	virtual int __stdcall getSoundChipType() = 0;
	virtual BOOL __stdcall setRegister(DWORD dAddr,DWORD dData) = 0;
	virtual DWORD __stdcall getRegister(DWORD dAddr) = 0;
	virtual BOOL __stdcall init() = 0;
	virtual DWORD __stdcall getSoundChipClock() = 0;
	virtual DWORD __stdcall getWrittenRegisterData(DWORD addr) = 0;
	virtual BOOL __stdcall isBufferEmpty() = 0;
};

#ifdef __cplusplus
extern "C" {
#endif

typedef SoundInterfaceManager* (__stdcall *SCCIFUNC)(void);

#ifdef __cplusplus
}
#endif

#endif