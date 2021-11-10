/*
Copyright ©2016-2021, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
provided that the above copyright notice appears in all copies, modifications, and distributions.
Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
any fitness of purpose of this software.
All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
*/

#include "stdafx.h"
#include <stdio.h>
#include "GenTextOut.h"


// ************************************************************************
// Text Output Stream Methods

CGenTextOutput::CGenTextOutput()
{
	m_f = nullptr;
	indentchar = _T('\t');
	indentlvl = 0;

	modestr = _T("%d");
}


CGenTextOutput::CGenTextOutput(FILE *f)
{
	m_f = f;
	indentchar = _T('\t');
	indentlvl = 0;

	modestr = _T("%d");
}


CGenTextOutput::~CGenTextOutput()
{
}


void CGenTextOutput::NextLine(UINT skip, bool indent)
{
	// end the current line...
	PrintF(_T("\n"));

	// then skip over the specified number of lines...
	while (skip)
	{
		PrintF(_T("\n"));
		skip--;
	}

	// then indent that line...
	if (indent)
		for (UINT32 i = 0; i < indentlvl; i++)
			PrintF(_T("%c"), indentchar);
}


void CGenTextOutput::SetIndentChar(TCHAR ch)
{
	indentchar = ch;
}


TCHAR CGenTextOutput::GetIndentChar()
{
	return indentchar;
}


void CGenTextOutput::IncIndent(UINT inc)
{
	indentlvl += inc;
	if (indentlvl > TEXTSTREAM_MAXINDENT)
		indentlvl = TEXTSTREAM_MAXINDENT;
}


void CGenTextOutput::DecIndent(UINT dec)
{
	if (dec <= indentlvl)
		indentlvl -= dec;
}


void CGenTextOutput::SetIntegerMode(TEXTSTREAM_MODE mode)
{
	switch(mode)
	{
		case TEXTSTREAM_HEXMODE:
			modestr = _T("%x");
			break;

		case TEXTSTREAM_DECMODE:
		default:
			modestr = _T("%d");
			break;
	}
}


void CGenTextOutput::Print(const TCHAR *d, UINT number)
{
	if (!m_f)
		return;

	while (number)
	{
		PrintF(_T("%s"), d);
		number--;
	}
}


void CGenTextOutput::PrintF(const TCHAR *format, ...)
{
	if (!m_f)
		return;

	va_list marker;
	va_start(marker, format);
	_vftprintf(m_f, format, marker);
}

CGenTextOutput& CGenTextOutput::operator << (INT64 d)
{
	PrintF(modestr.c_str(), d);

	return *this;
}


CGenTextOutput& CGenTextOutput::operator << (UINT64 d)
{
	PrintF(modestr.c_str(), d);

	return *this;
}


CGenTextOutput& CGenTextOutput::operator << (INT32 d)
{
	PrintF(modestr.c_str(), d);

	return *this;
}


CGenTextOutput& CGenTextOutput::operator << (UINT32 d)
{
	PrintF(modestr.c_str(), d);

	return *this;
}


CGenTextOutput& CGenTextOutput::operator << (DWORD d)
{
	PrintF(modestr.c_str(), d);

	return *this;
}


CGenTextOutput& CGenTextOutput::operator << (INT16 d)
{
	PrintF(modestr.c_str(), d);

	return *this;
}


CGenTextOutput& CGenTextOutput::operator << (UINT16 d)
{
	PrintF(modestr.c_str(), d);

	return *this;
}


CGenTextOutput& CGenTextOutput::operator << (INT8 d)
{
	PrintF(modestr.c_str(), d);

	return *this;
}


CGenTextOutput& CGenTextOutput::operator << (UINT8 d)
{
	PrintF(modestr.c_str(), d);

	return *this;
}


CGenTextOutput& CGenTextOutput::operator << (double d)
{
	PrintF(_T("%f"), d);

	return *this;
}


CGenTextOutput& CGenTextOutput::operator << (float d)
{
	PrintF(_T("%f"), d);

	return *this;
}


CGenTextOutput& CGenTextOutput::operator << (const TCHAR *d)
{
	PrintF(_T("\"%s\""), d);

	return *this;
}

void CGenTextOutput::Flush()
{
	fflush(m_f);
}