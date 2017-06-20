/*
Copyright �2016-2017, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement,
provided that the above copyright notice appears in all copies, modifications, and distributions.
Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for
any fitness of purpose of this software.
All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.
*/

#include "stdafx.h"
#include "GLlibgen.h"
#include "GenParser.h"
#include "GenTextOut.h"
#include <map>
#include <algorithm>
#include "HttpDownload.h"
#include <tinyxml2.h>
#include "GenCRC.h"

typedef struct sDocLinkData
{
	const TCHAR *baseurl;
	const TCHAR *ext;
	const TCHAR *descpath;
} SDocLinkData;

const SDocLinkData docdata[] =
{
	{ _T("https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/"), _T(".xml"), _T("body.div.div:2.p") },
	{ _T("https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/"), _T(".xhtml"), _T("body.div.div:2.p") },
	{ _T("https://www.opengl.org/sdk/docs/man/html/"), _T(".xhtml"), _T("body.div.div:2.p") },
	{ _T("https://www.opengl.org/sdk/docs/man2/xhtml/"), _T(".xml"), _T("body.div.div:2.p") },

	// keep this here so we know when to stop checking new URLs
	{ nullptr, nullptr, nullptr }
};

typedef enum eExtensionOSV
{
	EOSV_NONE = 0,

	EOSV_ARB,
	EOSV_EXT,
	EOSV_NV,
	EOSV_AMD,
	EOSV_ATI,
	EOSV_INTEL,
	EOSV_SGI,
	EOSV_SUN,
	EOSV_APPLE,
	EOSV_OES,
	EOSV_INGR,
	EOSV_KHR,

	EOSV_UNKNOWN

} EExtensionOSV;

TCHAR *OSVName[EOSV_UNKNOWN] =
{
	_T("?"),

	_T("GL_ARB"),
	_T("GL_EXT"),
	_T("GL_NV"),
	_T("GL_AMD"),
	_T("GL_ATI"),
	_T("GL_INTEL"),
	_T("GL_SGI"),
	_T("GL_SUN"),
	_T("GL_APPLE"),
	_T("GL_OES"),
	_T("GL_INGR"),
	_T("GL_KHR"),
};

typedef struct sVersion
{
	sVersion() { major = 1; minor = 0; }
	sVersion(UINT _major, UINT _minor) { major = _major; minor = _minor; }

	UINT major;
	UINT minor;

} SVersion;

typedef struct sFunctionData
{
	tstring ret;
	tstring params;
	bool needsfunctype;
	EExtensionOSV osv;
	SVersion ver;
} SFunctionData;

typedef std::map<tstring, tstring> TMapStrStr;
typedef std::pair<tstring, tstring> TPairStrStr;

typedef std::map<tstring, SFunctionData> TMapStrFuncData;
typedef std::pair<tstring, SFunctionData> TPairStrFuncData;


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// problem: glCreateSyncFromCLeventARB, glGetStringi (const returns), PathGlyphIndexRangeNV (bracketed params)

// The one and only application object

CWinApp theApp;

using namespace tinyxml2;
using namespace std;

// traverses an xml tree by node name, where the names are delimted by '.' characters
// to find a sequential number of nodes with the given name, use a ':' followed by a number, indicating the count of nodes to find
// ex. "node1.node2:7.node3:2 - finds the third "node3" under the seventh "node2" under the first "node1"
const XMLElement *EvaluatePath(const XMLElement *root, const TCHAR *path)
{
	const XMLElement *ret = nullptr;

	bool first_char = true;
	tstring pathcomp;
	while (path && *path)
	{
		if ((!first_char && _istalnum(*path)) || _istalpha(*path))
		{
			first_char = false;
			pathcomp += *path;
		}

		path++;

		if (!_istalnum(*path) && !pathcomp.empty())
		{
			UINT count = 1;

			if (*path == _T(':'))
			{
				path++;

				tstring numstr;

				while (*path && _istdigit(*path))
				{
					numstr += *path;
					path++;
				}

				count = _ttoi(numstr.c_str());
				if (!count)
					count = 1;
			}

			std::transform(pathcomp.begin(), pathcomp.end(), pathcomp.begin(), _totlower);

			std::string findpath;

#if defined(UNICODE)
			int len = WideCharToMultiByte(CP_UTF8, 0, pathcomp.c_str(), -1, NULL, 0, NULL, NULL);
			if (len)
			{
				findpath.reserve(len + 1);
				WideCharToMultiByte(CP_UTF8, 0, pathcomp.c_str(), -1, (char *)(findpath.c_str()), len, NULL, NULL);
			}
#else
			findpath = pathcomp.c_str();
#endif

			const XMLElement *tmpret = ((ret == nullptr) ? root : ret)->FirstChildElement(findpath.c_str());

			while (--count && tmpret)
			{
				tmpret = tmpret->NextSiblingElement(findpath.c_str());
			}

			if (!tmpret)
			{
				return nullptr;
			}

			ret = tmpret;

			pathcomp.clear();
			first_char = true;
		}
	}

	return ret;
}

tstring gOGLHeaderLocation = _T("gl.h");
tstring gOGLEXTHeaderLocation = _T("glext.h");
tstring gClassName = _T("COpenGL");
tstring gOutputBaseFileName = _T("gllib");
tstring gOutputDir = _T(".");
SVersion gOGLVersionMax(-1, 0);
bool gAfxInclude = false;
bool gOSVIncludes[EOSV_UNKNOWN] = { true, false, false, false, false, false, false, false, false, false, false, false, false };

CGenTextOutput *gLog = nullptr;

typedef struct sCmdLineParamData
{
	enum EParamType
	{
		CLPT_NONE = 0,

		CLPT_CMD,			// bool - just activates a behavior, like a bool that defaults to off
		CLPT_INT,			// INT
		CLPT_FLOAT,			// float
		CLPT_ONOFF,			// bool - keys on "off" or "on"
		CLPT_YESNO,			// bool - keys on "no" or "yes"
		CLPT_TRUEFALSE,		// bool - keys on "false" or "true"
		CLPT_STRING,		// tstring
		CLPT_VERSION,		// version

		CLPT_NUMTYPES
	};

	TCHAR *name;
	EParamType type;
	void *data;
	TCHAR *help;

} SCmdLineParamData;

SCmdLineParamData gParams[] =
{
	{ _T("glh"),		SCmdLineParamData::CLPT_STRING,		&gOGLHeaderLocation,		_T("local or remote (HTTP) file path to \"gl.h\"") },
	{ _T("glexth"),		SCmdLineParamData::CLPT_STRING,		&gOGLEXTHeaderLocation,		_T("local or remote (HTTP) file path to \"glext.h\"") },
	{ _T("class"),		SCmdLineParamData::CLPT_STRING,		&gClassName,				_T("name of the class that will be generated") },
	{ _T("basefile"),	SCmdLineParamData::CLPT_STRING,		&gOutputBaseFileName,		_T("base filename that the C++ code will go into (file.cpp and file.h)") },
	{ _T("outdir"),		SCmdLineParamData::CLPT_STRING,		&gOutputDir,				_T("directory where the code will be generated and gl headers copied") },
	{ _T("ver"),		SCmdLineParamData::CLPT_VERSION,	&gOGLVersionMax,			_T("determines the maximum version of OpemGL to support") },

	{ _T("arb"),		SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_ARB],	_T("includes ARB extensions") },
	{ _T("ext"),		SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_EXT],	_T("includes EXT extensions") },
	{ _T("nv"),			SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_NV],		_T("includes nVidia extensions") },
	{ _T("amd"),		SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_AMD],	_T("includes AMD extensions") },
	{ _T("ati"),		SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_ATI],	_T("includes ATI extensions") },
	{ _T("intel"),		SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_INTEL],	_T("includes Intel extensions") },
	{ _T("sgi"),		SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_SGI],	_T("includes Silicon Graphics extensions") },
	{ _T("sun"),		SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_SUN],	_T("includes Sun Microsystems extensions") },
	{ _T("apple"),		SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_APPLE],	_T("includes Apple extensions") },
	{ _T("oes"),		SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_OES],	_T("includes OES extensions") },
	{ _T("ingr"),		SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_OES],	_T("includes Intergraph extensions") },
	{ _T("khr"),		SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_OES],	_T("includes Khronos extensions") },

	{ _T("afx"),		SCmdLineParamData::CLPT_CMD,	&gAfxInclude,				_T("adds \"#include <stdafx.h>\"") },

	{ nullptr,			SCmdLineParamData::CLPT_NONE,	nullptr,					nullptr }
};

bool ParseCommandLineParams(const SCmdLineParamData *paramdata, const TCHAR *cmdline)
{
	UINT ordinal = 0;
	bool ret = true;

	CGenParser cp;
	cp.SetSourceData(cmdline, _tcslen(cmdline));

	while (cp.NextToken())
	{
		if (cp.IsToken(_T("-")))
		{
			cp.NextToken();

			const SCmdLineParamData *pd = paramdata;
			for (; ((pd != nullptr) && (pd->name != nullptr) && (pd->type != SCmdLineParamData::CLPT_NONE)); pd++)
			{
				if (cp.IsToken(pd->name))
					break;
			}

			if ((pd->name != nullptr) && (pd->type != SCmdLineParamData::CLPT_NONE))
			{
				if (pd->type != SCmdLineParamData::CLPT_CMD)
				{
					// skip the :
					cp.NextToken();

					cp.NextToken();
				}

				switch (pd->type)
				{
					case SCmdLineParamData::CLPT_VERSION:
					{
						TCHAR *c = cp.GetCurrentTokenString();
						TCHAR *d = _tcschr(c, _T('.'));
						if (d)
						{
							*d = _T('\0');
							d++;
						}
						else
						{
							d = NULL;
						}
						SVersion tmp;
						tmp.major = (UINT)_ttoi(c);
						tmp.minor = d ? (UINT)_ttoi(d) : 0;
						*((SVersion *)pd->data) = tmp;
						break;
					}

					case SCmdLineParamData::CLPT_STRING:
						*((tstring *)pd->data) = cp.GetCurrentTokenString();
						break;

					case SCmdLineParamData::CLPT_FLOAT:
						*((float *)pd->data) = (float)_ttof(cp.GetCurrentTokenString());
						break;

					case SCmdLineParamData::CLPT_INT:
						*((INT *)pd->data) = (INT)_ttoi(cp.GetCurrentTokenString());
						break;

					case SCmdLineParamData::CLPT_CMD:
						*((bool *)pd->data) = true;
						break;

					default:
						ret = false;
				}
			}
		}
	}

	return ret;
}

bool ParseAPIENTRYP(CGenParser &p, TMapStrStr &functype_to_funcname)
{
	p.NextToken();

	// this should be the function typedef

	tstring ft = p.GetCurrentTokenString();
	std::transform(ft.begin(), ft.end(), ft.begin(), _totupper);

	functype_to_funcname.insert(TPairStrStr(ft, tstring()));

	return true;
}

bool ParseFunctionsFromFile(const TCHAR *filename, TMapStrStr &functype_to_funcname, TMapStrFuncData &funcname_to_funcdata, TMapStrStr &define_to_value, CCrc32 &crc, UINT32 &crcval)
{
	bool ret = false;

	FILE *in_file = nullptr;
	if ((_tfopen_s(&in_file, filename, _T("rt, ccs=UTF-8")) != EINVAL) && in_file)
	{
		gLog->PrintF(_T("Parsing %s"), filename);
		gLog->NextLine(1);
		gLog->Flush();

		_fseeki64(in_file, 0, SEEK_END);
		size_t in_size = (size_t)_ftelli64(in_file);
		_fseeki64(in_file, 0, SEEK_SET);

		TCHAR *in_data = (in_size > 0) ? (TCHAR *)malloc(in_size * sizeof(TCHAR)) : nullptr;

		if (in_data)
		{
			if (fread(in_data, sizeof(TCHAR), in_size, in_file) > 0)
			{
				crcval = crc.Calculate((const UINT8 *)in_data, sizeof(TCHAR) * in_size, crcval);

				CGenParser p;
				p.SetSourceData(in_data, in_size);

				tstring prev;

				EExtensionOSV curosv = EOSV_NONE;
				SVersion curver;

				while (p.NextToken())
				{
					if (p.IsToken(_T("#")))
					{
						p.NextToken();
						if (p.IsToken(_T("define")))
						{
							p.NextToken();
							tstring def = p.GetCurrentTokenString();

							p.NextToken();
							tstring val = p.GetCurrentTokenString();

							define_to_value.insert(TPairStrStr(def, val));
						}
						else if (p.IsToken(_T("ifndef")))
						{
							p.NextToken();
							tstring chk = p.GetCurrentTokenString();

							if (_tcsstr(chk.c_str(), _T("GL_VERSION_")))		// we found a version block... parse out the numbers
							{
								UINT major = 1;
								UINT minor = 0;

								std::replace(chk.begin(), chk.end(), _T('_'), _T(' '));

								CGenParser verp;
								verp.SetSourceData(chk.c_str(), chk.length() * sizeof(TCHAR));

								while ((verp.GetCurrentTokenType() != CGenParser::TT_NUMBER) && verp.NextToken()) { }
								if (verp.GetCurrentTokenType() == CGenParser::TT_NUMBER)
								{
									major = _ttoi(verp.GetCurrentTokenString());
									if (verp.NextToken())
									{
										minor = _ttoi(verp.GetCurrentTokenString());
									}
								}

								curver.major = major;
								curver.minor = minor;
							}
							else	// look for the vendor extension name...
							{
								for (UINT eidx = 0; eidx < EOSV_UNKNOWN; eidx++)
								{
									if (_tcsstr(chk.c_str(), OSVName[eidx]))
										curosv = (EExtensionOSV)eidx;
								}
							}
						}

						p.NextLine();
					}
					else if (p.IsToken(_T("APIENTRYP")))
					{
						// API entry pointer typedef - cache the next token as the function type
						ParseAPIENTRYP(p, functype_to_funcname);
					}
					else if (p.IsToken(_T("APIENTRY")))
					{
						p.NextToken();	// this should be the function name

						if (p.IsToken(_T("*")))
						{
							ParseAPIENTRYP(p, functype_to_funcname);
							continue;
						}

						// make it upper case and prepend "PFN" to search for it in the type2name map
						tstring fn = p.GetCurrentTokenString(), ucname = fn;
						std::transform(ucname.begin(), ucname.end(), ucname.begin(), _totupper);

						ucname = ucname + _T("PROC");
						TMapStrStr::iterator it = functype_to_funcname.find(ucname);

						if (it == functype_to_funcname.end())
						{
							ucname = _T("PFN") + ucname;
							it = functype_to_funcname.find(ucname);
						}

						bool needsfunctype = false;

						it = functype_to_funcname.find(ucname);
						if (it == functype_to_funcname.end())
						{
							functype_to_funcname.insert(TPairStrStr(ucname, fn));
							needsfunctype = true;
						}
						else
						{
							it->second = fn;
						}

						// acquire the parameters to the function
						p.ToEndOfLine();
						TCHAR *pstr = p.GetCurrentTokenString();
						if (*pstr == _T(' '))
							pstr++;

						tstring params = pstr;
						size_t sci = params.find_first_of(_T(';'), 0);
						params.replace(sci, 1, _T("\0"));

						SFunctionData fd;
						fd.ret = prev;	// the token prior to APIENTRY should be the return type
						fd.params = params;	// to the end of the line should now be the params
						fd.osv = curosv;
						fd.ver = curver;
						fd.needsfunctype = needsfunctype;

						// store the function data mapped to the function name
						funcname_to_funcdata.insert(TPairStrFuncData(fn, fd));

						p.NextLine();
					}
					else
					{
						if (!_tcscmp(prev.c_str(), _T("const")) || p.IsToken(_T("*")))
						{
							prev += _T(" ");
							prev += p.GetCurrentTokenString();
						}
						else
							prev = p.GetCurrentTokenString();
					}
				}
			}

			free(in_data);

			ret = true;
		}

		fclose(in_file);
	}

	return ret;
}

#define CRCBUFSZ	16

bool WriteCPPWrapper(tstring &out_name_h, tstring &out_name_cpp, TMapStrStr &functype_to_funcname, TMapStrFuncData &funcname_to_funcdata, TMapStrStr &define_to_value, UINT32 crcval)
{
	FILE *out_file_h = nullptr;
	FILE *out_file_cpp = nullptr;

	bool h_crc_diff = true;
	bool cpp_crc_diff = true;

	if (PathFileExists(out_name_h.c_str()) && (_tfopen_s(&out_file_h, out_name_h.c_str(), _T("rt, ccs=UTF-8")) != EINVAL) && out_file_h)
	{
		gLog->PrintF(_T("Previously generated .h file found - checking CRC... ")); gLog->NextLine();

		// a small buffer should be fine here...
		TCHAR buf[CRCBUFSZ];

		_fseeki64(out_file_h, 0, SEEK_END);
		size_t len = _ftelli64(out_file_h) - 8; // 8 characters in the hex string
		_fseeki64(out_file_h, len, SEEK_SET);

		size_t bufrd = fread(buf, sizeof(TCHAR), CRCBUFSZ, out_file_h);
		if (bufrd)
		{
			buf[bufrd] = _T('\0');

			UINT32 tmpcrc;
			_stscanf_s(buf, _T("%08x"), &tmpcrc);

			h_crc_diff = (tmpcrc != crcval);
		}

		fclose(out_file_h);
		out_file_h = nullptr;
	}

	if (PathFileExists(out_name_cpp.c_str()) && (_tfopen_s(&out_file_cpp, out_name_cpp.c_str(), _T("rt, ccs=UTF-8")) != EINVAL) && out_file_cpp)
	{
		gLog->PrintF(_T("Previously generated .cpp file found - checking CRC... ")); gLog->NextLine();

		// a small buffer should be fine here...
		TCHAR buf[CRCBUFSZ];

		_fseeki64(out_file_cpp, 0, SEEK_END);
		size_t len = _ftelli64(out_file_cpp) - 8; // 8 characters in the hex string
		_fseeki64(out_file_cpp, len, SEEK_SET);

		size_t bufrd = fread(buf, sizeof(TCHAR), CRCBUFSZ, out_file_cpp);
		if (bufrd)
		{
			buf[bufrd] = _T('\0');

			UINT32 tmpcrc;
			_stscanf_s(buf, _T("%08x"), &tmpcrc);

			cpp_crc_diff = (tmpcrc != crcval);
		}

		fclose(out_file_cpp);
		out_file_cpp = nullptr;
	}

	if (!h_crc_diff && !cpp_crc_diff)
	{
		gLog->PrintF(_T("Files appear to have been generated from identical sources. Stopping.")); gLog->NextLine(1);
		return true;
	}

	_tfopen_s(&out_file_h, out_name_h.c_str(), _T("wt"));
	_tfopen_s(&out_file_cpp, out_name_cpp.c_str(), _T("wt"));

	CGenTextOutput oh(out_file_h ? out_file_h : stdout);
	CGenTextOutput oc(out_file_cpp ? out_file_cpp : stdout);

	gLog->PrintF(_T("Generating Code For: class %s"), gClassName.c_str());
	gLog->NextLine(1);

	// ********************************************
	// write the header
	// ********************************************

	gLog->PrintF(_T("Writing Header: %s"), out_name_h.c_str());
	gLog->IncIndent();
	gLog->NextLine(1);

	oh.PrintF(_T("/* This file was generated by GLlibgen, a utility by Keelan Stuart */")); oh.NextLine(1);
	oh.PrintF(_T("#pragma once")); oh.NextLine(2);
	oh.PrintF(_T("#include \"gl.h\"")); oh.NextLine();
	oh.PrintF(_T("#include \"glext.h\"")); oh.NextLine(1);
	oh.PrintF(_T("class %s"), gClassName.c_str()); oh.NextLine();
	oh.PrintF(_T("{")); oh.NextLine();

	oh.PrintF(_T("private:"));
	oh.IncIndent(); oh.NextLine();
	oh.PrintF(_T("void *GetAnyGLFuncAddress(const TCHAR *name);"));

	oh.DecIndent(); oh.NextLine(1);
	oh.PrintF(_T("public:"));
	oh.IncIndent(); oh.NextLine();

	oh.PrintF(_T("%s();"), gClassName.c_str()); oh.NextLine();
	oh.PrintF(_T("~%s();"), gClassName.c_str()); oh.NextLine();
	oh.PrintF(_T("bool Initialize();"));

	CHttpDownloader docdl;

	gLog->PrintF(_T("- Member Functions (may take a long time to process docs)"));
	gLog->IncIndent();
	gLog->NextLine();

	// write out all the wrapper functions
	for (TMapStrFuncData::const_iterator cit = funcname_to_funcdata.begin(); cit != funcname_to_funcdata.end(); cit++)
	{
		// is this extension's vendor allowed?
		if (!gOSVIncludes[cit->second.osv])
			continue;

		// is this extension's version is higher than what we're allowed?
		if (cit->second.ver.major <= gOGLVersionMax.major)
		{
			// the major version was ok... minor, too?
			if (cit->second.ver.minor > gOGLVersionMax.minor)
				continue;
		}
		else
			continue;

		gLog->Flush();

		gLog->PrintF(_T("%s"), cit->first.c_str());

		oh.NextLine();

		tstring nfn = cit->first.c_str() + 2;	//skip over the "gl" beginning
		tstring desctext = cit->first.c_str();

		UINT dli = 0;
		bool gotdoc = false;
		tstring doclink;
		tstring doclocal;

		while (docdata[dli].baseurl != nullptr)
		{
			doclink = docdata[dli].baseurl;
			doclink += cit->first.c_str();
			doclink += docdata[dli].ext;

			doclocal = _T("gllibgen_doctemp");
			doclocal += docdata[dli].ext;

			gLog->PrintF(_T("."));

			gotdoc = docdl.DownloadHttpFile(doclink.c_str(), doclocal.c_str(), _T("."));
			if (gotdoc)
				break;

			dli++;
		}

		if (gotdoc)
		{
			FILE *in_file_doc = nullptr;

			if ((_tfopen_s(&in_file_doc, doclocal.c_str(), _T("rt")) != EINVAL) && in_file_doc)
			{
				gLog->PrintF(_T("."));

				tinyxml2::XMLDocument doc;
				if (tinyxml2::XML_SUCCESS == doc.LoadFile(in_file_doc))
				{
					const tinyxml2::XMLElement *root = doc.RootElement();

					const tinyxml2::XMLElement *desc = EvaluatePath(root, docdata[dli].descpath);
					if (desc)
					{
#if defined(UNICODE)
						int len = MultiByteToWideChar(CP_UTF8, 0, desc->GetText(), -1, NULL, NULL);
						if (len)
						{
							desctext.resize(len + 1);
							MultiByteToWideChar(CP_UTF8, 0, desc->GetText(), -1, (TCHAR *)(desctext.c_str()), sizeof(TCHAR) * (len + 1));
						}
#else
						desctext = desc->GetText();
#endif
					}
				}
				else
				{
					fclose(in_file_doc);
					in_file_doc = nullptr;

					if ((_tfopen_s(&in_file_doc, doclocal.c_str(), _T("rt, ccs=UTF-8")) != EINVAL) && in_file_doc)
					{
						gLog->PrintF(_T("."));

						_fseeki64(in_file_doc, 0, SEEK_END);
						size_t in_size = (size_t)_ftelli64(in_file_doc);
						_fseeki64(in_file_doc, 0, SEEK_SET);

						TCHAR *buf = (in_size > 0) ? (TCHAR *)malloc(sizeof(TCHAR) * in_size) : nullptr;
						if (buf)
						{
							fread_s(buf, sizeof(TCHAR) * in_size, sizeof(TCHAR), in_size, in_file_doc);

							gLog->PrintF(_T("."));

							TCHAR *s = _tcsstr(buf, _T("refnamediv"));
							if (s)
							{
								gLog->PrintF(_T("."));

								s = _tcsstr(s, cit->first.c_str());
								if (s)
								{
									gLog->PrintF(_T("."));

									TCHAR *e = _tcsstr(s, _T("</p>"));
									if (e)
									{
										gLog->PrintF(_T("."));

										*e = _T('\0');
										desctext = s;
									}
								}
							}

							free(buf);
						}
					}
				}

				if (in_file_doc)
					fclose(in_file_doc);
			}

			DeleteFile(doclocal.c_str());
		}
		else
		{
			doclink.clear();
		}

		size_t invchr = desctext.find_first_of(_T('�'));
		if (invchr < desctext.length())
			desctext[invchr] = _T('-');

		gLog->PrintF(_T("."));

		oh.NextLine(); oh.PrintF(_T("/* [%d.%d] %s%s%s%s */"), cit->second.ver.major, cit->second.ver.minor, desctext.c_str() + 2, doclink.empty() ? _T("") : _T(" ("), doclink.c_str(), doclink.empty() ? _T("") : _T(")"));

		oh.NextLine(); oh.PrintF(_T("%s %s%s;"), cit->second.ret.c_str(), nfn.c_str(), cit->second.params.c_str());

		gLog->NextLine();
	}

	oh.DecIndent(); oh.NextLine(1);
	oh.PrintF(_T("protected:"));
	oh.IncIndent();

	gLog->DecIndent();
	gLog->NextLine();
	gLog->PrintF(_T("- Data Types"), out_name_h.c_str());
	gLog->NextLine(1);

	bool wrote_functype = false;
	for (TMapStrFuncData::const_iterator cit = funcname_to_funcdata.begin(); cit != funcname_to_funcdata.end(); cit++)
	{
		if (!cit->second.needsfunctype)
			continue;

		if (cit->second.osv > EOSV_ARB)
			continue;

		gLog->Flush();

		wrote_functype = true;

		oh.NextLine();

		tstring nfn = cit->first.c_str();	//skip over the "gl" beginning
		std::transform(nfn.begin(), nfn.end(), nfn.begin(), _totupper);
		nfn = _T("PFN") + nfn + _T("PROC");

		oh.PrintF(_T("typedef %s (APIENTRY *%s) %s;"), cit->second.ret.c_str(), nfn.c_str(), cit->second.params.c_str());
	}

	if (wrote_functype)
	{
		oh.DecIndent(); oh.NextLine();
		oh.IncIndent();
	}

	gLog->PrintF(_T("- Member Variables"), out_name_h.c_str());
	gLog->NextLine(1);

	// write out all the function pointer member variables
	for (TMapStrStr::const_iterator cit = functype_to_funcname.begin(); cit != functype_to_funcname.end(); cit++)
	{
		TMapStrFuncData::const_iterator fnit = funcname_to_funcdata.find(cit->second);
		if ((fnit == funcname_to_funcdata.end()) || fnit->second.osv > EOSV_ARB)
			continue;

		if (!cit->second.empty())
		{
			oh.NextLine();
			oh.PrintF(_T("%s _%s;"), cit->first.c_str(), cit->second.c_str());
		}
	}

	oh.DecIndent(); oh.NextLine();
	oh.PrintF(_T("};")); oh.NextLine(1);

	oh.PrintF(_T("// WARNING: DO NOT MODIFY BEYOND THIS LINE")); oh.NextLine();
	oh.PrintF(_T("// SOURCE FILE CRC: %08x"), crcval);

	gLog->DecIndent();
	gLog->NextLine();
	gLog->Flush();

	// ********************************************
	// write the actual code
	// ********************************************

	gLog->PrintF(_T("Writing Code: %s"), out_name_cpp.c_str());
	gLog->NextLine(1);
	gLog->IncIndent();

	oc.PrintF(_T("/* This file was generated by GLlibgen, a utility by Keelan Stuart */")); oc.NextLine(1);

	if (gAfxInclude)
	{
		oc.PrintF(_T("#include \"stdafx.h\"")); oc.NextLine();
	}

	TCHAR *fninc = PathFindFileName(out_name_h.c_str());
	if (fninc)
	{
		oc.PrintF(_T("#include \"%s\""), fninc); oc.NextLine();
	}

	oc.NextLine();

	oc.PrintF(_T("#if defined(_MSC_BUILD)")); oc.NextLine();
	oc.PrintF(_T("// wgl* functions are required on Windows")); oc.NextLine();
	oc.PrintF(_T("// Automatically include the opengl library if compiling with MSVC")); oc.NextLine();
	oc.PrintF(_T("// If building with another compiler, YMMV on pragmas")); oc.NextLine();
	oc.PrintF(_T("#pragma comment(lib, \"opengl32.lib\")")); oc.NextLine();
	oc.PrintF(_T("#endif")); oc.NextLine();
	oc.NextLine(2);

	oc.PrintF(_T("%s::%s()"), gClassName.c_str(), gClassName.c_str()); oc.NextLine();
	oc.PrintF(_T("{")); oc.IncIndent(); oc.NextLine();
	oc.PrintF(_T("memset(this, 0, sizeof(%s));\t//clear our memory here so we know which extensions were acquired"), gClassName.c_str());
	oc.DecIndent(); oc.NextLine();
	oc.PrintF(_T("}")); oc.NextLine(2);

	oc.PrintF(_T("%s::~%s() { }"), gClassName.c_str(), gClassName.c_str()); oc.NextLine(2);

	oc.PrintF(_T("#if defined(_UNICODE) || defined(UNICODE)\n\n// macros to convert to and from wide- and multi-byte- character strings from the other type on the stack\n\n" \
		"#define LOCAL_WCS2MBCS(wcs, mbcs) {\t\t\t\t\t\\\n\tsize_t origsize = _tcslen(wcs) + 1;\t\t\t\t\\\n" \
		"\tsize_t newsize = (origsize * 2) * sizeof(char);\t\\\n\tmbcs = (char *)_alloca(newsize);\t\t\t\t\\\n" \
		"\twcstombs_s(NULL, mbcs, newsize, wcs, newsize); }\n\n#else\n\n#define LOCAL_WCS2MBCS(wcs, mbcs) mbcs = (TCHAR *)wcs\n\n#endif"));
	oc.NextLine(2);

	oc.PrintF(_T("void *%s::GetAnyGLFuncAddress(const TCHAR *name)"), gClassName.c_str()); oc.NextLine();
	oc.PrintF(_T("{")); oc.IncIndent(); oc.NextLine();
	oc.PrintF(_T("char *_name;")); oc.NextLine();
	oc.PrintF(_T("LOCAL_WCS2MBCS(name, _name);")); oc.NextLine(1);
	oc.PrintF(_T("void *p = (void *)wglGetProcAddress(_name);")); oc.NextLine(1);
	oc.PrintF(_T("if ((p == nullptr) || (p == (void *)0x1) || (p == (void *)0x2) || (p == (void *)0x3) || (p == (void *)-1))")); oc.NextLine();
	oc.PrintF(_T("{")); oc.IncIndent(); oc.NextLine();
	oc.PrintF(_T("HMODULE module = LoadLibrary(_T(\"opengl32.dll\"));")); oc.NextLine();
	oc.PrintF(_T("p = (void *)GetProcAddress(module, _name);")); oc.DecIndent(); oc.NextLine();
	oc.PrintF(_T("}")); oc.NextLine(1);
	oc.PrintF(_T("return p;")); oc.DecIndent(); oc.NextLine();
	oc.PrintF(_T("}")); oc.NextLine(2);

	oc.PrintF(_T("bool %s::Initialize()"), gClassName.c_str()); oc.NextLine();
	oc.PrintF(_T("{")); oc.IncIndent();

	// write out all the function pointer member variables
	for (TMapStrStr::const_iterator cit = functype_to_funcname.begin(); cit != functype_to_funcname.end(); cit++)
	{
		TMapStrFuncData::const_iterator fnit = funcname_to_funcdata.find(cit->second);
		if ((fnit == funcname_to_funcdata.end()) || fnit->second.osv > EOSV_ARB)
			continue;

		if (!cit->second.empty())
		{
			oc.NextLine();
			oc.PrintF(_T("_%s = (%s)GetAnyGLFuncAddress(_T(\"%s\"));"), cit->second.c_str(), cit->first.c_str(), cit->second.c_str());
		}
	}

	oc.NextLine(1);
	oc.PrintF(_T("return true;"));
	oc.DecIndent(); oc.NextLine();
	oc.PrintF(_T("}")); oc.NextLine(2);

	for (TMapStrFuncData::const_iterator cit = funcname_to_funcdata.begin(); cit != funcname_to_funcdata.end(); cit++)
	{
		if (cit->second.osv > EOSV_ARB)
			continue;

		bool needsret = false;
		if (_tcscmp(cit->second.ret.c_str(), _T("void")))
			needsret = true;

		tstring nfn = cit->first.c_str() + 2;	//skip over the "gl" beginning
		oc.PrintF(_T("%s %s::%s%s"), cit->second.ret.c_str(), gClassName.c_str(), nfn.c_str(), cit->second.params.c_str()); oc.NextLine();
		oc.PrintF(_T("{")); oc.IncIndent(); oc.NextLine();

		if (needsret)
		{
			oc.PrintF(_T("%s ret = 0;"), cit->second.ret.c_str()); oc.NextLine(1);
		}

		oc.PrintF(_T("if (_%s != nullptr)"), cit->first.c_str()); oc.IncIndent(); oc.NextLine();
		if (needsret)
			oc.PrintF(_T("ret = "));

		oc.PrintF(_T("_%s("), cit->first.c_str());

		// make a call to the function pointer that we're wrapping... so parse out the parameters
		CGenParser pp;
		pp.SetSourceData(cit->second.params.c_str(), cit->second.params.length());

		// skip the '('
		pp.NextToken();

		while (pp.NextToken())
		{
			while (pp.IsToken(_T("const")) || pp.IsToken(_T("struct")))
				pp.NextToken();

			// should be the parameter type

			if (pp.IsToken(_T(")")))
				break;
			pp.NextToken();

			// skip pointer and reference indicators
			if (pp.IsToken(_T("*")) || pp.IsToken(_T("&")))
				pp.NextToken();

			if (pp.IsToken(_T("const")))
				pp.NextToken();

			if (pp.IsToken(_T("*")) || pp.IsToken(_T("&")))
				pp.NextToken();

			if (pp.IsToken(_T(")")))
				break;

			tstring pt = pp.GetCurrentTokenString();

			// should be the parameter name
			oc.PrintF(_T("%s"), pt.c_str());

			pp.NextToken();

			if (pp.IsToken(_T("[")))
			{
				do
				{
					pp.NextToken();
				} while (!pp.IsToken(_T("]")));

				pp.NextToken();
			}

			// should be a comma or end parenthesis
			if (pp.IsToken(_T(")")))
				break;

			oc.PrintF(_T("%s "), pp.GetCurrentTokenString());
		}

		oc.PrintF(_T(");"), cit->first.c_str()); oc.DecIndent();

		if (needsret)
		{
			oc.NextLine(1);
			oc.PrintF(_T("return ret;"));
		}

		oc.DecIndent(); oc.NextLine(); oc.PrintF(_T("}")); oc.NextLine(2);
	}

	oc.PrintF(_T("// WARNING: DO NOT MODIFY BEYOND THIS LINE")); oc.NextLine();
	oc.PrintF(_T("// SOURCE FILE CRC: %08x"), crcval);

	gLog->DecIndent();
	gLog->NextLine(1);

	if (out_file_h)
		fclose(out_file_h);

	if (out_file_cpp)
		fclose(out_file_cpp);

	return true;
}

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
	int nRetCode = 0;

	HMODULE hModule = ::GetModuleHandle(nullptr);

	if (hModule != nullptr)
	{
		// initialize MFC and print and error on failure
		if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
		{
			// TODO: change error code to suit your needs
			_tprintf(_T("Fatal Error: MFC initialization failed\n"));
			nRetCode = 1;
		}
		else
		{
			gLog = new CGenTextOutput(stderr);

			gLog->PrintF(_T("GLlibgen v1.0 - Copyright (c) 2016-2017, Keelan Stuart. All Rights Reserved."));
			gLog->NextLine(1);
			gLog->Flush();

			ParseCommandLineParams(gParams, ::GetCommandLine());

			TMapStrStr functype_to_funcname;
			TMapStrStr define_to_value;
			TMapStrFuncData funcname_to_funcdata;

			/*
			gOGLHeaderLocation = _T("gl.h");
			gOGLEXTHeaderLocation = _T("glext.h");
			gClassName = _T("COpenGL");
			gOutputBaseFileName = _T("gllib");
			gOutputDir = _T(".");
			gOGLVersionMax = FLT_MAX;
			gOSVIncludes[EOSV_UNKNOWN] = { true, true, true, false, false, false, false, false, false, false, false };
			*/

			tstring basedir = gOutputDir;
			if ((basedir.back() != _T('\\')) && (basedir.back() != _T('/')))
				basedir += _T('\\');

			tstring glloc = basedir + _T("gl.h");
			tstring glextloc = basedir + _T("glext.h");
			tstring out_name_h = basedir + gOutputBaseFileName + _T(".h");
			tstring out_name_cpp = basedir + gOutputBaseFileName + _T(".cpp");

			CHttpDownloader dl;

			if (PathIsURL(gOGLHeaderLocation.c_str()))
			{
				gLog->PrintF(_T("Downloading:"));
				gLog->IncIndent();
				gLog->NextLine();
				gLog->PrintF(_T("\"%s\" -> \"%s\""), gOGLHeaderLocation.c_str(), glloc.c_str());

				if (!dl.DownloadHttpFile(gOGLHeaderLocation.c_str(), _T("gl.h"), basedir.c_str()))
				{

				}
			}
			else
			{
				gLog->PrintF(_T("Copying:"));
				gLog->IncIndent();
				gLog->NextLine();
				gLog->PrintF(_T("\"%s\" -> \"%s\""), gOGLHeaderLocation.c_str(), glloc.c_str());

				CopyFile(gOGLHeaderLocation.c_str(), glloc.c_str(), FALSE);
			}

			gLog->DecIndent();

			gLog->NextLine(1);

			if (PathIsURL(gOGLEXTHeaderLocation.c_str()))
			{
				gLog->PrintF(_T("Downloading:"));
				gLog->IncIndent();
				gLog->NextLine();
				gLog->PrintF(_T("\"%s\" -> \"%s\""), gOGLEXTHeaderLocation.c_str(), glextloc.c_str());

				if (!dl.DownloadHttpFile(gOGLEXTHeaderLocation.c_str(), _T("glext.h"), basedir.c_str()))
				{

				}
			}
			else
			{
				gLog->PrintF(_T("Copying:"));
				gLog->IncIndent();
				gLog->NextLine();
				gLog->PrintF(_T("\"%s\" -> \"%s\""), gOGLEXTHeaderLocation.c_str(), glextloc.c_str());

				CopyFile(gOGLEXTHeaderLocation.c_str(), glextloc.c_str(), FALSE);
			}

			gLog->DecIndent();

			gLog->NextLine(1);

			gLog->Flush();

			CCrc32 crc;
			UINT32 crcval = CCrc32::INITVAL;

			// roll the settings that will change the contents of our generated code into the crc we generate
			crcval = crc.Calculate((const UINT8 *)gClassName.c_str(), sizeof(TCHAR) * gClassName.length(), crcval);
			crcval = crc.Calculate((const UINT8 *)&gOGLVersionMax, sizeof(SVersion), crcval);
			crcval = crc.Calculate((const UINT8 *)&gAfxInclude, sizeof(bool), crcval);
			crcval = crc.Calculate((const UINT8 *)gOSVIncludes, sizeof(bool) * EOSV_UNKNOWN, crcval);

			// parse out the opengl baseline header
			if (ParseFunctionsFromFile(glloc.c_str(), functype_to_funcname, funcname_to_funcdata, define_to_value, crc, crcval))
			{
				// parse out the opengl extensions header
				if (ParseFunctionsFromFile(glextloc.c_str(), functype_to_funcname, funcname_to_funcdata, define_to_value, crc, crcval))
				{
					WriteCPPWrapper(out_name_h, out_name_cpp, functype_to_funcname, funcname_to_funcdata, define_to_value, crcval);
				}
				else
				{
					gLog->PrintF(_T("Failed to open glext.h!"));
					gLog->NextLine(1);
				}
			}
			else
			{
				gLog->PrintF(_T("Failed to open gl.h!"));
				gLog->NextLine(1);
			}

			gLog->PrintF(_T("Finished!"));
			gLog->NextLine(1);
			gLog->Flush();

			if (gLog)
			{
				delete gLog;
				gLog = nullptr;
			}
		}
	}
	else
	{
		// TODO: change error code to suit your needs
		_tprintf(_T("Fatal Error: GetModuleHandle failed\n"));
		nRetCode = 1;
	}

	return nRetCode;
}