/*
 C86 driver(dummy)
 2015-04-22 ver 0.11
*/

#include <stdio.h>
#include "gmcdrv.h"


// ---------------------------------------------------------------------------
//
//

// 初期化と開放
int c86x_init()
{
	return 0;
}

void c86x_free()
{
}

// 認識デバイス数を得る
int c86x_getcount(void)
{
    return 0;
}


int c86x_getchip(int type, int count)
{
	return -1;
}

void c86x_reset(int mno)
{
}

void c86x_write(int mno, int addr, int data)
{
}

int c86x_read(int mno, int addr)
{
    return 0x00;
}


void c86x_setPLL(int mno, int clock)
{
}

void c86x_setSSGvol(int mno, int vol)
{
}

