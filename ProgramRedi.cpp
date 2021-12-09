#include "stdafx.h"
//#include <afx.h>
#include "ProgramRedi.h"
#include <string>
#include <assert.h>
#include <shlwapi.h>
#include <shellapi.h>

using namespace std;

std::wstring  wstr_tolower(const std::wstring &str)
{
	std::wstring str2;
	for (int i = 0; i < str.size(); i++)
	{
		str2.append(1, towlower(str[i]));
	}
	return str2;
}

//NoReadWaitMinisec 表示当多少毫秒没读到数据，算读完成。
CProgramRedi::CProgramRedi(int NoReadWaitMinisec) :
	m_hStdinWrite(NULL),
	m_hStdoutRead(NULL),
	m_hChildProcess(NULL),
	m_hOutputThread(NULL),
	m_hEvtStop(NULL),
	m_dwOutputThreadId(0),
	m_dwWaitTime(100),
	m_NoReadWaitMinisec(NoReadWaitMinisec)
{
	m_WriteOKEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_ReadOKEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
}

CProgramRedi::~CProgramRedi()
{
	Close();
}

//////////////////////////////////////////////////////////////////////
// CProgramRedi implementation
//////////////////////////////////////////////////////////////////////

BOOL CProgramRedi::Open(LPCTSTR pszProgram, LPCTSTR pszCmdLine, LPCTSTR DefWordDir, CStringArray *ffmpeg_error_list)
{
	this->ffmpeg_error_list = ffmpeg_error_list;
	//full param 
	std::wstring full_param = pszCmdLine;
	//获得文件完整路径
	std::wstring found_full_path = pszProgram;
	if (!PathFileExists(found_full_path.c_str()) || found_full_path[1]!=L':')
	{
		std::wstring fullpath2 = found_full_path;
		if (found_full_path.size()<=4 || wstr_tolower(found_full_path.substr(found_full_path.size() - 4)) != L".exe")
		{
			fullpath2 = found_full_path + L".exe";
		}
		if (PathFileExists(fullpath2.c_str()) && found_full_path[1] == L':')
		{
			found_full_path = fullpath2;
		}
		else{
			TCHAR *PATH = new TCHAR[64000];
			GetEnvironmentVariable(
				L"PATH",//环境变量名
				PATH,//指向保存环境变量值的缓冲区
				64000//缓冲区的大小
				);
			std::wstring programstr = pszProgram;
			programstr=wstr_tolower(programstr);
			if (programstr.size()<=4 || wstr_tolower(programstr.substr(programstr.size() - 4)) != L".exe")
			{
				programstr += L".exe";
			}
			if (programstr.substr(0,1) == L"\\")
			{
				programstr = programstr.substr(1);
			}
			std::wstring ddd;
			ddd = PATH;
			delete[]PATH;

			TCHAR dirpath[1024] = { 0 };
			GetModuleFileName(nullptr, dirpath, 1024);
			wcsrchr(dirpath, L'\\')[0] = 0;
			ddd += std::wstring() + L";" + dirpath;

			while (ddd.size() != 0)
			{
				std::wstring dir;
				if (ddd.find(L";") != -1)
				{
					dir = ddd.substr(0, ddd.find(L";"));
					ddd = ddd.substr(ddd.find(L";") + 1);
				}
				else{
					dir = ddd;
					ddd = L"";
				}
				if (!dir.empty())
				{
					if (dir.substr(1) != L"\\")
						dir += L"\\";

					std::wstring testpath;
					testpath = dir + programstr;
					if (PathFileExists(testpath.c_str()))
					{
						found_full_path = testpath;
						break;
					}
				}
			}
		}
	}

	full_param = L"\"" + found_full_path + L"\" " + full_param;

	HANDLE hStdoutReadTmp;				// parent stdout read handle
	HANDLE hStdoutWrite, hStderrWrite;	// child stdout write handle
	HANDLE hStdinWriteTmp;				// parent stdin write handle
	HANDLE hStdinRead;					// child stdin read handle
	SECURITY_ATTRIBUTES sa;

	Close();
	hStdoutReadTmp = NULL;
	hStdoutWrite = hStderrWrite = NULL;
	hStdinWriteTmp = NULL;
	hStdinRead = NULL;

	// Set up the security attributes struct.
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;

	BOOL bOK = FALSE;
	//__try
	{
		// Create a child stdout pipe.
		if (::CreatePipe(&hStdoutReadTmp, &hStdoutWrite, &sa, 0))
		{

			// Create a duplicate of the stdout write handle for the std
			// error write handle. This is necessary in case the child
			// application closes one of its std output handles.
			if (::DuplicateHandle(
				::GetCurrentProcess(),
				hStdoutWrite,
				::GetCurrentProcess(),
				&hStderrWrite,
				0, TRUE,
				DUPLICATE_SAME_ACCESS))
			{

				// Create a child stdin pipe.
				if (::CreatePipe(&hStdinRead, &hStdinWriteTmp, &sa, 0))
				{

					// Create new stdout read handle and the stdin write handle.
					// Set the inheritance properties to FALSE. Otherwise, the child
					// inherits the these handles; resulting in non-closeable
					// handles to the pipes being created.
					if (::DuplicateHandle(
						::GetCurrentProcess(),
						hStdoutReadTmp,
						::GetCurrentProcess(),
						&m_hStdoutRead,
						0, FALSE,			// make it uninheritable.
						DUPLICATE_SAME_ACCESS))
					{

						if (::DuplicateHandle(
							::GetCurrentProcess(),
							hStdinWriteTmp,
							::GetCurrentProcess(),
							&m_hStdinWrite,
							0, FALSE,			// make it uninheritable.
							DUPLICATE_SAME_ACCESS))
						{

							// Close inheritable copies of the handles we do not want to
							// be inherited.
							DestroyHandle(hStdoutReadTmp);
							DestroyHandle(hStdinWriteTmp);

							// launch the child process
							if (LaunchChild(found_full_path.c_str(), full_param.c_str(),
								hStdoutWrite, hStdinRead, hStderrWrite, DefWordDir))
							{

								// Child is launched. Close the parents copy of those pipe
								// handles that only the child should have open.
								// Make sure that no handles to the write end of the stdout pipe
								// are maintained in this process or else the pipe will not
								// close when the child process exits and ReadFile will hang.
								DestroyHandle(hStdoutWrite);
								DestroyHandle(hStdinRead);
								DestroyHandle(hStderrWrite);

								// Launch a thread to receive output from the child process.
								m_hEvtStop = ::CreateEvent(NULL, TRUE, FALSE, NULL);
								m_hOutputThread = ::CreateThread(
									NULL, 0,
									OutputThread,
									this,
									0,
									&m_dwOutputThreadId);
								if (m_hOutputThread)
								{
									WaitForSingleObject(m_hOutputThread, INFINITE);
									bOK = TRUE;
								}
							}
						}
					}
				}
			}
		}
	}

	if (!bOK)
	{
		DWORD dwOsErr = ::GetLastError();
		char szMsg[40];
		::sprintf_s(szMsg, 40, "Redirect console error: %x\r\n", dwOsErr);
		WriteStdError(szMsg);
		DestroyHandle(hStdoutReadTmp);
		DestroyHandle(hStdoutWrite);
		DestroyHandle(hStderrWrite);
		DestroyHandle(hStdinWriteTmp);
		DestroyHandle(hStdinRead);
		Close();
		::SetLastError(dwOsErr);
	}

	return bOK;
}

void CProgramRedi::Close()
{
	if (m_hOutputThread != NULL)
	{
		// this function might be called from redir thread
		if (::GetCurrentThreadId() != m_dwOutputThreadId)
		{
			::SetEvent(m_hEvtStop);
			//::WaitForSingleObject(m_hOutputThread, INFINITE);
			if (::WaitForSingleObject(m_hOutputThread, 5000) == WAIT_TIMEOUT)
			{
				WriteStdError("The redir thread is dead\r\n");
				::TerminateThread(m_hOutputThread, -2);
			}
		}

		DestroyHandle(m_hOutputThread);
	}

	DestroyHandle(m_hEvtStop);
	DestroyHandle(m_hChildProcess);
	DestroyHandle(m_hStdinWrite);
	DestroyHandle(m_hStdoutRead);
	m_dwOutputThreadId = 0;
}

// write data to the child's stdin
BOOL CProgramRedi::Printf(LPCTSTR TextMatch,  DWORD dwNoReadWaitMinisec, LPCTSTR pszFormat, ...)
{
	if (!m_hStdinWrite && TextMatch==NULL)
		return FALSE;
	if(TextMatch!=NULL)
	{
		WaitForSingleObject(m_ReadOKEvent, INFINITE);
		char cTextMatch[1024]={0};
		if(sizeof(TCHAR)==sizeof(wchar_t))
		{
			int Len = WideCharToMultiByte(CP_ACP, 0, TextMatch, wcslen(TextMatch), cTextMatch, 1024, NULL, NULL);
			cTextMatch[Len] = 0;
		}else{
			strcpy_s(cTextMatch, 1024, (char*)TextMatch);
		}
		char *tokcontext= NULL;
		char *pAs = strtok_s(cTextMatch,"*", &tokcontext);
		while(pAs!=NULL)
		{
			while(m_LastReadMsg.find(pAs) == std::string::npos)
			{
				return FALSE;
			}
			pAs = strtok_s( NULL, "*", &tokcontext);
		}
	}

	wchar_t        *strInput_temp=new wchar_t[2048];
	std::wstring   strInput;
	va_list        argList;

	va_start(argList, pszFormat);
	vswprintf(strInput_temp,pszFormat,argList);
	va_end(argList);
	strInput = strInput_temp;
	delete []strInput_temp;

	char cWriteBuf[1024]={0};
    int  cWriteBufDataLen = 0;
	if(sizeof(TCHAR)==sizeof(wchar_t))
	{
		cWriteBufDataLen = WideCharToMultiByte(CP_ACP, 0, strInput.c_str(), strInput.length(), cWriteBuf, 1024, NULL, NULL);
		cWriteBuf[cWriteBufDataLen] = 0;
	}else{
		memcpy(cWriteBuf, strInput.c_str(), strInput.length());
		cWriteBufDataLen = strInput.length();
	}

	DWORD  dwWritten;
	BOOL   IsWriteOK = ::WriteFile(m_hStdinWrite, (LPCTSTR)cWriteBuf,
		cWriteBufDataLen, &dwWritten, NULL);
	if(dwNoReadWaitMinisec!=0 && dwNoReadWaitMinisec > m_dwWaitTime)
		m_NoReadWaitMinisec = dwNoReadWaitMinisec;
	ResetEvent(m_ReadOKEvent);
	SetEvent(m_WriteOKEvent);
	return IsWriteOK;
}

BOOL CProgramRedi::LaunchChild(LPCTSTR pszProgram, LPCTSTR pszCmdLine,
							  HANDLE hStdOut,
							  HANDLE hStdIn,
							  HANDLE hStdErr,
							  LPCTSTR DefWorkDir
							  )
{
	PROCESS_INFORMATION pi;
	STARTUPINFO si;

	assert(m_hChildProcess == NULL);

	// Set up the start up info struct.
	::ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	si.hStdOutput = hStdOut;
	si.hStdInput = hStdIn;
	si.hStdError = hStdErr;
	si.wShowWindow = SW_HIDE;
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;

	// Note that dwFlags must include STARTF_USESHOWWINDOW if we
	// use the wShowWindow flags. This also assumes that the
	// CreateProcess() call will use CREATE_NEW_CONSOLE.

	TCHAR AppWorkDir[1024]={0};
	if (DefWorkDir != NULL)
	{
		wcscpy(AppWorkDir, DefWorkDir);
	}
	else{
		if (wcsrchr(pszProgram, L'\\') != NULL)
		{
			wcsncpy_s(AppWorkDir, 1024, pszProgram, wcsrchr(pszProgram, L'\\') - pszProgram + 1);
		}
	}

	// Launch the child process.
	if (!::CreateProcess(
		pszProgram,
		(LPTSTR)pszCmdLine,
		NULL, NULL,
		TRUE,
		CREATE_NEW_CONSOLE|CREATE_UNICODE_ENVIRONMENT,
		NULL, AppWorkDir,
		&si,
		&pi))
	{
		return FALSE;
	}

	m_hChildProcess = pi.hProcess;
	// Close any unuseful handles
	::CloseHandle(pi.hThread);
	return TRUE;
}

// redirect the child process's stdout:
// return: 1: no more data, 0: child terminated, -1: os error
int CProgramRedi::RedirectStdout()
{ 
	assert(m_hStdoutRead != NULL);
	bool bexit = false;
	DWORD proc_statue = 0;
	for (;;)
	{
		DWORD dwAvail = 0;
		if (!::PeekNamedPipe(m_hStdoutRead, NULL, 0, NULL,
			&dwAvail, NULL))			// error
			break;
		BOOL ExitCodeOK=GetExitCodeProcess(m_hChildProcess, &proc_statue);
		if (!dwAvail && ExitCodeOK)
		{
			if (proc_statue == STILL_ACTIVE)
			{
				Sleep(100);
				continue;
			}
		}

		if (!dwAvail)					// not data available
		{
			return 1;
		}

		char szOutput[256];
		DWORD dwRead = 0;
		if (!::ReadFile(m_hStdoutRead, szOutput, min(255, dwAvail),
			&dwRead, NULL) || !dwRead)	// error, the child might ended
			break;

		szOutput[dwRead] = 0;
		m_LastReadMsg.append(szOutput, dwRead);
		Sleep(100);
		WriteStdOut(szOutput);

		if (ffmpeg_error_list != NULL)
		{
			for (int i = 0; i < ffmpeg_error_list->GetSize(); i++)
			{
				if (m_LastReadMsg.find(CStringA((*ffmpeg_error_list)[i]).GetBuffer()) != std::string::npos)
				{
					DestroyHandle(m_hEvtStop);
					DestroyHandle(m_hStdinWrite);
					DestroyHandle(m_hStdoutRead);
					BOOL ExitCodeOK = GetExitCodeProcess(m_hChildProcess, &proc_statue);

					//DWORD pid=GetProcessId(m_hChildProcess);
					//TCHAR TerFF[100] = { 0 };
					//wsprintf(TerFF, L"/c taskkill /pid %d", pid);
					//SHELLEXECUTEINFO sei;
					//ZeroMemory(&sei, sizeof(SHELLEXECUTEINFO));
					//sei.cbSize = sizeof(SHELLEXECUTEINFO);
					//sei.fMask = SEE_MASK_NOCLOSEPROCESS;
					//sei.hwnd = NULL;
					//sei.lpVerb = NULL;
					//sei.lpFile = L"cmd";
					//sei.lpParameters = TerFF;
					//sei.lpDirectory = NULL; 
					//sei.nShow = SW_HIDE;
					//sei.hInstApp = NULL;
					//ShellExecuteEx(&sei);
					//WaitForSingleObject(sei.hProcess, INFINITE);

					BOOL bExit = TerminateProcess(m_hChildProcess, 0);

					DWORD proc_statue2;
					BOOL ExitCodeOK2 = GetExitCodeProcess(m_hChildProcess, &proc_statue2);
					bexit = true;
					break;
				}
			}
		}
	}

	//FILE *ff = fopen("task_log\ffmpeg_log.txt","w");
	//fwrite(m_LastReadMsg.c_str(), 1, m_LastReadMsg.size(), ff);
	//fclose(ff);

	DWORD dwError = ::GetLastError();
	if (dwError == ERROR_BROKEN_PIPE ||	// pipe has been ended
		dwError == ERROR_NO_DATA)		// pipe closing in progress
	{
#ifdef _TEST_REDIR
		WriteStdOut("\r\n<TEST INFO>: Child process ended\r\n");
#endif
		return 0;	// child process ended
	}

	WriteStdError("Read stdout pipe error\r\n");
	return -1;		// os error
}

void CProgramRedi::DestroyHandle(HANDLE& rhObject)
{
	if (rhObject != NULL)
	{
		::CloseHandle(rhObject);
		rhObject = NULL;
	}
}

void CProgramRedi::WriteStdOut(LPCSTR pszOutput)
{
	//TRACE("%s", pszOutput);
}

void CProgramRedi::WriteStdError(LPCSTR pszError)
{
	//TRACE("%s", pszError);
}

// thread to receive output of the child process
DWORD WINAPI CProgramRedi::OutputThread(LPVOID lpvThreadParam)
{
	Sleep(200);
	HANDLE aHandles[2];
	int nRet;
	CProgramRedi* pRedir = (CProgramRedi*) lpvThreadParam;

	assert(pRedir != NULL);
	aHandles[0] = pRedir->m_hChildProcess;
	aHandles[1] = pRedir->m_hEvtStop;
	int           ReadCnt = 0;
	int           TotalReadBytes = 0;
	int           ReadEmptyCnt = 0;
	for (;;)
	{
		// redirect stdout till there's no more data.
		nRet = pRedir->RedirectStdout();
		if (nRet <= 0)
			break;
		ReadCnt++;
		if(pRedir->m_LastReadMsg.length()==TotalReadBytes)
		{
			ReadEmptyCnt++;
		}else{
			TotalReadBytes = pRedir->m_LastReadMsg.length();
			ReadEmptyCnt = 0;
		}
		if(ReadEmptyCnt == pRedir->m_NoReadWaitMinisec/pRedir->m_dwWaitTime)
		{
			// if(TotalReadBytes!=0)
			// {
				// SetEvent(pRedir->m_ReadOKEvent);
				// WaitForSingleObject(pRedir->m_WriteOKEvent, INFINITE);
			// }
			// pRedir->m_LastReadMsg.clear();
			// ReadCnt =0;
			// TotalReadBytes = 0;
			// ReadEmptyCnt = 0;
		}

		// check if the child process has terminated.
		DWORD dwRc = ::WaitForMultipleObjects(
			2, aHandles, FALSE, pRedir->m_dwWaitTime);
		if (WAIT_OBJECT_0 == dwRc)		// the child process ended
		{
			nRet = pRedir->RedirectStdout();
			if (nRet > 0)
				nRet = 0;
			//FILE *f=NULL;
			//fopen_s(&f,"msg.txt","wb");
			//fwrite(pRedir->m_LastReadMsg.c_str(),1,pRedir->m_LastReadMsg.length(),f);
			//fclose(f);
			break;
		}
		if (WAIT_OBJECT_0+1 == dwRc)	// m_hEvtStop was signalled
		{
			nRet = 1;	// cancelled
			break;
		}
	}
	// close handles
	pRedir->Close();
	return nRet;
}


//sample
//CProgramRedi progredi;
//BOOL bOK = progredi.Open(L"F:\\testProjects\\TestPip\\Debug\\TestPip.exe",L"978");
//if(bOK)
//{
//    if(!progredi.Printf(L"your*name",3000,L"wang\r\n",234234,324324))
//        return -1;
//    if(!progredi.Printf(L"your*age",1000,L"999\r\n",234234,324324))
//        return -1;
//    if(!progredi.Printf(L"any*exit",0,L"kfghkg\r\n",234234,324324))
//        return -1;
//    progredi.Close();
//}
//return 0;
