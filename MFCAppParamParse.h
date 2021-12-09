#pragma once


#include <map>
#include <deque>

class MFCAppParamParse:public CCommandLineInfo 
{
public:
	MFCAppParamParse()
	{
	
	}
	virtual void ParseParam( 
		const TCHAR* pszParam,  
		BOOL bFlag, 
		BOOL bLast
		);

public:
	std::map<CString, CString> param_pair;
	CString                    m_current_command;
};


