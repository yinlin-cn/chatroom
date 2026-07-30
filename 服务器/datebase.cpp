#include<iostream>
#include"datebase.h"
#include<WinSock2.h>
#include <windows.h>
#include <ws2tcpip.h> 
#include<mysql.h>
#include<string>
#pragma comment(lib, "ws2_32.lib")
datebase::datebase(const std::string& host,
	const std::string& user,
	const std::string& password,
	const std::string& dbName)
{
	const char* HOST = host.c_str();
	const char* USER = user.c_str();
	const char* PWD = password.c_str();    // 你的MySQL密码
	const char* DB_NAME = dbName.c_str();  // 你的数据库名
	int PORT = 3306;
	msql = mysql_init(nullptr);
	if (!msql)
	{
		std::cout << "初始化失败！" << std::endl;
		return;
	}
	mysql_options(msql, MYSQL_SET_CHARSET_NAME, "utf8");
	if (!mysql_real_connect(
		msql,
		HOST,
		USER,
		PWD,
		DB_NAME,
		PORT,
		NULL, 0
	))
	{
		std::cout << "连接失败：" << mysql_error(msql) << std::endl;
		return;
	}
	std::cout << "连接成功" << std::endl;
}
datebase::~datebase() { mysql_close(msql); }
std::string datebase::find_login_message(std::string acount, std::string key)//登录信息查询函数（返回登录信息）
{
	std::string sql = "select mykey from login where username = '" + acount + "';";
	if (mysql_query(msql, sql.c_str()) != 0)
	{
		std::cout << "查询失败：" << mysql_error(msql) << std::endl;
		return "";
	}
	MYSQL_RES* res = mysql_store_result(msql);
	if (!res)std::cout << "无数据" << std::endl;
	MYSQL_ROW row;
	row = mysql_fetch_row(res);
	std::string fan;
	if (row != nullptr && row[0] != nullptr)
	{
		fan = row[0];
	}
	else
	{
		mysql_free_result(res); // 释放资源
		return "";
	}
	mysql_free_result(res);
	if (fan == key)return acount;
	else return"";
}
std::string datebase::find_acount_message(std::string acount)//个人信息查询函数（返回个人信息）
{
	std::string sql = "select username,birth,notes from person_message where username = '" + acount + "';";
	if (mysql_query(msql, sql.c_str()) != 0)
	{
		std::cout << "查询失败：" << mysql_error(msql) << std::endl;
		return "error";
	}
	std::string fan;
	fan = "username=";
	MYSQL_RES* res = mysql_store_result(msql);
	if (!res)std::cout << "无数据" << std::endl;
	MYSQL_ROW row;
	row = mysql_fetch_row(res);
	if (row != nullptr && row[0] != nullptr)
	{
		// 用户名一定有
		fan += row[0];

		// 生日可能为空，判断后再加
		if (row[1] != nullptr)
		{
			fan += "&birth=";
			fan += row[1];
		}
		else
		{
			fan += " NULL";
		}

		// 备注可能为空，判断后再加
		if (row[2] != nullptr)
		{
			fan += "&notes=";
			fan += row[2];
		}
		else
		{
			fan += " NULL";
		}
	}
	else
	{
		mysql_free_result(res); // 释放资源
		return "error";
	}
	mysql_free_result(res);
	return fan;
}
std::string datebase::find_chat_message(std::string last_time, int num)//历史聊天信息查询函数（返回聊天信息）
{
	std::string sql = "SELECT username, message, ctime FROM chat_message WHERE ctime < '" + last_time;
	sql += "' ORDER BY ctime DESC LIMIT ";
	sql+= std::to_string(num) +";";
	if (mysql_query(msql, sql.c_str()) != 0)
	{
		std::cout << "查询失败：" << mysql_error(msql) << std::endl;
		return "error";
	}
	std::string fan = "";
	MYSQL_RES* res = mysql_store_result(msql);
	if (!res)std::cout << "无数据" << std::endl;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(res)) != nullptr)
	{
		fan += "&username=";
		fan+= row[0];
		fan += "&message=";
		fan += row[1];
		fan += "&ctime=";
		fan += row[2];
		fan += "\n";
	}
	mysql_free_result(res);
	return fan;
}
std::string datebase::add_login_message(std::string acount, std::string key)//登录账号录入函数（返回个人信息地址）
{
	std::string sql = "insert into login(username,mykey) values('" + acount + "', '" +key + "');";
	// 执行失败
	if (mysql_query(msql, sql.c_str()) != 0)
	{
		// 获取错误码
		int err_num = mysql_errno(msql);

		// 错误码 1062 = 唯一性冲突（用户名重复）
		if (err_num == 1062)
		{
			std::cout << "注册失败：用户名已存在" << std::endl;
			return "repeat"; // 【重复专用返回值】
		}
		else
		{
			// 其他错误（数据库断开、权限不足等）
			std::cout << "注册失败：" << mysql_error(msql) << std::endl;
			return "error";
		}
	}
	return "success";
}
std::string datebase::add_acount_message(std::string acount,std::string birth,std::string notes)//个人信息创建函数
{
	std::string sql = "insert into person_message(username,birth,notes) values('" + acount + "', '" + birth + "','"+notes+"'); ";
	if (mysql_query(msql, sql.c_str()) != 0)
	{
		// 获取错误码
		int err_num = mysql_errno(msql);

		// 错误码 1062 = 唯一性冲突（用户名重复）
		if (err_num == 1062)
		{
			std::cout << "注册失败：用户名已存在" << std::endl;
			return "repeat"; // 【重复专用返回值】
		}
		else
		{
			// 其他错误（数据库断开、权限不足等）
			std::cout << "注册失败：" << mysql_error(msql) << std::endl;
			return "error";
		}
	}
	return "success";
}
std::string datebase::change_acount_message(std::string acount, std::string birth, std::string notes)//个人信息录入函数（返回个人信息地址）
{
	std::string sql = "update  person_message set  birth='" + birth + "',notes='" + notes + "' where username='" + acount + "'; ";
	if (mysql_query(msql, sql.c_str()) != 0)
	{
			// 其他错误（数据库断开、权限不足等）
			std::cout << "注册失败：" << mysql_error(msql) << std::endl;
			return "error";
	}
	return "success";
}
std::string datebase::add_chat_messages_batch(const std::string& batch_data)//聊天记录录入函数
{
	int rows = 0;
	std::string username, message, ctime, sql;
	while (rows <batch_data.size())
	{
		size_t user = batch_data.find("&username=", rows);
		size_t text = batch_data.find("&message=", rows);
		size_t time = batch_data.find("&ctime=", rows);
		size_t end = batch_data.find("\n", rows);
		if (user == std::string::npos || text == std::string::npos ||
			time == std::string::npos || end == std::string::npos)
		{
			rows = end + 1;; // 找不到就退出，绝不越界、绝不崩
			return "error";
		}
		username = batch_data.substr(user + 10, text - (user + 10));
		message = batch_data.substr(text+9, time - (text + 9));
		ctime = batch_data.substr(time+7,end-(time + 7));
		std::cout << ctime << std::endl;
		size_t time_mid = ctime.find("&");
		std::string date = ctime.substr(0, time_mid);
		date += " " + ctime.substr(time_mid + 1);
		sql = "insert into chat_message(username,message,ctime) values('" + username + "','" + message + "','" + date + "');";
		if (mysql_query(msql, sql.c_str()) != 0)
		{
			// 其他错误（数据库断开、权限不足等）
			std::cout << "写入失败：" << mysql_error(msql) << std::endl;
			return "error";
		}
		rows = end + 1;
	}
	return "success";
}
std::string datebase::find_last_time()//返回最后的聊天记录更新时间
{
	std::string sql = "SELECT ctime FROM chat_message ORDER BY ctime DESC LIMIT 1";
	if (mysql_query(msql, sql.c_str()) != 0)
	{
		// 其他错误（数据库断开、权限不足等）
		std::cout << "读取失败：" << mysql_error(msql) << std::endl;
		return "error";
	}
	MYSQL_RES* res = mysql_store_result(msql);
	if (!res) return "error";
	MYSQL_ROW row = mysql_fetch_row(res);
	if (row != nullptr && row[0] != nullptr)
	{
		mysql_free_result(res);
		return row[0];
	}
	else
	{
		mysql_free_result(res); // 释放资源
		return "error";
	}
}
std::string datebase::find_chat_message_between_time(std::string first_time, std::string last_time)//历史聊天信息查询函数（返回聊天信息）
{
	std::string sql = "SELECT username, message, ctime FROM chat_message WHERE ctime between '"+first_time+"' and '" + last_time + "';" ;
	if (mysql_query(msql, sql.c_str()) != 0)
	{
		std::cout << "查询失败：" << mysql_error(msql) << std::endl;
		return "error";
	}
	std::string fan = "";
	MYSQL_RES* res = mysql_store_result(msql);
	if (!res)
	{
		std::cout << "无数据" << std::endl;
		return "error";
	}
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(res)) != nullptr)
	{
		fan += "&username=";
		fan += row[0];
		fan += "&message=";
		fan += row[1];
		fan += "&ctime=";
		fan += row[2];
		fan += "\n";
	}
	mysql_free_result(res);
	return fan;
}
