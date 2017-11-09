#include "dx_local.h"
/*
================
Swap_Init
================
*/

qboolean	bigendien;

short(*_BigShort) (short l);
short(*_LittleShort) (short l);
int(*_BigLong) (int l);
int(*_LittleLong) (int l);
float(*_BigFloat) (float l);
float(*_LittleFloat) (float l);

short	BigShort(short l) { return _BigShort(l); }
short	LittleShort(short l) { return _LittleShort(l); }
int		BigLong(int l) { return _BigLong(l); }
int		LittleLong(int l) { return _LittleLong(l); }
float	BigFloat(float l) { return _BigFloat(l); }
float	LittleFloat(float l) { return _LittleFloat(l); }

short   ShortSwap(short l)
{
	byte    b1, b2;

	b1 = l & 255;
	b2 = (l >> 8) & 255;

	return (b1 << 8) + b2;
}

short	ShortNoSwap(short l)
{
	return l;
}

int    LongSwap(int l)
{
	byte    b1, b2, b3, b4;

	b1 = l & 255;
	b2 = (l >> 8) & 255;
	b3 = (l >> 16) & 255;
	b4 = (l >> 24) & 255;

	return ((int)b1 << 24) + ((int)b2 << 16) + ((int)b3 << 8) + b4;
}

int	LongNoSwap(int l)
{
	return l;
}

float FloatSwap(float f)
{
	union
	{
		float	f;
		byte	b[4];
	} dat1, dat2;


	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

float FloatNoSwap(float f)
{
	return f;
}

void Swap_Init(void)
{
	byte	swaptest[2] = { 1,0 };

	// set the byte swapping variables in a portable manner	
	if (*(short *)swaptest == 1)
	{
		bigendien = False;
		_BigShort = ShortSwap;
		_LittleShort = ShortNoSwap;
		_BigLong = LongSwap;
		_LittleLong = LongNoSwap;
		_BigFloat = FloatSwap;
		_LittleFloat = FloatNoSwap;
	}
	else
	{
		bigendien = True;
		_BigShort = ShortNoSwap;
		_LittleShort = ShortSwap;
		_BigLong = LongNoSwap;
		_LittleLong = LongSwap;
		_BigFloat = FloatNoSwap;
		_LittleFloat = FloatSwap;
	}
}