#ifndef PROGRAMREDI_H
#define PROGRAMREDI_H


#include <string>
#include <afx.h>
#include <Windows.h>


class CProgramRedi
{
public:
	CProgramRedi(int NoReadWaitMinisec = 500);
	virtual ~CProgramRedi();

private:
	HANDLE m_hOutputThread;		// thread to receive the output of the child process
	HANDLE m_hEvtStop;			// event to notify the redir thread to exit
	DWORD m_dwOutputThreadId;	// id of the redir output thread
	DWORD m_dwWaitTime;			// wait time to check the status of the child process

protected:
	HANDLE m_hStdinWrite;	// write end of child's stdin pipe
	HANDLE m_hStdoutRead;	// read end of child's stdout pipe
	HANDLE m_hChildProcess;

	BOOL LaunchChild(LPCTSTR pszProgram, LPCTSTR pszCmdLine,
		HANDLE hStdOut, HANDLE hStdIn, HANDLE hStdErr, LPCTSTR DefWordDir);
	int RedirectStdout();
	void DestroyHandle(HANDLE& rhObject);

	static DWORD WINAPI OutputThread(LPVOID lpvThreadParam);

protected:
	// overrides:
	virtual void WriteStdOut(LPCSTR pszOutput);
	virtual void WriteStdError(LPCSTR pszError);

public:
	BOOL Open(LPCTSTR pszProgram, LPCTSTR pszCmdLine, LPCTSTR DefWordDir, CStringArray *ffmpeg_error_list);
	virtual void Close();
	//TextMatch parameter suport "*",If it is NULL will direct write;
	//NoReadWaitMinisec 表示写完之后多少毫秒没读到数据，算下一次读完成，如0表示默认。
	BOOL Printf(LPCTSTR TextMatch, DWORD dwNoReadWaitMinisec, LPCTSTR pszFormat, ...);

	void SetWaitTime(DWORD dwWaitTime) { m_dwWaitTime = dwWaitTime; }
	
	//设置默认多少毫秒没有读到数据，算读完成。
	void SetNoReadWaitMinisec(DWORD dwNoReadWaitMinisec){ m_NoReadWaitMinisec = dwNoReadWaitMinisec; }
	//获取Console输出
	std::string GetConsoleOutput()
	{
		return m_LastReadMsg;
	}
private:
	std::string  m_LastReadMsg;
	HANDLE       m_ReadOKEvent;
	HANDLE       m_WriteOKEvent;
	DWORD        m_NoReadWaitMinisec;
	HANDLE       m_UserInputEvent;
	bool         m_bHasUserInput;
	CStringArray *ffmpeg_error_list=NULL;
};


#endif