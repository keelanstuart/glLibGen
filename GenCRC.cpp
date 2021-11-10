/*
Copyright ©2016-2021, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
provided that the above copyright notice appears in all copies, modifications, and distributions.
Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
any fitness of purpose of this software.
All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
*/

#include "stdafx.h"

#include "GenCRC.h"

UINT32 CCrc32::CrcTable[256];
BOOL CCrc32::TableFilled = false;

CCrc32::CCrc32()
{
	if (!TableFilled)
	{
		TableFilled = true;
		InitializeTable();
	}
}

void CCrc32::InitializeTable()
{

	// terms of polynomial defining this crc (except x^32):
	static const UINT8 p[] = {0, 1, 2, 4, 5, 7, 8, 10, 11, 12, 16, 22, 23, 26};	

	// polynomial exclusive-or pattern
	// make exclusive-or pattern from polynomial (0xedb88320L)
	UINT32 poly = 0;

	UINT32 n;

	for (n = 0; n < sizeof(p) / sizeof(UINT8); n++)
		poly |= 1L << (31 - p[n]);

	for (n = 0; n < 256; n++)
	{
		UINT32 c = (UINT32)n;

		for (UINT32 k = 0; k < 8; k++)
			c = c & 1 ? poly ^ (c >> 1) : c >> 1;

		CrcTable[n] = c;
	}
}

UINT32 CCrc32::Calculate(const UINT8 *data, size_t len, UINT32 initvalue)
{
	UINT32 ret = initvalue;

	while (len) 
	{
		ret = CrcTable[((int)ret ^ (*data++)) & 0xff] ^ (ret >> 8);

		--len;
	}

	return ret;
}

