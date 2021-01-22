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

int main()
{
	//start windows socket 2.x
	WORD ver = MAKEWORD(2, 2);
	WSADATA dat;
	WSAStartup(ver, &dat);

	//build a socket
	SOCKET _sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); //AF_INET��ʾipv4
	if (INVALID_SOCKET == _sock)
	{
		printf("����SOCKETʧ��...\n");
	}
	else
	{
		printf("�ɹ�����SOCKET...\n");
	}

	//bind a net port
	sockaddr_in _sin = {};
	_sin.sin_family = AF_INET;
	_sin.sin_port = htons(4567); //host to a net unsigned short
	_sin.sin_addr.S_un.S_addr = INADDR_ANY; //INADDR_ANY��ʾ���Ȿ��������ip��ַ
	if (SOCKET_ERROR == bind(_sock, (sockaddr*)&_sin, sizeof(_sin)))
	{
		printf("�󶨶˿�ʧ��...\n");
	}
	else
	{
		printf("�ɹ��󶨶˿�...\n");
	}

	//listen to the net port
	if (SOCKET_ERROR == listen(_sock, 5))
	{
		printf("�����˿�ʱ��������...\n");
	}
	else
	{
		printf("���ڼ���...\n");
	}

	//accept the connection
	sockaddr_in clientAddr = {};
	int nAddrLen = sizeof(sockaddr_in);
	SOCKET _cSock = INVALID_SOCKET;
	_cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
	if (INVALID_SOCKET == _cSock)
	{
		printf("连接客户端失败...\n");
	}
	printf("新客户端: IP = %s \n", inet_ntoa(clientAddr.sin_addr));
	while (true)
	{
		//receive the data from the client
		DataHeader header = { };
		int nLen = recv(_cSock, (char*)&header, sizeof(DataHeader), 0);
		if (nLen <= 0)
		{
			printf("客户端已离开...\n");
			break;
		}
		else
		{
			printf("接收到命令:%d， 数据长度:%d \n", header.cmd, header.dataLength);
		}

		//address the request
		switch (header.cmd)
		{
			case CMD_LOGIN:
			{
				Login login = {};
				int nLen = recv(_cSock, (char*)&login + sizeof(DataHeader), sizeof(Login) - sizeof(DataHeader), 0);
				printf("收到命令: CMD_LOGIN, 数据长度: %d, userName: %s, Password: %s \n", login.dataLength, login.userName, login.PassWord);
				LoginResult ret;
				send(_cSock, (const char*)&ret, sizeof(LoginResult), 0);
				printf("已成功响应命令 \n");
			}
			break;
			case CMD_LOGOUT:
			{
				Logout logout = {};
				int nLen = recv(_cSock, (char*)&logout + sizeof(DataHeader), sizeof(Logout) - sizeof(DataHeader), 0);
				printf("收到命令: CMD_LOGOUT, 数据长度: %d, userName: %s \n", logout.dataLength, logout.userName);
				LogoutResult ret;
				send(_cSock, (const char*)&ret, sizeof(LogoutResult), 0);
				printf("已成功响应命令 \n");
			}
			break;
			default:
			{
				printf("收到无效命令 \n");
			}
		}

	}

	//colse the socket
	closesocket(_sock);

	WSACleanup();
	printf("���˳�\n");
	getchar();

	return 0;
}