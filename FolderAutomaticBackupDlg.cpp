
// FolderAutomaticBackupDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "FolderAutomaticBackup.h"
#include "FolderAutomaticBackupDlg.h"
#include "afxdialogex.h"
#include <string>
#include <deque>
#include <regex>
#include <Shlwapi.h>
#include "..\UtilityFunc\BackupDirectoryWithCopy.h"
#include "..\UtilityFunc\FindFolderAllDir.h"
#include "..\UtilityFunc\SplitString.h"
#include "..\UtilityFunc\time_t_systemtime_convert.h"
#include <set>
#include "sqlite.h"
#include "..\UtilityFunc\time_t_to_isodatetime.h"
#include "..\UtilityFunc\MMutex.h"
#include "..\UtilityFunc\CopyDirectory.h"
#include "..\UtilityFunc\SplitString.h"
#include "..\UtilityFunc\ProgramRedi.h"
#include "..\UtilityFunc\LbLog.h"
#include "..\UtilityFunc\CreateDirs.h"
#include "..\UtilityFunc\GetFolderAllFiles.h"

using namespace std;

extern int g_show_dlg;

struct compresscopytask_info
{
	CString srcfile_or_dir;
	CString targetfile_or_dir;
	CString incwords;//分号分开
	CString excwords;
	int     backupcnt;
};
struct compresscopytaskana_info
{
	CString srcfile_or_dir;
	int    bdir_srcfile_or_dir;//-1 unkonw, 1 true,0 false
	std::deque<std::wstring> incwordsls;
	std::deque<std::wstring> excwordsls;
	compresscopytask_info task;
	std::deque<std::wstring> allmatchfiles;
	time_t lastrftime;
};
std::deque<compresscopytaskana_info>  compresscopytaskanadeq;
std::deque<compresscopytask_info>  compresscopytaskdeq;
MMutex compresscopytaskdeq_mutex;

void Text2compresscopytaskana(CString text)
{
	compresscopytaskdeq_mutex.Lock();
	compresscopytaskdeq.clear();
	compresscopytaskdeq_mutex.Unlock();
	compresscopytaskanadeq.clear();
	std::deque < std::wstring>  linels;
	splitString(text.GetBuffer(), L"\r\n", linels);
	for (int i = 0; i < (int)linels.size(); i++)
	{
		std::wstring linestr = linels[i];
		std::deque < std::wstring>  tabls;
		splitStringHaveEmpty(linestr.c_str(), L"|", tabls);
		if (tabls.size() == 4 || tabls.size() == 5)
		{
			compresscopytaskana_info info;
			info.task.srcfile_or_dir = tabls[0].c_str();
			info.task.targetfile_or_dir = tabls[1].c_str();
			info.task.incwords = tabls[2].c_str();//分号分开
			info.task.excwords = tabls[3].c_str();
			if (tabls.size() == 5)
			{
				info.task.backupcnt = _wtoi(tabls[4].c_str());
				if (info.task.backupcnt == 0)info.task.backupcnt = 1;
			}
			else{
				info.task.backupcnt = 1;
			}
			info.srcfile_or_dir = tabls[0].c_str();
			if (!(GetFileAttributes(info.srcfile_or_dir) & FILE_ATTRIBUTE_DIRECTORY))
			{
				info.bdir_srcfile_or_dir = 0;
			}
			else{
				info.bdir_srcfile_or_dir = 1;
			}
			info.lastrftime = 0;
			splitString(tabls[2], L";", info.incwordsls);
			splitString(tabls[3], L";", info.excwordsls);
			compresscopytaskanadeq.push_back(info);

			CString tarext = info.task.targetfile_or_dir;
			if (tarext.ReverseFind(L'\\') != -1 && tarext.ReverseFind(L'.') != -1 && tarext.ReverseFind(L'.')>tarext.ReverseFind(L'\\'))
			{
				tarext = tarext.Mid(tarext.ReverseFind(L'.'));
				tarext.MakeLower();
			}
			else{
				tarext = L"";
			}

			auto task = info.task;
			CString srcext = task.srcfile_or_dir;
			if (srcext.ReverseFind(L'\\') != -1 && srcext.ReverseFind(L'.') != -1 && srcext.ReverseFind(L'.')>srcext.ReverseFind(L'\\'))
			{
				srcext = srcext.Mid(srcext.ReverseFind(L'.'));
				srcext.MakeLower();
			}
			else{
				srcext = L"";
			}
			if (!(GetFileAttributes(task.srcfile_or_dir)&FILE_ATTRIBUTE_DIRECTORY)
				&& (PathFileExists(task.targetfile_or_dir) && !(GetFileAttributes(task.targetfile_or_dir)&FILE_ATTRIBUTE_DIRECTORY) && (tarext == ".7z" || tarext == ".zip" || srcext == tarext)
				|| PathFileExists(task.targetfile_or_dir) == FALSE && srcext.GetLength() > 0 && srcext == tarext
				|| PathFileExists(task.targetfile_or_dir) == FALSE && srcext.GetLength() && (tarext == ".7z" || tarext == ".zip"))
				)
			{
				FILETIME srctime, tartime;
				HANDLE h1 = CreateFile(task.srcfile_or_dir, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
				GetFileTime(h1, NULL, NULL, &srctime);
				CloseHandle(h1);
				HANDLE h2 = CreateFile(task.targetfile_or_dir, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
				GetFileTime(h2, NULL, NULL, &tartime);
				CloseHandle(h2);
				SYSTEMTIME srctimes, tartimes;
				FileTimeToSystemTime(&srctime, &srctimes);
				FileTimeToSystemTime(&tartime, &tartimes);
				auto t1 = SystemTimeToTime_t(srctimes);
				auto t2 = SystemTimeToTime_t(tartimes);

				if (t1 > t2)
				{
					//bak cnt>1
					for (int mi = task.backupcnt; mi >= 1; mi--)
					{
						std::wstring targetfile_or_dir_pre = task.targetfile_or_dir;
						if (mi > 1)
						{
							CString rep;
							rep.Format(L"_%d.", mi - 1);
							targetfile_or_dir_pre.replace(targetfile_or_dir_pre.rfind(L'.'), 1, rep);
						}

						std::wstring targetfile_or_dir_next = task.targetfile_or_dir;
						CString rep2;
						rep2.Format(L"_%d.", mi);
						targetfile_or_dir_next.replace(targetfile_or_dir_next.rfind(L'.'), 1, rep2);
						DeleteFile(targetfile_or_dir_next.c_str());
						MoveFile(targetfile_or_dir_pre.c_str(), targetfile_or_dir_next.c_str());
					}

					//源是文件,目标是文件，后缀不一样就压缩，一样就复制
					if (srcext == tarext)
					{
						CString dir = task.targetfile_or_dir;
						if (dir.ReverseFind(L'\\') != -1)
						{
							dir = dir.Mid(0, dir.ReverseFind(L'\\'));
							CreateFullDir(dir);
						}

						CopyFile(task.srcfile_or_dir, task.targetfile_or_dir, FALSE);
					}
					else{



						CString srcdir = task.srcfile_or_dir;
						srcdir = srcdir.Mid(0, srcdir.ReverseFind(L'\\'));
						std::wstring comprtypepram = L" -t7z ";
						if (tarext == ".zip")
						{
							comprtypepram = L" -tzip ";
						}
						if (PathFileExists(task.targetfile_or_dir))
						{
							MoveFileEx(task.targetfile_or_dir.GetBuffer(), task.targetfile_or_dir + L".bak", MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
						}
						std::wstring cpparam = L"a " + comprtypepram + L" \"" + task.targetfile_or_dir.GetBuffer() + L"\" \"" + task.srcfile_or_dir.GetBuffer() + L"\"";
						CProgramRedi  redi;
						redi.Open(L"7z.exe", cpparam.c_str(), srcdir);
						std::string outstr = redi.GetConsoleOutput();

						if (outstr.find("ERROR") == std::string::npos)
						{
							//success
						}
						else{
							//failed
							LBLOGD(ERROR << "backup failed:" << task.srcfile_or_dir.GetBuffer() << "   " << task.targetfile_or_dir.GetBuffer());
						}
					}
				}
			}
			else if ((GetFileAttributes(task.srcfile_or_dir)&FILE_ATTRIBUTE_DIRECTORY)
				&& !(GetFileAttributes(task.targetfile_or_dir)&FILE_ATTRIBUTE_DIRECTORY) && (tarext == ".7z" || tarext == ".zip")
				)
			{
				FILETIME tartime;
				HANDLE h2 = CreateFile(task.targetfile_or_dir, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
				GetFileTime(h2, NULL, NULL, &tartime);
				CloseHandle(h2);
				SYSTEMTIME tartimes;
				FileTimeToSystemTime(&tartime, &tartimes);
				auto t2 = SystemTimeToTime_t(tartimes);

				std::deque<std::wstring> *allmatchfiles2;
				GetDirectoryAllMatchFiles(compresscopytaskanadeq.back().srcfile_or_dir.GetBuffer(), compresscopytaskanadeq.back().srcfile_or_dir.GetBuffer(), compresscopytaskanadeq.back().allmatchfiles, compresscopytaskanadeq.back().incwordsls, compresscopytaskanadeq.back().excwordsls);
				allmatchfiles2 = &compresscopytaskanadeq.back().allmatchfiles;
				for (int mi = 0; mi < allmatchfiles2->size(); mi++)
				{
					HANDLE hff = CreateFile((*allmatchfiles2)[mi].c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
					if (hff != NULL && hff != INVALID_HANDLE_VALUE)
					{
						FILETIME modtim;
						GetFileTime(hff, NULL, NULL, &modtim);
						CloseHandle(hff);
						SYSTEMTIME systime;
						FileTimeToSystemTime(&modtim, &systime);
						if (SystemTimeToTime_t(systime)>t2)
						{
							compresscopytaskdeq_mutex.Lock();
							compresscopytaskdeq.push_back(compresscopytaskanadeq.back().task);
							compresscopytaskdeq_mutex.Unlock();
							break;
						}
					}
				}



			}


		}
	}
}



DWORD  WINAPI  ChangeAutoCompressOrCopyThread(LPVOID param)
{
	while (true)
	{
		compresscopytask_info task;
		compresscopytaskdeq_mutex.Lock();
		if (compresscopytaskdeq.size())
		{
			task = compresscopytaskdeq.front();
			compresscopytaskdeq.pop_front();
		}
		compresscopytaskdeq_mutex.Unlock();
		if (task.srcfile_or_dir.GetLength())
		{
			CString srcext = task.srcfile_or_dir;
			if (srcext.ReverseFind(L'\\') != -1 && srcext.ReverseFind(L'.') != -1 && srcext.ReverseFind(L'.')>srcext.ReverseFind(L'\\'))
			{
				srcext = srcext.Mid(srcext.ReverseFind(L'.'));
				srcext.MakeLower();
			}
			else{
				srcext = L"";
			}

			CString tarext = task.targetfile_or_dir;
			if (tarext.ReverseFind(L'\\') != -1 && tarext.ReverseFind(L'.') != -1 && tarext.ReverseFind(L'.')>tarext.ReverseFind(L'\\'))
			{
				tarext = tarext.Mid(tarext.ReverseFind(L'.'));
				tarext.MakeLower();
			}
			else{
				tarext = L"";
			}

			if ((GetFileAttributes(task.srcfile_or_dir)&FILE_ATTRIBUTE_DIRECTORY)
				&& PathFileExists(task.targetfile_or_dir) && (GetFileAttributes(task.targetfile_or_dir)&FILE_ATTRIBUTE_DIRECTORY))
			{
				//bak cnt>1
				for (int mi = task.backupcnt; mi >= 1; mi--)
				{
					std::wstring targetfile_or_dir_pre = task.targetfile_or_dir;
					if (mi > 1)
					{
						CString rep;
						rep.Format(L"_%d", mi - 1);
						targetfile_or_dir_pre += rep;
					}

					std::wstring targetfile_or_dir_next = task.targetfile_or_dir;
					CString rep2;
					rep2.Format(L"_%d", mi);
					targetfile_or_dir_next += rep2;

					SHELLEXECUTEINFO sei;
					ZeroMemory(&sei, sizeof(SHELLEXECUTEINFO));
					sei.cbSize = sizeof(SHELLEXECUTEINFO);
					sei.fMask = SEE_MASK_NOCLOSEPROCESS;
					sei.hwnd = NULL;
					sei.lpVerb = NULL;
					sei.lpFile = L"cmd.exe";
					std::wstring mkdir_param = L"/c move /Y \"" + targetfile_or_dir_pre + L"\" \"" + targetfile_or_dir_next + L"\"";
					sei.lpParameters = mkdir_param.c_str();
					sei.lpDirectory = NULL;
					sei.nShow = SW_HIDE;
					sei.hInstApp = NULL;
					ShellExecuteEx(&sei);
					WaitForSingleObject(sei.hProcess, INFINITE);
				}

				//都是目录,目标是目录,排除法获得文件，目录到目录复制
				std::deque<std::wstring> includewords;
				std::deque<std::wstring> ecxludewords;
				int rl1 = splitString(task.incwords.GetBuffer(), L";", includewords);
				int rl2 = splitString(task.excwords.GetBuffer(), L";", ecxludewords);
				CopyDirectory(task.srcfile_or_dir.GetBuffer(), task.srcfile_or_dir.GetBuffer(), task.targetfile_or_dir.GetBuffer(), includewords, ecxludewords, COPYDIRECTORY_COPY_ALL);
			}
			else if ((GetFileAttributes(task.srcfile_or_dir)&FILE_ATTRIBUTE_DIRECTORY)
				&& (PathFileExists(task.targetfile_or_dir) && !(GetFileAttributes(task.targetfile_or_dir)&FILE_ATTRIBUTE_DIRECTORY) && (tarext == ".7z" || tarext == ".zip")
				|| PathFileExists(task.targetfile_or_dir) == FALSE && (tarext == ".7z" || tarext == ".zip"))
				)
			{
				//源是目录,目标是压缩到路径，排除法获得文件，压缩到目标目录
				{
					std::deque<std::wstring> includewords;
					std::deque<std::wstring> ecxludewords;
					int rl1 = splitString(task.incwords.GetBuffer(), L";", includewords);
					int rl2 = splitString(task.excwords.GetBuffer(), L";", ecxludewords);

					std::deque<std::wstring> matchfiles;
					GetDirectoryAllMatchFiles(task.srcfile_or_dir.GetBuffer(), task.srcfile_or_dir.GetBuffer(), matchfiles, includewords, ecxludewords);

					CFile ff;
					CString compressfilspa = task.srcfile_or_dir + "\\lbfilechange.compress.temp";
					if (ff.Open(compressfilspa, CFile::modeWrite | CFile::modeCreate) == TRUE)
					{
						//bak cnt>1
						for (int mi = task.backupcnt; mi >= 1; mi--)
						{
							std::wstring targetfile_or_dir_pre = task.targetfile_or_dir;
							if (mi > 1)
							{
								CString rep;
								rep.Format(L"_%d.", mi - 1);
								targetfile_or_dir_pre.replace(targetfile_or_dir_pre.rfind(L'.'), 1, rep);
							}

							std::wstring targetfile_or_dir_next = task.targetfile_or_dir;
							CString rep2;
							rep2.Format(L"_%d.", mi);
							targetfile_or_dir_next.replace(targetfile_or_dir_next.rfind(L'.'), 1, rep2);
							DeleteFile(targetfile_or_dir_next.c_str());
							MoveFile(targetfile_or_dir_pre.c_str(), targetfile_or_dir_next.c_str());
						}


						//
						for (int i = 0; i < matchfiles.size(); i++)
						{
							std::wstring linestr = matchfiles[i] + L"\n";
							char clinestr[2048];
							DWORD l = WideCharToMultiByte(CP_UTF8, 0, linestr.c_str(), linestr.size(), clinestr, 2048, 0, 0);
							clinestr[l] = 0;
							ff.Write(clinestr, l);
						}
						ff.Close();

						std::wstring wsourcedir = task.srcfile_or_dir;
						std::wstring comprtypepram = L" -t7z ";
						if (tarext == ".zip")
						{
							comprtypepram = L" -tzip ";
						}
						std::wstring cpparam = std::wstring() + L"a " + comprtypepram + L" -i\"@" + compressfilspa.GetBuffer() + L"\" \"" + task.targetfile_or_dir.GetBuffer() + L"\"";
						CProgramRedi  redi;
						redi.Open(L"7z.exe", cpparam.c_str(), wsourcedir.c_str());
						std::string outstr = redi.GetConsoleOutput();
						DeleteFile(compressfilspa);
						if (outstr.find("ERROR") == std::string::npos)
						{
							//success
						}
						else{
							//failed
							LBLOGD(ERROR << "backup failed:" << task.srcfile_or_dir.GetBuffer() << "\t" << task.targetfile_or_dir.GetBuffer());
						}
					}
					else{
						LBLOGD(ERROR << "open failed:" << compressfilspa.GetBuffer());
					}
				}
			}
			else if (!(GetFileAttributes(task.srcfile_or_dir)&FILE_ATTRIBUTE_DIRECTORY)
				&& (PathFileExists(task.targetfile_or_dir) && !(GetFileAttributes(task.targetfile_or_dir)&FILE_ATTRIBUTE_DIRECTORY) && (tarext == ".7z" || tarext == ".zip" || srcext == tarext)
				|| PathFileExists(task.targetfile_or_dir) == FALSE && srcext.GetLength()>0 && srcext == tarext
				|| PathFileExists(task.targetfile_or_dir) == FALSE && srcext.GetLength() && (tarext == ".7z" || tarext == ".zip"))
				)
			{
				//bak cnt>1
				for (int mi = task.backupcnt; mi >= 1; mi--)
				{
					std::wstring targetfile_or_dir_pre = task.targetfile_or_dir;
					if (mi > 1)
					{
						CString rep;
						rep.Format(L"_%d.", mi - 1);
						targetfile_or_dir_pre.replace(targetfile_or_dir_pre.rfind(L'.'), 1, rep);
					}

					std::wstring targetfile_or_dir_next = task.targetfile_or_dir;
					CString rep2;
					rep2.Format(L"_%d.", mi);
					targetfile_or_dir_next.replace(targetfile_or_dir_next.rfind(L'.'), 1, rep2);
					DeleteFile(targetfile_or_dir_next.c_str());
					MoveFile(targetfile_or_dir_pre.c_str(), targetfile_or_dir_next.c_str());
				}

				//源是文件,目标是文件，后缀不一样就压缩，一样就复制
				if (srcext == tarext)
				{
					CString dir = task.targetfile_or_dir;
					if (dir.ReverseFind(L'\\') != -1)
					{
						dir = dir.Mid(0, dir.ReverseFind(L'\\'));
						CreateFullDir(dir);
					}
					CopyFile(task.srcfile_or_dir, task.targetfile_or_dir, FALSE);
				}
				else{
					CString srcdir = task.srcfile_or_dir;
					srcdir = srcdir.Mid(0, srcdir.ReverseFind(L'\\'));
					std::wstring comprtypepram = L" -t7z ";
					if (tarext == ".zip")
					{
						comprtypepram = L" -tzip ";
					}
					if (PathFileExists(task.targetfile_or_dir))
					{
						MoveFileEx(task.targetfile_or_dir.GetBuffer(), task.targetfile_or_dir + L".bak", MOVEFILE_REPLACE_EXISTING);
					}
					std::wstring cpparam = L"a " + comprtypepram + L" \"" + task.targetfile_or_dir.GetBuffer() + L"\" \"" + task.srcfile_or_dir.GetBuffer() + L"\"";
					CProgramRedi  redi;
					redi.Open(L"7z.exe", cpparam.c_str(), srcdir);
					std::string outstr = redi.GetConsoleOutput();

					if (outstr.find("ERROR") == std::string::npos)
					{
						//success
					}
					else{
						//failed
						LBLOGD(ERROR << "backup failed:" << task.srcfile_or_dir.GetBuffer()<<task.targetfile_or_dir.GetBuffer());
					}
				}
			}
			else if (!(GetFileAttributes(task.srcfile_or_dir)&FILE_ATTRIBUTE_DIRECTORY)
				&& PathFileExists(task.targetfile_or_dir) && (GetFileAttributes(task.targetfile_or_dir)&FILE_ATTRIBUTE_DIRECTORY))
			{
				//源是文件,目标是目录，复制到目录
				CString srcfname = task.srcfile_or_dir;
				if (srcfname.ReverseFind(L'\\') != -1)
				{
					srcfname = srcfname.Mid(srcfname.ReverseFind(L'\\'));
				}
				if (!PathFileExists(task.targetfile_or_dir))
				{
					CreateFullDir(task.targetfile_or_dir);
				}


				//bak cnt>1
				std::wstring real_targetfile_or_dir = task.targetfile_or_dir + L"\\" + srcfname;
				//bak cnt>1
				for (int mi = task.backupcnt; mi >= 1; mi--)
				{
					std::wstring targetfile_or_dir_pre = real_targetfile_or_dir;
					if (mi > 1)
					{
						CString rep;
						rep.Format(L"_%d.", mi - 1);
						targetfile_or_dir_pre.replace(targetfile_or_dir_pre.rfind(L'.'), 1, rep);
					}

					std::wstring targetfile_or_dir_next = real_targetfile_or_dir;
					CString rep2;
					rep2.Format(L"_%d.", mi);
					targetfile_or_dir_next.replace(targetfile_or_dir_next.rfind(L'.'), 1, rep2);
					DeleteFile(targetfile_or_dir_next.c_str());
					MoveFile(targetfile_or_dir_pre.c_str(), targetfile_or_dir_next.c_str());
				}


				if (CopyFile(task.srcfile_or_dir, task.targetfile_or_dir + L"\\" + srcfname, FALSE) == FALSE)
				{
					LBLOGD(ERROR << "Copy fail:" << task.srcfile_or_dir << "\t" << task.targetfile_or_dir);
				}
			}



		}
		else{
			Sleep(5000);
		}
	}
	return 0;
}





bool FindFolderAllFileWithTimeGreatThan(std::wstring finddirpath, std::deque<std::wstring> & deqfiles, CTime &curtime, int layercnt = 5)
{
	if (layercnt - 1 < 0)return false;
	if (finddirpath[finddirpath.size() - 1] != L'\\')
	{
		finddirpath = finddirpath + L"\\";
	}
	std::wstring finddirexp = finddirpath + L"*.*";

	WIN32_FIND_DATA fd;
	HANDLE hfind = FindFirstFile(finddirexp.c_str(), &fd);
	if (hfind != NULL && hfind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!(wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) && layercnt - 1 >= 0)
			{
				if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
				{
					std::wstring findd1 = finddirpath + fd.cFileName;
					FindFolderAllFileWithTimeGreatThan(findd1, deqfiles, curtime, layercnt - 1);
				}
				else{
					SYSTEMTIME systime;
					FileTimeToSystemTime(&fd.ftLastWriteTime, &systime);
					CTime ftime(systime);
					TIME_ZONE_INFORMATION tin;
					GetTimeZoneInformation(&tin);
					ftime -= CTimeSpan(0, 0, 0, tin.Bias * 60);
					CString timestr1 = ftime.Format(L"_%Y%m%d%H%M%S.");
					CString timestr2 = curtime.Format(L"_%Y%m%d%H%M%S.");
					if (ftime >= curtime)
					{
						std::wstring filepath = finddirpath + fd.cFileName;
						deqfiles.push_back(filepath);
					}
				}
			}
		} while (FindNextFile(hfind, &fd));
		FindClose(hfind);
	}

	return true;
}



//filenamematch与找文件名通常格式一样
bool FindDirFileMatchAndNewest(std::wstring finddirpath, std::wstring filenamematch, std::wstring &outfile, CTime &lasttime = CTime(0), int layercnt = 1)
{
	if (layercnt - 1 < 0)return false;
	if (finddirpath[finddirpath.size() - 1] != L'\\')
	{
		finddirpath = finddirpath + L"\\";
	}
	std::wstring finddirexp = finddirpath + L"*.*";

	WIN32_FIND_DATA fd;
	HANDLE hfind = FindFirstFile(finddirexp.c_str(), &fd);
	if (hfind != NULL && hfind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!(wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) && layercnt - 1 >= 0)
			{
				if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
				{
					std::wstring findd1 = finddirpath + fd.cFileName;
					FindDirFileMatchAndNewest(findd1, filenamematch, outfile, lasttime, layercnt - 1);
				}
				else{
					if (filenamematch.size() == wcslen(fd.cFileName))
					{
						std::wstring fname = fd.cFileName;
						if (filenamematch.substr(0, filenamematch.rfind(L"_") + 1) == fname.substr(0, fname.rfind(L"_") + 1))
						{
							if (filenamematch.substr(filenamematch.rfind(L".") + 1) == fname.substr(fname.rfind(L".") + 1))
							{
								SYSTEMTIME systime;
								FileTimeToSystemTime(&fd.ftLastWriteTime, &systime);
								CTime ftime(systime);
								TIME_ZONE_INFORMATION tin;
								GetTimeZoneInformation(&tin);
								ftime -= CTimeSpan(0, 0, 0, tin.Bias * 60);
								CString timestr1 = ftime.Format(L"_%Y%m%d%H%M%S.");
								if (ftime > lasttime)
								{
									lasttime = ftime;
									std::wstring filepath = finddirpath + fd.cFileName;
									outfile = filepath;
								}
							}
						}
					}
				}
			}
		} while (FindNextFile(hfind, &fd));
		FindClose(hfind);
	}

	return true;
}




//filenamematch与找文件名通常格式一样
bool ReserveFileWithNum(std::wstring filepath, int maxreserve= 100)
{
	if (maxreserve< 1)return false;
	auto filedir = filepath;
	filedir = filedir.substr(0, filedir.rfind(L"\\")+1);
	auto filename = filepath;
	filename = filename.substr(filename.rfind(L"\\") + 1);
	if (filename.rfind(L".") == std::wstring::npos)return false;
	filename = filename.substr(0, filename.rfind(L"."));
	auto fileext = filepath;
	fileext = fileext.substr(fileext.rfind(L".") + 1);
	std::wstring finddirexp = filedir + filename + L"_*." + fileext;

	WIN32_FIND_DATA fd;
	HANDLE hfind = FindFirstFile(finddirexp.c_str(), &fd);
	std::map<__time64_t,std::wstring> filelistbytime;
	if (hfind != NULL && hfind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!(wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0))
			{
				if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
				{
					//std::wstring findd1 = finddirpath + fd.cFileName;
					//FindDirFileMatchAndNewest(findd1, filenamematch, outfile, lasttime, layercnt - 1);
				}
				else{
					std::wstring fname2 = fd.cFileName;
					std::wstring timestr = fname2.substr(fname2.rfind(L"_") + 1);
					if (timestr.rfind(L".") != std::wstring::npos){
						timestr = timestr.substr(0, timestr.rfind(L"."));
						if (timestr.size() == 14){
							bool ballnum = true;
							for (int i = 0; i < timestr.size(); i++){
								if (!iswdigit(timestr[i])){
									ballnum = false;
									break;
								}
							}
							if (ballnum){
								CTime ftime(_wtoi(timestr.substr(0, 4).data()), _wtoi(timestr.substr(4, 2).data()), _wtoi(timestr.substr(6, 2).data()), _wtoi(timestr.substr(8, 2).data()), _wtoi(timestr.substr(10, 2).data()), _wtoi(timestr.substr(12, 2).data()));
								filelistbytime.insert(std::make_pair(ftime.GetTime(), fname2));
							}
						}
					}
				}
			}
		} while (FindNextFile(hfind, &fd));
		FindClose(hfind);
	}
	if (filelistbytime.size() > maxreserve){
		int delcnt = filelistbytime.size()-maxreserve;
		for (auto it = filelistbytime.begin(); it != filelistbytime.end(); it++){
			if (delcnt > 0){
				DeleteFile((filedir+it->second).data());
				delcnt--;
			}
			else{
				break;
			}
		}
	}
	return true;
}





#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

	// 对话框数据
	enum { IDD = IDD_ABOUTBOX };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CFolderAutomaticBackupDlg 对话框



CFolderAutomaticBackupDlg::CFolderAutomaticBackupDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CFolderAutomaticBackupDlg::IDD, pParent)
	, cstrItemTextEdit(_T(""))
	, binitdlg(false)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CFolderAutomaticBackupDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT1, m_srcdir);
	DDX_Control(pDX, IDC_EDIT4, m_targetdir);
	DDX_Control(pDX, IDC_EDIT5, m_houzuied);
	DDX_Control(pDX, IDC_BUT_STARTSPY, m_startbtn);
	DDX_Control(pDX, IDC_BUT_STOPSPY, m_stopbtn);
	DDX_Control(pDX, IDC_EDIT3, m_filedircomprcopydefedit);
	DDX_Control(pDX, IDC_EDIT6, mexcludepathword);
	DDX_Control(pDX, IDC_STARTHIDECK, starthideck);
	DDX_Control(pDX, IDC_MAXRESERVEED, maxreserveed);
	DDX_Control(pDX, IDC_EDIT7, hotkeyed);
	DDX_Control(pDX, IDC_SHADOWMOVEOUT, shadowbtn);
	DDX_Control(pDX, IDC_MOVENOUSEOUT, nousemoveoutbtn);
}

BEGIN_MESSAGE_MAP(CFolderAutomaticBackupDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_BACKUPINFO, OnBackupInfoDbClicked)
	ON_BN_CLICKED(IDC_BUT_ADDTASK, &CFolderAutomaticBackupDlg::OnBnClickedButAddtask)
	ON_BN_CLICKED(IDC_BUT_DELTASK, &CFolderAutomaticBackupDlg::OnBnClickedButDeltask)
	ON_BN_CLICKED(IDC_BUT_STARTSPY, &CFolderAutomaticBackupDlg::OnBnClickedButStartspy)
	ON_BN_CLICKED(IDC_BUT_STOPSPY, &CFolderAutomaticBackupDlg::OnBnClickedButStopspy)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_BUT_HIDE, &CFolderAutomaticBackupDlg::OnBnClickedButHide)
	ON_MESSAGE(WM_HOTKEY, OnHotKey)
	ON_BN_CLICKED(IDC_STARTHIDECK, &CFolderAutomaticBackupDlg::OnBnClickedStarthideck)
	ON_EN_CHANGE(IDC_MAXRESERVEED, &CFolderAutomaticBackupDlg::OnEnChangeMaxreserveed)
	ON_EN_CHANGE(IDC_EDIT7, &CFolderAutomaticBackupDlg::OnEnChangeEdit7)
	ON_EN_CHANGE(IDC_EDIT6, &CFolderAutomaticBackupDlg::OnEnChangeEdit6)
	ON_WM_DEVICECHANGE()
	ON_BN_CLICKED(IDC_SHADOWMOVEOUT, &CFolderAutomaticBackupDlg::OnBnClickedShadowmoveout)
	ON_BN_CLICKED(IDC_MOVENOUSEOUT, &CFolderAutomaticBackupDlg::OnBnClickedMovenouseout)
	ON_EN_CHANGE(IDC_EDIT1, &CFolderAutomaticBackupDlg::OnEnChangeEdit1)
	ON_EN_CHANGE(IDC_EDIT4, &CFolderAutomaticBackupDlg::OnEnChangeEdit4)
	ON_EN_CHANGE(IDC_EDIT5, &CFolderAutomaticBackupDlg::OnEnChangeEdit5)
END_MESSAGE_MAP()


// CFolderAutomaticBackupDlg 消息处理程序

BOOL CFolderAutomaticBackupDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO:  在此添加额外的初始化代码
	m_filedircomprcopydefedit.SetLimitText(300 * 1024 * 1024);

	TCHAR BufStr[1024] = { 0 };
	GetModuleFileName(NULL, BufStr, 1024);
	wcsrchr(BufStr, L'\\')[1] = 0;
	SetCurrentDirectory(BufStr);

	DoLoad();

	OnBnClickedStarthideckState=0;

	if (g_srcdir == "")g_srcdir = ".";
	m_srcdir.SetWindowText(u8tow(g_srcdir).c_str());
	if (g_targetdir == "")g_targetdir = "auto";
	m_targetdir.SetWindowText(u8tow(g_targetdir).c_str());
	if (g_monexts == "")g_monexts = "rc2;ico;bmp;jpg;png;gif;tiff;user;sln;dsw;vcproj;go;pro;ai;conf;w;m;htm;html;js;cdr;ai;psd;cpp;h;c;java;cc;hpp;txt;ui;qrc;vcxproj;filters;rc;rcs;py;bat;vbs;js;php;reg;cfg;";
	m_houzuied.SetWindowText(u8tow(g_monexts).c_str());
	m_filedircomprcopydefedit.SetWindowText(u8tow(g_autocompcopy).c_str());

	UpdateData(FALSE);

	if (g_hotkey == "")g_hotkey = "B";

	::RegisterHotKey(GetSafeHwnd(), WM_HOTKEY, MOD_ALT | MOD_CONTROL | MOD_SHIFT, toupper(g_hotkey.data()[0]));


	m_Mytip.Create(this);
	m_Mytip.SetMaxTipWidth(500);
	m_Mytip.AddTool(GetDlgItem(IDC_EDIT3),
		L"格式：源文件或目录|目标文件或目录|文件包含词(;分开)|文件排除词(;分开)|备份个数(不写1个)\n都是目录,目标是目录,排除法获得文件，目录到目录复制\n源是目录,目标是压缩到路径，排除法获得文件，压缩到目标目录,压缩路径支持7z与zip后缀\n源是文件,目标是文件，后缀不一样就压缩，一样就复制\n源是文件,目标是目录，复制到目录");
	m_Mytip.SetDelayTime(200); //设置延迟
	m_Mytip.SetDelayTime(TTDT_AUTOPOP, 12000);
	m_Mytip.SetTipTextColor(RGB(0, 0, 255)); //设置提示文本的颜色
	m_Mytip.SetTipBkColor(RGB(255, 255, 255)); //设置提示框的背景颜色
	m_Mytip.Activate(TRUE); //设置是否启用提示


	if (g_starthide == "1")
		starthideck.SetCheck(BST_CHECKED);
	if (g_maxreservenum == "")g_maxreservenum = "360";
	maxreserveed.SetWindowText(u8tow(g_maxreservenum).data());
	
	hotkeyed.SetWindowText(u8tow(g_hotkey).c_str());
	mexcludepathword.SetWindowText(u8tow(g_mexcludepathword).data());

	//auto hand = CreateThread(NULL, 524288, ChangeAutoCompressOrCopyThread, NULL, 0, NULL);
	devicechangestarttime = time(NULL);

	SetTimer(1, 10, NULL);

	if (g_show_dlg == 0)
	{

		OnBnClickedButStartspy();
	}

	binitdlg=true;
	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CFolderAutomaticBackupDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CFolderAutomaticBackupDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CFolderAutomaticBackupDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

//处理多列编辑开始
void CFolderAutomaticBackupDlg::OnBackupInfoDbClicked(NMHDR* pNMHDR, LRESULT* pResult)
{
	//LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<NMITEMACTIVATE>(pNMHDR);   
	// TODO: 在此添加控件通知处理程序代码   
	NM_LISTVIEW *pNMListCtrl = (NM_LISTVIEW *)pNMHDR;
	if (pNMListCtrl->iItem != -1)
	{
		if (pNMListCtrl->iSubItem < 3 || pNMListCtrl->iSubItem == 4 || pNMListCtrl->iSubItem == 5 || pNMListCtrl->iSubItem == 6 || pNMListCtrl->iSubItem == 7)
		{
			CRect rect, dlgRect;
			//获得当前列的宽度以适应如果用户调整宽度    
			//此处不用获得的子项rect矩形框来设置宽度   
			//是因第一列时返回的宽度是整行的宽度，我也不知道为啥   
			int width = m_backuptasklist.GetColumnWidth(pNMListCtrl->iSubItem);
			m_backuptasklist.GetSubItemRect(pNMListCtrl->iItem, pNMListCtrl->iSubItem, LVIR_BOUNDS, rect);
			//保存选择的列表项索引   
			//这个因为我是用了两个列表控件一个CEdit   
			//所以需要保存列表的索引   
			//以及子项相对应的行列号索引    
			listSelFlag[0] = 1;//列表1   
			listSelFlag[1] = pNMListCtrl->iItem;
			listSelFlag[2] = pNMListCtrl->iSubItem;
			//获得listctrl矩形框    
			//获得父对话框的位置以调整CEdit的显示位置，具体见下面代码    
			m_backuptasklist.GetWindowRect(&dlgRect);


			//调整与父窗口对应   
			ScreenToClient(&dlgRect);
			int height = rect.Height();
			rect.top += (dlgRect.top + 1);
			rect.left += (dlgRect.left + 1);
			rect.bottom = rect.top + height + 2;
			rect.right = rect.left + width + 2;
			int selindex = m_backuptasklist.GetSelectionMark();
			CString ItemText = m_backuptasklist.GetItemText(selindex, pNMListCtrl->iSubItem);
			m_SubItemEdit.SetWindowText(ItemText);
			m_SubItemEdit.MoveWindow(&rect);
			m_SubItemEdit.ShowWindow(SW_SHOW);
			m_SubItemEdit.SetFocus();
		}
		else{
			{
				CString ItemText = m_backuptasklist.GetItemText(pNMListCtrl->iItem, pNMListCtrl->iSubItem);
				if (ItemText == L"是")
				{
					m_backuptasklist.SetItemText(pNMListCtrl->iItem, pNMListCtrl->iSubItem, L"否");
				}
				else{
					m_backuptasklist.SetItemText(pNMListCtrl->iItem, pNMListCtrl->iSubItem, L"是");
				}
				DoSave();
			}
		}
	}
	//*pResult = 0;   

}

void CFolderAutomaticBackupDlg::SetListItemText(void)
{
	UpdateData(TRUE);
	//AfxMessageBox(cstrItemTextEdit);  
	if (listSelFlag[0] == 2)//出售列表  
	{
		//此处的cstrItemTextEdit是CEdit控件的字符串关联变量  
		m_backuptasklist.SetItemText(listSelFlag[1], listSelFlag[2], cstrItemTextEdit);
		//重置编辑框文本  
		m_SubItemEdit.SetWindowText(L"");
		//隐藏编辑框  
		m_SubItemEdit.ShowWindow(SW_HIDE);
		DoSave();
	}
	if (listSelFlag[0] == 1)//购买列表  
	{
		m_backuptasklist.SetItemText(listSelFlag[1], listSelFlag[2], cstrItemTextEdit);
		m_SubItemEdit.SetWindowText(L"");
		m_SubItemEdit.ShowWindow(SW_HIDE);
		DoSave();
	}
	//强制刷新列表控件（否则视觉上有感觉有点不爽，可以试试^_^）  
	m_backuptasklist.Invalidate();
}

void CFolderAutomaticBackupDlg::OnEnKillfocusEdit2()
{
	// TODO: 在此添加控件通知处理程序代码
	SetListItemText();
}


BOOL CFolderAutomaticBackupDlg::PreTranslateMessage(MSG* pMsg)
{
	// TODO: 在此添加专用代码和/或调用基类    
	if (pMsg->message == WM_MOUSEMOVE)
		m_Mytip.RelayEvent(pMsg);

	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
	{
		if (GetFocus()->GetDlgCtrlID() == IDC_EDIT3)
		{
			return CDialog::PreTranslateMessage(pMsg);
			//if (listSelFlag[0] == 1)
			//	m_backuptasklist.SetFocus();//使列表控件获得焦点，则CEdit会自动失去焦点，触发EN_KILLFOCUS消息   
			//if (listSelFlag[0] == 2)
			//	m_backuptasklist.SetFocus();
		}
		return TRUE;
	}
	return CDialog::PreTranslateMessage(pMsg);
}

//处理多列编辑结束



void CFolderAutomaticBackupDlg::OnBnClickedButAddtask()
{
	// TODO:  在此添加控件通知处理程序代码
	if (m_backuptasklist.GetItemCount() > 100)return;
	int itemid = m_backuptasklist.InsertItem(m_backuptasklist.GetItemCount(), L"", -1);
	m_backuptasklist.SetItemText(itemid, 3, L"是");
	m_backuptasklist.SetItemText(itemid, 4, L"(del /S index.lock;git add -A;git commit -am \"commit-$time\";git push origin master)");
	m_backuptasklist.SetItemText(itemid, 5, L"*.h;*.cc;*.sln;*.vcproj;*.filter;*.rc;*.cpp;*.c;vcxproj;*.qrc;*.ui;*.txt;*.hpp;*.cfg;*.ini;");
	m_backuptasklist.SetItemText(itemid, 6, L"10");
	m_backuptasklist.SetItemText(itemid, 7, L"20");
	m_backuptasklist.SetItemText(itemid, 8, L"是");
	m_backuptasklist.SetItemText(itemid, 9, L"是");
	m_backuptasklist.SetItemText(itemid, 10, L"是");
	m_backuptasklist.SetItemText(itemid, 11, L"是");
}


void CFolderAutomaticBackupDlg::OnBnClickedButDeltask()
{
	// TODO:  在此添加控件通知处理程序代码
	int selindex = m_backuptasklist.GetSelectionMark();
	m_backuptasklist.DeleteItem(selindex);
	DoSave();
}


void CFolderAutomaticBackupDlg::DoSave()
{

}

void CFolderAutomaticBackupDlg::DoLoad()
{
}



int splitString(CString str, CString split, CStringArray& strArray)
{
	strArray.RemoveAll();

	CString strTemp = str; //此赋值不能少 
	int nIndex = 0; // 
	while (1)
	{
		nIndex = strTemp.Find(split);
		if (nIndex >= 0)
		{
			CString addstr = strTemp.Left(nIndex);
			if (!addstr.IsEmpty())strArray.Add(addstr);
			strTemp = strTemp.Right(strTemp.GetLength() - nIndex - split.GetLength());
		}
		else break;
	}
	if (!strTemp.IsEmpty())strArray.Add(strTemp);
	return strArray.GetSize();
}

void RemoveDir(wstring dir) {
	if (dir[dir.length() - 1] != '\\') {
		dir += L"\\";
	}
	WIN32_FIND_DATA fd;
	auto fh = FindFirstFile((dir + L"*").data(), &fd);
	if (fh != INVALID_HANDLE_VALUE) {
		do {
			if (!(wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)) {
				if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
					RemoveDir((dir + fd.cFileName).data());
				}
				else {
					DeleteFile((dir + fd.cFileName).data());
				}
			}
		} while (FindNextFile(fh, &fd));
		FindClose(fh);
		RemoveDirectory(dir.data());
	}
}



int maxreservenum = 360;
std::deque<std::deque<std::wstring>> excludeworddeq;
bool MakeSameFindFile(std::wstring finddirpath, MonitorDirs *backupinfo,char *srcbuf)
{
	if (finddirpath[finddirpath.size() - 1] != L'\\')
	{
		finddirpath = finddirpath + L"\\";
	}


	if (backupinfo->DirTarget == "auto"){
		auto sourcedirname = backupinfo->DirSource;
		while (sourcedirname.Right(1) == L"\\")sourcedirname = sourcedirname.Mid(0, sourcedirname.GetLength() - 1);
		if (sourcedirname.ReverseFind(L'\\') != -1){
			sourcedirname = sourcedirname.Mid(sourcedirname.ReverseFind(L'\\') + 1);
		}
		auto srcpath = sourcedirname + L"_backup\\";
		if (srcpath[1] != ':'){
			srcpath = L"C:\\" + srcpath;
		}
		CString tdir;
		wchar_t drivs[80];
		GetLogicalDriveStrings(80,drivs);
		wchar_t *pdrivs = drivs;
		while (*pdrivs){
			tdir = srcpath;
			tdir.SetAt(0, *pdrivs);
			if (PathFileExists(tdir.GetBuffer())){
				auto targetdirpath = tdir + finddirpath.substr(backupinfo->DirSource.GetLength()).data();
				if(PathFileExists(targetdirpath.GetBuffer())==FALSE){
					CreateDirectory(targetdirpath.GetBuffer(),NULL);
				}
			}
			pdrivs += wcslen(pdrivs) + 1;
		}
	}else{
		auto targetdirpath = backupinfo->DirTarget + finddirpath.substr(backupinfo->DirSource.GetLength()).data();
		if(PathFileExists(targetdirpath.GetBuffer())==FALSE){
			CreateDirectory(targetdirpath.GetBuffer(),NULL);
		}
	}

	std::wstring finddirexp = finddirpath + L"*.*";

	WIN32_FIND_DATA fd;
	HANDLE hfind = FindFirstFile(finddirexp.c_str(), &fd);
	if (hfind != NULL)
	{
		do
		{
			if (!(wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0))
			{
				if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
				{
					std::wstring findd1 = finddirpath + fd.cFileName;
					MakeSameFindFile(findd1, backupinfo, srcbuf);
				}
				else{
					//_20181225162348.
					auto lastdot=wcsrchr(fd.cFileName,L'.');

					bool bbackupfile=false;
					if(lastdot!=nullptr
						&& lastdot-fd.cFileName>=15
						&& (lastdot-15)[0]=='_'
						&& isdigit((lastdot-14)[0])
						&& isdigit((lastdot-13)[0])
						&& isdigit((lastdot-12)[0])
						&& isdigit((lastdot-11)[0])
						&& isdigit((lastdot-10)[0])
						&& isdigit((lastdot-9)[0])
						&& isdigit((lastdot-8)[0])
						&& isdigit((lastdot-7)[0])
						&& isdigit((lastdot-6)[0])
						&& isdigit((lastdot-5)[0])
						&& isdigit((lastdot-4)[0])
						&& isdigit((lastdot-3)[0])
						&& isdigit((lastdot-2)[0])
						&& isdigit((lastdot-1)[0])
						&& (lastdot-0)[0]=='.'
						){
							bbackupfile=true;
					}
					if(bbackupfile==false){
						std::wstring srcfilepath = finddirpath + fd.cFileName;

						bool bfexclude = false;
						for (int ewi = 0; ewi < excludeworddeq.size(); ewi++)
						{
							int estartpos = 0;
							bool ballma = true;
							for (int ej = 0; ej < excludeworddeq[ewi].size(); ej++){
								auto efndrl = srcfilepath.find(excludeworddeq[ewi][ej], estartpos);
								if (efndrl != std::wstring::npos){
									estartpos = efndrl + excludeworddeq[ewi][ej].size();
								}
								else{
									ballma = false;
									break;
								}
							}
							if (excludeworddeq[ewi].size() == 0)ballma = false;
							if (ballma == true){
								bfexclude = true;
								break;
							}
						}
						if (bfexclude == false){
							HANDLE hsrc = CreateFile((L"\\\\?\\" + srcfilepath).data(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
							if (hsrc == INVALID_HANDLE_VALUE){
								continue;
							}
							FILETIME srccreattime, srcwritetime;
							GetFileTime(hsrc, &srccreattime, NULL, &srcwritetime);
							DWORD srcsizehi = 0;
							DWORD srcsizelo = GetFileSize(hsrc, &srcsizehi);
							long long int filesize = srcsizelo | (srcsizehi << 32);
							std::wstring targfilepath;
							if (backupinfo->DirTarget == "auto"){
								auto sourcedirname = backupinfo->DirSource;
								while (sourcedirname.Right(1) == L"\\")sourcedirname = sourcedirname.Mid(0, sourcedirname.GetLength() - 1);
								if (sourcedirname.ReverseFind(L'\\') != -1){
									sourcedirname = sourcedirname.Mid(sourcedirname.ReverseFind(L'\\') + 1);
								}
								auto srcpath = sourcedirname + L"_backup\\";
								if (srcpath[1] != ':'){
									srcpath = L"C:\\" + srcpath;
								}
								CString tdir;
								wchar_t drivs[80];
								GetLogicalDriveStrings(80,drivs);
								wchar_t *pdrivs = drivs;
								while (*pdrivs){
									tdir = srcpath;
									tdir.SetAt(0, *pdrivs);
									if (PathFileExists(tdir.GetBuffer())){
										SetFilePointer(hsrc, 0, 0, FILE_BEGIN);
										auto targetfilepath = tdir.GetBuffer() + srcfilepath.substr(backupinfo->DirSource.GetLength());
										HANDLE htar = CreateFile((L"\\\\?\\"+targetfilepath).data(), GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
										if (htar == INVALID_HANDLE_VALUE){
											auto dirattr = GetFileAttributes((L"\\\\?\\" + targetfilepath).data());
											if (dirattr!=-1 && (dirattr&FILE_ATTRIBUTE_DIRECTORY) != 0) {
												RemoveDir((L"\\\\?\\" + targetfilepath).data());
											}
											auto targetfiledir = targetfilepath;
											targetfiledir = targetfiledir.substr(0, targetfiledir.rfind(L"\\"));
											CreateFullDir(targetfiledir.data());
											htar = CreateFile((L"\\\\?\\"+targetfilepath).data(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
											long long int curpos = 0;
											while (true){
												DWORD readcnt;
												if (curpos >= filesize){
													break;
												}
												else if (curpos + 10 * 1024 * 1024 < filesize){
													readcnt = 10 * 1024 * 1024;
												}
												else{
													readcnt = filesize - curpos;
												}
												DWORD readcnt2;
												auto readrl = ReadFile(hsrc, srcbuf, readcnt, &readcnt2, 0);
												if (readrl != TRUE || readcnt != readcnt2){
													LBLOGD(ERROR << "read error:" << srcfilepath);
													break;
												}
												else{
													DWORD writecnt;
													auto writerl = WriteFile(htar, srcbuf, readcnt, &writecnt, 0);
													if (writerl == FALSE || writecnt != readcnt){
														LBLOGD(ERROR << "write error:" << targetfilepath);
														break;
													}
												}
												curpos += readcnt;
											}
											SetFileTime(htar, &srccreattime, 0, &srcwritetime);
											CloseHandle(htar);
										}
										else{
											FILETIME tarcreattime, tarwritetime;
											GetFileTime(htar, &tarcreattime, NULL, &tarwritetime);
											DWORD tarsizehi = 0;
											DWORD tarsizelo = GetFileSize(htar, &tarsizehi);
											if (memcmp(&srccreattime, &tarcreattime, sizeof(FILETIME)) != 0 || memcmp(&srcwritetime, &tarwritetime, sizeof(FILETIME)) != 0 || !(srcsizelo == tarsizelo && srcsizehi == tarsizehi)){
												if (targetfilepath.rfind(L".") != std::wstring::npos && targetfilepath.rfind(L".")>targetfilepath.rfind(L"\\")){
													CTime curtim = CTime::GetCurrentTime();
													CString timestr = curtim.Format(L"_%Y%m%d%H%M%S.");
													std::wstring makesmaebakfilepath = targetfilepath.substr(0, targetfilepath.rfind(L".")) + timestr.GetBuffer() + targetfilepath.substr(targetfilepath.rfind(L".")+1);
													CopyFile((L"\\\\?\\"+targetfilepath).data(), (L"\\\\?\\" + makesmaebakfilepath).data(), FALSE);
												}

												LONG srcsizehi2 = (LONG)srcsizehi;
												SetFilePointer(htar, srcsizelo, &srcsizehi2, FILE_BEGIN);
												SetEndOfFile(htar);
												SetFilePointer(htar, 0, NULL, FILE_BEGIN);
												SetFilePointer(hsrc, 0, 0, FILE_BEGIN);
												long long int curpos = 0;
												while (true){
													DWORD readcnt;
													if (curpos >= filesize){
														break;
													}
													else if (curpos + 10 * 1024 * 1024 < filesize){
														readcnt = 10 * 1024 * 1024;
													}
													else{
														readcnt = filesize - curpos;
													}
													DWORD readcnt2;
													auto readrl = ReadFile(hsrc, srcbuf, readcnt, &readcnt2, 0);
													if (readrl != TRUE || readcnt != readcnt2){
														LBLOGD(ERROR << "read error:" << srcfilepath);
														break;
													}
													else{
														DWORD writecnt;
														auto writerl = WriteFile(htar, srcbuf, readcnt, &writecnt, 0);
														if (writerl == FALSE || writecnt != readcnt){
															LBLOGD(ERROR << "write error:" << targetfilepath);
															break;
														}
													}
													curpos += readcnt;
												}
												SetFileTime(htar, &srccreattime, 0, &srcwritetime);
											}
											CloseHandle(htar);
										}
										ReserveFileWithNum(targetfilepath, maxreservenum);
									}
									pdrivs += wcslen(pdrivs) + 1;
								}
							}
							else{
								auto targetfilepath = backupinfo->DirTarget + srcfilepath.substr(backupinfo->DirSource.GetLength()).data();
								HANDLE htar = CreateFile(L"\\\\?\\"+targetfilepath, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
								if (htar == INVALID_HANDLE_VALUE){
									htar = CreateFile(L"\\\\?\\" + targetfilepath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
									long long int curpos = 0;
									while (true){
										DWORD readcnt;
										if (curpos >= filesize){
											break;
										}
										else if (curpos + 10 * 1024 * 1024 < filesize){
											readcnt = 10 * 1024 * 1024;
										}
										else{
											readcnt = filesize - curpos;
										}
										DWORD readcnt2;
										auto readrl = ReadFile(hsrc, srcbuf, readcnt, &readcnt2, 0);
										if (readrl != TRUE || readcnt != readcnt2){
											LBLOGD(ERROR << "read error:" << srcfilepath);
											break;
										}
										else{
											DWORD writecnt;
											auto writerl = WriteFile(htar, srcbuf, readcnt, &writecnt, 0);
											if (writerl == FALSE || writecnt != readcnt){
												LBLOGD(ERROR << "write error:" << targetfilepath);
												break;
											}
										}
										curpos += readcnt;
									}
									SetFileTime(htar, &srccreattime, 0, &srcwritetime);
									CloseHandle(htar);
								}
								else{
									FILETIME tarcreattime, tarwritetime;
									GetFileTime(htar, &tarcreattime, NULL, &tarwritetime);
									DWORD tarsizehi = 0;
									DWORD tarsizelo = GetFileSize(htar, &tarsizehi);
									if (memcmp(&srccreattime, &tarcreattime, sizeof(FILETIME)) != 0 || memcmp(&srcwritetime, &tarwritetime, sizeof(FILETIME)) != 0 || !(srcsizelo == tarsizelo && srcsizehi == tarsizehi)){
										if (targetfilepath.ReverseFind(L'.') != -1 && targetfilepath.ReverseFind(L'.')>targetfilepath.ReverseFind(L'\\')){
											CTime curtim = CTime::GetCurrentTime();
											CString timestr = curtim.Format(L"_%Y%m%d%H%M%S.");
											std::wstring makesmaebakfilepath = targetfilepath.Mid(0, targetfilepath.ReverseFind(L'.')) + timestr.GetBuffer() + targetfilepath.Mid(targetfilepath.ReverseFind(L'.') + 1);
											CopyFile(targetfilepath, makesmaebakfilepath.data(), FALSE);
										}
										LONG srcsizehi2 = (LONG)srcsizehi;
										SetFilePointer(htar, srcsizelo, &srcsizehi2, FILE_BEGIN);
										SetEndOfFile(htar);
										SetFilePointer(htar, 0, NULL, FILE_BEGIN);
										long long int curpos = 0;
										while (true){
											DWORD readcnt;
											if (curpos >= filesize){
												break;
											}
											else if (curpos + 10 * 1024 * 1024 < filesize){
												readcnt = 10 * 1024 * 1024;
											}
											else{
												readcnt = filesize - curpos;
											}
											DWORD readcnt2;
											auto readrl = ReadFile(hsrc, srcbuf, readcnt, &readcnt2, 0);
											if (readrl != TRUE || readcnt != readcnt2){
												LBLOGD(ERROR << "read error:" << srcfilepath);
												break;
											}
											else{
												DWORD writecnt;
												auto writerl = WriteFile(htar, srcbuf, readcnt, &writecnt, 0);
												if (writerl == FALSE || writecnt != readcnt){
													LBLOGD(ERROR << "write error:" << targetfilepath);
													break;
												}
											}
											curpos += readcnt;
										}
										SetFileTime(htar, &srccreattime, 0, &srcwritetime);
									}
									CloseHandle(htar);
								}
								ReserveFileWithNum(targetfilepath.GetBuffer(), maxreservenum);
							}
							CloseHandle(hsrc);
						}
					}
				}
			}
		} while (FindNextFile(hfind, &fd));
		FindClose(hfind);
	}
	

	return true;
}





DWORD  WINAPI  MakeSameThread(LPVOID param)
{
	MonitorDirs *backupinfo = (MonitorDirs *)param;
	if (maxreservenum < 1)maxreservenum = 360;
	if (backupinfo->DirSource == L"."){
		wchar_t apppath[1024] = { 0 };
		GetModuleFileName(NULL, apppath, 1024);
		wcsrchr(apppath, '\\')[1] = 0;
		backupinfo->DirSource = apppath;
	}
	if (backupinfo->DirSource.GetLength()>0 && backupinfo->DirSource.Right(1) != '\\')
		backupinfo->DirSource += L"\\";


	if (backupinfo->DirTarget == L""){
		backupinfo->DirTarget = L"auto";
	}
	if (backupinfo->DirTarget != L"auto" && backupinfo->DirTarget.Right(1) != '\\')
		backupinfo->DirTarget += L"\\";

	auto srcbuf = new char[10 * 1024 * 1024];
	bool rl = MakeSameFindFile(backupinfo->DirSource.GetBuffer(), backupinfo, srcbuf);
	delete[]srcbuf;

	return 0;
}


bool CopyFileWithTime(std::wstring srcfilepath, std::wstring targetfilepath){
	HANDLE hsrc = CreateFile(srcfilepath.data(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (hsrc == INVALID_HANDLE_VALUE){
		Sleep(10);
		hsrc = CreateFile(srcfilepath.data(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		if (hsrc == INVALID_HANDLE_VALUE){
			return false;
		}
	}
	FILETIME srccreattime, srcwritetime;
	GetFileTime(hsrc, &srccreattime, NULL, &srcwritetime);
	DWORD srcsizehi = 0;
	DWORD srcsizelo = GetFileSize(hsrc, &srcsizehi);
	long long int filesize = srcsizelo | (srcsizehi << 32);
	auto srcbuf = new char[10 * 1024 * 1024];
	HANDLE htar = CreateFile(targetfilepath.data(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (htar == INVALID_HANDLE_VALUE){
		auto targetfiledir = targetfilepath;
		targetfiledir = targetfiledir.substr(0, targetfiledir.rfind(L"\\"));
		if (!PathFileExists(targetfiledir.data()))
			CreateFullDir(targetfiledir.data());
		htar = CreateFile(targetfilepath.data(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
		if (htar != INVALID_HANDLE_VALUE){
			long long int curpos = 0;
			while (true){
				DWORD readcnt;
				if (curpos >= filesize){
					break;
				}
				else if (curpos + 10 * 1024 * 1024 < filesize){
					readcnt = 10 * 1024 * 1024;
				}
				else{
					readcnt = filesize - curpos;
				}
				DWORD readcnt2;
				auto readrl = ReadFile(hsrc, srcbuf, readcnt, &readcnt2, 0);
				if (readrl != TRUE || readcnt != readcnt2){
					LBLOGD(ERROR << "read error:" << srcfilepath);
					break;
				}
				else{
					DWORD writecnt;
					auto writerl = WriteFile(htar, srcbuf, readcnt, &writecnt, 0);
					if (writerl == FALSE || writecnt != readcnt){
						LBLOGD(ERROR << "write error:" << targetfilepath);
						break;
					}
				}
				curpos += readcnt;
			}
			SetFileTime(htar, &srccreattime, 0, &srcwritetime);
			CloseHandle(htar);
		}
		else{
			LBLOGD(ERROR << "create file error:" << targetfilepath);
		}
	}
	else{
		FILETIME tarcreattime, tarwritetime;
		GetFileTime(htar, &srccreattime, NULL, &srcwritetime);
		DWORD tarsizehi = 0;
		DWORD tarsizelo = GetFileSize(htar, &tarsizehi);
		if (memcmp(&srccreattime, &tarcreattime, sizeof(FILETIME)) != 0 || memcmp(&srcwritetime, &tarwritetime, sizeof(FILETIME)) != 0 || !(srcsizelo == tarsizelo && srcsizehi == tarsizehi)){
			LONG srcsizehi2 = (LONG)srcsizehi;
			SetFilePointer(htar, srcsizelo, &srcsizehi2, FILE_BEGIN);
			SetEndOfFile(htar);
			SetFilePointer(htar, 0, NULL, FILE_BEGIN);
			SetFilePointer(hsrc, 0, 0, FILE_BEGIN);
			long long int curpos = 0;
			while (true){
				DWORD readcnt;
				if (curpos >= filesize){
					break;
				}
				else if (curpos + 10 * 1024 * 1024 < filesize){
					readcnt = 10 * 1024 * 1024;
				}
				else{
					readcnt = filesize - curpos;
				}
				DWORD readcnt2;
				auto readrl = ReadFile(hsrc, srcbuf, readcnt, &readcnt2, 0);
				if (readrl != TRUE || readcnt != readcnt2){
					LBLOGD(ERROR << "read error:" << srcfilepath);
					break;
				}
				else{
					DWORD writecnt;
					auto writerl = WriteFile(htar, srcbuf, readcnt, &writecnt, 0);
					if (writerl == FALSE || writecnt != readcnt){
						LBLOGD(ERROR << "write error:" << targetfilepath);
						break;
					}
				}
				curpos += readcnt;
			}
			SetFileTime(htar, &srccreattime, 0, &srcwritetime);
		}
		CloseHandle(htar);
	}
	delete[]srcbuf;
	CloseHandle(hsrc);
	return true;
}


DWORD  WINAPI  DoCopyThread(LPVOID param)
{
	MonitorDirs *backupinfo = (MonitorDirs *)param;
	while (true){
		std::wstring file;
		backupinfo->copyfilesm.Lock();
		if (backupinfo->copyfiles.size() > 0){
			file = *backupinfo->copyfiles.begin(); 
			backupinfo->copyfiles.erase(backupinfo->copyfiles.begin());
		}
		backupinfo->copyfilesm.Unlock();
		if (file.size() > 0 && !PathIsDirectory(file.data())){
			//+		FileName	0x02ae0360 L"PythonProjects\\pyext\\urlzhuaqu\\testpyd\\testpyd.cpp~RFd9159b.TMP"	wchar_t[1]
			
			std::wstring realsrcfile = file;
			auto filerelapth=file.substr(backupinfo->DirSource.GetLength());
			std::wstring srcext = realsrcfile;
			if (srcext.rfind(L"\\") != std::wstring::npos && srcext.rfind(L".") != std::wstring::npos && srcext.rfind(L".") > srcext.rfind(L"\\"))
			{
				srcext = srcext.substr(srcext.rfind(L".")+1);
			}
			else{
				srcext = L"";
			}

			bool bfexclude = false;
			for (int ewi = 0; ewi < excludeworddeq.size(); ewi++)
			{
				int estartpos = 0;
				bool ballma = true;
				for (int ej = 0; ej < excludeworddeq[ewi].size(); ej++){
					auto efndrl = realsrcfile.find(excludeworddeq[ewi][ej], estartpos);
					if (efndrl != std::wstring::npos){
						estartpos = efndrl + excludeworddeq[ewi][ej].size();
					}
					else{
						ballma = false;
						break;
					}
				}
				if (excludeworddeq[ewi].size() == 0)ballma = false;
				if (ballma == true){
					bfexclude = true;
					break;
				}
			}

			if (bfexclude == false && !(srcext == L"temp" || srcext == L"tmp" || srcext == L"") && backupinfo->MonExts.Find(srcext.c_str()) != -1)
			{
				bool bfexclude = false;
				//if (!(GetFileAttributes(realsrcfile.c_str())&FILE_ATTRIBUTE_DIRECTORY))
				if (bfexclude == false && !srcext.empty())
				{
					CTime curtim = CTime::GetCurrentTime();
					CString timestr = curtim.Format(L"_%Y%m%d%H%M%S.");
					std::wstring targfilepath;
					if (backupinfo->DirTarget == "auto"){
						auto sourcedirname = backupinfo->DirSource;
						while (sourcedirname.Right(1) == L"\\")sourcedirname = sourcedirname.Mid(0, sourcedirname.GetLength() - 1);
						if (sourcedirname.ReverseFind(L'\\') != -1){
							sourcedirname = sourcedirname.Mid(sourcedirname.ReverseFind(L'\\') + 1);
						}
						auto srcpath = sourcedirname + L"_backup\\";
						if (srcpath[1] != ':'){
							srcpath = L"C:\\" + srcpath;
						}
						CString tdir;
						wchar_t drivs[80];
						GetLogicalDriveStrings(80, drivs);
						wchar_t *pdrivs = drivs;
						while (*pdrivs){
							tdir = srcpath;
							tdir.SetAt(0, *pdrivs);
							if (PathFileExists(tdir.GetBuffer())){
								targfilepath = tdir.GetBuffer() + filerelapth;
								if (targfilepath != L"" && targfilepath.rfind(L".") > targfilepath.rfind(L"\\"))
								{
									std::wstring filedir;
									filedir = targfilepath;// backupinfo->DirTarget.GetBuffer() + file;
									filedir = filedir.substr(0, filedir.rfind(L"\\"));
									if (!PathFileExists(filedir.c_str()))
									{
										CreateFullDir(filedir.data());
									}

									std::wstring realbakfilepath = targfilepath;
									auto previousfilepathnewpath = targfilepath;
									previousfilepathnewpath.replace(previousfilepathnewpath.rfind(L"."), 1, timestr.GetBuffer());
									MoveFileEx(realbakfilepath.data(), previousfilepathnewpath.data(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);

									CopyFileWithTime(realsrcfile, targfilepath);
								}
							}
							pdrivs += wcslen(pdrivs) + 1;
						}
					}
					else{
						targfilepath = backupinfo->DirTarget.GetBuffer() + filerelapth;
						if (targfilepath != L"" && targfilepath.rfind(L".") > targfilepath.rfind(L"\\"))
						{
							std::wstring filedir;
							filedir = targfilepath;// backupinfo->DirTarget.GetBuffer() + file;
							filedir = filedir.substr(0, filedir.rfind(L"\\"));
							if (!PathFileExists(filedir.c_str()))
							{
								CreateFullDir(filedir.data());
							}

							std::wstring realbakfilepath = targfilepath;
							auto previousfilepathnewpath = targfilepath;
							previousfilepathnewpath.replace(previousfilepathnewpath.rfind(L"."), 1, timestr.GetBuffer());
							MoveFile(realbakfilepath.data(), previousfilepathnewpath.data());

							CopyFileWithTime(realsrcfile, targfilepath);

						}
					}
				}
			}
		}
		else{
			Sleep(1000);
		}
	}
}

std::deque<HANDLE> docopythrdls;
DWORD  WINAPI  DoTaskThread(LPVOID param)
{
	MonitorDirs *backupinfo = (MonitorDirs *)param;
	auto hand3 = CreateThread(NULL, 524288, DoCopyThread, backupinfo, 0, NULL);
	docopythrdls.push_back(hand3);
	if (backupinfo->DirSource == L"."){
		wchar_t apppath[1024] = { 0 };
		GetModuleFileName(NULL, apppath, 1024);
		wcsrchr(apppath, '\\')[1] = 0;
		backupinfo->DirSource = apppath;
	}
	if (backupinfo->DirSource.GetLength()>0 && backupinfo->DirSource.Right(1) != '\\')
		backupinfo->DirSource += L"\\";


	if (backupinfo->DirTarget == L""){
		backupinfo->DirTarget = L"auto";
	}
	if (backupinfo->DirTarget != L"auto" && backupinfo->DirTarget.Right(1) != '\\')
		backupinfo->DirTarget += L"\\";

	std::deque<std::wstring> extsdeq;
	splitString(backupinfo->Mon_Exts.GetBuffer(), L";", extsdeq);

	/*
	std::deque<std::deque<std::wstring>> excludeworddeq;
	std::deque<std::wstring> excludeworddeq2;
	splitString(backupinfo->mexcludepathword.GetBuffer(), L";", excludeworddeq2);
	for (int i = 0; i < excludeworddeq2.size(); i++){
		excludeworddeq.push_back(std::deque<std::wstring>());
	    splitString(excludeworddeq2[i], L"*", excludeworddeq.back());
	}
	excludeworddeq2.clear();
	*/

	HANDLE hEvent;//监控句柄
	DWORD MoType = 0;

	if (!PathFileExists(backupinfo->DirSource))return 0;


	//MoType |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
	//MoType |= FILE_NOTIFY_CHANGE_SECURITY;
	MoType |= FILE_NOTIFY_CHANGE_CREATION;
	MoType |= FILE_NOTIFY_CHANGE_FILE_NAME;
	MoType |= FILE_NOTIFY_CHANGE_SIZE;
	MoType |= FILE_NOTIFY_CHANGE_LAST_WRITE;
	MoType |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
	MoType |= FILE_NOTIFY_CHANGE_DIR_NAME;



	hEvent = FindFirstChangeNotification(backupinfo->DirSource, TRUE, MoType);//查看指定目录下文件属性的改变 

	if (hEvent == NULL || hEvent == INVALID_HANDLE_VALUE)
	{
		ExitProcess(GetLastError());//获取错误
		return 0;
	}
	int MaxBakupNum = wcstol(backupinfo->MaxBakupNum.GetBuffer(), NULL, 10);
	int MinIntervalBackup = wcstol(backupinfo->MinIntervalBackup.GetBuffer(), NULL, 10);
	time_t lastbackup = time(nullptr) - MinIntervalBackup;
	char *buf = new char[1024000];

	DWORD dwBufWrittenSize;
	HANDLE hDir;
	if (backupinfo->MonExts.GetLength()>0 && backupinfo->MonExts[backupinfo->MonExts.GetLength() - 1] != ';')backupinfo->MonExts += L";";
	if (backupinfo->DirSource[backupinfo->DirSource.GetLength() - 1] != '\\')backupinfo->DirSource += L"\\";
	hDir = CreateFile(backupinfo->DirSource, FILE_LIST_DIRECTORY, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (hDir == INVALID_HANDLE_VALUE || hDir == NULL)
	{
		DWORD dwErrorCode;
		dwErrorCode = GetLastError();
		CloseHandle(hDir);
		//exit(0);
		return 0;
	}


	char *sqerr;
	sqlite3 *flsdb = 0;
	auto sqopenrl = sqlite3_open_v2("lastmodlist.db", &flsdb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);
	auto sqcrrl = sqlite3_exec(flsdb, "create table if not exists filelastmodlist(id INTEGER primary key AUTOINCREMENT, filefullpath varchar(1024) unique, modtime TIMESTAMP)", NULL, NULL, &sqerr);
	//5 is locked
	auto tickcnt=GetTickCount();
	SYSTEMTIME loct;
	GetLocalTime(&loct);
	auto loctmsec = SystemTimeToMsec(loct);
	while (TRUE)//循环监控
	{
		//DWORD nObj = WaitForSingleObject(hEvent, INFINITE);//等待，文件夹任何动作，都返回0，顾不能知道具体动作和具体哪个文件发生了变化		if (nObj)
		CTime curtim = CTime::GetCurrentTime();
		curtim -= CTimeSpan(0, 0, 0, 1);
		backupinfo->bInTask = TRUE;
		DWORD bufrl = 0;
		memset(buf, 0, 1024000);
		BOOL br = ReadDirectoryChangesW(
			hDir,
			buf,
			1024000,
			TRUE,
			FILE_NOTIFY_CHANGE_CREATION|FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_SIZE|FILE_NOTIFY_CHANGE_LAST_WRITE|FILE_NOTIFY_CHANGE_ATTRIBUTES|FILE_NOTIFY_CHANGE_DIR_NAME,
			&bufrl,
			NULL,
			NULL
			);
		
		if (br)
		{
			GetLocalTime(&loct);
			auto loctmsec2 = SystemTimeToMsec(loct);

			char *pbuf = buf;
			FILE_NOTIFY_INFORMATION *fi = (FILE_NOTIFY_INFORMATION*)pbuf;
			std::set<std::wstring> dirmoned;
			std::set<std::wstring> fileset,dirset;
			std::set<int> onebackset;
			do
			{
				std::wstring file;
				file.assign(fi->FileName, fi->FileNameLength / sizeof(wchar_t));

				if (file.size() > 4 && file.substr(file.size() - 4) == L".TMP" && (
					file.find(L".cpp~") != std::string::npos && file.find(L".cpp~") > file.rfind(L"\\")
					|| file.find(L".cc~") != std::string::npos && file.find(L".cc~") > file.rfind(L"\\")
					|| file.find(L".h~") != std::string::npos && file.find(L".h~") > file.rfind(L"\\")
					|| file.find(L".hpp~") != std::string::npos && file.find(L".hpp~") > file.rfind(L"\\")
					|| file.find(L".c~") != std::string::npos && file.find(L".c~") > file.rfind(L"\\")
					))
				{
					file = file.substr(0, file.rfind(L"~"));
				}
				else if (file.size()>12 && file[file.size() - 11] == '.' && file[file.size() - 10] == 'c' && file[file.size() - 9] == 'p' && file[file.size() - 8] == 'p' && file[file.size() - 7] == '.' && file[file.size() - 4] == '3' && file[file.size() - 3] == '4' && file[file.size() - 2] == '4' && file[file.size() - 1] == '4'){
					file = file.substr(0, file.rfind(L"."));
				}
				else if (file.size()>10 && file[file.size() - 9] == '.' && file[file.size() - 8] == 'h' && file[file.size() - 7] == '.' && file[file.size() - 4] == '3' && file[file.size() - 3] == '4' && file[file.size() - 2] == '4' && file[file.size() - 1] == '4'){
					file = file.substr(0, file.rfind(L"."));
				}
				else if (file.size()>12 && file[file.size() - 11] == '.' && file[file.size() - 10] == 'p' && file[file.size() - 9] == 'r' && file[file.size() - 8] == 'o' && file[file.size() - 7] == '.' && file[file.size() - 4] == '3' && file[file.size() - 3] == '4' && file[file.size() - 2] == '4' && file[file.size() - 1] == '4'){
					file = file.substr(0, file.rfind(L"."));
				}
				else if (file.size()>12 && file[file.size() - 11] == '.' && file[file.size() - 10] == 'q' && file[file.size() - 9] == 'r' && file[file.size() - 8] == 'c' && file[file.size() - 7] == '.' && file[file.size() - 4] == '3' && file[file.size() - 3] == '4' && file[file.size() - 2] == '4' && file[file.size() - 1] == '4'){
					file = file.substr(0, file.rfind(L"."));
				}
				else if (file.size()>10 && file[file.size() - 9] == '.' && file[file.size() - 9] == 'u' && file[file.size() - 8] == 'i' && file[file.size() - 7] == '.' && file[file.size() - 4] == '3' && file[file.size() - 3] == '4' && file[file.size() - 2] == '4' && file[file.size() - 1] == '4'){
					file = file.substr(0, file.rfind(L"."));
				}

				backupinfo->copyfilesm.Lock();
				backupinfo->copyfiles.insert(backupinfo->DirSource.GetBuffer()+file);
				backupinfo->copyfilesm.Unlock();
				//LBLOGD(ERROR << file);
				auto filedir = backupinfo->DirSource.GetBuffer()+file;
				auto changedir=filedir.substr(0, filedir.rfind(L"\\")+1);
				filedir = changedir+L"*.*";
				if (dirset.find(filedir) == dirset.end()){
					std::wstring filereladir;
					if (file.rfind(L"\\") != std::wstring::npos){
						filereladir = file.substr(0, file.rfind(L"\\") + 1);
					}
					WIN32_FIND_DATA ff;
					HANDLE hf = FindFirstFile(filedir.data(), &ff);
					if (hf != INVALID_HANDLE_VALUE){
						do{
							if (!(wcscmp(ff.cFileName, L".") == 0 || wcscmp(ff.cFileName, L"..") == 0)){
								if (!(ff.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)){
									auto fmsec = FiletimeToMsecLocal(ff.ftLastWriteTime);
									if (fmsec > loctmsec ){//&& !PathIsDirectory((filereladir + ff.cFileName).data())){
										//LBLOGD(ERROR << filereladir + ff.cFileName);
										backupinfo->copyfilesm.Lock();
										backupinfo->copyfiles.insert(changedir + ff.cFileName);
										backupinfo->copyfilesm.Unlock();
									}
								}
							}
						} while (FindNextFile(hf, &ff));
						FindClose(hf);
					}
					dirset.insert(filedir);
				}
				

				/*
				*/

				int e = 3243;
				if (fi->NextEntryOffset != 0)
				{
					pbuf += fi->NextEntryOffset;
					fi = (FILE_NOTIFY_INFORMATION*)pbuf;
				}
				else{
					break;
				}
			} while (true);
			loctmsec = loctmsec2;
		}
		backupinfo->bInTask = FALSE;
		//继续监控
		if (FALSE == FindNextChangeNotification(hEvent))
		{
			ExitProcess(GetLastError());
			return 0;
		}
	}
	FindCloseChangeNotification(hEvent);
	CloseHandle(hDir);
	delete[]buf;

	if (flsdb)
		sqlite3_close_v2(flsdb);

	return 0;
}


int monlssize = 0;
std::deque<HANDLE> makesamethread;
DWORD  WINAPI  MonBackupDirThread(LPVOID param)
{
	Sleep(30000);
	MonitorDirs **monls = (MonitorDirs **)param;
	std::deque<std::wstring> curtargetdirls, oldtargetdirls;
	makesamethread.clear();
	while (true){
		curtargetdirls.clear();
		for (int i = 0; i < monlssize; i++){
			if (monls[i]->DirSource == L"."){
				wchar_t apppath[1024] = { 0 };
				GetModuleFileName(NULL, apppath, 1024);
				wcsrchr(apppath, '\\')[1] = 0;
				monls[i]->DirSource = apppath;
			}
			if (monls[i]->DirSource.GetLength() > 0 && monls[i]->DirSource.Right(1) != '\\')
				monls[i]->DirSource += L"\\";

			if (monls[i]->DirTarget == L""){
				monls[i]->DirTarget = L"auto";
			}
			if (monls[i]->DirTarget != L"auto" && monls[i]->DirTarget.Right(1) != '\\')
				monls[i]->DirTarget += L"\\";
			if (monls[i]->DirTarget == L"auto"){
				auto sourcedirname = monls[i]->DirSource; 
				while (sourcedirname.Right(1) == L"\\")sourcedirname = sourcedirname.Mid(0, sourcedirname.GetLength() - 1);
				if (sourcedirname.ReverseFind(L'\\') != -1){
					sourcedirname = sourcedirname.Mid(sourcedirname.ReverseFind(L'\\') + 1);
				}
				auto srcpath = sourcedirname + L"_backup\\";
				if (srcpath[1] != ':'){
					srcpath = L"C:\\" + srcpath;
				}
				CString tdir;
				wchar_t drivs[80];
				GetLogicalDriveStrings(80, drivs);
				wchar_t *pdrivs = drivs;
				while (*pdrivs){
					tdir = srcpath;
					tdir.SetAt(0, *pdrivs);
					if (PathFileExists(tdir.GetBuffer())){
						std::wstring backupdir = tdir;
						bool bfound = false;
						for (auto it2 = oldtargetdirls.begin(); it2 != oldtargetdirls.end(); it2++){
							if (*it2 == backupdir){
								bfound = true;
								break;
							}
						}  
						if (bfound == false){
							auto hand=CreateThread(NULL, 5*1024*1024, MakeSameThread, monls[i], 0, NULL);
							makesamethread.push_back(hand);
						}
						curtargetdirls.push_back(backupdir);
					}
					pdrivs += wcslen(pdrivs) + 1;
				}
			}
			else{
				std::wstring backupdir = monls[i]->DirTarget;
				bool bfound = false;
				for (auto it2 = oldtargetdirls.begin(); it2 != oldtargetdirls.end(); it2++){
					if (*it2 == backupdir){
						bfound = true;
						break;
					}
				}
				if (bfound == false){
					auto hand=CreateThread(NULL, 5 * 1024 * 1024, MakeSameThread, monls[i], 0, NULL);
					makesamethread.push_back(hand);
				}
				curtargetdirls.push_back(backupdir);
			}
		}
		oldtargetdirls = curtargetdirls;
		Sleep(30000);
	}
	return 0;
}

void CFolderAutomaticBackupDlg::OnBnClickedButStartspy()
{
	// TODO:  在此添加控件通知处理程序代码
	DoSave();

	std::deque<std::wstring> excludeworddeq2;
	CString excludewords;
	mexcludepathword.GetWindowText(excludewords);
	splitString(excludewords.GetBuffer(), L";", excludeworddeq2);
	excludeworddeq.clear();
	for (int i = excludeworddeq2.size() - 1; i >= 0; i--){
		if (excludeworddeq2[i] == L""){
			excludeworddeq2.erase(excludeworddeq2.begin() + i);
			continue;
		}
		excludeworddeq.push_back(std::deque<std::wstring>());
		splitString(excludeworddeq2[i], L"*", excludeworddeq.back());
		for (int j = excludeworddeq.back().size() - 1; j >= 0; j--){
			if (excludeworddeq.back()[j] == L""){
				excludeworddeq.back().erase(excludeworddeq.back().begin() + j);
			}
		}
	}
	excludeworddeq2.clear();


	CString reservnumstr;
	maxreserveed.GetWindowText(reservnumstr);
	maxreservenum = _wtoi(reservnumstr.GetBuffer());
	CString specbackuptext;
	m_filedircomprcopydefedit.GetWindowText(specbackuptext);
	Text2compresscopytaskana(specbackuptext);
	if(binitdlg){
		if(OnBnClickedStarthideckState!=0)IniTxtSaveSetting("autocompcopy", specbackuptext.GetBuffer());
	}
	CString SourceDirs;
	m_srcdir.GetWindowText(SourceDirs);
	if(binitdlg){
		if(OnBnClickedStarthideckState!=0)IniTxtSaveSetting("srcdir", SourceDirs.GetBuffer());
	}
	std::deque<std::wstring> srcdirdeq;
	splitString(SourceDirs.GetBuffer(), L";", srcdirdeq);
	docopythrdls.clear();
	MonitorDirs **moninfols = new MonitorDirs*[srcdirdeq.size()];
	int moninfolsi = 0;
	for (int i = 0; i < srcdirdeq.size(); i++)
	{
		if(srcdirdeq[i]==L""){
			continue;
		}
		MonitorDirs *moninfo = new MonitorDirs;
		moninfo->DirSource = srcdirdeq[i].c_str();
		m_targetdir.GetWindowText(moninfo->DirTarget);
		CString monexts;
		m_houzuied.GetWindowText(monexts);
		moninfo->MonExts = L";" + monexts + L";";
		//mexcludepathword.GetWindowText(moninfo->mexcludepathword);
		moninfo->bInTask = FALSE;

		if (i == 0 && binitdlg){
			if(OnBnClickedStarthideckState!=0)IniTxtSaveSetting("targetdir", moninfo->DirTarget.GetBuffer());
			if(OnBnClickedStarthideckState!=0)IniTxtSaveSetting("monexts", monexts.GetBuffer());
			//IniTxtSaveSetting("mexcludepathword", moninfo->mexcludepathword.GetBuffer());
			if(OnBnClickedStarthideckState!=0)IniTxtSaveSetting("genflsexts", moninfo->FileLsExts.GetBuffer());
		}

		auto hand = CreateThread(NULL, 524288, DoTaskThread, moninfo, 0, NULL);
		TaskThreadDeq.push_back(hand);
		moninfols[moninfolsi] = moninfo;
		moninfolsi += 1;
	}
	monlssize = moninfolsi;
	auto hand3 = CreateThread(NULL, 524288, MonBackupDirThread, moninfols, 0, NULL);
	TaskThreadDeq.push_back(hand3);

	m_startbtn.EnableWindow(FALSE);
	m_stopbtn.EnableWindow(TRUE);
	m_srcdir.EnableWindow(FALSE);
	m_targetdir.EnableWindow(FALSE);
	m_houzuied.EnableWindow(FALSE);
	m_filedircomprcopydefedit.EnableWindow(FALSE);
	mexcludepathword.EnableWindow(FALSE);
	shadowbtn.EnableWindow(FALSE);
	nousemoveoutbtn.EnableWindow(FALSE);
	OnBnClickedStarthideckState=0;
}


void CFolderAutomaticBackupDlg::OnBnClickedButStopspy()
{
	// TODO:  在此添加控件通知处理程序代码
	for (int i = 0; i < makesamethread.size(); i++){
		TerminateThread(makesamethread[i], 0);
	}
	for (int i = 0; i < docopythrdls.size(); i++){
		TerminateThread(docopythrdls[i], 0);
	}
	docopythrdls.clear();
	for (int i = 0; i < TaskThreadDeq.size(); i++)
	{
		TerminateThread(TaskThreadDeq[i], 0);
	}
	TaskThreadDeq.clear();
	for (int i = 0; i < moninfodeq.size(); i++)
	{
		delete moninfodeq[i];
	}
	moninfodeq.clear();

	m_startbtn.EnableWindow(TRUE);
	m_stopbtn.EnableWindow(FALSE);
	m_srcdir.EnableWindow(TRUE);
	m_targetdir.EnableWindow(TRUE);
	m_houzuied.EnableWindow(TRUE);
	m_filedircomprcopydefedit.EnableWindow(TRUE);
	mexcludepathword.EnableWindow(TRUE);
	shadowbtn.EnableWindow(TRUE);
	nousemoveoutbtn.EnableWindow(TRUE);
}

void CFolderAutomaticBackupDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == 1223){
		KillTimer(nIDEvent);
		OnBnClickedButStopspy();
		OnBnClickedButStartspy();
	}
	else{
		KillTimer(nIDEvent);

		if (g_show_dlg == 0)
			ShowWindow(SW_HIDE);
	}
}


void CFolderAutomaticBackupDlg::OnBnClickedButHide()
{
	// TODO:  在此添加控件通知处理程序代码
	ShowWindow(SW_HIDE);
}

LRESULT CFolderAutomaticBackupDlg::OnHotKey(WPARAM wParam, LPARAM lParam)
{
	if(wParam==786 && lParam==5963778){
		//auto ei=GetMessageExtraInfo();
		//if(bmiddleindown==false){
		//	::SetCapture(::GetForegroundWindow());
		//	mouse_event(MOUSEEVENTF_MIDDLEDOWN,0,0,0,0);
		//	bmiddleindown=true;
		//}else{
		//	mouse_event(MOUSEEVENTF_MIDDLEUP,0,0,0,0);
		//	::ReleaseCapture();
		//	bmiddleindown=false;
		//}
	//}else if(wParam==786 && 4325383==lParam){//lParam>>16==g_hotkey.data()[0] && 4325383&0xFFFF==lParam&0xFFFF){
	}else if(wParam==786 && lParam>>16==g_hotkey.data()[0] ){
		if (IsWindowVisible())
		{
			ShowWindow(SW_HIDE);//显示窗口

			if (m_startbtn.IsWindowEnabled() == TRUE)
			{
				OnBnClickedButStartspy();
			}

		}
		else{
			this->CenterWindow();
			ShowWindow(SW_SHOW);//显示窗口
			this->SetForegroundWindow();
			this->SetActiveWindow();
		}
	}
	return 0;
}


void CFolderAutomaticBackupDlg::OnBnClickedStarthideck()
{
	// TODO:  在此添加控件通知处理程序代码
	if (starthideck.GetCheck()==BST_CHECKED){
		IniTxtSaveSetting("starthide", "1");
	}
	else{
		IniTxtSaveSetting("starthide", "0");
	}
}


void CFolderAutomaticBackupDlg::OnEnChangeMaxreserveed()
{
	// TODO:  如果该控件是 RICHEDIT 控件，它将不
	// 发送此通知，除非重写 CDialogEx::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
	// TODO:  在此添加控件通知处理程序代码
	if(binitdlg==true){
		CString reservenum;
		maxreserveed.GetWindowText(reservenum);
		IniTxtSaveSetting("maxreservenum", reservenum.GetBuffer());
	}
}


void CFolderAutomaticBackupDlg::OnEnChangeEdit7()
{
	// TODO:  如果该控件是 RICHEDIT 控件，它将不
	// 发送此通知，除非重写 CDialogEx::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
	if(binitdlg){
		CString hotkey;
		hotkeyed.GetWindowText(hotkey);
		IniTxtSaveSetting("hotkey", hotkey.GetBuffer());
	}
}


BOOL CFolderAutomaticBackupDlg::OnDeviceChange(UINT nEventType, DWORD dwData)
{
	//这里进行信息匹配,比如guid等
	//针对各个事件进行处理.
	if (time(NULL) - devicechangestarttime >= 2){
		devicechangestarttime = time(NULL);
		SetTimer(1223, 1500, NULL);
	}
	return TRUE;
}

void ShadowFindAndMoveOut(std::wstring dir,std::wstring rootdir,std::wstring outrootdir){
	if(dir.size()==0){
		return;
	}
	if (wcsncmp(dir.data(), L"\\\\?\\", 4) != 0) {
		dir = L"\\\\?\\" + dir;
	}
	if (wcsncmp(rootdir.data(), L"\\\\?\\", 4) != 0) {
		rootdir = L"\\\\?\\" + rootdir;
	}
	if (wcsncmp(outrootdir.data(), L"\\\\?\\", 4) != 0) {
		outrootdir = L"\\\\?\\" + outrootdir;
	}
	if(dir[dir.size()-1]!='\\'){
		dir+=L"\\";
	}
	WIN32_FIND_DATA fd;
	auto hfind=FindFirstFile((dir+L"*").data(),&fd);
	if (hfind != NULL && hfind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!(wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0))
			{
				if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
				{
					std::wstring findd1 = dir + fd.cFileName;
					ShadowFindAndMoveOut(findd1,rootdir,outrootdir);
				}
				else{
					auto lastdot=wcsrchr(fd.cFileName,L'.');
					bool bbackupfile=false;
					if(lastdot!=nullptr
						&& lastdot-fd.cFileName>=15
						&& (lastdot-15)[0]=='_'
						&& isdigit((lastdot-14)[0])
						&& isdigit((lastdot-13)[0])
						&& isdigit((lastdot-12)[0])
						&& isdigit((lastdot-11)[0])
						&& isdigit((lastdot-10)[0])
						&& isdigit((lastdot-9)[0])
						&& isdigit((lastdot-8)[0])
						&& isdigit((lastdot-7)[0])
						&& isdigit((lastdot-6)[0])
						&& isdigit((lastdot-5)[0])
						&& isdigit((lastdot-4)[0])
						&& isdigit((lastdot-3)[0])
						&& isdigit((lastdot-2)[0])
						&& isdigit((lastdot-1)[0])
						&& (lastdot-0)[0]=='.'
						){
							bbackupfile=true;
					}
					if(bbackupfile){
						auto bakfpath=dir+fd.cFileName;
						auto outpath=outrootdir+bakfpath.substr(rootdir.size());
						auto outdir=outpath.substr(0,outpath.rfind(L'\\'));
						CreateFullDir(outdir.data());
						MoveFile(bakfpath.data(),outpath.data());
					}
				}
			}
		} while (FindNextFile(hfind, &fd));
		FindClose(hfind);
	}
}

void CFolderAutomaticBackupDlg::OnBnClickedShadowmoveout()
{
	// TODO: 在此添加控件通知处理程序代码
	wchar_t apppath[1024]={0};
	GetModuleFileName(NULL,	apppath,1024);
	wcsrchr(apppath,L'\\')[0]=0;
	if(wcsrchr(apppath,L'\\')==NULL){
		MessageBox(L"path error.",L"error");
		return;
	}
	wchar_t outrootdir[1024]={0};
	wcscpy(outrootdir,apppath);
	wcscat(outrootdir,L"_shadow");
	CTime ftime=CTime::GetCurrentTime();
	CString timestr2 = ftime.Format(L"%Y%m%d%H%M%S");
	wcscat(outrootdir,timestr2);	
	ShadowFindAndMoveOut(apppath,apppath,outrootdir);
}


void ShadowFindAndMoveNoUseOut(std::wstring dir,std::wstring rootdir,std::wstring outrootdir,std::wstring sourcedir){
	if(dir.size()==0){
		return;
	}
	if (wcsncmp(dir.data(), L"\\\\?\\", 4) != 0) {
		dir = L"\\\\?\\" + dir;
	}
	if (wcsncmp(rootdir.data(), L"\\\\?\\", 4) != 0) {
		rootdir = L"\\\\?\\" + rootdir;
	}
	if (wcsncmp(outrootdir.data(), L"\\\\?\\", 4) != 0) {
		outrootdir = L"\\\\?\\" + outrootdir;
	}
	if (wcsncmp(sourcedir.data(), L"\\\\?\\", 4) != 0) {
		sourcedir = L"\\\\?\\" + sourcedir;
	}
	if(dir[dir.size()-1]!='\\'){
		dir+=L"\\";
	}
	WIN32_FIND_DATA fd;
	auto hfind=FindFirstFile((dir+L"*").data(),&fd);
	if (hfind != NULL && hfind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!(wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0))
			{
				if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
				{
					auto bakfpath=dir+fd.cFileName;
					auto sourcepath=sourcedir+bakfpath.substr(rootdir.size());
					if(!PathFileExists(sourcepath.data())){
						auto outpath=outrootdir+bakfpath.substr(rootdir.size());
						auto outdir=outpath.substr(0,outpath.rfind(L'\\'));
						CreateFullDir(outdir.data());
						MoveFile(bakfpath.data(),outpath.data());
					}else{
						std::wstring findd1 = dir + fd.cFileName;
						ShadowFindAndMoveNoUseOut(findd1,rootdir,outrootdir,sourcedir);
					}
				}
				else{
					auto lastdot=wcsrchr(fd.cFileName,L'.');
					bool bbackupfile=false;
					if(lastdot!=nullptr
						&& lastdot-fd.cFileName>=15
						&& (lastdot-15)[0]=='_'
						&& isdigit((lastdot-14)[0])
						&& isdigit((lastdot-13)[0])
						&& isdigit((lastdot-12)[0])
						&& isdigit((lastdot-11)[0])
						&& isdigit((lastdot-10)[0])
						&& isdigit((lastdot-9)[0])
						&& isdigit((lastdot-8)[0])
						&& isdigit((lastdot-7)[0])
						&& isdigit((lastdot-6)[0])
						&& isdigit((lastdot-5)[0])
						&& isdigit((lastdot-4)[0])
						&& isdigit((lastdot-3)[0])
						&& isdigit((lastdot-2)[0])
						&& isdigit((lastdot-1)[0])
						&& (lastdot-0)[0]=='.'
						){
							bbackupfile=true;
					}

					if(!bbackupfile){
						auto bakfpath=dir+fd.cFileName;
						auto sourcepath=sourcedir+bakfpath.substr(rootdir.size());
						if(!PathFileExists(sourcepath.data())){
							auto outpath=outrootdir+bakfpath.substr(rootdir.size());
							auto outdir=outpath.substr(0,outpath.rfind(L'\\'));
							CreateFullDir(outdir.data());
							MoveFile(bakfpath.data(),outpath.data());
						}
					}else{
						auto bakfpath=dir+fd.cFileName;
						auto outpath=outrootdir+bakfpath.substr(rootdir.size());
						auto outdir=outpath.substr(0,outpath.rfind(L'\\'));
						CreateFullDir(outdir.data());
						MoveFile(bakfpath.data(),outpath.data());
					}
				}
			}
		} while (FindNextFile(hfind, &fd));
		FindClose(hfind);
	}
}

std::deque<std::wstring> GetBackupDirList(std::wstring srcdir) {
	std::deque<std::wstring> bakdirls;
	CString sourcedirname = srcdir.data();
	while (sourcedirname.Right(1) == L"\\")sourcedirname = sourcedirname.Mid(0, sourcedirname.GetLength() - 1);
	if (sourcedirname.ReverseFind(L'\\') != -1){
		sourcedirname = sourcedirname.Mid(sourcedirname.ReverseFind(L'\\') + 1);
	}
	auto srcpath = sourcedirname + L"_backup\\";
	if (srcpath[1] != ':'){
		srcpath = L"C:\\" + srcpath;
	}
	CString tdir;
	wchar_t drivs[80];
	GetLogicalDriveStrings(80,drivs);
	wchar_t *pdrivs = drivs;
	while (*pdrivs){
		tdir = srcpath;
		tdir.SetAt(0, *pdrivs);
		if (PathFileExists(tdir.GetBuffer())){
			bakdirls.push_back(tdir.GetBuffer());
		}
		pdrivs += wcslen(pdrivs) + 1;
	}
	return bakdirls;
}

void CFolderAutomaticBackupDlg::OnBnClickedMovenouseout()
{
	// TODO: 在此添加控件通知处理程序代码
	CString SourceDirs;
	m_srcdir.GetWindowText(SourceDirs);
	if(binitdlg){
		IniTxtSaveSetting("srcdir", SourceDirs.GetBuffer());
	}
	std::deque<std::wstring> srcdirdeq;
	splitString(SourceDirs.GetBuffer(), L";", srcdirdeq);
	docopythrdls.clear();
	int moninfolsi = 0;
	for (int i = 0; i < srcdirdeq.size(); i++)
	{
		if(srcdirdeq[i]==L""){
			continue;
		}
		if(srcdirdeq[i]==L"."){
			wchar_t apppath[1024] = { 0 };
			GetModuleFileName(NULL, apppath, 1024);
			wcsrchr(apppath, '\\')[1] = 0;
			srcdirdeq[i] = apppath;
		}
		auto bakdirlist=GetBackupDirList(srcdirdeq[i]);
		for(int j=0;j<bakdirlist.size();j++){
			wchar_t outrootdir[1024]={0};
			wcscpy(outrootdir,bakdirlist[j].data());
			outrootdir[wcslen(outrootdir)-1]=0;
			wcscat(outrootdir,L"_shadow");
			CTime ftime=CTime::GetCurrentTime();
			CString timestr2 = ftime.Format(L"%Y%m%d%H%M%S");
			wcscat(outrootdir,timestr2);
			wcscat(outrootdir,L"\\");
			ShadowFindAndMoveNoUseOut(bakdirlist[j],bakdirlist[j],outrootdir,srcdirdeq[i]);
		}
	}
}


void CFolderAutomaticBackupDlg::OnEnChangeEdit1()
{
	OnBnClickedStarthideckState=1;
	// TODO:  如果该控件是 RICHEDIT 控件，它将不
	// 发送此通知，除非重写 CDialogEx::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
}


void CFolderAutomaticBackupDlg::OnEnChangeEdit4()
{
	OnBnClickedStarthideckState=2;
	// TODO:  如果该控件是 RICHEDIT 控件，它将不
	// 发送此通知，除非重写 CDialogEx::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
}


void CFolderAutomaticBackupDlg::OnEnChangeEdit5()
{
	OnBnClickedStarthideckState=3;
	// TODO:  如果该控件是 RICHEDIT 控件，它将不
	// 发送此通知，除非重写 CDialogEx::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
}

void CFolderAutomaticBackupDlg::OnEnChangeEdit6()
{
	// TODO:  如果该控件是 RICHEDIT 控件，它将不
	// 发送此通知，除非重写 CDialogEx::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
	OnBnClickedStarthideckState=4;
	if(binitdlg){
		CString excludewords;
		mexcludepathword.GetWindowText(excludewords);
		if (excludewords.Right(1) == ";"){
			IniTxtSaveSetting("mexcludepathword", excludewords.GetBuffer());
		}
	}
}