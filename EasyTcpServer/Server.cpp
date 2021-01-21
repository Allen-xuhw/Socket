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
		printf("���ӿͻ���ʧ��...\n");
	}
	printf("�¿ͻ���: IP = %s \n", inet_ntoa(clientAddr.sin_addr));
	char _recvBuf[128] = {};
	while (true)
	{
		//receive the data from the client
		int nLen = recv(_cSock, _recvBuf, 128, 0);
		if (nLen <= 0)
		{
			printf("�ͻ������뿪...\n");
			break;
		}
		else
		{
			printf("���յ�����:%s \n", _recvBuf);
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
	printf("���˳�\n");
	getchar();

	return 0;
}