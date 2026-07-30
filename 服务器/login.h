#pragma once
#include<iostream>
#include"datebase.h"
#include <winsock2.h>
#include <windows.h> 
#include <ws2tcpip.h>
#include<string>
class login
{
private:
	datebase* msql;
	HANDLE h_login;
public:
	login(datebase*);
	~login();
	void stop_login_server();
	void init();
	std::string get_message_sql(std::string account,std::string key);//查询数据库函数（返回个人信息地址）
	void add_thread();//登录通信建立线程监听函数
	static DWORD WINAPI login_thread(LPVOID lpParam);//通信线程函数（返回登录以及个人信息）//主程序
	std::string add_acount(std::string acount, std::string key, std::string birth, std::string notes);//注册添加账号函数（返回个人信息地址）
	static DWORD WINAPI listen_thread(LPVOID lpParam);
};