#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS


#include<WinSock2.h>
#include<windows.h>
#include<iostream>

//#pragma comment(lib,"ws2_32.lib")

enum CMD
{
	CMD_LOGIN,
	CMD_LOGIN_RESULT,
	CMD_LOGOUT,
	CMD_LOGOUT_RESULT,
	CMD_ERROR
};

struct DataHeader
{
	short cmd;
	short dataLength;
};

struct Login: public DataHeader
{
	Login()
	{
		dataLength = sizeof(Login);
		cmd = CMD_LOGIN;
	}
	char userName[32];
	char PassWord[32];
};

struct LoginResult: public DataHeader
{
	LoginResult()
	{
		dataLength = sizeof(LoginResult);
		cmd = CMD_LOGIN_RESULT;
	}
	int result;
};

struct Logout: public DataHeader
{
	Logout()
	{
		dataLength = sizeof(Logout);
		cmd = CMD_LOGOUT;
	}
	char userName[32];
};

struct LogoutResult: public DataHeader
{
	LogoutResult()
	{
		dataLength = sizeof(LogoutResult);
		cmd = CMD_LOGOUT_RESULT;
	}
	int result;
};

int main()
{
	//start windows socket 2.x
	WORD ver = MAKEWORD(2, 2);
	WSADATA dat;
	WSAStartup(ver, &dat);

	//build a socket
	SOCKET _sock = socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == _sock)
	{
		printf("建立SOCKET失败...\n");
	}
	else
	{
		printf("成功建立SOCKET...\n");
	}

	//connnet to the server
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

	while (true)
	{
		//input the command
		char cmdBuf[128] = {};
		scanf_s("%s", cmdBuf,128);

		//address the command
		if (0 == strcmp(cmdBuf, "exit"))
		{
			printf("收到退出命令...\n");
			break;
		}
		else if(0 == strcmp(cmdBuf, "login"))
		{
			Login login;
			strcpy_s(login.userName, "Allen");
			strcpy_s(login.PassWord, "4584");
			send(_sock, (char*)&login, sizeof(Login), 0);

			//receive the information from server
			DataHeader retHeader = {};
			LoginResult loginRet = {};
			recv(_sock, (char*)&loginRet, sizeof(LoginResult), 0);
			printf("LoginResult: %d \n", loginRet.result);
		}
		else if (0 == strcmp(cmdBuf, "logout"))
		{
			Logout logout;
			strcpy_s(logout.userName, "Allen");
			send(_sock, (char*)&logout, sizeof(Logout), 0);

			//receive the information from server
			DataHeader retHeader = {};
			LogoutResult logoutRet = {};
			recv(_sock, (char*)&logoutRet, sizeof(LogoutResult), 0);
			printf("LogoutResult: %d \n", logoutRet.result);
		}
		else
		{
			printf("不支持的命令，请重新输入...\n");
		}
	}


	//colse the socket
	closesocket(_sock);

	WSACleanup();
	printf("已退出 \n");
	getchar();

	return 0;
}