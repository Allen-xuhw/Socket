#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS


#include<WinSock2.h>
#include<windows.h>
#include<iostream>

//#pragma comment(lib,"ws2_32.lib")

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
	_sin.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); //INADDR_ANY表示任意本机的任意ip地址
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
		else
		{
			send(_sock, cmdBuf, strlen(cmdBuf) + 1, 0);
			printf("数据已发送...\n");
		}

		//receive the information from server
		char recvBuf[128] = {};
		int nlen = recv(_sock, recvBuf, 128, 0); //recv function will return the length of the received information
		if (nlen > 0)
		{
			printf("成功接收数据：%s \n", recvBuf);
		}
	}


	//colse the socket
	closesocket(_sock);

	WSACleanup();
	printf("已退出 \n");
	getchar();

	return 0;
}