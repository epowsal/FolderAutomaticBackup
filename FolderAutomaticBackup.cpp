
// FolderAutomaticBackup.cpp : 定义应用程序的类行为。
//

#include "stdafx.h"
#include "FolderAutomaticBackup.h"
#include "FolderAutomaticBackupDlg.h"
#include "MFCAppParamParse.h"
#include "config_load_save_of_cpp.h"


int g_show_dlg=1;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CFolderAutomaticBackupApp

BEGIN_MESSAGE_MAP(CFolderAutomaticBackupApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CFolderAutomaticBackupApp 构造

CFolderAutomaticBackupApp::CFolderAutomaticBackupApp()
{
	// TODO:  在此处添加构造代码，
	// 将所有重要的初始化放置在 InitInstance 中
}


// 唯一的一个 CFolderAutomaticBackupApp 对象

CFolderAutomaticBackupApp theApp;


// CFolderAutomaticBackupApp 初始化

BOOL CFolderAutomaticBackupApp::InitInstance()
{
	CWinApp::InitInstance();


	AfxEnableControlContainer();

	// 创建 shell 管理器，以防对话框包含
	// 任何 shell 树视图控件或 shell 列表视图控件。
	CShellManager *pShellManager = new CShellManager;

	// 激活“Windows Native”视觉管理器，以便在 MFC 控件中启用主题
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	// 标准初始化
	// 如果未使用这些功能并希望减小
	// 最终可执行文件的大小，则应移除下列
	// 不需要的特定初始化例程
	// 更改用于存储设置的注册表项
	// TODO:  应适当修改该字符串，
	// 例如修改为公司或组织名
	//SetRegistryKey(_T("应用程序向导生成的本地应用程序"));

	wchar_t apppath[1024] = { 0 };
	GetModuleFileName(NULL, apppath, 1024);
	auto apl = wcslen(apppath);
	for (int i = 4; i < apl; i+=8){
		apppath[0] ^= apppath[i];
		if(i+1<apl)apppath[1] ^= apppath[i+1];
		if (i + 2<apl)apppath[2] ^= apppath[i + 2];
		if (i + 3<apl)apppath[3] ^= apppath[i + 3];
		if (i + 4<apl)apppath[4] ^= apppath[i + 4];
		if (i + 5<apl)apppath[5] ^= apppath[i + 5];
		if (i + 6<apl)apppath[6] ^= apppath[i + 6];
		if (i + 7<apl)apppath[7] ^= apppath[i + 7];
	}
	wchar_t aph[33] = { 0 };
	for (int i = 0; i < 8; i++){
		wsprintf(aph+i*2, L"%02x", apppath[i]);
	}
	HANDLE OneProgram = CreateMutex(NULL, FALSE, aph);// L"FolderAutomaticBackup");
	if (GetLastError() == ERROR_ALREADY_EXISTS) //检测程序是否已运行
	{
		MessageBox(NULL, L"前面已经运行过", L"已经运行", 0);
		return FALSE;
	}
	
	

	/*
	bool greg = false;
	wchar_t appdir[1024] = { 0 };
	GetModuleFileName(NULL, appdir, 1024);
	wcsrchr(appdir, '\\')[1] = 0;
	wcscat(appdir, L"注册码.txt");
	HANDLE hf = CreateFile(appdir, GENERIC_READ, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
	DWORD hfsize = GetFileSize(hf, 0), rdcnt;
	char *regbuf = 0;
	if (hfsize == 16 || hfsize == 40)regbuf = new char[hfsize + 1];
	memset(regbuf, 0, hfsize + 1);
	ReadFile(hf, regbuf, hfsize, &rdcnt, 0);
	CloseHandle(hf);
	if (hf == INVALID_HANDLE_VALUE || hfsize != 40 || hfsize == 40 && checkregcode(std::string(regbuf, hfsize)) == false)
	{
		wcsrchr(appdir, '\\')[1] = 0;
		std::string url = "http://www.lebb.cc/lebbmall.php?id=48&mc=" + getmachinecode();
		ShellExecuteA(0, "open", url.c_str(), 0, 0, SW_SHOW);
		SetCurrentDirectory(appdir);
		std::string mcode = getmacode(false);
		FILE *mcodef = fopen("机器码.txt", "wb");
		fwrite(mcode.c_str(), 1, mcode.size(), mcodef);
		fclose(mcodef);
		FILE *regcodef = fopen("注册码.txt", "rb");
		if (regcodef == NULL)
		{
			regcodef = fopen("注册码.txt", "wb");
			fwrite("请用注册码覆盖我", 1, strlen("请用注册码覆盖我"), regcodef);
			fclose(regcodef);
		}
		return 0;
	}
	else{
		greg = true;
	}

	if (hf == INVALID_HANDLE_VALUE || !(hfsize == 16 || hfsize == 40))greg = false;


	if (strtol(std::string(sha1_encode((unsigned char*)getmc().c_str()) + 0, 2).c_str(), 0, 16) ^ strtol(checkregcod().substr(10, 2).c_str(), 0, 16) ^ 0 != strtol(std::string(regbuf + 10, 2).c_str(), 0, 16))greg = false;
	*/


	TCHAR BufStr[1024] = { 0 };
	GetModuleFileName(NULL, BufStr, 1024);
	wcsrchr(BufStr, L'\\')[1] = 0;
	SetCurrentDirectory(BufStr);


	//if (!greg)((char*)0)[0] = 0;

	LoadConfig();

	//param parse
	MFCAppParamParse cmdInfo;
	ParseCommandLine(cmdInfo);

	auto cmdline=GetCommandLine();

	if (wcsstr(cmdline, L"show=false") != NULL)
	{
		g_show_dlg = 0;
	}
	else{
		if (g_starthide == "1"){
			g_show_dlg = 0;
		}
		else{
			g_show_dlg = 1;
		}
	}


	//if (!greg)((char*)0)[0] = 0;

	

	CFolderAutomaticBackupDlg dlg;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO:  在此放置处理何时用
		//  “确定”来关闭对话框的代码
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO:  在此放置处理何时用
		//  “取消”来关闭对话框的代码
	}
	else if (nResponse == -1)
	{
		TRACE(traceAppMsg, 0, "警告: 对话框创建失败，应用程序将意外终止。\n");
		TRACE(traceAppMsg, 0, "警告: 如果您在对话框上使用 MFC 控件，则无法 #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS。\n");
	}

	// 删除上面创建的 shell 管理器。
	if (pShellManager != NULL)
	{
		delete pShellManager;
	}

	// 由于对话框已关闭，所以将返回 FALSE 以便退出应用程序，
	//  而不是启动应用程序的消息泵。
	return FALSE;
}

