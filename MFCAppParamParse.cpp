#include "stdafx.h"
#include "MFCAppParamParse.h"


void MFCAppParamParse::ParseParam( 
		const TCHAR* pszParam,  
		BOOL bFlag, 
		BOOL bLast
		)
{
	if(bFlag)
	{
		if (wcscmp(pszParam, _T("show")) == 0)
		{
			param_pair.insert(std::make_pair(L"show",L""));
			m_current_command = L"show";
		}
		else{
			m_current_command = "";
		}
	}else{
		if (!m_current_command.IsEmpty())
		{
			param_pair[m_current_command] = pszParam;
		}
	}
}