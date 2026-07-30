#include<iostream>
#include<WinSock2.h>
#include <windows.h>
#include <ws2tcpip.h> 
#include<mysql.h>
#include<string>
#include"login.h"
#include"datebase.h"
#pragma comment(lib, "ws2_32.lib")
static SOCKET g_login_server_socket = INVALID_SOCKET;
struct bao
{
	login* loginobj;
	SOCKET clientsocket;
};
login::login(datebase* a)
{
	msql =a;
}
login::~login() {
	stop_login_server();
	std::cout << "Login模块资源已释放" << std::endl;
};
DWORD WINAPI login::login_thread(LPVOID lpParam)//通信线程函数（返回登录以及个人信息）//主程序
{
	bao* newbao = (bao*)lpParam;
	login* loginobj = newbao->loginobj;
	SOCKET clientsocket = newbao->clientsocket;
	char recvBuf[1024] = { 0 };
	int recvLen = 0;
	std::string username, password, cmd, infoAddr,birth,notes;
	sockaddr_in clientAddr;
	int clientAddrLen = sizeof(clientAddr);
	getpeername(clientsocket, (sockaddr*)&clientAddr, &clientAddrLen);
	char clientIp[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, INET_ADDRSTRLEN);
	std::cout << "客户端连接：IP=" << clientIp << "，开始处理登录/注册请求" << std::endl;
	recvLen = recv(clientsocket, recvBuf, sizeof(recvBuf) - 1, 0);
	if (recvLen <= 0) {
		std::cout << "客户端（IP=" << clientIp << "）断开连接/接收失败" << std::endl;
		closesocket(clientsocket);
		delete newbao; // 释放参数内存
		return 1;
	}
	recvBuf[recvLen] = '\0'; // 手动加结束符
	std::string recvMsg = recvBuf;
	// 4. 简单解析命令（格式：cmd=login&username=xxx&password=xxx）
	size_t cmdPos = recvMsg.find("cmd=");
	size_t userPos = recvMsg.find("&username=");
	size_t pwdPos = recvMsg.find("&password=");
	size_t birthpos= recvMsg.find("&birth=");
	size_t notespos = recvMsg.find("&notes=");
	if (cmdPos == std::string::npos || userPos == std::string::npos || pwdPos == std::string::npos) {
		send(clientsocket, "error:命令格式错误", 16, 0);
		closesocket(clientsocket);
		delete newbao;
		return 1;
	}
	cmd = recvMsg.substr(cmdPos + 4, userPos - (cmdPos + 4));
	username = recvMsg.substr(userPos + 10, pwdPos - (userPos + 10));
	if(birthpos==std::string::npos)
	password = recvMsg.substr(pwdPos + 10);
	else
	password = recvMsg.substr(pwdPos + 10,birthpos-(pwdPos + 10));
	// 5. 处理登录/注册
	if (cmd == "login") {
		infoAddr = loginobj->get_message_sql(username, password); // 调用查询数据库函数
	}
	else if (cmd == "register") {
		birth= recvMsg.substr(birthpos+7,notespos-(birthpos + 7));
		notes= recvMsg.substr(notespos+7);
		infoAddr = loginobj->add_acount(username, password,birth,notes); // 调用注册账号函数
	}
	else {
		send(clientsocket, "error:未知命令", 12, 0);
		closesocket(clientsocket);
		delete newbao;
		return 1;
	}
	// 6. 返回结果给客户端
	if (!infoAddr.empty()) {
		std::string sendMsg = "success=1&info_addr=" + infoAddr;
		send(clientsocket, sendMsg.c_str(), static_cast<int>(sendMsg.length()), 0);
		std::cout << "客户端（IP=" << clientIp << "）" << cmd << "成功，个人信息地址：" << infoAddr << std::endl;
	}
	else {
		send(clientsocket, "success=0", 9, 0);
		std::cout << "客户端（IP=" << clientIp << "）" << cmd << "失败" << std::endl;
	}

	// 7. 释放资源
	closesocket(clientsocket);
	delete newbao; // 释放线程参数
	return 0;
}
void login::add_thread()//登录通信建立线程监听函数
{
	WSADATA wsadata;
	sockaddr_in cilentaddr;
	int cilentlen = sizeof(cilentaddr);
	HANDLE hThread;
	if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0)
	{
		std::cout << "WSAStartup失败: " << WSAGetLastError() << std::endl;
		return;
	}
	SOCKET serversocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	g_login_server_socket = serversocket;
	if (serversocket == INVALID_SOCKET) {
		std::cout << "创建socket失败: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return;
	}
	sockaddr_in serveraddr;
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = INADDR_ANY;
	serveraddr.sin_port = htons(8888);
	if (bind(serversocket, (sockaddr*)&serveraddr, sizeof(serveraddr)) == SOCKET_ERROR) {
		std::cout << "绑定失败: " << WSAGetLastError() << std::endl;
		closesocket(serversocket);
		WSACleanup();
		return;
	}
	if (listen(serversocket, 5) == SOCKET_ERROR) {
		std::cout << "监听失败: " << WSAGetLastError() << std::endl;
		closesocket(serversocket);
		WSACleanup();
		return;
	}
	while (true)
	{
		SOCKET clientsocket = accept(serversocket, (sockaddr*)&cilentaddr, &cilentlen);
		if (clientsocket == INVALID_SOCKET) {
			std::cout << "接受连接失败: " << WSAGetLastError() << std::endl;
			continue;
		}
		bao* newbao = new bao;
		newbao->loginobj = this;
		newbao->clientsocket = clientsocket;
		hThread = CreateThread(
			NULL,                   // 默认安全属性
			0,                      // 线程栈大小（默认）
			login::login_thread,       // 线程函数（处理该客户端的收发）
			(LPVOID)newbao,   // 线程参数（传递客户端socket）
			0,                      // 线程创建标志（默认）
			NULL                    // 线程ID（不需要，传NULL）
		);
		if (hThread == NULL) {
			// 线程创建失败，关闭该客户端socket，避免资源泄露
			std::cout << "创建客户端线程失败: " << GetLastError() << std::endl;
			closesocket(clientsocket);
			delete newbao;
		}
		else {
			// 不需要等待线程结束，释放线程句柄（线程仍在后台运行）
			CloseHandle(hThread);
		}
	}
}
std::string login::get_message_sql(std::string acount, std::string key)
{
	// 空值校验：账号/密码为空直接返回失败
	if (acount.empty() || key.empty()) {
		std::cerr << "登录失败：账号或密码不能为空" << std::endl;
		return "";
	}

	// 调用数据库层查询登录信息 → 获取个人信息地址
	std::string infoAddr = msql->find_login_message(acount, key);

	// 结果判断：空=登录失败，非空=登录成功
	if (infoAddr.empty()) {
		std::cerr << "登录失败：" << std::endl;
	}
	else {
		std::cout << "登录成功！"<< std::endl;
	}
	return infoAddr;
}
std::string login::add_acount(std::string acount, std::string key,std::string birth,std::string notes)
{
	// 空值校验：账号/密码为空直接返回失败
	if (acount.empty() || key.empty()) {
		std::cerr << "注册失败：账号或密码不能为空" << std::endl;
		return "";
	}

	std::string saveResult = msql->add_login_message(acount, key);
	if (saveResult != "success") {
		std::cerr << "注册失败：保存登录信息失败"<< std::endl;
		return "";
	}
	std::string infoAddr = msql->add_acount_message(acount,birth,notes);
	if (infoAddr != "success") {
		std::cerr << "注册失败：保存个人信息失败" << std::endl;
		return "";
	}
	return acount;
}
DWORD WINAPI login::listen_thread(LPVOID lpParam)
{
	login* login_obj = (login*)lpParam;
	// 调用原有监听逻辑（add_chat_thread会阻塞该线程，但不阻塞主线程）
	login_obj->add_thread();

	return 0;
}
void login::init()
{
	h_login = CreateThread(nullptr, 0, listen_thread, this, 0, nullptr);
	if (h_login == nullptr) {
		std::cerr << "登录线程创建失败，错误码：" << GetLastError() << std::endl;
		return;
	}
	std::cout << "登录线程创建成功,端口号8888" << std::endl;
}
void login::stop_login_server() {
	// 1. 关闭登录服务端Socket
	if (g_login_server_socket != INVALID_SOCKET) {
		closesocket(g_login_server_socket);
		g_login_server_socket = INVALID_SOCKET;
	}

	// 2. 关闭线程句柄
	if (h_login != nullptr) {
		WaitForSingleObject(h_login, 1000);
		CloseHandle(h_login);
		h_login = nullptr;
	}

	// 3. 清理WSA（注意：如果chat也用了WSA，只需要在最后析构时调用一次，这里可注释）
	WSACleanup();
}
