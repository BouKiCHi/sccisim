/*
 C86 driver for nezplay asl
 2015-04-22 ver 0.11
 m88 GIMIC対応版のソースを多いに参考にさせていただきましたが、
 BSDライセンスということで一つよろしくお願いいたします。BouKiCHi

*/

#include <stdio.h>
#include <windows.h>
#include "c86ctl.h"
#include "gmcdrv.h"


static HMODULE			g_mod = 0;
static IRealChipBase	*g_chipbase = 0;

struct C86XDRV
{
	IGimic *gimic;
	IRealChip *chip;
	int	type;
	bool reserve;
};

// 最大モジュール数
#define MAX_GMCMOD 8

static C86XDRV c86xdrv[MAX_GMCMOD];
static int c86x_count = 0;


// DLLの呼び出し
static bool LoadDLL()
{
	// まだDLLがロードされてない
	if (!g_mod) 
	{
		g_mod = ::LoadLibrary( "c86ctl.dll" );

		if (!g_mod)
			return false;

		// インスタンス作成を呼び出す
		typedef HRESULT (WINAPI *TCreateInstance)( REFIID, LPVOID* );
		TCreateInstance pCI;
		pCI = (TCreateInstance)::GetProcAddress( g_mod, "CreateInstance" );

		if (pCI)
		{
			// チップを列挙するクラスの取得
			(*pCI)(IID_IRealChipBase, (void**)&g_chipbase);
			
			// 成功したら初期化
			if (g_chipbase) 
			{
				g_chipbase->initialize();
			} 
			else
			{
				::FreeLibrary( g_mod );
				g_mod = 0;
			}
		} 
		else 
		{
			// 失敗
			::FreeLibrary( g_mod );
			g_mod = 0;
		}
	}
	return g_mod ? true : false;
}

// DLLの開放
static void FreeDLL()
{
	if (g_chipbase) 
	{
		g_chipbase->deinitialize();
		g_chipbase = 0;
	}
	
	if (g_mod) 
	{
		::FreeLibrary( g_mod );
		g_mod = 0;
	}
}

// ---------------------------------------------------------------------------
//
//

// デバイス名の確認と追加
static int c86x_check_devname(IGimic *gimic, IRealChip *chip,
            const char *devname, const char *gmc_id, int chiptype)
{
    if (strcmp(devname, gmc_id) == 0)
    {
        // 表示
        ::printf("Added:%s\n", devname);
        
        
        // インターフェースの設定
        int pos = c86x_count;
        
        c86xdrv[pos].gimic = gimic;
        c86xdrv[pos].chip = chip;
        c86xdrv[pos].type = chiptype;
        chip->reset();
        
        c86x_count++;
    }
    // 最大個数に達した場合は終了

    if (c86x_count >= MAX_GMCMOD)
        return -1;
    
    return 0;
}

// 初期化する
int c86x_init()
{
	int i = 0;

	if (!LoadDLL())
		return -1;
		
	// 初期化
	c86x_count = 0;
	memset(c86xdrv, 0, sizeof(c86xdrv));
	
	// チップの個数を得る
	int numOfChips = g_chipbase->getNumberOfChip();

    // ::printf("numOfChips:%d\n", numOfChips);
    
    // チップの個数分、繰り返す
	for (i = 0; i < numOfChips; i++) 
	{
		IGimic *gimic;
		IRealChip *chip;
		
		// インターフェースの取得
		g_chipbase->getChipInterface( i, IID_IGimic, (void**)&gimic );
		g_chipbase->getChipInterface( i, IID_IRealChip, (void**)&chip );
		
		// モジュール上方の取得
		Devinfo info;
		gimic->getModuleInfo( &info );
		
		// モジュール名の文字列化
		char devname[256];
        int len = sizeof(info.Devname);
        strncpy(devname, info.Devname, len);
        devname[len] = 0x00;
        
        // ::printf("Module:%s\n", devname);
        
        // GMC-OPL3
        if (c86x_check_devname(gimic, chip, devname, "GMC-OPL3", C86XDRV_OPL3) < 0)
            break;
        
        // GMC-OPM
        if (c86x_check_devname(gimic, chip, devname, "GMC-OPM", C86XDRV_OPM) < 0)
            break;

        // GMC-OPN3L
        if (c86x_check_devname(gimic, chip, devname, "GMC-OPN3L", C86XDRV_OPN3L) < 0)
            break;

        // GMC-OPNA
        if (c86x_check_devname(gimic, chip, devname, "GMC-OPNA", C86XDRV_OPNA) < 0)
            break;

    }
	
	// 追加できるチップが存在しなかった
	if (c86x_count == 0) 
		return -1;
    
	return 0;
}

// 開放する
void c86x_free()
{
    int i;
    for(i = 0; i < c86x_count; i++)
    {
        c86xdrv[i].chip->reset();
    }
    
    FreeDLL();

    c86x_count = 0;
}

// デバイス数を得る
int c86x_getcount(void)
{
    return c86x_count;
}

// typeのチップのcount個目のidを得る
int c86x_getchip(int type, int count)
{
	int i;
	for(i = 0; i < c86x_count; i++)
	{
		if (c86xdrv[i].type == type)
        {
            if (count > 0)
                count--;
            else
                return i;
        }
	}
	return -1;
}

// リセットする
void c86x_reset(int mno)
{
	if (mno < 0)
		return;

	c86xdrv[mno].chip->reset();
}

// 書き込む
void c86x_write(int mno, int addr, int data)
{
	if (mno < 0)
		return;
	
	c86xdrv[mno].chip->out(addr, data);
}

// 読み出す
int c86x_read(int mno, int addr)
{
	if (mno < 0)
		return 0x00;
	
	return c86xdrv[mno].chip->in(addr);
}

// PLLの周波数設定
void c86x_setPLL(int mno, int clock)
{
	if (mno < 0)
	return;
	
	c86xdrv[mno].gimic->setPLLClock( clock );
}

// SSGボリュームの設定
void c86x_setSSGvol(int mno, int vol)
{
	if (mno < 0)
	return;
	
	c86xdrv[mno].gimic->setSSGVolume( vol );
}

