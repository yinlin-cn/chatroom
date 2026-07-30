#pragma once
#include<iostream>
#include<WinSock2.h>
#include <windows.h>
#include <ws2tcpip.h> 
#include<mysql.h>
#include<string>
class datebase
{
private:
	MYSQL* msql;
public:
	datebase(const std::string& host,
		const std::string& user,
		const std::string& password,
		const std::string& dbName);
	~datebase();
	std::string find_login_message(std::string acount, std::string key);//登录信息查询函数（返回登录信息）
	std::string find_acount_message(std::string acount);//个人信息查询函数（返回个人信息）
	std::string find_chat_message(std::string last_time,int num);//历史聊天信息查询函数（返回聊天信息）
	std::string find_chat_message_between_time(std::string first_time,std::string last_time);//历史聊天信息查询函数（返回聊天信息）
	std::string add_login_message(std::string acount, std::string key);//登录账号录入函数（返回个人信息地址）
	std::string add_acount_message(std::string acount, std::string birth, std::string notes);//个人信息创建函数
	std::string change_acount_message(std::string acount, std::string birth, std::string notes);//个人信息录入函数（返回个人信息地址）
	std::string add_chat_messages_batch(const std::string& batch_data);//聊天记录录入函数
	std::string find_last_time();//返回最后的聊天记录更新时间
};
