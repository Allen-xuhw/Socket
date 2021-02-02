#ifndef _EasyTcpServer_hpp_
#define _EasyTcpServer_hpp_

#ifdef _WIN32
	#define FD_SETSIZE 4024
	#include<WinSock2.h>
	#include<windows.h>
	#define WIN32_LEAN_AND_MEAN
	#define _WINSOCK_DEPRECATED_NO_WARNINGS
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
#include"CELLTimestamp.hpp"

#ifndef RECV_BUFF_SIZE
	#define RECV_BUFF_SIZE 10240
#endif

class ClientSocket
{
private:
	SOCKET _sockfd; 
	char _szMsgBuf[10 * RECV_BUFF_SIZE] = {}; //��Ϣ������
	int _lastPos = 0; //��Ϣ���������ݳ���
public:
	ClientSocket(SOCKET sockfd = INVALID_SOCKET)
	{
		_sockfd = sockfd;
		memset(_szMsgBuf, 0, sizeof(_szMsgBuf));
		_lastPos = 0;
	}

	SOCKET sockfd()
	{
		return _sockfd;
	}

	char* msgBuf()
	{
		return _szMsgBuf;
	}

	int getLastPos()
	{
		return _lastPos;
	}

	void setLastPos(int pos)
	{
		_lastPos = pos;
	}
};

class EasyTcpServer
{
private:
	std::vector<ClientSocket*> _clients; //����ָ����ʽ�����Ա��������new���ɵı���λ�ڶ��У����ᵼ�±�ջ
	SOCKET _sock;
	CELLTimestamp _tTime;
	int _recvCount;
public:
	EasyTcpServer()
	{
		_sock = INVALID_SOCKET;
		_recvCount = 0;
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
		SOCKET cSock = INVALID_SOCKET;
#ifdef _WIN32
		cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
#else
		cSock = accept(_sock, (sockaddr*)&clientAddr, (socklen_t*)&nAddrLen);
#endif

		if (INVALID_SOCKET == cSock)
		{
			printf("<Socket=%d>���󣬽��յ���Ч�ͻ���SOCKET...\n", (int)_sock);
		}
		else
		{
			//printf("<Socket=%d>���յ��¿ͻ���<socket=%d>: IP = %s, Socket = %d \n", (int)_sock, (int)_clients.size(), inet_ntoa(clientAddr.sin_addr), (int)cSock);//�¿ͻ��˼���
			//NewUserJoin userJoin;
			//SendDataToAll(&userJoin); //֪ͨ���пͻ������¿ͻ��˼���
			_clients.push_back(new ClientSocket(cSock));
		}
		return cSock;
	}


	//�ر�Socket
	void Close()
	{
		//�ر�Win Sock 2.x����
		if (_sock != INVALID_SOCKET) //����Ƿ�δ����
		{
#ifdef _WIN32
			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				closesocket(_clients[n]->sockfd());
				delete _clients[n];
			}
			closesocket(_sock);
			WSACleanup();
#else
			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				close(_clients[n]->sockfd());
				delete _clients[n];
			}
			close(_sock);
#endif
			_sock = INVALID_SOCKET;
			_clients.clear();
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
			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				FD_SET(_clients[n]->sockfd(), &fdRead);
				if (maxSock < _clients[n]->sockfd())
				{
					maxSock = _clients[n]->sockfd();
				}
			}

			//nfds��һ������ֵ��ָfd_set����������������(socket)�ķ�Χ������������
			//��nfds�������ļ������������ֵ+1����windows���������Ҳ����Ϊ0
			timeval t = { 1,0 };
			int ret = select(maxSock + 1, &fdRead, &fdWrite, &fdExp, &t);
			if (ret < 0)
			{
				printf("select������� \n");
				Close();
				return false;
			}

			//�ж�_sock�Ƿ���Read������
			if (FD_ISSET(_sock, &fdRead))
			{
				FD_CLR(_sock, &fdRead);
				Accept();
				return true;
			}

			//�����_sock���յ��ģ�������Client��_cSock������n��socket�˳��������_clients���Ƴ�
			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				if (FD_ISSET(_clients[n]->sockfd(), &fdRead))
				{
					if (-1 == RecvData(_clients[n]))
					{
						auto iter = _clients.begin(); //std::vector<SOCKET>::iterator
						iter += n;
						if (iter != _clients.end())
						{
							delete _clients[n];
							_clients.erase(iter);
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


	//������
	char _szRecv[RECV_BUFF_SIZE] = {};

	//�������� ����ճ�� ��ְ�
	int RecvData(ClientSocket* pClient)
	{
		//���տͻ�������
		int nLen = (int)recv(pClient->sockfd(), _szRecv, RECV_BUFF_SIZE, 0);
		if (nLen <= 0)
		{
			printf("�ͻ���<Socket=%d>���뿪...\n", (int)pClient->sockfd());
			return -1;
		}

		memcpy(pClient->msgBuf() + pClient->getLastPos(), _szRecv, nLen); //����ȡ�������ݿ�������Ϣ������
		pClient->setLastPos(pClient->getLastPos() + nLen);

		while (pClient->getLastPos() >= sizeof(DataHeader))
		{
			DataHeader* header = (DataHeader*)pClient->msgBuf();
			if (pClient->getLastPos() >= header->dataLength)
			{
				int nSize = pClient->getLastPos() - header->dataLength; //ʣ��δ������Ϣ���������ݵĳ���
				OnNetMsg(header, pClient->sockfd());
				memcpy(pClient->msgBuf(), pClient->msgBuf() + header->dataLength, nSize); //����Ϣ������δ��������ǰ��
				pClient->setLastPos(nSize);
			}
			else
			{
				break;
			}
		}
		return 0;
	}


	//��Ӧ������Ϣ
	virtual void OnNetMsg(DataHeader* header, SOCKET cSock)
	{
		//����ͻ�������

		_recvCount++;
		auto t1 = _tTime.getElapsedSecond();
		if (_tTime.getElapsedSecond() >= 1.0)
		{
			printf("time<%1f>,socket<%d>,clients<%d>,recvCount<%d>\n", t1, (int)_sock,(int)_clients.size(), _recvCount);
			_tTime.update();
			_recvCount = 0;
		}

		switch (header->cmd)
		{
		case CMD_LOGIN:
		{
			Login* login = (Login*)header;
			//printf("�յ�<Socket=%d>����: CMD_LOGIN, ���ݳ���: %d, userName: %s, Password: %s \n", (int)cSock, login->dataLength, login->userName, login->PassWord);
			//LoginResult ret;
			//send(cSock, (const char*)&ret, sizeof(LoginResult), 0);
			//printf("�ѳɹ���Ӧ���� \n");
		}
		break;
		case CMD_LOGOUT:
		{
			Logout* logout = (Logout*)header;
			//printf("�յ�<Socket=%d>����: CMD_LOGOUT, ���ݳ���: %d, userName: %s \n", (int)cSock, logout->dataLength, logout->userName);
			//LogoutResult ret;
			//send(cSock, (const char*)&ret, sizeof(LogoutResult), 0);
			//printf("�ѳɹ���Ӧ���� \n");
		}
		break;
		default:
		{
			printf("<socket=%d>�յ�δ������Ϣ�����ݳ��ȣ�%d\n", (int)cSock, header->dataLength);
			//DataHeader ret;
			//SendData(cSock, &ret);
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
		for (int n = (int)_clients.size() - 1; n >= 0; n--) //֪ͨ���пͻ������¿ͻ��˼���
		{
			SendData(_clients[n]->sockfd(), header);
		}
	}
};

#endif