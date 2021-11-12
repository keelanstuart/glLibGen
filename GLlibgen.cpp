/*
Copyright ©2016-2021, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved.
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
#include <Pool/Include/Pool.h>
#include <chrono>
#include <ctime>

typedef struct sDocLinkData
{
	const TCHAR *baseurl;
	const TCHAR *ext;
	const TCHAR *descpath;
} SDocLinkData;

const SDocLinkData docdata[] =
{
	{ _T("https://www.opengl.org/sdk/docs/man/html/"), _T(".xhtml"), _T("body.div.div:2.p") },
	{ _T("https://www.opengl.org/sdk/docs/man2/xhtml/"), _T(".xml"), _T("body.div.div:2.p") },
	{ _T("https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/"), _T(".xml"), _T("body.div.div:2.p") },
	{ _T("https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/"), _T(".xhtml"), _T("body.div.div:2.p") },

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
	EOSV_HP,
	EOSV_IBM,
	EOSV_GREMEDY,

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
	_T("GL_HP"),
	_T("GL_IBM"),
	_T("GL_GREMEDY"),
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
	tstring desc;
	tstring doclink;
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
tstring gOGLKHRPlatformHeaderLocation = _T("khrplatform.h");
tstring gWGLEXTHeaderLocation = _T("wglext.h");
tstring gClassName = _T("COpenGL");
tstring gOutputBaseFileName = _T("gllib");
tstring gOutputDir = _T(".");
SVersion gOGLVersionMax(-1, 0);
bool gAfxInclude = false;
bool gPchInclude = false;
bool gOSVIncludes[EOSV_UNKNOWN] = { true, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false };
bool gProvideLogCallback = false;
bool gPullDocs = false;

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
	{ _T("glh"),		SCmdLineParamData::CLPT_STRING,		&gOGLHeaderLocation,	        	_T("local or remote (HTTP) file path to \"gl.h\"") },
	{ _T("glexth"),		SCmdLineParamData::CLPT_STRING,		&gOGLEXTHeaderLocation,		        _T("local or remote (HTTP) file path to \"glext.h\"") },
    { _T("khrh"),		SCmdLineParamData::CLPT_STRING,		&gOGLKHRPlatformHeaderLocation,		_T("local or remote (HTTP) file path to \"khrplatform.h\"") },
	{ _T("wglexth"),	SCmdLineParamData::CLPT_STRING,		&gWGLEXTHeaderLocation,		        _T("local or remote (HTTP) file path to \"wglext.h\"") },
	{ _T("class"),		SCmdLineParamData::CLPT_STRING,		&gClassName,			        	_T("name of the class that will be generated") },
	{ _T("basefile"),	SCmdLineParamData::CLPT_STRING,		&gOutputBaseFileName,	        	_T("base filename that the C++ code will go into (file.cpp and file.h)") },
	{ _T("outdir"),		SCmdLineParamData::CLPT_STRING,		&gOutputDir,			        	_T("directory where the code will be generated and gl headers copied") },
	{ _T("ver"),		SCmdLineParamData::CLPT_VERSION,	&gOGLVersionMax,		        	_T("determines the maximum version of OpemGL to support") },
	{ _T("docs"),		SCmdLineParamData::CLPT_CMD,		&gPullDocs,				            _T("downloads and includes the documentation") },

	{ _T("arb"),		SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_ARB],	            _T("includes ARB extensions") },
	{ _T("ext"),		SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_EXT],	            _T("includes EXT extensions") },
	{ _T("nv"),			SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_NV],		            _T("includes nVidia extensions") },
	{ _T("amd"),		SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_AMD],	            _T("includes AMD extensions") },
	{ _T("ati"),		SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_ATI],	            _T("includes ATI extensions") },
	{ _T("intel"),		SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_INTEL],	            _T("includes Intel extensions") },
	{ _T("sgi"),		SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_SGI],	            _T("includes Silicon Graphics extensions") },
	{ _T("sun"),		SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_SUN],	            _T("includes Sun Microsystems extensions") },
	{ _T("apple"),		SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_APPLE],	            _T("includes Apple extensions") },
	{ _T("oes"),		SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_OES],	            _T("includes OES extensions") },
	{ _T("ingr"),		SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_INGR],	            _T("includes Intergraph extensions") },
	{ _T("khr"),		SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_KHR],	            _T("includes Khronos extensions") },
	{ _T("hp"),			SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_HP],		            _T("includes Hewlett-Packard extensions") },
	{ _T("ibm"),		SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_IBM],	            _T("includes IBM extensions") },
	{ _T("gremedy"),	SCmdLineParamData::CLPT_CMD,	&gOSVIncludes[EOSV_GREMEDY],	        _T("includes GRemedy extensions") },

	{ _T("afx"),		SCmdLineParamData::CLPT_CMD,	&gAfxInclude,				            _T("adds \"#include <stdafx.h>\"") },
	{ _T("pch"),		SCmdLineParamData::CLPT_CMD,	&gPchInclude,				            _T("adds \"#include <pch.h>\"") },

	{ _T("logcb"),		SCmdLineParamData::CLPT_CMD,	&gProvideLogCallback,		            _T("adds a mechanism to report the OpenGL calls made") },

	{ nullptr,			SCmdLineParamData::CLPT_NONE,	nullptr,					            nullptr }
};

bool ParseCommandLineParams(const SCmdLineParamData *paramdata, const TCHAR *cmdline)
{
	UINT ordinal = 0;
	bool ret = true;

	CGenParser cp;
	cp.SetSourceData(cmdline, (UINT)_tcslen(cmdline));

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
                        else if (p.IsToken(_T("endif")))
                        {
                            p.ToEndOfLine();
                            tstring chk = p.GetCurrentTokenString();
                            const TCHAR *vstr = nullptr;

                            // we found a version end-block... parse out the numbers
                            if (nullptr != (vstr = _tcsstr(chk.c_str(), _T("GL_VERSION_"))))
                            {
                                UINT major = 1;
                                UINT minor = 0;

                                std::replace(chk.begin(), chk.end(), _T('_'), _T(' '));

                                CGenParser verp;
                                verp.SetSourceData(vstr, _tcslen(vstr) * sizeof(TCHAR));

                                while ((verp.GetCurrentTokenType() != CGenParser::TT_NUMBER) && verp.NextToken()) {}
                                if (verp.GetCurrentTokenType() == CGenParser::TT_NUMBER)
                                {
                                    major = _ttoi(verp.GetCurrentTokenString());
                                    if (verp.NextToken())
                                    {
                                        minor = _ttoi(verp.GetCurrentTokenString());
                                    }
                                }

                                if ((curver.major == major) && (curver.minor == minor))
                                {
                                    curver.major = 1;
                                    curver.minor = 0;
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

bool DownloadAndExtractDecriptions(TMapStrFuncData& funcname_to_funcdata)
{
	gLog->PrintF(_T("Beginning auto-documentation process (%d functions)"), funcname_to_funcdata.size());

	pool::IThreadPool *ptp = pool::IThreadPool::Create(1, 0);

	for (TMapStrFuncData::iterator it = funcname_to_funcdata.begin(); it != funcname_to_funcdata.end(); it++)
	{
		// is this extension's vendor allowed?
		if (!gOSVIncludes[it->second.osv])
			continue;

		// is this extension's version is higher than what we're allowed?
		if (it->second.ver.major <= gOGLVersionMax.major)
		{
			// the major version was ok... minor, too?
			if (it->second.ver.minor > gOGLVersionMax.minor)
				continue;
		}
		else
			continue;

		SFunctionData *funcdata = &(it->second);
		const TCHAR *funcname = it->first.c_str();

		ptp->RunTask([](void *param0, void *param1, size_t task_number) -> pool::IThreadPool::TASK_RETURN
		{
			const TCHAR *funcname = (const TCHAR *)param0;
			SFunctionData *funcdata = (SFunctionData *)param1;

			tstring desctext = funcname;

			UINT dli = 0;
			bool gotdoc = false;
			tstring doclink;
			tstring doclocal;

			while (docdata[dli].baseurl != nullptr)
			{
				doclink = docdata[dli].baseurl;
				doclink += funcname;
				doclink += docdata[dli].ext;

				doclocal = _T("gllibgen_doctemp");
				doclocal += funcname;
				doclocal += docdata[dli].ext;

				gLog->PrintF(_T("."));

				CHttpDownloader taskdl;
				gotdoc = taskdl.DownloadHttpFile(doclink.c_str(), doclocal.c_str(), _T("."));
				if (gotdoc)
				{
					char buf[2049];
					FILE *tmpf = nullptr;

					// khronos returns a page that says 404, but doesn't indicate that in the html response... boooo...
					// so look for their string in the returned data.
					if ((_tfopen_s(&tmpf, doclocal.c_str(), _T("rb")) != EINVAL) && tmpf)
					{
						size_t tmpbuflen = fread(buf, sizeof(char), 2048, tmpf);
						buf[tmpbuflen] = 0;
						if (strstr(buf, "404… Oops"))
							gotdoc = false;

						fclose(tmpf);
					}

					if (gotdoc)
						break;
				}

				dli++;
			}

			if (gotdoc)
			{
				FILE *in_file_doc = nullptr;

				if ((_tfopen_s(&in_file_doc, doclocal.c_str(), _T("rt")) != EINVAL) && in_file_doc)
				{
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
								gLog->PrintF(_T("o"));

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
							_fseeki64(in_file_doc, 0, SEEK_END);
							size_t in_size = (size_t)_ftelli64(in_file_doc);
							_fseeki64(in_file_doc, 0, SEEK_SET);

							TCHAR *buf = (in_size > 0) ? (TCHAR *)malloc(sizeof(TCHAR) * in_size) : nullptr;
							if (buf)
							{
								fread_s(buf, sizeof(TCHAR) * in_size, sizeof(TCHAR), in_size, in_file_doc);

								TCHAR *s = _tcsstr(buf, _T("refnamediv"));
								if (s)
								{
									s = _tcsstr(s, funcname);
									if (s)
									{
										//gLog->PrintF(_T("."));

										TCHAR *e = _tcsstr(s, _T("</p>"));
										if (e)
										{
											*e = _T('\0');
											gLog->PrintF(_T("o"));
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

			size_t invchr = desctext.find_first_of(_T('—'));
			if (invchr < desctext.length())
				desctext[invchr] = _T('-');

			//gLog->PrintF(_T("."));
			funcdata->desc = desctext;
			funcdata->doclink = doclink;

			return pool::IThreadPool::TASK_RETURN::TR_OK;

		}, (void *)funcname, (void *)funcdata);
	}

	ptp->Flush();
	ptp->Release();
	gLog->NextLine();
	gLog->PrintF(_T("Done."));
	gLog->NextLine(1);

	return true;
}

void AppendLogComponentsFromType(const TCHAR * paramtype, bool ptr, const TCHAR *paramname, tstring & str, tstring & arg)
{
	if (!_tcsicmp(paramtype, _T("GLchar")) && ptr)
	{
		str += _T("%S");
		arg += paramname;
	}
	else if (!_tcsicmp(paramtype, _T("GLenum")) || ptr)
	{
		str += _T("0x%X");
		arg += _T("(unsigned int)");
		arg += paramname;
	}
	else if (!_tcsicmp(paramtype, _T("GLboolean")))
	{
		str += _T("%s");
		arg += paramname;
		arg += _T(" ? _T(\"true\") : _T(\"false\")");
	}
	else if ((!_tcsicmp(paramtype, _T("GLfloat"))) ||
		(!_tcsicmp(paramtype, _T("GLclampf"))) ||
		(!_tcsicmp(paramtype, _T("GLfloat"))) ||
		(!_tcsicmp(paramtype, _T("GLclampd"))))
	{
		str += _T("%f");
		arg += paramname;
	}
	else
	{
		if (_tcsstr(paramtype, _T("64")))
			str += _T("%ll");
		str += _T("%d");
		arg += paramname;
	}
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
		long long len = _ftelli64(out_file_h) - 8; // 8 characters in the hex string
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
		long long len = _ftelli64(out_file_cpp) - 8; // 8 characters in the hex string
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

	oh.PrintF(_T("#if defined(_MSC_BUILD)")); oh.NextLine();
	oh.PrintF(_T("#include \"wglext.h\"")); oh.NextLine();
	oh.PrintF(_T("#endif")); oh.NextLine(1);

	oh.PrintF(_T("class %s"), gClassName.c_str()); oh.NextLine();
	oh.PrintF(_T("{")); oh.NextLine();

	if (gProvideLogCallback)
	{
		oh.PrintF(_T("public:"));
		oh.IncIndent(); oh.NextLine();

		oh.PrintF(_T("typedef void(__cdecl* GLLIBGEN_LOGFUNC)(const wchar_t *msg, void *userdata);")); oh.NextLine();
		oh.DecIndent(); oh.NextLine(1);
	}

	oh.PrintF(_T("private:"));
	oh.IncIndent(); oh.NextLine();
	if (gProvideLogCallback)
	{
		oh.PrintF(_T("GLLIBGEN_LOGFUNC m_pLogFunc;")); oh.NextLine();
		oh.PrintF(_T("void *m_LogFuncUserData;")); oh.NextLine(1);
	}

	oh.PrintF(_T("void *GetAnyGLFuncAddress(const TCHAR *name);"));

	oh.DecIndent(); oh.NextLine(1);
	oh.PrintF(_T("public:"));
	oh.IncIndent(); oh.NextLine();

	oh.PrintF(_T("%s();"), gClassName.c_str()); oh.NextLine();
	oh.PrintF(_T("~%s();"), gClassName.c_str()); oh.NextLine();
	oh.PrintF(_T("bool Initialize();")); oh.NextLine(1);

	if (gProvideLogCallback)
	{
		oh.PrintF(_T("void SetLogFunc(GLLIBGEN_LOGFUNC logfunc = nullptr, void *logfunc_userdata = nullptr);"));
	}

	CHttpDownloader docdl;

	gLog->PrintF(_T("- Member Functions"));
	gLog->IncIndent();
	gLog->NextLine();

	// write out all the wrapper functions
	for (const auto &cit : funcname_to_funcdata)
	{
        // is this extension's vendor allowed?
		if (!gOSVIncludes[cit.second.osv])
			continue;

		// is this extension's version is higher than what we're allowed?
		if (cit.second.ver.major <= gOGLVersionMax.major)
		{
			// the major version was ok... minor, too?
			if (cit.second.ver.minor > gOGLVersionMax.minor)
				continue;
		}
		else
			continue;

		gLog->Flush();

		gLog->PrintF(_T("%s"), cit.first.c_str());

		oh.NextLine();

		tstring nfn = cit.first.c_str() + 2;	//skip over the "gl" beginning

		oh.NextLine(); oh.PrintF(_T("/* [OpenGL %d.%d] %s%s%s%s*/"), cit.second.ver.major, cit.second.ver.minor, cit.second.desc.c_str() + 2,
			cit.second.doclink.empty() ? _T(" ") : _T(" ("), cit.second.doclink.c_str(), cit.second.doclink.empty() ? _T("") : _T(") "));

		oh.NextLine(); oh.PrintF(_T("%s %s%s;"), cit.second.ret.c_str(), nfn.c_str(), cit.second.params.c_str());

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
	for (const auto &cit : funcname_to_funcdata)
	{
        if (!cit.second.needsfunctype)
			continue;

        // is this extension's vendor allowed?
        if (!gOSVIncludes[cit.second.osv])
            continue;

        // is this extension's version is higher than what we're allowed?
        if (cit.second.ver.major <= gOGLVersionMax.major)
        {
            // the major version was ok... minor, too?
            if (cit.second.ver.minor > gOGLVersionMax.minor)
                continue;
        }
        else
            continue;

		gLog->Flush();

		wrote_functype = true;

		oh.NextLine();

		tstring nfn = cit.first.c_str();	//skip over the "gl" beginning
		std::transform(nfn.begin(), nfn.end(), nfn.begin(), _totupper);
		nfn = _T("PFN") + nfn + _T("PROC");

		oh.PrintF(_T("typedef %s (APIENTRY *%s) %s;"), cit.second.ret.c_str(), nfn.c_str(), cit.second.params.c_str());
	}

	if (wrote_functype)
	{
		oh.DecIndent(); oh.NextLine();
		oh.IncIndent();
	}

	gLog->PrintF(_T("- Member Variables"), out_name_h.c_str());
	gLog->NextLine(1);

	// write out all the function pointer member variables
	for (const auto &cit : functype_to_funcname)
	{
        const auto &fnit = funcname_to_funcdata.find(cit.second);
		if ((fnit == funcname_to_funcdata.end()))
			continue;

        // is this extension's vendor allowed?
        if (!gOSVIncludes[fnit->second.osv])
            continue;

        // is this extension's version is higher than what we're allowed?
        if (fnit->second.ver.major <= gOGLVersionMax.major)
        {
            // the major version was ok... minor, too?
            if (fnit->second.ver.minor > gOGLVersionMax.minor)
                continue;
        }
        else
            continue;

        if (!cit.second.empty())
		{
			oh.NextLine();
			oh.PrintF(_T("%s _%s;"), cit.first.c_str(), cit.second.c_str());
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

	oc.PrintF(_T("/* This file was generated by GLlibgen, a utility by Keelan Stuart */")); oc.NextLine(2);
    oc.PrintF(_T("/*")); oc.IncIndent(); oc.NextLine(1);
    oc.PrintF(_T("The command line options used to generate this code: \"%s\""), ::GetCommandLine());
    oc.DecIndent(); oc.NextLine(1); oc.PrintF(_T("*/"));
    oc.NextLine(2);

	if (gAfxInclude)
	{
		oc.PrintF(_T("#include \"stdafx.h\"")); oc.NextLine();
	}

	if (gPchInclude)
	{
		oc.PrintF(_T("#include \"pch.h\"")); oc.NextLine();
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

	if (gProvideLogCallback)
	{
		oc.PrintF(_T("void %s::SetLogFunc(GLLIBGEN_LOGFUNC logfunc, void *logfunc_userdata)"), gClassName.c_str()); oc.NextLine();
		oc.PrintF(_T("{")); oc.IncIndent(); oc.NextLine();
		oc.PrintF(_T("m_pLogFunc = logfunc;")); oc.NextLine();
		oc.PrintF(_T("m_LogFuncUserData = logfunc_userdata;")); oc.DecIndent(); oc.NextLine();
		oc.PrintF(_T("}")); oc.NextLine(2);
	}

	oc.PrintF(_T("bool %s::Initialize()"), gClassName.c_str()); oc.NextLine();
	oc.PrintF(_T("{")); oc.IncIndent();

	// write out all the function pointer member variables
	for (const auto &cit : functype_to_funcname)
	{
        const auto &fnit = funcname_to_funcdata.find(cit.second);
		if ((fnit == funcname_to_funcdata.end()))
			continue;

        // is this extension's vendor allowed?
        if (!gOSVIncludes[fnit->second.osv])
            continue;

        // is this extension's version is higher than what we're allowed?
        if (fnit->second.ver.major <= gOGLVersionMax.major)
        {
            // the major version was ok... minor, too?
            if (fnit->second.ver.minor > gOGLVersionMax.minor)
                continue;
        }
        else
            continue;

		if (!cit.second.empty())
		{
			oc.NextLine();
			oc.PrintF(_T("_%s = (%s)GetAnyGLFuncAddress(_T(\"%s\"));"), cit.second.c_str(), cit.first.c_str(), cit.second.c_str());
		}
	}

	oc.NextLine(1);
	oc.PrintF(_T("return true;"));
	oc.DecIndent(); oc.NextLine();
	oc.PrintF(_T("}")); oc.NextLine(2);

	tstring cbcode_str, cbcode_args;
	cbcode_str.reserve(4096);
	cbcode_args.reserve(4096);

	for (const auto &cit : funcname_to_funcdata)
	{
        // is this extension's vendor allowed?
        if (!gOSVIncludes[cit.second.osv])
            continue;

        // is this extension's version is higher than what we're allowed?
        if (cit.second.ver.major <= gOGLVersionMax.major)
        {
            // the major version was ok... minor, too?
            if (cit.second.ver.minor > gOGLVersionMax.minor)
                continue;
        }
        else
            continue;

        bool needsret = false;
		if (_tcscmp(cit.second.ret.c_str(), _T("void")))
			needsret = true;

		tstring nfn = cit.first.c_str() + 2;	//skip over the "gl" beginning
		oc.PrintF(_T("%s %s::%s%s"), cit.second.ret.c_str(), gClassName.c_str(), nfn.c_str(), cit.second.params.c_str()); oc.NextLine();
		oc.PrintF(_T("{")); oc.IncIndent(); oc.NextLine();

		if (needsret)
		{
			oc.PrintF(_T("%s ret = { 0 };"), cit.second.ret.c_str()); oc.NextLine(1);
		}

		oc.PrintF(_T("if (_%s != nullptr)"), cit.first.c_str()); oc.IncIndent(); oc.NextLine();
		if (needsret)
			oc.PrintF(_T("ret = "));

		oc.PrintF(_T("_%s("), cit.first.c_str());

		// make a call to the function pointer that we're wrapping... so parse out the parameters
		CGenParser pp;
		pp.SetSourceData(cit.second.params.c_str(), cit.second.params.length());

		// skip the '('
		pp.NextToken();

		cbcode_str.clear();
		cbcode_args.clear();

		cbcode_str += nfn.c_str();
		cbcode_str += _T("(");
		size_t argnum = 0;

		while (pp.NextToken())
		{
			bool ptr = false;
			bool ref = false;

			while (pp.IsToken(_T("const")) || pp.IsToken(_T("struct")))
				pp.NextToken();

			// should be the parameter type
			tstring pt = pp.GetCurrentTokenString();

			if (pp.IsToken(_T(")")))
				break;
			pp.NextToken();

			// skip pointer and reference indicators
			if (pp.IsToken(_T("*")))
			{
				pp.NextToken();
				ptr = true;
			}

			if (pp.IsToken(_T("&")))
			{
				pp.NextToken();
				ref = true;
			}

			if (pp.IsToken(_T("const")))
				pp.NextToken();

			if (pp.IsToken(_T("*")))
			{
				pp.NextToken();
				ptr = true;
			}

			if (pp.IsToken(_T("&")))
			{
				pp.NextToken();
				ref = true;
			}

			if (pp.IsToken(_T(")")))
				break;

			// should be the parameter name
			tstring pn = pp.GetCurrentTokenString();

			if (argnum > 0)
			{
				cbcode_str += _T(", ");
				cbcode_args += _T(", ");
			}

			AppendLogComponentsFromType(pt.c_str(), ptr, pn.c_str(), cbcode_str, cbcode_args);

			// should be the parameter name
			oc.PrintF(_T("%s"), pn.c_str());

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

			argnum++;
		}

		oc.PrintF(_T(");"), cit.first.c_str()); oc.DecIndent();
		cbcode_str += _T(")");
		if (needsret)
		{
			cbcode_str += _T(" => ");
			if (!cbcode_args.empty())
				cbcode_args += _T(", ");
			AppendLogComponentsFromType(cit.second.ret.c_str(), (cit.second.ret.find('*') == tstring::npos) ? false : true, _T("ret"), cbcode_str, cbcode_args);
		}


		if (gProvideLogCallback)
		{
			oc.DecIndent();
			oc.NextLine(1);
			oc.PrintF(_T("#if defined(GLLIBGEN_LOGCALLS)")); oc.IncIndent(); oc.NextLine();
			oc.PrintF(_T("if (m_pLogFunc)")); oc.NextLine();
			oc.PrintF(_T("{")); oc.IncIndent(); oc.NextLine();
			oc.PrintF(_T("wchar_t msg[2048];")); oc.NextLine();
			oc.PrintF(_T("_snwprintf_s(msg, 2048, L\"%s\"%s%s);"), cbcode_str.c_str(), cbcode_args.empty() ? _T("") : _T(", "), cbcode_args.c_str()); oc.NextLine();
			oc.PrintF(_T("m_pLogFunc(msg, m_LogFuncUserData);")); oc.DecIndent(); oc.NextLine();
			oc.PrintF(_T("}")); oc.DecIndent(); oc.NextLine();
			oc.PrintF(_T("#endif"));
			oc.IncIndent();
		}

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

void CopyOrDownload(CHttpDownloader &dl, const TCHAR *src, const TCHAR *dstname, const TCHAR *dstfullname, const TCHAR *dstdir)
{
    if (PathIsURL(src))
    {
        gLog->PrintF(_T("Downloading:"));
        gLog->IncIndent();
        gLog->NextLine();
        gLog->PrintF(_T("\"%s\" -> \"%s\""), src, dstfullname);

        if (!dl.DownloadHttpFile(src, dstname, dstdir))
        {

        }
    }
    else
    {
        gLog->PrintF(_T("Copying:"));
        gLog->IncIndent();
        gLog->NextLine();
        gLog->PrintF(_T("\"%s\" -> \"%s\""), src, dstfullname);

        CopyFile(src, dstfullname, FALSE);
    }

    gLog->DecIndent();

    gLog->NextLine(1);
}

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
	std::time_t finish_op, start_op = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	struct tm finish_tm, start_tm;
	localtime_s(&start_tm, &start_op);

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

			gLog->PrintF(_T("GLlibgen v1.1 - Copyright (c) 2016-2021, Keelan Stuart. All Rights Reserved."));
			gLog->NextLine(1);
			gLog->Flush();

			ParseCommandLineParams(gParams, ::GetCommandLine());

			TMapStrStr functype_to_funcname;
			TMapStrStr define_to_value;
			TMapStrFuncData funcname_to_funcdata;

			tstring basedir = gOutputDir;
			if ((basedir.back() != _T('\\')) && (basedir.back() != _T('/')))
				basedir += _T('\\');

            tstring khrplatformbasedir = basedir + _T("KHR\\");

			tstring glloc = basedir + _T("gl.h");
			tstring glextloc = basedir + _T("glext.h");
            tstring khrplatformloc = khrplatformbasedir + _T("khrplatform.h");
			tstring wglextloc = basedir + _T("wglext.h");
			tstring out_name_h = basedir + gOutputBaseFileName + _T(".h");
			tstring out_name_cpp = basedir + gOutputBaseFileName + _T(".cpp");

			CHttpDownloader dl;

            CopyOrDownload(dl, gOGLHeaderLocation.c_str(), _T("gl.h"), glloc.c_str(), basedir.c_str());

            CopyOrDownload(dl, gOGLEXTHeaderLocation.c_str(), _T("glext.h"), glextloc.c_str(), basedir.c_str());

            CopyOrDownload(dl, gOGLKHRPlatformHeaderLocation.c_str(), _T("khrplatform.h"), khrplatformloc.c_str(), khrplatformbasedir.c_str());

			CopyOrDownload(dl, gWGLEXTHeaderLocation.c_str(), _T("wglext.h"), wglextloc.c_str(), basedir.c_str());

			gLog->Flush();

			CCrc32 crc;
			UINT32 crcval = CCrc32::INITVAL;

			// roll the settings that will change the contents of our generated code into the crc we generate
			crcval = crc.Calculate((const UINT8 *)gClassName.c_str(), sizeof(TCHAR) * gClassName.length(), crcval);
			crcval = crc.Calculate((const UINT8 *)&gOGLVersionMax, sizeof(SVersion), crcval);
			crcval = crc.Calculate((const UINT8 *)&gAfxInclude, sizeof(bool), crcval);
			crcval = crc.Calculate((const UINT8 *)&gPchInclude, sizeof(bool), crcval);
			crcval = crc.Calculate((const UINT8 *)&gPullDocs, sizeof(bool), crcval);
			crcval = crc.Calculate((const UINT8 *)&gProvideLogCallback, sizeof(bool), crcval);
			crcval = crc.Calculate((const UINT8 *)gOSVIncludes, sizeof(bool) * EOSV_UNKNOWN, crcval);

			// parse out the opengl baseline header
			if (ParseFunctionsFromFile(glloc.c_str(), functype_to_funcname, funcname_to_funcdata, define_to_value, crc, crcval))
			{
				// parse out the opengl extensions header
				if (ParseFunctionsFromFile(glextloc.c_str(), functype_to_funcname, funcname_to_funcdata, define_to_value, crc, crcval))
				{
					// parse out the wgl extensions header
					if (ParseFunctionsFromFile(wglextloc.c_str(), functype_to_funcname, funcname_to_funcdata, define_to_value, crc, crcval))
					{
						if (gPullDocs)
							DownloadAndExtractDecriptions(funcname_to_funcdata);

						WriteCPPWrapper(out_name_h, out_name_cpp, functype_to_funcname, funcname_to_funcdata, define_to_value, crcval);
					}
					else
					{
						gLog->PrintF(_T("Failed to open wglext.h!"));
						gLog->NextLine(1);
					}
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

			time(&finish_op);
			int elapsed = (int)difftime(finish_op, start_op);

			int hours = elapsed / 3600;
			elapsed %= 3600;

			int minutes = elapsed / 60;
			int seconds = elapsed % 60;

			localtime_s(&finish_tm, &finish_op);

			TCHAR timebuf[160];
			_tcsftime(timebuf, 160, _T("%R:%S   %A, %e %B %Y"), &finish_tm);

			gLog->PrintF(_T("Elapsed Time: %02d:%02d:%02d"), hours, minutes, seconds);
			gLog->NextLine(1);

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
