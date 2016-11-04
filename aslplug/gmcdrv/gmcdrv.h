#ifndef __GMCDRV_H__
#define __GMCDRV_H__

enum {
	C86XDRV_NONE = 0,
	C86XDRV_OPL3 = 1,
	C86XDRV_OPM = 2,
	C86XDRV_OPN3L = 3,
    C86XDRV_OPNA = 4,
};

#ifdef __cplusplus
extern "C" {
#endif

// 初期化と開放
int c86x_init();
void c86x_free();

// 認識デバイス数の取得
int c86x_getcount(void);


// チップのマップ番号を返す。存在しない場合 = -1
int c86x_getchip(int type, int count);

// リセット
void c86x_reset(int mno);

// 書き込み・読み出し
void c86x_write(int mno, int addr, int data);
int c86x_read(int mno, int addr);

// 設定
void c86x_setPLL(int mno, int clock);
void c86x_setSSGvol(int mno, int vol);
    
#ifdef __cplusplus
}
#endif

#endif