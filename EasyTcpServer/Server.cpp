#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS


#include<WinSock2.h>
#include<windows.h>
#include<iostream>
#include<vector>

//#pragma comment(lib,"ws2_32.lib")

enum CMD
{
	CMD_LOGIN,
	CMD_LOGIN_RESULT,
	CMD_LOGOUT,
	CMD_LOGOUT_RESULT,
	CMD_NEW_USER_JOIN,
	CMD_ERROR
};

struct DataHeader
{
	short cmd;
	short dataLength;
};

struct Login : public DataHeader
{
	Login()
	{
		dataLength = sizeof(Login);
		cmd = CMD_LOGIN;
	}
	char userName[32];
	char PassWord[32];
};

struct LoginResult : public DataHeader
{
	LoginResult()
	{
		dataLength = sizeof(LoginResult);
		cmd = CMD_LOGIN_RESULT;
		result = 0;
	}
	int result;
};

struct Logout : public DataHeader
{
	Logout()
	{
		dataLength = sizeof(Logout);
		cmd = CMD_LOGOUT;
	}
	char userName[32];
};

struct LogoutResult : public DataHeader
{
	LogoutResult()
	{
		dataLength = sizeof(LogoutResult);
		cmd = CMD_LOGOUT_RESULT;
		result = 0;
	}
	int result;
};

struct NewUserJoin: public DataHeader
{
	NewUserJoin()
	{
		dataLength = sizeof(NewUserJoin);
		cmd = CMD_NEW_USER_JOIN;
		Socket_ID = 0;
	}
	int Socket_ID;
};

std::vector<SOCKET> g_clients;


int processor(SOCKET _cSock)
{
	//缓冲区
	char szRecv[1024] = {};

	//接收客户端请求
	int nLen = recv(_cSock, szRecv, sizeof(DataHeader), 0);
	if (nLen <= 0)
	{
		printf("客户端<Socket=%d>已离开...\n", _cSock);
		return -1;
	}
	DataHeader* header = (DataHeader*)szRecv;

	//处理客户端请求
	switch (header->cmd)
	{
		case CMD_LOGIN:
		{
			int nLen = recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
			Login* login = (Login*)szRecv;
			printf("收到<Socket=%d>请求: CMD_LOGIN, 数据长度: %d, userName: %s, Password: %s \n", _cSock, login->dataLength, login->userName, login->PassWord);
			LoginResult ret;
			send(_cSock, (const char*)&ret, sizeof(LoginResult), 0);
			printf("已成功响应请求 \n");
		}
		break;
		case CMD_LOGOUT:
		{
			int nLen = recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
			Logout* logout = (Logout*)szRecv;
			printf("收到<Socket=%d>请求: CMD_LOGOUT, 数据长度: %d, userName: %s \n", _cSock, logout->dataLength, logout->userName);
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
	return 0;
}


int main()
{
	//start windows socket 2.x
	WORD ver = MAKEWORD(2, 2);
	WSADATA dat;
	WSAStartup(ver, &dat);

	//建立_sock
	SOCKET _sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); //AF_INET表示ipv4
	if (INVALID_SOCKET == _sock)
	{
		printf("建立SOCKET失败...\n");
	}
	else
	{
		printf("成功建立SOCKET...\n");
	}

	//绑定网络端口
	sockaddr_in _sin = {};
	_sin.sin_family = AF_INET;
	_sin.sin_port = htons(4567); //host to a net unsigned short
	_sin.sin_addr.S_un.S_addr = INADDR_ANY; //INADDR_ANY表示任意本机的任意ip地址
	if (SOCKET_ERROR == bind(_sock, (sockaddr*)&_sin, sizeof(_sin)))
	{
		printf("绑定端口失败...\n");
	}
	else
	{
		printf("成功绑定端口...\n");
	}

	//监听网络端口
	if (SOCKET_ERROR == listen(_sock, 5))
	{
		printf("监听失败...\n");
	}
	else
	{
		printf("正在监听...\n");
	}


	while (true)
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

		//将上一次新加入的_csock写入可读集合中
		for(int n = g_clients.size() - 1; n >= 0; n--) 
		{
			FD_SET(g_clients[n], &fdRead);
		}

		//nfds是一个整数值，指fd_set集合中所有描述符(socket)的范围，而不是数量
		//即nfds是所有文件描述符中最大值+1，在windows下这个参数也设置为0
		timeval t = { 4,0 };
		int ret = select(_sock + 1, &fdRead, &fdWrite, &fdExp, &t);
		if (ret < 0)
		{
			printf("select任务结束 \n");
			break;
		}

		//处理服务端建立的_sock，即接收新客户端链接
		if (FD_ISSET(_sock, &fdRead))
		{
			FD_CLR(_sock, &fdRead);
			//接收新客户端链接
			sockaddr_in clientAddr = {};
			int nAddrLen = sizeof(sockaddr_in);
			SOCKET _cSock = INVALID_SOCKET;
			_cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
			if (INVALID_SOCKET == _cSock)
			{
				printf("错误，接收到无效客户端SOCKET...\n");
			}
			else
			{
				printf("新客户端: IP = %s, Socket = %d \n", inet_ntoa(clientAddr.sin_addr), _cSock);//新客户端加入
				for (int n = g_clients.size() - 1; n >= 0; n--) //通知现有客户端有新客户端加入
				{
					NewUserJoin userJoin;
					send(g_clients[n], (const char*)&userJoin, sizeof(NewUserJoin), 0);
				}
				g_clients.push_back(_cSock);
			}
		}

		//处理从_sock接收到的，即来自Client的_cSock；若第n个socket退出，将其从g_clients中移除
		for (int n = 0; n < fdRead.fd_count; n++)
		{
			if (-1 == processor(fdRead.fd_array[n]))
			{
				auto iter = find(g_clients.begin(), g_clients.end(), fdRead.fd_array[n]);
				if (iter != g_clients.end())
				{
					g_clients.erase(iter);
				}
			}
		}

		printf("空闲时间处理其他业务... \n");
	}

	for (int n = g_clients.size() - 1; n >= 0; n--)
	{
		closesocket(g_clients[n]);
	}
	closesocket(_sock);

	WSACleanup();
	printf("已退出\n");
	getchar();

	return 0;
}