/*
Copyright ©2016-2021, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
provided that the above copyright notice appears in all copies, modifications, and distributions.
Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
any fitness of purpose of this software.
All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
*/

#include "stdafx.h"
#include "GenParser.h"


CGenParser::CGenParser()
{
	m_data = nullptr;
	m_datalen = 0;
	m_pos = 0;

	m_curType = TT_NONE;
}


CGenParser::~CGenParser()
{
}


void CGenParser::SetSourceData(const TCHAR *data, size_t datalen)
{
	m_data = (TCHAR *)data;
	m_datalen = datalen;
	m_pos = 0;

	m_curType = TT_NONE;
	m_curStr.clear();
}


bool CGenParser::NextToken()
{
	if (!m_data || !m_datalen)
		return false;

	m_curType = TT_NONE;
	m_curStr.clear();
	TCHAR strdelim = _T('\0');

	while (m_pos < m_datalen)
	{
		// skip whitespace
		if (_tcschr(_T("\n\r\t "), m_data[m_pos]))
		{
			if (m_curType == TT_NONE)
			{
				// skip whitespace
				m_pos++;
			}
			else if (m_curType == TT_SHORTCOMMENT)
			{
				// short comments end at EOL
				if (_tcschr(_T("\n\r"), m_data[m_pos]))
					break;

				m_curStr += m_data[m_pos];
				m_pos++;
			}
			else if ((m_curType == TT_STRING) || (m_curType == TT_LONGCOMMENT))
			{
				m_curStr += m_data[m_pos];
				m_pos++;
			}
			else
			{
				break;
			}

			continue;
		}

		if (_tcschr(_T("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_"), m_data[m_pos]))
		{
			if (m_curType == TT_NONE)
			{
				// nothing yet?  this is an identifier
				m_curType = TT_IDENT;

				m_curStr += m_data[m_pos];
				m_pos++;
			}
			else if (m_curType == TT_HEXNUMBER)
			{
				if (_tcschr(_T("abcdefABCDEF"), m_data[m_pos]))
				{
					m_curStr += m_data[m_pos];
					m_pos++;
				}
				else
					break;
			}
			else if ((m_curType == TT_STRING) || (m_curType == TT_IDENT) || (m_curType == TT_SHORTCOMMENT) || (m_curType == TT_LONGCOMMENT))
			{
				// already an identifier or string? just add and move on
				m_curStr += m_data[m_pos];
				m_pos++;
			}
			else
			{
				// if we're a number or a symbol and hit one of these characters, get out
				break;
			}

			continue;
		}

		if (_tcschr(_T("0123456789"), m_data[m_pos]))
		{
			if (m_curType == TT_NONE)
			{
				// nothing yet?  this is a number
				m_curType = TT_NUMBER;

				if ((m_data[m_pos + 1] == _T('x')) || (m_data[m_pos + 1] == _T('X')))
				{
					m_curType = TT_HEXNUMBER;
					m_pos += 2;
				}
				else
				{
					m_curStr += m_data[m_pos];
					m_pos++;
				}
			}
			else if ((m_curType == TT_NUMBER) || (m_curType == TT_STRING) || (m_curType == TT_IDENT) || (m_curType == TT_HEXNUMBER) || (m_curType == TT_SHORTCOMMENT) || (m_curType == TT_LONGCOMMENT))
			{
				// already an identifier or string? just add and move on
				m_curStr += m_data[m_pos];
				m_pos++;
			}
			else
			{
				// if we're a number or a symbol and hit one of these characters, get out
				break;
			}

			continue;
		}

		// symbols of every kind
		if (_tcschr(_T("`~!@#$%^&*()-=+[]{}\\|;:,.<>/?"), m_data[m_pos]))
		{
			if (m_curType == TT_NONE)
			{
				if (m_data[m_pos] == _T('-'))
				{
					if (((m_pos + 1) < m_datalen) && _tcschr(_T("0123456789"), m_data[m_pos + 1]))
					{
						// nothing yet?  this is a number
						m_curType = TT_NUMBER;

						m_curStr += m_data[m_pos];
						m_pos++;

						continue;
					}
				}
				else if (m_data[m_pos] == _T('/'))
				{
					if (((m_pos + 1) < m_datalen) && _tcschr(_T("/*"), m_data[m_pos + 1]))
					{
						// nothing yet?  this is a comment
						m_curType = (m_data[m_pos + 1] == _T('/')) ? TT_SHORTCOMMENT : TT_LONGCOMMENT;

						m_curStr += m_data[m_pos];
						m_curStr += m_data[m_pos + 1];
						m_pos += 2;

						continue;
					}
				}

				// nothing yet?  this is a number
				m_curType = TT_SYMBOL;

				m_curStr += m_data[m_pos];
				m_pos++;
			}
			else if (m_curType == TT_NUMBER)
			{
				// a decimal point can only be inserted if it's already a number and there's no pre-existing decimal point
				if ((m_data[m_pos] == _T('.')) && !_tcschr(m_curStr.c_str(), _T('.')))
				{
					m_curStr += m_data[m_pos];
					m_pos++;

					continue;
				}
			}
			else if ((m_curType == TT_STRING) || (m_curType == TT_SHORTCOMMENT))
			{
				m_curStr += m_data[m_pos];
				m_pos++;

				continue;
			}
			else if (m_curType == TT_LONGCOMMENT)
			{
				bool ast = (m_data[m_pos] == _T('*'));

				m_curStr += m_data[m_pos];
				m_pos++;

				if (ast && (m_pos < m_datalen) && (m_data[m_pos] == _T('/')))
				{
					m_curStr += m_data[m_pos];
					m_pos++;
					break;
				}

				continue;
			}

			break;
		}

		if (_tcschr(_T("\"'"), m_data[m_pos]) != nullptr)
		{
			if (m_curType == TT_NONE)
			{
				m_curType = TT_STRING;
				strdelim = m_data[m_pos];
				m_pos++;
			}
			else if (m_curType == TT_STRING)
			{
				if (strdelim != m_data[m_pos])
				{
					m_curStr += m_data[m_pos];
					m_pos++;
				}
				else
				{
					m_pos++;
					strdelim = _T('\0');
					break;
				}
			}
			else if ((m_curType != TT_LONGCOMMENT) && (m_curType != TT_SHORTCOMMENT))
				continue;
		}

		if ((m_curType == TT_STRING) || (m_curType == TT_SHORTCOMMENT) || (m_curType == TT_LONGCOMMENT))
		{
			m_curStr += m_data[m_pos];
		}

		m_pos++;
	}

	if (m_curStr.empty())
	{
		return false;
	}

	return true;
}


bool CGenParser::NextLine()
{
	if (!m_data || !m_datalen)
		return false;

	m_curType = TT_NONE;
	m_curStr.clear();

	while (m_pos < m_datalen)
	{
		// skip over everything except EOLs
		if (_tcschr(_T("\n\r"), m_data[m_pos]))
		{
			break;
		}

		m_pos++;
	}

	while (m_pos < m_datalen)
	{
		// skip over all EOLs until we hit something else
		if (!_tcschr(_T("\n\r"), m_data[m_pos]))
		{
			return true;
		}

		m_pos++;
	}

	return false;
}

bool CGenParser::ToEndOfLine()
{
	if (!m_data || !m_datalen)
		return false;

	m_curType = TT_NONE;
	m_curStr.clear();

	while (m_pos < m_datalen)
	{
		// skip over everything except EOLs
		if (_tcschr(_T("\n\r"), m_data[m_pos]))
		{
			return true;
		}

		m_curStr += m_data[m_pos];
		m_pos++;
	}

	return false;
}

CGenParser::TOKEN_TYPE CGenParser::GetCurrentTokenType()
{
	return m_curType;
}


TCHAR *CGenParser::GetCurrentTokenString()
{
	return (TCHAR *)m_curStr.c_str();
}

bool CGenParser::IsToken(TCHAR *s, bool case_sensitive)
{
	if (case_sensitive)
		return !_tcscmp(m_curStr.c_str(), s);

	return !_tcsicmp(m_curStr.c_str(), s);
}
