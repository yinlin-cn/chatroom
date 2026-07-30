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
#pragma comment(lib, "ws2_32.lib")
chat::chat(datebase* a)
{
	msql = a;
	is_server_running = true;
}
chat::~chat() {
	// 1. 停止服务器运行（通知所有线程退出）
	is_server_running = false;

	// 2. 批量清理所有客户端连接
	{
		std::lock_guard<std::mutex> lock(login_list_mutex);
		for (auto conn : login_list) {
			cleanup_client_conn(conn);
		}
		login_list.clear();
	}

	// 3. 等待并关闭线程句柄
	if (h_timer_sum) {
		WaitForSingleObject(h_timer_sum, 1000);
		CloseHandle(h_timer_sum);
		h_timer_sum = nullptr;
	}
	if (h_timer_renew) {
		WaitForSingleObject(h_timer_renew, 1000);
		CloseHandle(h_timer_renew);
		h_timer_renew = nullptr;
	}
	if (h_listen_thread) {
		WaitForSingleObject(h_listen_thread, 1000);
		CloseHandle(h_listen_thread);
		h_listen_thread = nullptr;
	}

	// 4. 关闭服务端Socket
	if (server_socket != INVALID_SOCKET) {
		closesocket(server_socket);
		server_socket = INVALID_SOCKET;
	}

	// 5. 清理WSA
	WSACleanup();

	std::cout << "Chat模块所有资源已释放" << std::endl;
};
void chat::add_chat_thread()//线程创建监听函数
{
	WSADATA wsadata;
	if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0)
	{
		std::cout << "WSAStartup失败: " << WSAGetLastError() << std::endl;
		return;
	}
	server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server_socket == INVALID_SOCKET) {
		std::cout << "创建socket失败: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return;
	}
	sockaddr_in server_addr = { AF_INET, htons(8889), INADDR_ANY };
	if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
		std::cout << "绑定失败: " << WSAGetLastError() << std::endl;
		closesocket(server_socket);
		WSACleanup();
		return;
	}
	if (listen(server_socket, 5) == SOCKET_ERROR) {
		std::cout << "监听失败: " << WSAGetLastError() << std::endl;
		closesocket(server_socket);
		WSACleanup();
		return;
	}
	std::cout << "通信服务器启动，监听8889端口..." << std::endl;
	while (is_server_running)
	{
		SOCKET client_sock = accept(server_socket, nullptr, nullptr);
		if (client_sock == INVALID_SOCKET) continue;

		// 接收客户端的info_addr（登录后获取的唯一标识）
		char info_addr_buf[1024] = { 0 };
		recv(client_sock, info_addr_buf, sizeof(info_addr_buf) - 1, 0);
		std::string info_addr = info_addr_buf;
		ClientConn* logindata = new ClientConn;
		logindata->info_addr = info_addr;
		logindata->is_connected = true;
		logindata->sock = client_sock;
		// 保护login_list（线程安全写入）
		{
			std::lock_guard<std::mutex> lock(login_list_mutex);
			login_list.push_back(logindata);
		}

		// 1. 新建参数结构体，赋值
		ThreadParam* param = new ThreadParam;
		param->chat_instance = this;    // chat类实例指针
		param->client_sock = client_sock;// 当前客户端Socket
		param->info_addr = info_addr;   // 当前客户端用户标识
		param->is_running = true;
		param->msg_queue = &logindata->msg_queue;
		param->queue_mutex = &logindata->queue_mutex;
		// 2. 创建输入线程：传递param（包含所有需要的信息）
		CreateThread(nullptr, 0, in_thread, param, 0, nullptr);
		// 3. 创建输出线程：传递同一个param（输入/输出线程共享客户端信息）
		CreateThread(nullptr, 0, out_thread, param, 0, nullptr);
	}
}
DWORD WINAPI chat::in_thread(LPVOID lpParam)//通信线程输入函数//主程序
{
	ThreadParam* param = (ThreadParam*)lpParam;
	if (!param) return 1; // 兜底：参数为空直接退出
	chat* chat_obj = param->chat_instance;
	SOCKET client_sock = param->client_sock;
	std::string info_addr = param->info_addr; // 保存用户标识，供清理用
	char recv_buf[4096] = { 0 };//发送最后更新时间
	param->msg_queue->push(chat_obj->msql->find_last_time());
	std::cout << "输入线程启动成功" << info_addr << std::endl;
	while (param->is_running) {
		// 1. 接收客户端消息
		int recv_len = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
		if (recv_len <= 0) {
			// ========== 核心修改：断开连接时触发清理 ==========
			param->is_running = false; // 通知输出线程退出
			std::cout << "客户端[" << info_addr << "]断开连接，开始清理资源" << std::endl;

			// 1. 关闭Socket（原有逻辑保留）
			closesocket(client_sock);
			// 2. 调用清理函数，删除ClientConn并从login_list移除
			chat_obj->cleanup_client_by_info_addr(info_addr);

			break; // 退出输入线程循环
		}
		recv_buf[recv_len] = '\0';
		std::string client_msg = recv_buf;

		// 2. 调用message_divide查询信息（仅查询，不存储到类成员）
		std::string query_result = chat_obj->message_divide(client_msg);
		// 3. 核心：将查询结果直接塞到输入/输出线程的专属队列（点对点传递）
		if (query_result != "success"&& query_result != "error")
		{
			std::lock_guard<std::mutex> lock(*param->queue_mutex); // 仅锁队列
			param->msg_queue->push(query_result);
		}

		memset(recv_buf, 0, sizeof(recv_buf));
	}
	if (param) {
		param->is_running = false;
		// 注意：输出线程会删除param，这里不重复删
	}
	return 0;
}
DWORD WINAPI chat::out_thread(LPVOID lpParam)//通信线程输出函数//主程序
{
	ThreadParam* param = (ThreadParam*)lpParam;
	SOCKET client_sock = param->client_sock;
	std::string info_addr = param->info_addr; // 保存用户标识
	// 循环从专属队列取数据并发送（核心：直接消费输入线程的查询结果）
	std::cout << "输出线程启动成功" << info_addr << std::endl;
	while (param->is_running) {
		std::string send_msg;
		// 加锁取队列数据（非阻塞，避免空等）
		{
			std::lock_guard<std::mutex> lock(*param->queue_mutex);
			if (!param->msg_queue->empty()) {
				send_msg = param->msg_queue->front();
				param->msg_queue->pop(); // 取出并删除
			}
		}
		// 有数据则发送
		if (!send_msg.empty()) {
			send(client_sock, send_msg.c_str(), static_cast<int>(send_msg.length()), 0);
			Sleep(10);
		}
		else {
			Sleep(100); // 无数据时短暂休眠，降低CPU占用
		}
	}
	// ========== 新增：输出线程退出时兜底清理 ==========
	std::cout << "输出线程退出，兜底清理客户端[" << info_addr << "]资源" << std::endl;
	if (param->chat_instance) {
		param->chat_instance->cleanup_client_by_info_addr(info_addr);
	}
	delete param;
	return 0;
}
DWORD WINAPI chat::time_back_message(LPVOID lpParam)//定时汇总信息函数（返回汇总后的信息）
{
	chat* obj = (chat*)lpParam;
	while (obj->is_server_running)
	{
		Sleep(10);
		if (!obj->message_cache.empty())
		{
			// 修复：锁的作用域拆分，先加cache_mutex读取，再加renew_mutex写入，统一锁顺序避免死锁
			std::vector<std::string> temp_cache;
			{
				std::lock_guard<std::mutex> lock_cache(obj->cache_mutex);
				temp_cache = obj->message_cache;
				obj->message_cache.clear();
			}

			{
				std::lock_guard<std::mutex> lock_renew(obj->renew_mutex);
				for (const auto& msg : temp_cache) {
					obj->message_for_renew += msg + "\n"; // 修复：换行符改为\n（原/n是普通字符串）
				}
			}
		};
	}
	return 0;
}
DWORD WINAPI  chat::time_renew_message(LPVOID lpParam)//定时信息刷新函数（用于向客户端发送更新的信息和待处理的请求）
{
	chat* obj = (chat*)lpParam;
	while (obj->is_server_running)
	{
		Sleep(30);
		if (!obj->message_for_renew.empty())
		{
			std::string renew = "send_message&";
			{
				std::lock_guard<std::mutex> lock_renew(obj->renew_mutex);
				renew = obj->message_for_renew;
				obj->message_for_renew.clear();
			}
			// 保护login_list（线程安全读取）
			std::lock_guard<std::mutex> lock_login(obj->login_list_mutex);
			for (auto it = obj->login_list.begin(); it != obj->login_list.end();)
			{
				ClientConn* zan = *it;
				// ========== 新增：判断连接是否有效 ==========
				if (zan->is_connected && zan->sock != INVALID_SOCKET) {
					std::lock_guard<std::mutex> lock_renew(zan->queue_mutex);
					zan->msg_queue.push(renew);
					++it; // 有效连接，迭代器后移
				}
				else {
					// 发现无效连接，直接清理
					obj->cleanup_client_conn(zan);
					it = obj->login_list.erase(it); // 从容器移除
				}
			}
			obj->msql->add_chat_messages_batch(renew);
		};
	}
	return 0;
}
void chat::message_add_on_time(std::string message)
{
	std::lock_guard<std::mutex> lock(cache_mutex);
	message_cache.push_back(message);
}
//简单解析命令（格式：login&username=xxx&password=xxx）cmd:发送信息=send_message&信息内容 按时间查询聊天记录 find_message_time&first_time=xxx&last_time=xxx  按数量查询聊天记录 find_message_num&last_time=xxx&num=xxx  查询个人信息 find_person&username=xxx
std::string chat::message_divide(std::string client_msg)//信息分类处理函数（对从客户端获取的信息进行解析，得知操作命令后进行处理）（返回操作后的信息）
{
	size_t cmd_pos = client_msg.find("&");//对于时间采取以下格式2026-04-15&00:00:00
	std::string cmd = client_msg.substr(0, cmd_pos);
	std::cout << client_msg << std::endl;
	if (cmd == "send_message")
	{
		std::string fan = client_msg.substr(cmd_pos);
		message_add_on_time(fan);
		return "success";
	}
	else if (cmd == "find_message_num")
	{
		size_t last_time_key_pos = client_msg.find("last_time=");
		size_t num_pos = client_msg.find("&num=");
		std::string last_time = client_msg.substr(last_time_key_pos + 10,  // "last_time="长度是10，跳过key取值
			num_pos - (last_time_key_pos + 10)
		);
		std::string num = client_msg.substr(num_pos + 5);
		size_t time_mid = last_time .find("&");
		std::string date = last_time.substr(0, time_mid);
		date += " " + last_time.substr(time_mid + 1);
		std::string back = "find_message_num&";
		back += msql->find_chat_message(date,std::stoi(num));
		return back;
	}
	else if(cmd == "find_message_time"){
		size_t first_time_key_pos = client_msg.find("first_time=");
		size_t last_pos = client_msg.find("&last_time=");
		std::string first_time = client_msg.substr(first_time_key_pos + 11,  // "first_time="长度是11，跳过key取值
			last_pos - (first_time_key_pos + 11));
		std::string last_time = client_msg.substr(last_pos + 11);
		size_t time_mid_fi = first_time.find("&");
		size_t time_mid_la = last_time.find("&");
		std::string ft = first_time.substr(0, time_mid_fi);
		ft += " " + first_time.substr(time_mid_fi + 1);
		std::string lt = last_time.substr(0, time_mid_la);
		lt += " " + last_time.substr(time_mid_la + 1);
		std::string  back = "find_message_time&";
		back += msql->find_chat_message_between_time(ft,lt);
		return back;
	}
	else {
		std::string username = client_msg.substr(cmd_pos + 10);
		std::string back = "find_person&";
		back += msql->find_acount_message(username);
		return back;
	}
	return "error";
}
// 新增：静态监听线程函数（异步运行add_chat_thread）
DWORD WINAPI chat::listen_thread(LPVOID lpParam)
{
	chat* chat_obj = (chat*)lpParam;
	// 调用原有监听逻辑（add_chat_thread会阻塞该线程，但不阻塞主线程）
	chat_obj->add_chat_thread();

	return 0;
}
void chat::init()
{
	// 2. 创建定时汇总线程（time_back_message）
	h_timer_sum = CreateThread(nullptr, 0, time_back_message, this, 0, nullptr);
	if (h_timer_sum == nullptr) {
		std::cerr << "定时汇总线程创建失败，错误码：" << GetLastError() << std::endl;
		return;
	}
	std::cout << "定时汇总线程创建成功" << std::endl;

	// 3. 创建定时刷新线程（time_renew_message）
	h_timer_renew = CreateThread(nullptr, 0, time_renew_message, this, 0, nullptr);
	if (h_timer_renew == nullptr) {
		std::cerr << "定时刷新线程创建失败，错误码：" << GetLastError() << std::endl;
		CloseHandle(h_timer_sum); // 回滚已创建的线程
		return;
	}
	std::cout << "定时刷新线程创建成功" << std::endl;

	// 4. 核心：异步创建通信监听线程（非阻塞）
	h_listen_thread = CreateThread(nullptr, 0, listen_thread, this, 0, nullptr);
	if (h_listen_thread == nullptr) {
		std::cerr << "[初始化失败] 通信监听线程创建失败，错误码：" << GetLastError() << std::endl;
		// 回滚已创建的线程，避免资源泄漏
		CloseHandle(h_timer_sum);
		CloseHandle(h_timer_renew);
		return;
	}
	std::cout << "[初始化] 通信监听线程创建成功（异步运行，监听8889端口）" << std::endl;
}
void chat::stop_server() {
	// 1. 标记服务器停止，让循环退出
	is_server_running = false;

	// 2. 关闭服务端Socket（触发accept退出）
	if (server_socket != INVALID_SOCKET) {
		closesocket(server_socket);
		server_socket = INVALID_SOCKET;
	}

	// 3. 关闭所有客户端Socket，通知线程退出
	{
		std::lock_guard<std::mutex> lock(login_list_mutex);
		for (auto conn : login_list) {
			if (conn->sock != INVALID_SOCKET) {
				closesocket(conn->sock);
			}
			delete conn; // 释放ClientConn对象
		}
		login_list.clear();
	}

	// 4. 等待并关闭线程句柄（避免内存泄漏）
	if (h_timer_sum != nullptr) {
		WaitForSingleObject(h_timer_sum, 1000); // 等待1秒让线程退出
		CloseHandle(h_timer_sum);
		h_timer_sum = nullptr;
	}
	if (h_timer_renew != nullptr) {
		WaitForSingleObject(h_timer_renew, 1000);
		CloseHandle(h_timer_renew);
		h_timer_renew = nullptr;
	}
	if (h_listen_thread != nullptr) {
		WaitForSingleObject(h_listen_thread, 1000);
		CloseHandle(h_listen_thread);
		h_listen_thread = nullptr;
	}

	// 5. 清理WSA环境
	WSACleanup();
}
// 核心：清理单个ClientConn的所有资源
void chat::cleanup_client_conn(ClientConn* conn) {
	if (!conn) return;

	// 1. 标记连接断开（阻止定时线程继续操作）
	conn->is_connected = false;

	// 2. 关闭Socket句柄
	if (conn->sock != INVALID_SOCKET) {
		closesocket(conn->sock);
		conn->sock = INVALID_SOCKET;
	}

	// 3. 清空消息队列（释放内存）
	{
		std::lock_guard<std::mutex> lock(conn->queue_mutex);
		while (!conn->msg_queue.empty()) {
			conn->msg_queue.pop();
		}
	}

	// 4. 释放ClientConn对象本身
	delete conn;
}

// 根据info_addr查找并清理连接（遍历login_list）
void chat::cleanup_client_by_info_addr(const std::string& info_addr) {
	std::lock_guard<std::mutex> lock(login_list_mutex);
	for (auto it = login_list.begin(); it != login_list.end();) {
		ClientConn* conn = *it;
		if (conn->info_addr == info_addr) {
			cleanup_client_conn(conn);
			it = login_list.erase(it); // 从容器中移除并删除迭代器
		}
		else {
			++it;
		}
	}
}