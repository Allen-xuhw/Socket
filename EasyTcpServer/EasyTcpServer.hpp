#ifndef _EasyTcpServer_hpp_
#define _EasyTcpServer_hpp_

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
#include<vector>
#include"MessageHeader.hpp"

class EasyTcpServer
{
private:
	std::vector<SOCKET> g_clients;
	SOCKET _sock;
public:
	EasyTcpServer()
	{
		_sock = INVALID_SOCKET;
	}
	virtual ~EasyTcpServer()
	{
		Close();
	}

	//��ʼ��Socket
	void InitSocket()
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
	}


	//��ip�Ͷ˿ں�
	int Bind(const char* ip, unsigned short port)
	{
		if (INVALID_SOCKET == _sock)
		{
			InitSocket();
		}

		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(port); //host to a net unsigned short
#ifdef _WIN32
		if (ip)
		{
			_sin.sin_addr.S_un.S_addr = inet_addr(ip);
		}
		else
		{
			_sin.sin_addr.S_un.S_addr = INADDR_ANY; //INADDR_ANY��ʾ���Ȿ��������ip��ַ
		}
#else
		if (ip)
		{
			_sin.sin_addr.s_addr = inet_addr(ip);
		}
		else
		{
			_sin.sin_addr.s_addr = INADDR_ANY;
		}
#endif
		int ret = bind(_sock, (sockaddr*)&_sin, sizeof(_sin));
		if (SOCKET_ERROR == ret)
		{
			printf("�󶨶˿�<%d>ʧ��...\n", port);
		}
		else
		{
			printf("�ɹ��󶨶˿�<%d>...\n", port);
		}
		return ret;
	}


	//�����˿ں�
	int Listen(int n)
	{
		int ret = listen(_sock, n);
		if (SOCKET_ERROR == ret)
		{
			printf("<Socket=%d>���󣬼���ʧ��...\n", (int)_sock);
		}
		else
		{
			printf("<Socket=%d>���ڼ���...\n", (int)_sock);
		}
		return ret;
	}


	//���ܿͻ�������
	SOCKET Accept()
	{
		sockaddr_in clientAddr = {};
		int nAddrLen = sizeof(sockaddr_in);
		SOCKET _cSock = INVALID_SOCKET;
#ifdef _WIN32
		_cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
#else
		_cSock = accept(_sock, (sockaddr*)&clientAddr, (socklen_t*)&nAddrLen);
#endif

		if (INVALID_SOCKET == _cSock)
		{
			printf("<Socket=%d>���󣬽��յ���Ч�ͻ���SOCKET...\n", (int)_sock);
		}
		else
		{
			printf("<Socket=%d>���յ��¿ͻ���: IP = %s, Socket = %d \n", (int)_sock, inet_ntoa(clientAddr.sin_addr), (int)_cSock);//�¿ͻ��˼���
			NewUserJoin userJoin;
			SendDataToAll(&userJoin); //֪ͨ���пͻ������¿ͻ��˼���
			g_clients.push_back(_cSock);
		}
		return _cSock;
	}


	//�ر�Socket
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
			//�������׽��� BSD socket
			fd_set fdRead;  //������(socket)����
			fd_set fdWrite;
			fd_set fdExp;
			FD_ZERO(&fdRead);
			FD_ZERO(&fdWrite);
			FD_ZERO(&fdExp);

			//��_sock���뼯����
			FD_SET(_sock, &fdRead);
			FD_SET(_sock, &fdWrite);
			FD_SET(_sock, &fdExp);
			SOCKET maxSock = _sock;

			//����һ���¼����_csockд��ɶ�������
			for (int n = (int)g_clients.size() - 1; n >= 0; n--)
			{
				FD_SET(g_clients[n], &fdRead);
				if (maxSock < g_clients[n])
				{
					maxSock = g_clients[n];
				}
			}

			//nfds��һ������ֵ��ָfd_set����������������(socket)�ķ�Χ������������
			//��nfds�������ļ������������ֵ+1����windows���������Ҳ����Ϊ0
			timeval t = { 4,0 };
			int ret = select(maxSock + 1, &fdRead, &fdWrite, &fdExp, &t);
			if (ret < 0)
			{
				printf("select������� \n");
				Close();
				return false;
			}

			//�������˽�����_sock���������¿ͻ�������
			if (FD_ISSET(_sock, &fdRead))
			{
				FD_CLR(_sock, &fdRead);
				Accept();
			}

			//�����_sock���յ��ģ�������Client��_cSock������n��socket�˳��������g_clients���Ƴ�
			for (int n = (int)g_clients.size() - 1; n >= 0; n--)
			{
				if (FD_ISSET(g_clients[n], &fdRead))
				{
					if (-1 == RecvData(g_clients[n]))
					{
						auto iter = g_clients.begin(); //std::vector<SOCKET>::iterator
						iter += n;
						if (iter != g_clients.end())
						{
							g_clients.erase(iter);
						}
					}
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
	int RecvData(SOCKET _cSock)
	{
		//������
		char szRecv[1024] = {};

		//���տͻ�������
		int nLen = (int)recv(_cSock, szRecv, sizeof(DataHeader), 0);
		if (nLen <= 0)
		{
			printf("�ͻ���<Socket=%d>���뿪...\n", (int)_cSock);
			return -1;
		}
		DataHeader* header = (DataHeader*)szRecv;
		recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
		OnNetMsg(header, _cSock);
		return 0;
	}


	//��Ӧ������Ϣ
	virtual void OnNetMsg(DataHeader* header, SOCKET _cSock)
	{
		//����ͻ�������
		switch (header->cmd)
		{
		case CMD_LOGIN:
		{
			Login* login = (Login*)header;
			printf("�յ�<Socket=%d>����: CMD_LOGIN, ���ݳ���: %d, userName: %s, Password: %s \n", (int)_cSock, login->dataLength, login->userName, login->PassWord);
			LoginResult ret;
			send(_cSock, (const char*)&ret, sizeof(LoginResult), 0);
			printf("�ѳɹ���Ӧ���� \n");
		}
		break;
		case CMD_LOGOUT:
		{
			Logout* logout = (Logout*)header;
			printf("�յ�<Socket=%d>����: CMD_LOGOUT, ���ݳ���: %d, userName: %s \n", (int)_cSock, logout->dataLength, logout->userName);
			LogoutResult ret;
			send(_cSock, (const char*)&ret, sizeof(LogoutResult), 0);
			printf("�ѳɹ���Ӧ���� \n");
		}
		break;
		default:
		{
			DataHeader header = { 0,CMD_ERROR };
			send(_cSock, (char*)&header, sizeof(header), 0);
			printf("�յ���Ч���� \n");
		}
		}
	}


	//���͸�ָ��Socket����
	int SendData(SOCKET _cSock, DataHeader* header)
	{
		if (isRun() && header)
		{
			return send(_cSock, (const char*)header, header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}


	//Ⱥ������
	void SendDataToAll(DataHeader* header)
	{
		for (int n = (int)g_clients.size() - 1; n >= 0; n--) //֪ͨ���пͻ������¿ͻ��˼���
		{
			SendData(g_clients[n], header);
		}
	}
};

#endif