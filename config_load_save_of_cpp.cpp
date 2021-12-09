#include "config_load_save_of_cpp.h"
#include <fstream>
#include <Windows.h>
#include "..\UtilityFunc\StringReplace.h"

using namespace std;
 

std::string g_sqlserver_ip_edit;
std::string g_sqlserver_user_edit;
std::string g_sqlserver_password_edit;
std::string g_sqlserver_dbname_edit;
int g_nouseidtable_create=-1;
std::string g_srcdir;
std::string g_targetdir;
std::string g_monexts, g_genflsexts,g_autocompcopy;
std::string g_starthide, g_maxreservenum, g_hotkey, g_mexcludepathword;


std::string  settingfilename="FolderAutomaticBackup.cfg";

void LoadConfig()
{
	string cfg_imagefrom;

	ifstream settingf;
	settingf.open(settingfilename, ios_base::in);
	if (!settingf.is_open())
	{
		std::ofstream outf;
		outf.open(settingfilename, ios_base::out);
		outf.close();
		settingf.open(settingfilename, ios_base::in);
	}
	while (!settingf.fail())
	{
		std::string linestr;
		getline(settingf, linestr);
		while(linestr.size() && (linestr[0]=='\r' || linestr[0]=='\n'))linestr=linestr.substr(1);
		while(linestr.size() && (linestr[linestr.size()-1]=='\r' || linestr[linestr.size()-1]=='\n'))linestr=linestr.substr(0,linestr.size()-1);
		if(linestr.empty()){
			continue;
		}
		if (linestr[0] == '#')
			continue; 



		//替换换行符为空
		std::string linestr2 = linestr.c_str();
		while (linestr2.find("\r") != std::string::npos || linestr2.find("\n") != std::string::npos)
		{
			int startpos = linestr2.find("\r"), crlflength = 0;
			int startpos_ne = 1;
			while (linestr2[startpos - startpos_ne] == '\r' || linestr2[startpos - startpos_ne] == '\n')
			{
				startpos_ne--;
			}
			if (startpos_ne > 1)
			{
				startpos_ne -= 1;
				startpos -= startpos_ne;
			}
			startpos_ne = 0;
			while (linestr2[startpos + startpos_ne] == '\r' || linestr2[startpos - startpos_ne] == '\n')
			{
				crlflength--;
			}
			linestr2.replace(startpos, crlflength, "\\r\\n");
		}

		linestr = linestr2;

		if (strncmp(linestr.c_str(), "sqlserver_ip_edit=", strlen("sqlserver_ip_edit=")) == 0)
		{
			g_sqlserver_ip_edit = linestr.substr(strlen("sqlserver_ip_edit="));
		}
		if (strncmp(linestr.c_str(), "sqlserver_user_edit=", strlen("sqlserver_user_edit=")) == 0)
		{
			g_sqlserver_user_edit = linestr.substr(strlen("sqlserver_user_edit=")); 
		}
		if (strncmp(linestr.c_str(), "sqlserver_password_edit=", strlen("sqlserver_password_edit=")) == 0)
		{
			g_sqlserver_password_edit = linestr.substr(strlen("sqlserver_password_edit="));
		}
        if (strncmp(linestr.c_str(), "sqlserver_dbname_edit=", strlen("sqlserver_dbname_edit=")) == 0)
		{
            g_sqlserver_dbname_edit = linestr.substr(strlen("sqlserver_dbname_edit="));
		}

        if (strncmp(linestr.c_str(), "nouseidtable_create=", strlen("nouseidtable_create=")) == 0)
        {
            g_nouseidtable_create = atoi(linestr.substr(strlen("nouseidtable_create=")).c_str());
        }

		if (strncmp(linestr.c_str(), "srcdir=", strlen("srcdir=")) == 0)
		{
			g_srcdir = linestr.substr(strlen("srcdir="));
		}
		if (strncmp(linestr.c_str(), "targetdir=", strlen("targetdir=")) == 0)
		{
			g_targetdir = linestr.substr(strlen("targetdir="));
		}

		if (strncmp(linestr.c_str(), "monexts=", strlen("monexts=")) == 0)
		{
			g_monexts = linestr.substr(strlen("monexts=")).c_str();
		}
		if (strncmp(linestr.c_str(), "starthide=", strlen("starthide=")) == 0)
		{
			g_starthide= linestr.substr(strlen("starthide=")).c_str();
		}

		if (strncmp(linestr.c_str(), "genflsexts=", strlen("genflsexts=")) == 0)
		{
			g_genflsexts = linestr.substr(strlen("genflsexts=")).c_str();
		}

		if (strncmp(linestr.c_str(), "autocompcopy=", strlen("autocompcopy=")) == 0)
		{
			g_autocompcopy = linestr.substr(strlen("autocompcopy=")).c_str();
			g_autocompcopy=StringReplace(g_autocompcopy,"\\n","\r\n");
		}
		if (strncmp(linestr.c_str(), "maxreservenum=", strlen("maxreservenum=")) == 0)
		{
			g_maxreservenum = linestr.substr(strlen("maxreservenum=")).c_str();
		}
		if (strncmp(linestr.c_str(), "hotkey=", strlen("hotkey=")) == 0)
		{
			g_hotkey = linestr.substr(strlen("hotkey=")).c_str();
		}
		if (strncmp(linestr.c_str(), "mexcludepathword=", strlen("mexcludepathword=")) == 0)
		{
			g_mexcludepathword = linestr.substr(strlen("mexcludepathword=")).c_str();
		}
		

	}
	settingf.close();
}


bool SaveConfig()
{
	IniTxtSaveSetting("sqlserver_ip_edit", g_sqlserver_ip_edit);
	IniTxtSaveSetting("sqlserver_user_edit", g_sqlserver_user_edit);
	IniTxtSaveSetting("sqlserver_password_edit", g_sqlserver_password_edit);
	IniTxtSaveSetting("sqlserver_dbname_edit", g_sqlserver_dbname_edit);
	return true;
}

bool IniTxtSaveSetting(const string &segname, const string &segvalue4)
{
	std::string segvalue = segvalue4;
	segvalue = StringReplace(segvalue, "\r\n", "\\n");

	std::string cfg_newcontent;

	//替换换行符为空
	std::string linestr2 = segvalue;
	while (linestr2.find("\r") != std::string::npos || linestr2.find("\n") != std::string::npos)
	{
		int startpos = linestr2.find("\r"), crlflength = 0;
		int startpos_ne = 1;
		while (linestr2[startpos - startpos_ne] == '\r' || linestr2[startpos - startpos_ne] == '\n')
		{
			startpos_ne--;
		}
		if (startpos_ne > 1)
		{
			startpos_ne -= 1;
			startpos -= startpos_ne;
		}
		startpos_ne = 0;
		while (linestr2[startpos + startpos_ne] == '\r' || linestr2[startpos - startpos_ne] == '\n')
		{
			crlflength--;
		}
		linestr2.replace(startpos, crlflength, "\\r\\n");
	}
	std::string segvalue2 = linestr2;


	std::ifstream settingf;
	settingf.open(settingfilename, ios_base::in);
	if (!settingf.is_open())
	{
		std::ofstream outf;
		outf.open(settingfilename, ios_base::out);
		outf.close();
		settingf.open(settingfilename, ios_base::in);
	}
	if (settingf.is_open())
	{
		bool bfoundsegname = false;
		while (!settingf.fail())
		{
			std::string linestr;
			getline(settingf, linestr);
			while(linestr.size() && (linestr[0]=='\r' || linestr[0]=='\n'))linestr=linestr.substr(1);
			while(linestr.size() && (linestr[linestr.size()-1]=='\r' || linestr[linestr.size()-1]=='\n'))linestr=linestr.substr(0,linestr.size()-1);
			if(linestr.empty()){
				continue;
			}
			if (linestr[0] == '#')
				continue;
			if (linestr.size() == 0)
				continue;

			if (strncmp(linestr.c_str(), (segname + "=").c_str(), (segname + "=").size()) == 0)
			{
				cfg_newcontent += segname + "=" + segvalue2 + "\n";
				bfoundsegname = true;
			}

			else{
				cfg_newcontent += linestr + "\n";
			}
		}
		settingf.close();

		if (bfoundsegname == false)
		{
			cfg_newcontent += segname + "=" + segvalue2 + "\n";
		}

		std::ofstream osettingf;
		osettingf.open(settingfilename, ios_base::out);
		if (osettingf.is_open())
		{
			osettingf.write(cfg_newcontent.c_str(), cfg_newcontent.size());
			osettingf.close();
		}
	}
	return true;
}



bool IniTxtSaveSetting(const std::string &segname, const std::wstring &segvalue)
{
	std::wstring segvalue2 = segvalue;
	segvalue2 = wstringReplace(segvalue2, L"\r\n", L"\\n");
	int buflen = segvalue2.size() * 3 + 100000;
	char *buf = new char[buflen];
	DWORD len = WideCharToMultiByte(CP_UTF8, 0, segvalue2.c_str(), segvalue2.size(), buf, buflen, 0, 0);
	std::string bufstr(buf, len);
	delete[]buf;
	return IniTxtSaveSetting(segname, bufstr);
}


std::wstring u8tow(std::string str)
{
	int wbuflen = str.size() * 3 + 100000;
	wchar_t *wbuf = new wchar_t[wbuflen];
	DWORD len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.size(), wbuf,wbuflen);
	std::wstring bufstr(wbuf, len);
	return bufstr;
}

