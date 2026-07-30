#pragma once
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
class chat;
struct ClientConn {
	SOCKET sock;
	std::string info_addr;// 当前客户端的用户标识（比如登录后的唯一ID）
	std::atomic<bool> is_connected;
	std::queue<std::string> msg_queue; // 输入线程塞数据，输出线程取数据
	std::mutex queue_mutex;    // 仅保护msg_queue的push/pop（轻量锁）``	
};
struct ThreadParam {
	chat* chat_instance;  // 指向chat类实例（访问类成员/函数）
	SOCKET client_sock;   // 当前客户端的Socket（关键：区分不同客户端）
	std::string info_addr;// 当前客户端的用户标识（比如登录后的唯一ID）
	std::atomic<bool> is_running;
	// 核心：输入→输出的专属通信队列的指针（点对点）
	std::queue<std::string>* msg_queue; // 输入线程塞数据，输出线程取数据
	std::mutex* queue_mutex;    // 仅保护msg_queue的push/pop（轻量锁）
};
class chat 
{
	private:
		datebase* msql;

		std::vector<ClientConn*> login_list;////用于存储连接的客户端信息及对应线程的信息队列和互斥锁
		std::mutex login_list_mutex;

		std::vector<std::string> message_cache;//用于定时汇总信息函数、线程信息按时存储函数
		std::mutex cache_mutex;

		std::string message_for_renew;//用于定时汇总信息函数、定时信息刷新函数、信息分时输出管理函数
		std::mutex renew_mutex;

		std::vector<HANDLE> thread_cache;//用于定时信息刷新函数,存储各个输出线程的线程句柄
		std::mutex thread_cache_mutex;

		// 线程句柄（用于线程管理函数操作线程）
		HANDLE h_timer_sum;    // 定时汇总信息线程
		HANDLE h_timer_renew;  // 定时信息刷新线程
		HANDLE h_thread_monitor;// 线程管理线程
		HANDLE h_listen_thread;

		// 通信监听Socket
		SOCKET server_socket;
		const u_short port = 8889;
		std::atomic<bool> is_server_running;
	public:
		chat(datebase*);
		~chat();
		void stop_server();
		void init();
		void add_chat_thread();//线程创建监听函数
		static DWORD WINAPI in_thread(LPVOID lpParam);//通信线程输入函数//主程序
		static DWORD WINAPI out_thread(LPVOID lpParam);//通信线程输出函数//主程序
		static DWORD WINAPI time_back_message(LPVOID lpParam);//定时汇总信息函数（返回汇总后的信息）
		//static DWORD WINAPI thread_monitor(LPVOID lpParam);//线程管理函数（用于对线程进行操作）暂时不使用
		void message_add_on_time(std::string);//线程信息按时存储函数（用于准确存储用户信息）
		std::string message_divide(std::string client_msg);//信息分类处理函数（对从客户端获取的信息进行解析，得知操作命令后进行处理）（返回操作后的信息）
		//std::string message_out_on_time();//信息分时输出管理函数（返回要输出的信息）已内置于输出线程
		static DWORD WINAPI  time_renew_message(LPVOID lpParam);//定时信息刷新函数（用于向客户端发送更新的信息和待处理的请求）
		static DWORD WINAPI listen_thread(LPVOID lpParam);
		// 清理单个客户端连接（线程安全）
		void cleanup_client_conn(ClientConn* conn);
		// 根据info_addr查找并清理连接（供外部调用）
		void cleanup_client_by_info_addr(const std::string& info_addr);
};