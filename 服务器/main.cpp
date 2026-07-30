#include<iostream>
#include<WinSock2.h>
#include <windows.h>
#include <ws2tcpip.h> 
#include<mysql.h>
#include<string>
#include<queue>
#include<mutex>
#include<atomic>  
#include<map>     
#include<vector>
#include"datebase.h"
#include"chat.h"
#include"login.h"
int main()
{
	datebase sql("localhost", "root", "031214Lgy@", "chat");
	login lg(&sql);
	chat ct(&sql);
	ct.init();
	lg.init();
	while(true){}
	return 0;
}