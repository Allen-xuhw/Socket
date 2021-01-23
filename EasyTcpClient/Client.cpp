#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS


#include<WinSock2.h>
#include<windows.h>
#include<iostream>
#include<thread>

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

struct NewUserJoin : public DataHeader
{
	NewUserJoin()
	{
		dataLength = sizeof(NewUserJoin);
		cmd = CMD_NEW_USER_JOIN;
		Socket_ID = 0;
	}
	int Socket_ID;
};


int processor(SOCKET _cSock)
{
	//缓冲区
	char szRecv[1024] = {};

	//接收客户端请求
	int nLen = recv(_cSock, szRecv, sizeof(DataHeader), 0);
	if (nLen <= 0)
	{
		printf("与服务端断开连接，任务结束...\n", _cSock);
		return -1;
	}
	DataHeader* header = (DataHeader*)szRecv;

	//处理客户端请求
	switch (header->cmd)
	{
		case CMD_LOGIN_RESULT:
		{
			int nLen = recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
			LoginResult* loginResult = (LoginResult*)szRecv;
			printf("收到服务端消息: CMD_LOGIN_RESULT, 数据长度: %d \n", loginResult->dataLength);

		}
		break;
		case CMD_LOGOUT_RESULT:
		{
			int nLen = recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
			LogoutResult* logoutResult = (LogoutResult*)szRecv;
			printf("收到服务端消息: CMD_LOGOUT_RESULT, 数据长度: %d \n", logoutResult->dataLength);
		}
		break;
		case CMD_NEW_USER_JOIN:
		{
			int nLen = recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
			NewUserJoin* userJoin = (NewUserJoin*)szRecv;
			printf("收到服务端消息: CMD_NEW_USER_JOIN, 数据长度: %d \n", userJoin->dataLength);
		}
		break;
	}
	return 0;
}


bool g_bRun = true;


void cmdThread(SOCKET _sock)
{
	while (true)
	{
		char cmdBuf[128] = {};
		scanf_s("%s", cmdBuf, 128);

		//address the command
		if (0 == strcmp(cmdBuf, "exit"))
		{
			g_bRun = false;
			printf("收到退出命令...\n");
			return;
		}
		else if (0 == strcmp(cmdBuf, "login"))
		{
			Login login;
			strcpy_s(login.userName, "Allen");
			strcpy_s(login.PassWord, "4584");
			send(_sock, (char*)&login, sizeof(Login), 0);
		}
		else if (0 == strcmp(cmdBuf, "logout"))
		{
			Logout logout;
			strcpy_s(logout.userName, "Allen");
			send(_sock, (char*)&logout, sizeof(Logout), 0);
		}
		else
		{
			printf("不支持的命令，请重新输入... \n");
		}
	}
}


int main()
{
	//start windows socket 2.x
	WORD ver = MAKEWORD(2, 2);
	WSADATA dat;
	WSAStartup(ver, &dat);

	//建立socket
	SOCKET _sock = socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == _sock)
	{
		printf("建立SOCKET失败...\n");
	}
	else
	{
		printf("成功建立SOCKET...\n");
	}

	//连接到服务器
	sockaddr_in _sin = {};
	_sin.sin_family = AF_INET;
	_sin.sin_port = htons(4567);
	_sin.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); //NADDR_ANY表示任意本机的任意ip地址
	if (SOCKET_ERROR == connect(_sock, (sockaddr*)&_sin, sizeof(sockaddr_in)))
	{
		printf("连接服务器失败...\n");
	}
	else
	{
		printf("成功连接服务器...\n");
	}

	//启动线程
	std::thread t1(cmdThread, _sock);
	t1.detach();

	while (g_bRun)
	{
		fd_set fdReads;
		FD_ZERO(&fdReads);
		FD_SET(_sock, &fdReads);
		timeval t = { 4,0 };
		int ret = select(_sock, &fdReads, 0, 0, &t);
		if (ret < 0)
		{
			printf("select任务结束\n");
			break;
		}

		if (FD_ISSET(_sock, &fdReads))
		{
			FD_CLR(_sock, &fdReads);

			if (-1 == processor(_sock))
			{
				printf("select任务结束\n");
				break;
			}
		}

		printf("空闲时间处理其他业务... \n");
	}


	//关闭socket
	closesocket(_sock);

	WSACleanup();
	printf("已退出 \n");
	getchar();

	return 0;
}