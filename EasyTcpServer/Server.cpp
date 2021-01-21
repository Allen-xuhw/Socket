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
	SOCKET _sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); //AF_INET表示ipv4
	if (INVALID_SOCKET == _sock)
	{
		printf("建立SOCKET失败...\n");
	}
	else
	{
		printf("成功建立SOCKET...\n");
	}

	//bind a net port
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

	//listen to the net port
	if (SOCKET_ERROR == listen(_sock, 5))
	{
		printf("监听端口时产生错误...\n");
	}
	else
	{
		printf("正在监听...\n");
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
	char _recvBuf[128] = {};
	while (true)
	{
		//receive the data from the client
		int nLen = recv(_cSock, _recvBuf, 128, 0);
		if (nLen <= 0)
		{
			printf("客户端已离开...\n");
			break;
		}
		else
		{
			printf("接收到命令:%s \n", _recvBuf);
		}

		//address the request
		if (0 == strcmp(_recvBuf, "getName"))
		{
			char msgBuf[] = "Allen";
			int bufLen = strlen(msgBuf) + 1;
			send(_cSock, msgBuf, bufLen, 0);
		}
		else if (0 == strcmp(_recvBuf, "getAge"))
		{
			char msgBuf[] = "22.";
			int bufLen = strlen(msgBuf) + 1;
			send(_cSock, msgBuf, bufLen, 0);
		}
		else
		{
			char msgBuf[] = "Hello, I am server.";
			int bufLen = strlen(msgBuf) + 1;
			send(_cSock, msgBuf, bufLen, 0);
		}
	}

	//colse the socket
	closesocket(_sock);

	WSACleanup();
	printf("已退出\n");
	getchar();

	return 0;
}