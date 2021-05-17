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


//客户端数据类型
class ClientSocket
{
private:
	SOCKET _sockfd; 
	char _szMsgBuf[RECV_BUFF_SIZE] = {}; //消息缓冲区
	int _lastPos = 0; //消息缓冲区数据尾部

	char _szSendBuf[SEND_BUFF_SIZE] = {}; //发送缓冲区
	int _lastSendPos = 0; //发送缓冲区数据尾部
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

	//发送数据
	int SendData(DataHeader* header)
	{
		int ret = SOCKET_ERROR;
		int nSendLen = header->dataLength; //发送数据长度
		const char* pSendData = (const char*)header; //要发送的数据

		while (true)
		{
			if (_lastSendPos + nSendLen >= SEND_BUFF_SIZE) //新加入的发送数据将使得发送缓冲区变满，则进行发送
			{
				//可拷贝进发送缓冲区的数据长度
				int nCopyLen = SEND_BUFF_SIZE - _lastSendPos;
				//拷贝数据
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
//网络事件接口
class INetEvent
{
public:
	//客户端加入事件
	virtual void OnJoin(ClientSocket* pClient) = 0;
	//客户端离开事件
	virtual void OnLeave(ClientSocket* pClient) = 0; //纯虚函数
	//客户端消息事件
	virtual void OnNetMsg(CellServer* pCellServer,  ClientSocket* pClient, DataHeader* header) = 0;
	//recv事件
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

	//执行任务
	void doTask()
	{
		_pClient->SendData(_pHeader);
		delete _pHeader;
	}
};


//网络消息接收处理服务类型
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
	//处理网络信息
	bool OnRun()
	{
		_clients_change = true;
		while(isRun()) //判断是否socket正在工作中
		{
			//从缓冲区中取出客户数据
			if (_clientsBuff.size() > 0)
			{
				std::lock_guard<std::mutex> lock(_mutex);
				for (auto pClient : _clientsBuff) //遍历_clientsBuff中的元素
				{
					_clients[pClient->sockfd()]=pClient;
				}
				_clientsBuff.clear();
				_clients_change = true;
			}

			//如果正式客户端队列中没有需要处理的客户端，则跳过循环
			if (_clients.empty())
			{
				std::chrono::milliseconds t(1);
				std::this_thread::sleep_for(t);
				continue;
			}

			//伯克利套接字 BSD socket
			fd_set fdRead;  //描述符(socket)集合
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

			//nfds是一个整数值，指fd_set集合中所有描述符(socket)的范围，而不是数量
			//即nfds是所有文件描述符中最大值+1，在windows下这个参数也设置为0
			int ret = select(_maxSock + 1, &fdRead, nullptr, nullptr, nullptr);
			if (ret < 0)
			{
				printf("select任务结束 \n");
				Close();
				return false;
			}

			//处理从_sock接收到的，即来自Client的_cSock；若第n个socket退出，将其从_clients中移除
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
							_pNetEvent->OnLeave(iter->second); //_pNetEvent是类EasyTcpServer的指针
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

	//判断socket是否工作中
	bool isRun()
	{
		return _sock != INVALID_SOCKET;
	}

	//关闭Socket
	void Close()
	{
		//关闭Win Sock 2.x环境
		if (_sock != INVALID_SOCKET) //检测是否还未连接
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

	//缓冲区
	//char _szRecv[RECV_BUFF_SIZE] = {};

	//接收数据 处理粘包 拆分包
	int RecvData(ClientSocket* pClient)
	{
		//接收客户端请求
		char* szRecv = pClient->msgBuf() + pClient->getLastPos();
		int nLen = (int)recv(pClient->sockfd(), szRecv, (RECV_BUFF_SIZE) - pClient->getLastPos(), 0);
		_pNetEvent->OnNetRecv(pClient);
		if (nLen <= 0)
		{
			//printf("客户端<Socket=%d>已离开...\n", (int)pClient->sockfd());
			return -1;
		}

		//memcpy(pClient->msgBuf() + pClient->getLastPos(), _szRecv, nLen); //将收取到的数据拷贝到消息缓冲区
		pClient->setLastPos(pClient->getLastPos() + nLen);

		while (pClient->getLastPos() >= sizeof(DataHeader))
		{
			DataHeader* header = (DataHeader*)pClient->msgBuf();
			if (pClient->getLastPos() >= header->dataLength)
			{
				int nSize = pClient->getLastPos() - header->dataLength; //剩余未处理消息缓冲区数据的长度
				OnNetMsg(header, pClient);
				memcpy(pClient->msgBuf(), pClient->msgBuf() + header->dataLength, nSize); //将消息缓冲区未处理数据前移
				pClient->setLastPos(nSize);
			}
			else
			{
				break;
			}
		}
		return 0;
	}

	//响应网络消息
	virtual void OnNetMsg(DataHeader* header, ClientSocket* pClient)
	{
		//处理客户端请求
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
	std::map<SOCKET, ClientSocket*> _clients; //正式队列
	SOCKET _sock;
	std::vector<ClientSocket*> _clientsBuff; //缓冲队列
	std::mutex _mutex;  //缓冲队列的锁
	std::thread* _pThread;
	INetEvent* _pNetEvent; //网络事件对象
	CellTaskServer _taskServer;
};


//服务器主线程
class EasyTcpServer : public INetEvent
{
private:
	std::vector<CellServer*> _cellServers;  //消息处理对象，内部会创建线程
	SOCKET _sock;
	CELLTimestamp _tTime;  //每秒数据包数量的计时
	std::atomic<int> _msgCount; //收到数据包的计数
	std::atomic<int> _clientCount;  //接入客户端的计数
	std::atomic<int> _recvCount;  //recv计数
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

	//初始化Socket
	void InitSocket()
	{
		//启动Win Sock 2.x环境
#ifdef _WIN32
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		if (_sock != INVALID_SOCKET) //检测是否已有连接
		{
			printf("<socket=%d>关闭旧连接...\n", (int)_sock);
			Close();
		}
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock)
		{
			printf("建立SOCKET失败...\n");
		}
		else
		{
			printf("成功建立SOCKET=<%d>...\n", (int)_sock);
		}
	}


	//绑定ip和端口号
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
			_sin.sin_addr.S_un.S_addr = INADDR_ANY; //INADDR_ANY表示任意本机的任意ip地址
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
			printf("绑定端口<%d>失败...\n", port);
		}
		else
		{
			printf("成功绑定端口<%d>...\n", port);
		}
		return ret;
	}


	//监听端口号
	int Listen(int n)
	{
		int ret = listen(_sock, n);
		if (SOCKET_ERROR == ret)
		{
			printf("<Socket=%d>错误，监听失败...\n", (int)_sock);
		}
		else
		{
			printf("<Socket=%d>正在监听...\n", (int)_sock);
		}
		return ret;
	}


	//接受客户端链接
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
			printf("<Socket=%d>错误，接收到无效客户端SOCKET...\n", (int)_sock);
		}
		else
		{
			//printf("<Socket=%d>接收到新客户端<socket=%d>: IP = %s, Socket = %d \n", (int)_sock, (int)_clients.size(), inet_ntoa(clientAddr.sin_addr), (int)cSock);//新客户端加入
			//NewUserJoin userJoin;
			//SendDataToAll(&userJoin); //通知现有客户端有新客户端加入
			addClientToCellServer(new ClientSocket(cSock));  //将新客户端分配给客户数量最少的cellServer
		}
		return cSock;
	}


	void addClientToCellServer(ClientSocket* pClient)
	{
		auto pMinServer = _cellServers[0];
		//查找客户端数量最少的消息处理线程
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
			auto ser = new CellServer(_sock);  //创建消息处理对象
			_cellServers.push_back(ser);
			//EasyTcpServer继承自INetEvent，故将自身指针当作INetEvent的指针传给每个线程对应的类CellClient的函数setEventObj，在该函数中关闭EasyTcpServer中队列_clients的某个对应类实例ClientSocket
			ser->setEventObj(this);  //注册网络事件接受对象
			ser->Start();  //启动消息处理线程
		}
	}


	//关闭Socket
	void Close()
	{
		//关闭Win Sock 2.x环境
		if (_sock != INVALID_SOCKET) //检测是否还未连接
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


	//处理网络信息
	bool OnRun()
	{
		if (isRun()) //判断是否socket正在工作中
		{

			time4package();
			//伯克利套接字 BSD socket
			fd_set fdRead;  //描述符(socket)集合
			//fd_set fdWrite;
			//fd_set fdExp;
			FD_ZERO(&fdRead);
			//FD_ZERO(&fdWrite);
			//FD_ZERO(&fdExp);

			//将_sock加入集合中
			FD_SET(_sock, &fdRead);
			//FD_SET(_sock, &fdWrite);
			//FD_SET(_sock, &fdExp);

			//nfds是一个整数值，指fd_set集合中所有描述符(socket)的范围，而不是数量
			//即nfds是所有文件描述符中最大值+1，在windows下这个参数也设置为0
			timeval t = { 0, 10 };
			int ret = select(_sock + 1, &fdRead, nullptr, nullptr, &t);
			if (ret < 0)
			{
				printf("select任务结束 \n");
				Close();
				return false;
			}

			//判断_sock是否在Read集合中
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


	//判断socket是否工作中
	bool isRun()
	{
		return _sock != INVALID_SOCKET;
	}


	//计算并输出每秒收到的网络消息
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


	////群发数据
	//void SendDataToAll(DataHeader* header)
	//{
	//	for (int n = (int)_clients.size() - 1; n >= 0; n--) //通知现有客户端有新客户端加入
	//	{
	//		SendData(_clients[n]->sockfd(), header);
	//	}
	//}

	//只会被一个线程调用
	virtual void OnJoin(ClientSocket* pClient)
	{
		_clientCount++;
	}

	//多个线程触发 不安全
	virtual void OnLeave(ClientSocket* pClient)
	{
		_clientCount--;
	}

	//多个线程触发 不安全
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