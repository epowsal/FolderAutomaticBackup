
// FolderAutomaticBackup.cpp : ����Ӧ�ó��������Ϊ��
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


// CFolderAutomaticBackupApp ����

CFolderAutomaticBackupApp::CFolderAutomaticBackupApp()
{
	// TODO:  �ڴ˴���ӹ�����룬
	// ��������Ҫ�ĳ�ʼ�������� InitInstance ��
}


// Ψһ��һ�� CFolderAutomaticBackupApp ����

CFolderAutomaticBackupApp theApp;


// CFolderAutomaticBackupApp ��ʼ��

BOOL CFolderAutomaticBackupApp::InitInstance()
{
	CWinApp::InitInstance();


	AfxEnableControlContainer();

	// ���� shell ���������Է��Ի������
	// �κ� shell ����ͼ�ؼ��� shell �б���ͼ�ؼ���
	CShellManager *pShellManager = new CShellManager;

	// ���Windows Native���Ӿ����������Ա��� MFC �ؼ�����������
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	// ��׼��ʼ��
	// ���δʹ����Щ���ܲ�ϣ����С
	// ���տ�ִ���ļ��Ĵ�С����Ӧ�Ƴ�����
	// ����Ҫ���ض���ʼ������
	// �������ڴ洢���õ�ע�����
	// TODO:  Ӧ�ʵ��޸ĸ��ַ�����
	// �����޸�Ϊ��˾����֯��
	//SetRegistryKey(_T("Ӧ�ó��������ɵı���Ӧ�ó���"));

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
	if (GetLastError() == ERROR_ALREADY_EXISTS) //�������Ƿ�������
	{
		MessageBox(NULL, L"ǰ���Ѿ����й�", L"�Ѿ�����", 0);
		return FALSE;
	}
	
	

	/*
	bool greg = false;
	wchar_t appdir[1024] = { 0 };
	GetModuleFileName(NULL, appdir, 1024);
	wcsrchr(appdir, '\\')[1] = 0;
	wcscat(appdir, L"ע����.txt");
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
		FILE *mcodef = fopen("������.txt", "wb");
		fwrite(mcode.c_str(), 1, mcode.size(), mcodef);
		fclose(mcodef);
		FILE *regcodef = fopen("ע����.txt", "rb");
		if (regcodef == NULL)
		{
			regcodef = fopen("ע����.txt", "wb");
			fwrite("����ע���븲����", 1, strlen("����ע���븲����"), regcodef);
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
		// TODO:  �ڴ˷��ô����ʱ��
		//  ��ȷ�������رնԻ���Ĵ���
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO:  �ڴ˷��ô����ʱ��
		//  ��ȡ�������رնԻ���Ĵ���
	}
	else if (nResponse == -1)
	{
		TRACE(traceAppMsg, 0, "����: �Ի��򴴽�ʧ�ܣ�Ӧ�ó���������ֹ��\n");
		TRACE(traceAppMsg, 0, "����: ������ڶԻ�����ʹ�� MFC �ؼ������޷� #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS��\n");
	}

	// ɾ�����洴���� shell ��������
	if (pShellManager != NULL)
	{
		delete pShellManager;
	}

	// ���ڶԻ����ѹرգ����Խ����� FALSE �Ա��˳�Ӧ�ó���
	//  ����������Ӧ�ó������Ϣ�á�
	return FALSE;
}

