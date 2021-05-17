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
#include<map>
#include"MessageHeader.hpp"
#include"CELLTimestamp.hpp"
#include"CellTask.hpp"

#ifndef RECV_BUFF_SIZE
	#define RECV_BUFF_SIZE 10240*5
	#define SEND_BUFF_SIZE 10240*5
#endif

#define _CellServer_THREAD_COUNT 4


//�ͻ�����������
class ClientSocket
{
private:
	SOCKET _sockfd; 
	char _szMsgBuf[RECV_BUFF_SIZE] = {}; //��Ϣ������
	int _lastPos = 0; //��Ϣ����������β��

	char _szSendBuf[SEND_BUFF_SIZE] = {}; //���ͻ�����
	int _lastSendPos = 0; //���ͻ���������β��
public:
	ClientSocket(SOCKET sockfd = INVALID_SOCKET)
	{
		_sockfd = sockfd;
		memset(_szMsgBuf, 0, RECV_BUFF_SIZE);
		memset(_szSendBuf, 0, SEND_BUFF_SIZE);
		_lastPos = 0;
		_lastSendPos = 0;
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
		int ret = SOCKET_ERROR;
		int nSendLen = header->dataLength; //�������ݳ���
		const char* pSendData = (const char*)header; //Ҫ���͵�����

		while (true)
		{
			if (_lastSendPos + nSendLen >= SEND_BUFF_SIZE) //�¼���ķ������ݽ�ʹ�÷��ͻ���������������з���
			{
				//�ɿ��������ͻ����������ݳ���
				int nCopyLen = SEND_BUFF_SIZE - _lastSendPos;
				//��������
				memcpy(_szSendBuf + _lastSendPos, pSendData, nCopyLen);
				pSendData += nCopyLen;
				nSendLen -= nCopyLen;
				ret = send(_sockfd, _szSendBuf, SEND_BUFF_SIZE, 0);
				_lastSendPos = 0;

				if (SOCKET_ERROR)
				{
					return ret;
				}
			}
			else
			{
				memcpy(_szSendBuf + _lastSendPos, pSendData, nSendLen);
				_lastSendPos += nSendLen;
				break;
			}
		}
		return ret;
	}
};


class CellServer;
//�����¼��ӿ�
class INetEvent
{
public:
	//�ͻ��˼����¼�
	virtual void OnJoin(ClientSocket* pClient) = 0;
	//�ͻ����뿪�¼�
	virtual void OnLeave(ClientSocket* pClient) = 0; //���麯��
	//�ͻ�����Ϣ�¼�
	virtual void OnNetMsg(CellServer* pCellServer,  ClientSocket* pClient, DataHeader* header) = 0;
	//recv�¼�
	virtual void OnNetRecv(ClientSocket* pClient) = 0;

private:

};


class CellSendMsg2ClientTask :public CellTask
{
private:
	ClientSocket* _pClient;
	DataHeader* _pHeader;

public:
	CellSendMsg2ClientTask(ClientSocket* pClient,DataHeader* header)
	{
		_pClient = pClient;
		_pHeader = header;
	}

	//ִ������
	void doTask()
	{
		_pClient->SendData(_pHeader);
		delete _pHeader;
	}
};


//������Ϣ���մ����������
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


	fd_set _fdRead_bak;
	bool _clients_change;
	SOCKET _maxSock;
	//����������Ϣ
	bool OnRun()
	{
		_clients_change = true;
		while(isRun()) //�ж��Ƿ�socket���ڹ�����
		{
			//�ӻ�������ȡ���ͻ�����
			if (_clientsBuff.size() > 0)
			{
				std::lock_guard<std::mutex> lock(_mutex);
				for (auto pClient : _clientsBuff) //����_clientsBuff�е�Ԫ��
				{
					_clients[pClient->sockfd()]=pClient;
				}
				_clientsBuff.clear();
				_clients_change = true;
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

			if (_clients_change)
			{
				_clients_change = false;
				_maxSock = _clients.begin()->second->sockfd();
				for (auto iter : _clients)
				{
					FD_SET(iter.second->sockfd(), &fdRead);
					if (_maxSock < iter.second->sockfd())
					{
						_maxSock = iter.second->sockfd();
					}
				}
				memcpy(&_fdRead_bak, &fdRead, sizeof(fd_set));
			}
			else
			{
				memcpy(&fdRead, &_fdRead_bak, sizeof(fd_set));
			}

			//nfds��һ������ֵ��ָfd_set����������������(socket)�ķ�Χ������������
			//��nfds�������ļ������������ֵ+1����windows���������Ҳ����Ϊ0
			int ret = select(_maxSock + 1, &fdRead, nullptr, nullptr, nullptr);
			if (ret < 0)
			{
				printf("select������� \n");
				Close();
				return false;
			}

			//�����_sock���յ��ģ�������Client��_cSock������n��socket�˳��������_clients���Ƴ�
#ifdef _WIN32
			for (int n = 0; n < fdRead.fd_count; n++)
			{
				auto iter = _clients.find(fdRead.fd_array[n]);
				if (iter != _clients.end())
				{
					if (-1 == RecvData(iter->second))
					{
						_clients_change = true;
						if (_pNetEvent)
							_pNetEvent->OnLeave(iter->second); //_pNetEvent����EasyTcpServer��ָ��
						delete iter->second;
						_clients.erase(iter->first);
					}
				}
			}
#else
			std::vector<ClientSocket*> temp;
			for (auto iter : _clients)
			{
				if (FD_ISSET(iter.second->sockfd(), &fdRead))
				{
					if (-1 == RecvData(iter.second))
					{
						if (_pNetEvent)
							_pNetEvent->OnNetLeave(iter.second);
						_clients_change = true;
						temp.push_back(iter.second);
					}
				}
			}
			for (auto pClient : temp)
			{
				_clients.erase(pClient->sockfd());
				delete pClient;
			}
#endif

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
			for (auto iter:_clients)
			{
				closesocket(iter.second->sockfd());
				delete iter.second;
			}
			closesocket(_sock);
#else
			for (auto iter : _clients)
			{
				close(iter.second->sockfd());
				delete iter.second;
			}
			close(_sock);
#endif
			_sock = INVALID_SOCKET;
			_clients.clear();
		}
	}

	//������
	//char _szRecv[RECV_BUFF_SIZE] = {};

	//�������� ����ճ�� ��ְ�
	int RecvData(ClientSocket* pClient)
	{
		//���տͻ�������
		char* szRecv = pClient->msgBuf() + pClient->getLastPos();
		int nLen = (int)recv(pClient->sockfd(), szRecv, (RECV_BUFF_SIZE) - pClient->getLastPos(), 0);
		_pNetEvent->OnNetRecv(pClient);
		if (nLen <= 0)
		{
			//printf("�ͻ���<Socket=%d>���뿪...\n", (int)pClient->sockfd());
			return -1;
		}

		//memcpy(pClient->msgBuf() + pClient->getLastPos(), _szRecv, nLen); //����ȡ�������ݿ�������Ϣ������
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
		_pNetEvent->OnNetMsg(this, pClient, header);
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
		_taskServer.Start();
	}

	size_t getClientCount()
	{
		return _clients.size() + _clientsBuff.size();
	}

	void addSendTask(DataHeader* header, ClientSocket* pClient)
	{
		CellSendMsg2ClientTask* task = new CellSendMsg2ClientTask(pClient, header);
		_taskServer.addTask(task);
	}

private:
	std::map<SOCKET, ClientSocket*> _clients; //��ʽ����
	SOCKET _sock;
	std::vector<ClientSocket*> _clientsBuff; //�������
	std::mutex _mutex;  //������е���
	std::thread* _pThread;
	INetEvent* _pNetEvent; //�����¼�����
	CellTaskServer _taskServer;
};


//���������߳�
class EasyTcpServer : public INetEvent
{
private:
	std::vector<CellServer*> _cellServers;  //��Ϣ��������ڲ��ᴴ���߳�
	SOCKET _sock;
	CELLTimestamp _tTime;  //ÿ�����ݰ������ļ�ʱ
	std::atomic<int> _msgCount; //�յ����ݰ��ļ���
	std::atomic<int> _clientCount;  //����ͻ��˵ļ���
	std::atomic<int> _recvCount;  //recv����
public:
	EasyTcpServer()
	{
		_sock = INVALID_SOCKET;
		_recvCount = 0;
		_clientCount = 0;
		_msgCount = 0;
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
			printf("thread<%d>,time<%1f>,socket<%d>,clients<%d>,recvCount<%d>,msgCount<%d>\n", (int)_cellServers.size(), t1, (int)_sock, (int)_clientCount,(int)(_recvCount/t1),(int)(_msgCount/t1));
			_recvCount = 0;
			_msgCount = 0;
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
	virtual void OnNetMsg(CellServer* pCellServer, ClientSocket* pClient, DataHeader* header)
	{
		_msgCount++;
	}

	virtual void OnNetRecv(ClientSocket* pClient)
	{
		_recvCount++;
	}
};

#endif