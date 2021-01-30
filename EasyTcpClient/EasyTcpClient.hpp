#ifndef _EasyTcpClient_hpp_
#define _EasyTcpClient_hpp_

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#ifdef _WIN32
	#include<WinSock2.h>
	#include<windows.h>
	#define WIN32_LEAN_AND_MEAN
	#pragma comment(lib,"ws2_32.lib")
#else
	#include<unistd.h>
	#include<arpa/inet.h>
	#include<string.h>

	#define SOCKET int
	#define INVALID_SOCKET (SOCKET)(~0)
	#define SOCKET_ERROR (-1)
#endif

#include<stdio.h>
#include"MessageHeader.hpp"

class EasyTcpClient
{
	SOCKET _sock;
public:
	EasyTcpClient()
	{
		_sock = INVALID_SOCKET;
	}

	virtual ~EasyTcpClient()
	{
		Close();
	}

	//��ʼ��socket
	SOCKET InitSocket()
	{
		//����Win Sock 2.x����
#ifdef _WIN32
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		if (_sock != INVALID_SOCKET) //����Ƿ���������
		{
			printf("<socket=%d>�رվ�����...\n", (int)_sock);
			Close();
		}
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock)
		{
			printf("����SOCKETʧ��...\n");
		}
		else
		{
			printf("�ɹ�����SOCKET=<%d>...\n", (int)_sock);
		}
		return _sock;
	}

	//���ӷ�����
	int Connect(const char* ip, unsigned short port)
	{
		if (_sock == INVALID_SOCKET) //����Ƿ��ѳ�ʼ��
		{
			InitSocket();
		}
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(port);
#ifdef _WIN32
		_sin.sin_addr.S_un.S_addr = inet_addr(ip); //NADDR_ANY��ʾ���Ȿ��������ip��ַ
#else
		_sin.sin_addr.s_addr = inet_addr(ip);
#endif
		int ret = connect(_sock, (sockaddr*)&_sin, sizeof(sockaddr_in));
		if (SOCKET_ERROR == ret)
		{
			printf("<socket=%d>���ӷ�����<%s:%d>ʧ��...\n", (int)_sock, ip, port);
		}
		else
		{
			printf("<socket=%d>�ɹ����ӷ�����<%s:%d>...\n", (int)_sock, ip, port);
		}
		return ret;
	}

	//�ر�socket
	void Close()
	{
		//�ر�Win Sock 2.x����
		if (_sock != INVALID_SOCKET) //����Ƿ�δ����
		{
#ifdef _WIN32
			closesocket(_sock);
			WSACleanup();
#else
			close(_sock);
#endif
			_sock = INVALID_SOCKET;
		}
	}

	//����������Ϣ
	bool OnRun()
	{
		if (isRun()) //�ж��Ƿ�socket���ڹ�����
		{
			fd_set fdReads;
			FD_ZERO(&fdReads);
			FD_SET(_sock, &fdReads);
			timeval t = { 0,0 };
			int ret = select(_sock, &fdReads, 0, 0, &t);
			if (ret < 0)
			{
				printf("<socket=%d>select�������\n", (int)_sock);
				Close();
				return false;
			}

			if (FD_ISSET(_sock, &fdReads))
			{
				FD_CLR(_sock, &fdReads);

				if (-1 == RecvData())
				{
					printf("<socket=%d>select�������\n", (int)_sock);
					Close();
					return false;
				}
			}
			return true;
		}
		return false;
	}

	//�ж�socket�Ƿ�����
	bool isRun()
	{
		return _sock != INVALID_SOCKET;
	}


	//�������� ����ճ�� ��ְ�
#ifndef RECV_BUFF_SIZE
	#define RECV_BUFF_SIZE 10240
#endif
	char _szRecv[RECV_BUFF_SIZE] = {}; //���ջ�����
	char _szMsgBuf[10 * RECV_BUFF_SIZE] = {}; //��Ϣ������
	int _lastPos = 0; //��Ϣ���������ݳ���

	int RecvData()
	{
		//���տͻ�������
		int nLen = (int)recv(_sock, _szRecv, RECV_BUFF_SIZE, 0);
		if (nLen <= 0)
		{
			printf("<socket=%d>�����˶Ͽ����ӣ��������...\n", (int)_sock);
			return -1;
		}

		memcpy(_szMsgBuf + _lastPos, _szRecv, nLen); //����ȡ�������ݿ�������Ϣ������
		_lastPos += nLen;

		while (_lastPos >= sizeof(DataHeader))
		{
			DataHeader* header = (DataHeader*)_szMsgBuf;
			if (_lastPos >= header->dataLength)
			{
				int nSize = _lastPos - header->dataLength; //ʣ��δ������Ϣ���������ݵĳ���
				OnNetMsg(header);
				memcpy(_szMsgBuf, _szMsgBuf + header->dataLength, nSize); //����Ϣ������δ��������ǰ��
				_lastPos = nSize;
			}
			else
			{
				break;
			}
		}
		return 0;
	}

	//��Ӧ������Ϣ
	virtual void OnNetMsg(DataHeader* header)
	{
		switch (header->cmd)
		{
		case CMD_LOGIN_RESULT:
		{
			LoginResult* loginResult = (LoginResult*)header; //������Ϣ���Ǵ�DataHeader�̳в�����
			//printf("<socket=%d>�յ��������Ϣ: CMD_LOGIN_RESULT, ���ݳ���: %d \n", (int)_sock, loginResult->dataLength);

		}
		break;
		case CMD_LOGOUT_RESULT:
		{
			LogoutResult* logoutResult = (LogoutResult*)header;
			//printf("<socket=%d>�յ��������Ϣ: CMD_LOGOUT_RESULT, ���ݳ���: %d \n", (int)_sock, logoutResult->dataLength);
		}
		break;
		case CMD_NEW_USER_JOIN:
		{
			NewUserJoin* userJoin = (NewUserJoin*)header;
			//printf("<socket=%d>�յ��������Ϣ: CMD_NEW_USER_JOIN, ���ݳ���: %d \n", (int)_sock, userJoin->dataLength);
		}
		break;
		case CMD_ERROR:
		{
			printf("<socket=%d>�յ��������Ϣ��CMD_ERROR, ���ݳ���: %d \n", (int)_sock, header->dataLength);
		}
		break;
		default:
		{
			printf("<socket=%d>�յ�δ������Ϣ, ���ݳ���: %d \n", (int)_sock, header->dataLength);
		}
		}
	}

	//��������
	int SendData(DataHeader* header)
	{
		if (isRun() && header)
		{
			return send(_sock, (const char*)header, header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}

private:
};

#endif