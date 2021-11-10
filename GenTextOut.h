/*
Copyright ©2016-2021, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
provided that the above copyright notice appears in all copies, modifications, and distributions.
Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
any fitness of purpose of this software.
All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
*/

#pragma once


#define TEXTSTREAM_MAXINDENT		255


class CGenTextOutput
{
	enum TEXTSTREAM_MODE
	{
		TEXTSTREAM_DECMODE = 0,
		TEXTSTREAM_HEXMODE,

		TEXTSTREAM_NUMMODES
	};

public:
	CGenTextOutput();
	CGenTextOutput(FILE *f);
	virtual ~CGenTextOutput();

	// Write a string to the stream without formatting; allows the string to be optionally repeated
	// a number of times
	void Print(const TCHAR *d, UINT32 number = 1);

	// Writes a string of formatted text to the stream (see the standard c printf documentation for usage)
	void PrintF(const TCHAR *format, ...);

	// Overloaded operators for writing certain types in the C++ fashion
	CGenTextOutput& operator << (INT64  d);
	CGenTextOutput& operator << (UINT64 d);
	CGenTextOutput& operator << (INT32  d);
	CGenTextOutput& operator << (UINT32 d);
	CGenTextOutput& operator << (DWORD  d);
	CGenTextOutput& operator << (INT16  d);
	CGenTextOutput& operator << (UINT16 d);
	CGenTextOutput& operator << (INT8   d);
	CGenTextOutput& operator << (UINT8  d);
	CGenTextOutput& operator << (double d);
	CGenTextOutput& operator << (float d);
	CGenTextOutput& operator << (const TCHAR *d);

	// Begins the next line
	// Allows you to skip over a number of lines
	// Also allows you to optionally disable indenting
	void NextLine(UINT skip = 0, bool indent = true);

	// Indentation character functions
	// The indentation character is the character that will be inserted n times (where n is the
	// current indentation level) before each new line that is written.
	void SetIndentChar(TCHAR ch = _T('\t'));
	TCHAR GetIndentChar();

	// Indentation level increase
	void IncIndent(UINT inc = 1);

	// Indentation level decrease
	void DecIndent(UINT dec = 1);

	// Set the way in which integer-based types will be written to the stream
	// options are:
	//   TEXTSTREAM_DECMODE for decimal output (ie., 254 -> "254")
	//   TEXTSTREAM_HEXMODE for hexadecimal output (ie., 254 -> "0xFE")
	void SetIntegerMode(TEXTSTREAM_MODE mode = TEXTSTREAM_DECMODE);

	// Flush write caches
	void Flush();

protected:
	UINT indentlvl;
	TCHAR indentchar;

	UINT modeflags;
	tstring modestr;

	FILE *m_f;

};
