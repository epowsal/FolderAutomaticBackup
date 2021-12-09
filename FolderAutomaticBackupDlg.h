
// FolderAutomaticBackupDlg.h : ͷ�ļ�
//

#pragma once
#include "afxcmn.h"
#include "CEditListCtrl.h"
#include "afxwin.h"
#include "config_load_save_of_cpp.h"
#include <deque>
#include <ctime>
#include <set>
#include "..\UtilityFunc\MMutex.h"

struct MonitorDirs
{
	CString DirSource;
	CString DirTarget;
	CString GitCmdStr;
	CString Mon_Exts;
	CString MaxBakupNum;
	CString MinIntervalBackup;
	CString MoCreate;
	CString MoNameChange;
	CString MoSize;
	CString MoLastWrite;
	BOOL    bInTask;
	CString MonExts;//";"����
	CString mexcludepathword;//·���ų���
	CString FileLsExts;//";"����
	std::set<std::wstring>  copyfiles;
	MMutex copyfilesm;
};



// CFolderAutomaticBackupDlg �Ի���
class CFolderAutomaticBackupDlg : public CDialogEx
{
// ����
public:
	CFolderAutomaticBackupDlg(CWnd* pParent = NULL);	// ��׼���캯��

// �Ի�������
	enum { IDD = IDD_FOLDERAUTOMATICBACKUP_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV ֧��


// ʵ��
protected:
	HICON m_hIcon;

	// ���ɵ���Ϣӳ�亯��
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg BOOL OnDeviceChange(UINT nEventType, DWORD dwData);
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	void OnBackupInfoDbClicked(NMHDR* pNMHDR, LRESULT* pResult);
	void SetListItemText(void);
	void OnEnKillfocusEdit2();
	BOOL PreTranslateMessage(MSG* pMsg);
	int  listSelFlag[3];

	CListCtrl m_backuptasklist;
	afx_msg void OnBnClickedButAddtask();
	CEdit m_SubItemEdit;
	CString cstrItemTextEdit;
	afx_msg void OnBnClickedButDeltask();
	void DoSave();
	void DoLoad();
	afx_msg void OnBnClickedButStartspy();
	afx_msg void OnBnClickedButStopspy();
	void OnTimer(UINT_PTR nIDEvent);
	LRESULT OnHotKey(WPARAM wParam, LPARAM lParam);


	std::deque<HANDLE> TaskThreadDeq;
	afx_msg void OnBnClickedButHide();
	CEdit m_srcdir;
	CEdit m_targetdir;
	CEdit m_houzuied;
	CButton m_startbtn;
	CButton m_stopbtn;
	std::deque<MonitorDirs *> moninfodeq;

	CToolTipCtrl    m_Mytip; 
	CEdit m_filedircomprcopydefedit;
	CEdit mexcludepathword;
	CButton starthideck;
	afx_msg void OnBnClickedStarthideck();
	CEdit maxreserveed;
	afx_msg void OnEnChangeMaxreserveed();
	CEdit hotkeyed;
	afx_msg void OnEnChangeEdit7();
	afx_msg void OnEnChangeEdit6();

	time_t devicechangestarttime;
	bool binitdlg;
	afx_msg void OnBnClickedShadowmoveout();
	CButton shadowbtn;
	afx_msg void OnBnClickedMovenouseout();
	CButton nousemoveoutbtn;
	bool bmiddleindown;
	int OnBnClickedStarthideckState;
	afx_msg void OnEnChangeEdit1();
	afx_msg void OnEnChangeEdit4();
	afx_msg void OnEnChangeEdit5();
};
