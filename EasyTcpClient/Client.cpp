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
		printf("FAIL TO BUILD A SOCKET...\n");
	}
	else
	{
		printf("SUCCESSFULLY BUILD A SOCKET...\n");
	}

	//connnet to the server
	sockaddr_in _sin = {};
	_sin.sin_family = AF_INET;
	_sin.sin_port = htons(4567); //host to a net unsigned short
	_sin.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); //INADDR_ANY表示任意本机的任意ip地址
	if (SOCKET_ERROR == connect(_sock, (sockaddr*)&_sin, sizeof(sockaddr_in)))
	{
		printf("ERROR CONNECTION...\n");
	}
	else
	{
		printf("SUCCESSFULLY CONNECT...\n");
	}

	//receive the information from server
	char recvBuf[256] = {};
	int nlen = recv(_sock, recvBuf, 256, 0); //recv function will return the length of the received information
	if (nlen > 0)
	{
		printf("SUCCESSFULLY RECEIVE：%s \n", recvBuf);
	}

	//close the socket
	closesocket(_sock);

	WSACleanup();
	getchar();

	return 0;
}