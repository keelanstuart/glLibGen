/*
Copyright ©2016-2017, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
provided that the above copyright notice appears in all copies, modifications, and distributions.
Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
any fitness of purpose of this software.
All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
*/

#pragma once

class CCrc32
{
public:
	enum { INITVAL = 0xFFFFFFFF };

	CCrc32();

	UINT32 Calculate(const UINT8 *data, size_t len, UINT32 initvalue = INITVAL);

protected:
	void InitializeTable();
	static UINT32 CrcTable[256];
	static BOOL TableFilled;
};
