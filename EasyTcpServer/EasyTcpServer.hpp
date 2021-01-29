#ifndef _EasyTcpServer_hpp_
#define _EasyTcpServer_hpp_

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#ifdef _WIN32
#include<WinSock2.h>
#include<windows.h>
#define WIN32_LEAN_AND_MEAN
#pragma comment(lib,"ws2_32.lib")
#else
#include<unistd.h>
#include<arpa/inet.h>
#include<string.h>

#define SOCKET int
#define INVALID_SOCKET (SOCKET)(~0)
#define SOCKET_ERROR (-1)
#endif

#include<stdio.h>
#include<vector>
#include"MessageHeader.hpp"

class EasyTcpServer
{
private:
	std::vector<SOCKET> g_clients;
	SOCKET _sock;
public:
	EasyTcpServer()
	{
		_sock = INVALID_SOCKET;
	}
	virtual ~EasyTcpServer()
	{
		Close();
	}

	//初始化Socket
	void InitSocket()
	{
		//启动Win Sock 2.x环境
#ifdef _WIN32
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		if (_sock != INVALID_SOCKET) //检测是否已有连接
		{
			printf("<socket=%d>关闭旧连接...\n", (int)_sock);
			Close();
		}
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock)
		{
			printf("建立SOCKET失败...\n");
		}
		else
		{
			printf("成功建立SOCKET=<%d>...\n", (int)_sock);
		}
	}


	//绑定ip和端口号
	int Bind(const char* ip, unsigned short port)
	{
		if (INVALID_SOCKET == _sock)
		{
			InitSocket();
		}

		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(port); //host to a net unsigned short
#ifdef _WIN32
		if (ip)
		{
			_sin.sin_addr.S_un.S_addr = inet_addr(ip);
		}
		else
		{
			_sin.sin_addr.S_un.S_addr = INADDR_ANY; //INADDR_ANY表示任意本机的任意ip地址
		}
#else
		if (ip)
		{
			_sin.sin_addr.s_addr = inet_addr(ip);
		}
		else
		{
			_sin.sin_addr.s_addr = INADDR_ANY;
		}
#endif
		int ret = bind(_sock, (sockaddr*)&_sin, sizeof(_sin));
		if (SOCKET_ERROR == ret)
		{
			printf("绑定端口<%d>失败...\n", port);
		}
		else
		{
			printf("成功绑定端口<%d>...\n", port);
		}
		return ret;
	}


	//监听端口号
	int Listen(int n)
	{
		int ret = listen(_sock, n);
		if (SOCKET_ERROR == ret)
		{
			printf("<Socket=%d>错误，监听失败...\n", (int)_sock);
		}
		else
		{
			printf("<Socket=%d>正在监听...\n", (int)_sock);
		}
		return ret;
	}


	//接受客户端链接
	SOCKET Accept()
	{
		sockaddr_in clientAddr = {};
		int nAddrLen = sizeof(sockaddr_in);
		SOCKET _cSock = INVALID_SOCKET;
#ifdef _WIN32
		_cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
#else
		_cSock = accept(_sock, (sockaddr*)&clientAddr, (socklen_t*)&nAddrLen);
#endif

		if (INVALID_SOCKET == _cSock)
		{
			printf("<Socket=%d>错误，接收到无效客户端SOCKET...\n", (int)_sock);
		}
		else
		{
			printf("<Socket=%d>接收到新客户端: IP = %s, Socket = %d \n", (int)_sock, inet_ntoa(clientAddr.sin_addr), (int)_cSock);//新客户端加入
			NewUserJoin userJoin;
			SendDataToAll(&userJoin); //通知现有客户端有新客户端加入
			g_clients.push_back(_cSock);
		}
		return _cSock;
	}


	//关闭Socket
	void Close()
	{
		//关闭Win Sock 2.x环境
		if (_sock != INVALID_SOCKET) //检测是否还未连接
		{
#ifdef _WIN32
			closesocket(_sock);
			WSACleanup();
#else
			close(_sock);
#endif
			_sock = INVALID_SOCKET;
		}
	}


	//处理网络信息
	bool OnRun()
	{
		if (isRun()) //判断是否socket正在工作中
		{
			//伯克利套接字 BSD socket
			fd_set fdRead;  //描述符(socket)集合
			fd_set fdWrite;
			fd_set fdExp;
			FD_ZERO(&fdRead);
			FD_ZERO(&fdWrite);
			FD_ZERO(&fdExp);

			//将_sock加入集合中
			FD_SET(_sock, &fdRead);
			FD_SET(_sock, &fdWrite);
			FD_SET(_sock, &fdExp);
			SOCKET maxSock = _sock;

			//将上一次新加入的_csock写入可读集合中
			for (int n = (int)g_clients.size() - 1; n >= 0; n--)
			{
				FD_SET(g_clients[n], &fdRead);
				if (maxSock < g_clients[n])
				{
					maxSock = g_clients[n];
				}
			}

			//nfds是一个整数值，指fd_set集合中所有描述符(socket)的范围，而不是数量
			//即nfds是所有文件描述符中最大值+1，在windows下这个参数也设置为0
			timeval t = { 4,0 };
			int ret = select(maxSock + 1, &fdRead, &fdWrite, &fdExp, &t);
			if (ret < 0)
			{
				printf("select任务结束 \n");
				Close();
				return false;
			}

			//处理服务端建立的_sock，即接收新客户端链接
			if (FD_ISSET(_sock, &fdRead))
			{
				FD_CLR(_sock, &fdRead);
				Accept();
			}

			//处理从_sock接收到的，即来自Client的_cSock；若第n个socket退出，将其从g_clients中移除
			for (int n = (int)g_clients.size() - 1; n >= 0; n--)
			{
				if (FD_ISSET(g_clients[n], &fdRead))
				{
					if (-1 == RecvData(g_clients[n]))
					{
						auto iter = g_clients.begin(); //std::vector<SOCKET>::iterator
						iter += n;
						if (iter != g_clients.end())
						{
							g_clients.erase(iter);
						}
					}
				}
			}
			return true;
		}
		return false;
	}


	//判断socket是否工作中
	bool isRun()
	{
		return _sock != INVALID_SOCKET;
	}


	//接收数据 处理粘包 拆分包
	int RecvData(SOCKET _cSock)
	{
		//缓冲区
		char szRecv[1024] = {};

		//接收客户端请求
		int nLen = (int)recv(_cSock, szRecv, sizeof(DataHeader), 0);
		if (nLen <= 0)
		{
			printf("客户端<Socket=%d>已离开...\n", (int)_cSock);
			return -1;
		}
		DataHeader* header = (DataHeader*)szRecv;
		recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
		OnNetMsg(header, _cSock);
		return 0;
	}


	//响应网络消息
	virtual void OnNetMsg(DataHeader* header, SOCKET _cSock)
	{
		//处理客户端请求
		switch (header->cmd)
		{
		case CMD_LOGIN:
		{
			Login* login = (Login*)header;
			printf("收到<Socket=%d>请求: CMD_LOGIN, 数据长度: %d, userName: %s, Password: %s \n", (int)_cSock, login->dataLength, login->userName, login->PassWord);
			LoginResult ret;
			send(_cSock, (const char*)&ret, sizeof(LoginResult), 0);
			printf("已成功响应请求 \n");
		}
		break;
		case CMD_LOGOUT:
		{
			Logout* logout = (Logout*)header;
			printf("收到<Socket=%d>请求: CMD_LOGOUT, 数据长度: %d, userName: %s \n", (int)_cSock, logout->dataLength, logout->userName);
			LogoutResult ret;
			send(_cSock, (const char*)&ret, sizeof(LogoutResult), 0);
			printf("已成功响应请求 \n");
		}
		break;
		default:
		{
			DataHeader header = { 0,CMD_ERROR };
			send(_cSock, (char*)&header, sizeof(header), 0);
			printf("收到无效请求 \n");
		}
		}
	}


	//发送给指定Socket数据
	int SendData(SOCKET _cSock, DataHeader* header)
	{
		if (isRun() && header)
		{
			return send(_cSock, (const char*)header, header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}


	//群发数据
	void SendDataToAll(DataHeader* header)
	{
		for (int n = (int)g_clients.size() - 1; n >= 0; n--) //通知现有客户端有新客户端加入
		{
			SendData(g_clients[n], header);
		}
	}
};

#endif