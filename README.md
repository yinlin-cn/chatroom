# 网络聊天室

一个基于 C/S 架构的多人在线聊天项目：服务端使用 C++ 编写，以 Windows Socket 实现 TCP 通信、以多线程支持多客户端并发、通过 MySQL C API 持久化用户与聊天数据；客户端位于 `chat_client/`，是一个 Qt Widgets 工程。

目前服务端的主体功能已经实现；`chat_client/` 中已经建立 Qt Widgets 客户端工程与主窗口骨架，登录界面、聊天界面和网络通信部分仍在实现中。整体规划与技术细节可参考 [技术文档](技术文档/网络聊天室开发架构.txt) 和 [聊天室服务器架构](技术文档/聊天室服务器架构.txt)。

## 当前功能

- 账号注册与登录校验（用户名唯一，注册时可选填生日、备注）
- 多客户端同时连接，每连接独立收发线程，互不阻塞
- 聊天消息定时汇总、广播给在线客户端
- 聊天记录批量持久化到 MySQL
- 按消息条数倒序查询历史记录
- 按起止时间区间查询历史记录
- 按用户名查询个人资料
- `chat_client/` Qt Widgets 客户端工程与主窗口骨架（登录、聊天界面与网络通信待接入）

## 技术栈

| 部分 | 技术 |
| --- | --- |
| 开发语言 | C++17 |
| 网络通信 | Windows Socket 2（TCP） |
| 并发模型 | Win32 多线程（每客户端一对收发线程 + 定时线程） |
| 数据存储 | MySQL + MySQL C API |
| 客户端 | Qt 6 Widgets（`chat_client/`） |

## 项目结构

```text
.
├── 服务器/                     # 服务端源码
│   ├── main.cpp                # 入口：初始化数据库并启动 login/chat 模块
│   ├── login.h / login.cpp     # 登录注册服务，监听 8888 端口
│   ├── chat.h / chat.cpp       # 聊天通信服务，监听 8889 端口
│   ├── datebase.h / datebase.cpp # MySQL 数据库访问层
│   └── chat.sql                # 建库建表脚本及示例语句
├── chat_client/                # Qt Widgets 客户端工程（qmake）
│   ├── chat_client.pro
│   ├── main.cpp
│   ├── widget.h / widget.cpp / widget.ui
└── 技术文档/                   # 架构设计、协议与 MySQL API 参考
```

## 服务端架构

程序启动后由 `main.cpp` 创建一个共享的 `datebase` 连接，并分别启动两个监听模块：

```text
多客户端
   │
   ├── TCP 8888 ──> login 模块   注册 / 登录校验
   └── TCP 8889 ──> chat 模块    聊天长连接
                          │
                    MySQL（chat 库）
```

`login` 模块为每个登录/注册请求创建临时处理线程，处理完即关闭连接。

`chat` 模块的线程模型：

- 监听线程持续 `accept` 新连接，并把首个数据包作为该用户的 `info_addr`
- 每个在线客户端分配一对线程：输入线程负责收包、解析、查库，输出线程从该客户端的消息队列取数据并发送
- 定时汇总线程把新消息集中到缓存，定时刷新线程按批广播给所有在线客户端，并批量写入数据库

客户端断开后，服务端会关闭 Socket、清理消息队列并从在线列表移除。

## 数据库

用 [服务器/chat.sql](服务器/chat.sql) 初始化，主要包含三张表：

| 表 | 作用 | 关键字段 |
| --- | --- | --- |
| `login` | 登录账号密码 | `username`（唯一）、`mykey` |
| `person_message` | 用户个人资料 | `username`（唯一）、`birth`、`notes` |
| `chat_message` | 聊天记录 | `username`、`message`、`ctime` |

`ctime` 使用 `DATETIME` 类型保存完整日期时间；`username`、`ctime` 都建有索引。

## 通信协议

前后端使用轻量文本协议，请求以 `指令&参数=值&参数=值` 形式拼接，响应以指令开头、用 `&key=value` 与换行分隔记录。

### 登录 / 注册（端口 8888）

```text
登录：
cmd=login&username=xxx&password=xxx

注册：
cmd=register&username=xxx&password=xxx&birth=yyyy-MM-dd&notes=xxx
```

成功响应：

```text
success=1&info_addr=用户名
```

失败响应：

```text
success=0
```

### 聊天通信（端口 8889）

连接 8889 后，客户端需要先发送登录阶段获得的 `info_addr`。时间格式使用 `日期&时间`（例如 `2026-09-06&12:00:00`），发送与查询指令如下：

```text
发送消息：
send_message&username=xxx&message=xxx&ctime=2026-09-06&12:00:00

按数量查询（截止时间往前取 num 条）：
find_message_num&last_time=2026-09-06&12:00:00&num=20

按时间区间查询：
find_message_time&first_time=2026-09-01&00:00:00&last_time=2026-09-06&23:59:59

查询个人资料：
find_person&username=xxx
```

新连接建立后，服务端会先下发数据库中最新的聊天时间，供客户端做增量同步。

## 编译与运行

### 服务端

环境要求：Windows、支持 C++17 的 MSVC 编译器、MySQL（含开发头文件与 `libmysql` 库）。

1. 执行 [服务器/chat.sql](服务器/chat.sql) 创建 `chat` 数据库及三张表：
   ```bash
   mysql -uroot -p < 服务器/chat.sql
   ```
2. 修改 [服务器/main.cpp](服务器/main.cpp) 中的数据库连接配置：
   ```cpp
   datebase sql("localhost", "root", "你的MySQL密码", "chat");
   ```
3. 在工程中包含 MySQL 的 include 目录，链接 `libmysql.lib` 与 `ws2_32.lib`，编译并运行服务端。

服务端会输出初始化日志，登录模块监听 `8888`，聊天模块监听 `8889`。入口处使用 `while(true){}` 保持进程运行，停止服务端时直接结束进程即可。

### 客户端

使用 Qt Creator 打开 [chat_client/chat_client.pro](chat_client/chat_client.pro) 即可编译。`chat_client/` 就是本项目的 Qt 客户端：目前 `Widget` 是程序的主窗口，代码中还没有接入登录、聊天界面与 Socket 通信逻辑。

## 待办与可扩展方向

- 完成 Qt 登录/注册界面
- 完成 Qt 聊天室界面与消息增量刷新
- 客户端接入 Socket 收发线程并解析协议
- 支持修改个人资料、好友等规划功能
- 将数据库配置与口令移到配置文件，避免硬编码在源码中
- SQL 改为参数化语句，降低注入风险
- 增加服务端优雅退出机制，替代 `while(true)`
