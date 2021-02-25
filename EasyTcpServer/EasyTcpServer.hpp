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
#include<thread>
#include<mutex>
#include<atomic>
#include"MessageHeader.hpp"
#include"CELLTimestamp.hpp"

#ifndef RECV_BUFF_SIZE
	#define RECV_BUFF_SIZE 10240
#endif

#define _CellServer_THREAD_COUNT 4


//�ͻ�����������
class ClientSocket
{
private:
	SOCKET _sockfd; 
	char _szMsgBuf[5 * RECV_BUFF_SIZE] = {}; //��Ϣ������
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

	//��������
	int SendData(DataHeader* header)
	{
		if (header)
		{
			return send(_sockfd, (const char*)header, header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}
};


//����ʱ��ӿ�
class INetEvent
{
public:
	//�ͻ��˼����¼�
	virtual void OnJoin(ClientSocket* pClient) = 0;
	//�ͻ����뿪�¼�
	virtual void OnLeave(ClientSocket* pClient) = 0; //���麯��
	//�ͻ�����Ϣ�¼�
	virtual void OnNetMsg(ClientSocket* pClient, DataHeader* header) = 0;

private:

};


class CellServer
{
public:
	CellServer(SOCKET sock=INVALID_SOCKET)
	{
		_sock = sock;
		_pThread = nullptr;
		_pNetEvent = nullptr;
	}

	~CellServer()
	{
		delete _pThread;
		Close();
		_sock = INVALID_SOCKET;
	}

	void setEventObj(INetEvent* event)
	{
		_pNetEvent = event;
	}

	//����������Ϣ
	bool OnRun()
	{
		while(isRun()) //�ж��Ƿ�socket���ڹ�����
		{
			//�ӻ�������ȡ���ͻ�����
			if (_clientsBuff.size() > 0)
			{
				std::lock_guard<std::mutex> lock(_mutex);
				for (auto pClient : _clientsBuff) //����_clientsBuff�е�Ԫ��
				{
					_clients.push_back(pClient);
				}
				_clientsBuff.clear();
			}

			//�����ʽ�ͻ��˶�����û����Ҫ����Ŀͻ��ˣ�������ѭ��
			if (_clients.empty())
			{
				std::chrono::milliseconds t(1);
				std::this_thread::sleep_for(t);
				continue;
			}

			//�������׽��� BSD socket
			fd_set fdRead;  //������(socket)����
			//fd_set fdWrite;
			//fd_set fdExp;
			FD_ZERO(&fdRead);
			//FD_ZERO(&fdWrite);
			//FD_ZERO(&fdExp);

			SOCKET maxSock = _clients[0]->sockfd();
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
			int ret = select(maxSock + 1, &fdRead, nullptr, nullptr, nullptr);
			if (ret < 0)
			{
				printf("select������� \n");
				Close();
				return false;
			}


			//�����_sock���յ��ģ�������Client��_cSock������n��socket�˳��������_clients���Ƴ�
			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				if (FD_ISSET(_clients[n]->sockfd(), &fdRead))
				{
					if (-1 == RecvData(_clients[n])) //����ֵΪ-1��ʾ�ÿͻ������뿪
					{
						auto iter = _clients.begin(); //std::vector<SOCKET>::iterator
						iter += n;
						if (iter != _clients.end())
						{
							if(_pNetEvent)
								_pNetEvent->OnLeave(_clients[n]); //_pNetEvent����EasyTcpServer��ָ��
							delete _clients[n];
							_clients.erase(iter);
						}
					}
				}
			}
		}
	}

	//�ж�socket�Ƿ�����
	bool isRun()
	{
		return _sock != INVALID_SOCKET;
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

	//������
	char _szRecv[RECV_BUFF_SIZE] = {};

	//�������� ����ճ�� ��ְ�
	int RecvData(ClientSocket* pClient)
	{
		//���տͻ�������
		int nLen = (int)recv(pClient->sockfd(), _szRecv, RECV_BUFF_SIZE, 0);
		if (nLen <= 0)
		{
			//printf("�ͻ���<Socket=%d>���뿪...\n", (int)pClient->sockfd());
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
				OnNetMsg(header, pClient);
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
	virtual void OnNetMsg(DataHeader* header, ClientSocket* pClient)
	{
		//����ͻ�������
		_pNetEvent->OnNetMsg(pClient, header);

		//_recvCount++;
		//auto t1 = _tTime.getElapsedSecond();
		//if (t1 >= 1.0)
		//{
		//	printf("time<%1f>,socket<%d>,clients<%d>,recvCount<%d>\n", t1, (int)_sock, (int)_clients.size(), _recvCount);
		//	_tTime.update();
		//	_recvCount = 0;
		//}

		switch (header->cmd)
		{
		case CMD_LOGIN:
		{
			Login* login = (Login*)header;
			//printf("�յ�<Socket=%d>����: CMD_LOGIN, ���ݳ���: %d, userName: %s, Password: %s \n", (int)cSock, login->dataLength, login->userName, login->PassWord);
			//LoginResult ret;
			//pClient->SendData(&ret);
		}
		break;
		case CMD_LOGOUT:
		{
			Logout* logout = (Logout*)header;
			//printf("�յ�<Socket=%d>����: CMD_LOGOUT, ���ݳ���: %d, userName: %s \n", (int)cSock, logout->dataLength, logout->userName);
			//LogoutResult ret;
			//pClient->SendData(&ret);
		}
		break;
		default:
		{
			printf("<socket=%d>�յ�δ������Ϣ�����ݳ��ȣ�%d\n", (int)pClient->sockfd(), header->dataLength);
			//DataHeader ret;
			//SendData(cSock, &ret);
		}
		}
	}

	void addClient(ClientSocket* pClient)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		//_mutex.lock();
		_clientsBuff.push_back(pClient);
		//_mutex.unlock();
	}

	void Start()
	{
		_pThread = new std::thread(&CellServer::OnRun, this);
	}

	size_t getClientCount()
	{
		return _clients.size() + _clientsBuff.size();
	}

private:
	std::vector<ClientSocket*> _clients; //��ʽ����
	SOCKET _sock;
	std::vector<ClientSocket*> _clientsBuff; //�������
	std::mutex _mutex;  //������е���
	std::thread* _pThread;
	INetEvent* _pNetEvent; //�����¼�����
};


class EasyTcpServer : public INetEvent
{
private:
	std::vector<CellServer*> _cellServers;  //��Ϣ��������ڲ��ᴴ���߳�
	SOCKET _sock;
	CELLTimestamp _tTime;  //ÿ�����ݰ������ļ�ʱ
	std::atomic<int> _recvCount; //�յ����ݰ��ļ���
	std::atomic<int> _clientCount;  //����ͻ��˵ļ���
public:
	EasyTcpServer()
	{
		_sock = INVALID_SOCKET;
		_recvCount = 0;
		_clientCount = 0;
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
			addClientToCellServer(new ClientSocket(cSock));  //���¿ͻ��˷�����ͻ��������ٵ�cellServer
		}
		return cSock;
	}


	void addClientToCellServer(ClientSocket* pClient)
	{
		auto pMinServer = _cellServers[0];
		//���ҿͻ����������ٵ���Ϣ�����߳�
		for (auto pCellServer : _cellServers)
		{
			if (pMinServer->getClientCount() > pCellServer->getClientCount())
			{
				pMinServer = pCellServer;
			}
		}
		pMinServer->addClient(pClient);
		OnJoin(pClient);
	}


	void Start()
	{
		for (int n = 0; n < _CellServer_THREAD_COUNT; n++)
		{
			auto ser = new CellServer(_sock);  //������Ϣ�������
			_cellServers.push_back(ser);
			//EasyTcpServer�̳���INetEvent���ʽ�����ָ�뵱��INetEvent��ָ�봫��ÿ���̶߳�Ӧ����CellClient�ĺ���setEventObj���ڸú����йر�EasyTcpServer�ж���_clients��ĳ����Ӧ��ʵ��ClientSocket
			ser->setEventObj(this);  //ע�������¼����ܶ���
			ser->Start();  //������Ϣ�����߳�
		}
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

			time4package();
			//�������׽��� BSD socket
			fd_set fdRead;  //������(socket)����
			//fd_set fdWrite;
			//fd_set fdExp;
			FD_ZERO(&fdRead);
			//FD_ZERO(&fdWrite);
			//FD_ZERO(&fdExp);

			//��_sock���뼯����
			FD_SET(_sock, &fdRead);
			//FD_SET(_sock, &fdWrite);
			//FD_SET(_sock, &fdExp);

			//nfds��һ������ֵ��ָfd_set����������������(socket)�ķ�Χ������������
			//��nfds�������ļ������������ֵ+1����windows���������Ҳ����Ϊ0
			timeval t = { 0, 10 };
			int ret = select(_sock + 1, &fdRead, nullptr, nullptr, &t);
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
			return true;
		}
		return false;
	}


	//�ж�socket�Ƿ�����
	bool isRun()
	{
		return _sock != INVALID_SOCKET;
	}


	//���㲢���ÿ���յ���������Ϣ
	void time4package()
	{
		auto t1 = _tTime.getElapsedSecond();
		if (t1 >= 1.0)
		{
			printf("thread<%d>,time<%1f>,socket<%d>,clients<%d>,recvCount<%d>\n", (int)_cellServers.size(), t1, (int)_sock, (int)_clientCount,(int)(_recvCount/t1));
			_recvCount = 0;
			_tTime.update();
		}
	}


	////Ⱥ������
	//void SendDataToAll(DataHeader* header)
	//{
	//	for (int n = (int)_clients.size() - 1; n >= 0; n--) //֪ͨ���пͻ������¿ͻ��˼���
	//	{
	//		SendData(_clients[n]->sockfd(), header);
	//	}
	//}

	//ֻ�ᱻһ���̵߳���
	virtual void OnJoin(ClientSocket* pClient)
	{
		_clientCount++;
	}

	//����̴߳��� ����ȫ
	virtual void OnLeave(ClientSocket* pClient)
	{
		_clientCount--;
	}

	//����̴߳��� ����ȫ
	virtual void OnNetMsg(ClientSocket* pClient, DataHeader* header)
	{
		_recvCount++;
	}
};

#endif