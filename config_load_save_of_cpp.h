#ifndef config_load_save_of_cpp_h
#define config_load_save_of_cpp_h

#include <string>


extern std::string g_sqlserver_ip_edit;
extern std::string g_sqlserver_user_edit;
extern std::string g_sqlserver_password_edit;
extern std::string g_sqlserver_dbname_edit;
extern int g_nouseidtable_create;
extern std::string g_srcdir;
extern std::string g_targetdir;
extern std::string g_monexts, g_genflsexts, g_autocompcopy;
extern std::string g_starthide, g_maxreservenum, g_hotkey, g_mexcludepathword;

extern void LoadConfig();


extern bool SaveConfig();

extern bool IniTxtSaveSetting(const std::string &segname, const std::string &segvalue);
extern bool IniTxtSaveSetting(const std::string &segname, const std::wstring &segvalue);


std::wstring u8tow(std::string str);


#endif

