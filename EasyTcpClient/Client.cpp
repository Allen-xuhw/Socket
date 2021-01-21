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
		printf("����SOCKETʧ��...\n");
	}
	else
	{
		printf("�ɹ�����SOCKET...\n");
	}

	//connnet to the server
	sockaddr_in _sin = {};
	_sin.sin_family = AF_INET;
	_sin.sin_port = htons(4567);
	_sin.sin_addr.S_un.S_addr = inet_addr("127.0.0.1"); //INADDR_ANY��ʾ���Ȿ��������ip��ַ
	if (SOCKET_ERROR == connect(_sock, (sockaddr*)&_sin, sizeof(sockaddr_in)))
	{
		printf("���ӷ�����ʧ��...\n");
	}
	else
	{
		printf("�ɹ����ӷ�����...\n");
	}

	while (true)
	{
		//input the command
		char cmdBuf[128] = {};
		scanf_s("%s", cmdBuf,128);

		//address the command
		if (0 == strcmp(cmdBuf, "exit"))
		{
			printf("�յ��˳�����...\n");
			break;
		}
		else
		{
			send(_sock, cmdBuf, strlen(cmdBuf) + 1, 0);
			printf("�����ѷ���...\n");
		}

		//receive the information from server
		char recvBuf[128] = {};
		int nlen = recv(_sock, recvBuf, 128, 0); //recv function will return the length of the received information
		if (nlen > 0)
		{
			printf("�ɹ��������ݣ�%s \n", recvBuf);
		}
	}


	//colse the socket
	closesocket(_sock);

	WSACleanup();
	printf("���˳� \n");
	getchar();

	return 0;
}