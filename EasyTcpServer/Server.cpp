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
		printf("FAIL TO BUILD A SOCKET...\n");
	}
	else
	{
		printf("SUCCESSFULLY BUILD A SOCKET...\n");
	}

	//bind a net port
	sockaddr_in _sin = {};
	_sin.sin_family = AF_INET;
	_sin.sin_port = htons(4567); //host to a net unsigned short
	_sin.sin_addr.S_un.S_addr = INADDR_ANY; //INADDR_ANY表示任意本机的任意ip地址
	if (SOCKET_ERROR == bind(_sock, (sockaddr*)&_sin, sizeof(_sin)))
	{
		printf("AN ERROR OCCURED IN BINDING...\n");
	}
	else
	{
		printf("SUCCESSFULLY BIND...\n");
	}

	//listen to the net port
	if (SOCKET_ERROR == listen(_sock, 5))
	{
		printf("AN ERROR OCCURED IN LISTENING...\n");
	}
	else
	{
		printf("LISTENING...\n");
	}

	//accept the connection
	sockaddr_in clientAddr = {};
	int nAddrLen = sizeof(sockaddr_in);
	SOCKET _cSock = INVALID_SOCKET;
	char msgBuf[] = "Hello, I'm Server.";
	int bufLen = strlen(msgBuf) + 1;
	while (true)
	{
		_cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
		if (INVALID_SOCKET == _cSock)
		{
			printf("FAIL TO ACCEPT THE CLIENT...\n");
		}
		printf("a new client: IP = %s \n", inet_ntoa(clientAddr.sin_addr));
		//send a data to the client
		send(_cSock, msgBuf, bufLen, 0); //0 是结尾符
	}

	//colse the socket
	closesocket(_sock);

	WSACleanup();

	return 0;
}