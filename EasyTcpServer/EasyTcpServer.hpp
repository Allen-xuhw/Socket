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


//客户端数据类型
class ClientSocket
{
private:
	SOCKET _sockfd; 
	char _szMsgBuf[5 * RECV_BUFF_SIZE] = {}; //消息缓冲区
	int _lastPos = 0; //消息缓冲区数据长度
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

	//发送数据
	int SendData(DataHeader* header)
	{
		if (header)
		{
			return send(_sockfd, (const char*)header, header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}
};


//网络时间接口
class INetEvent
{
public:
	//客户端加入事件
	virtual void OnJoin(ClientSocket* pClient) = 0;
	//客户端离开事件
	virtual void OnLeave(ClientSocket* pClient) = 0; //纯虚函数
	//客户端消息事件
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

	//处理网络信息
	bool OnRun()
	{
		while(isRun()) //判断是否socket正在工作中
		{
			//从缓冲区中取出客户数据
			if (_clientsBuff.size() > 0)
			{
				std::lock_guard<std::mutex> lock(_mutex);
				for (auto pClient : _clientsBuff) //遍历_clientsBuff中的元素
				{
					_clients.push_back(pClient);
				}
				_clientsBuff.clear();
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

			SOCKET maxSock = _clients[0]->sockfd();
			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				FD_SET(_clients[n]->sockfd(), &fdRead);
				if (maxSock < _clients[n]->sockfd())
				{
					maxSock = _clients[n]->sockfd();
				}
			}

			//nfds是一个整数值，指fd_set集合中所有描述符(socket)的范围，而不是数量
			//即nfds是所有文件描述符中最大值+1，在windows下这个参数也设置为0
			int ret = select(maxSock + 1, &fdRead, nullptr, nullptr, nullptr);
			if (ret < 0)
			{
				printf("select任务结束 \n");
				Close();
				return false;
			}


			//处理从_sock接收到的，即来自Client的_cSock；若第n个socket退出，将其从_clients中移除
			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				if (FD_ISSET(_clients[n]->sockfd(), &fdRead))
				{
					if (-1 == RecvData(_clients[n])) //返回值为-1表示该客户端已离开
					{
						auto iter = _clients.begin(); //std::vector<SOCKET>::iterator
						iter += n;
						if (iter != _clients.end())
						{
							if(_pNetEvent)
								_pNetEvent->OnLeave(_clients[n]); //_pNetEvent是类EasyTcpServer的指针
							delete _clients[n];
							_clients.erase(iter);
						}
					}
				}
			}
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

	//缓冲区
	char _szRecv[RECV_BUFF_SIZE] = {};

	//接收数据 处理粘包 拆分包
	int RecvData(ClientSocket* pClient)
	{
		//接收客户端请求
		int nLen = (int)recv(pClient->sockfd(), _szRecv, RECV_BUFF_SIZE, 0);
		if (nLen <= 0)
		{
			//printf("客户端<Socket=%d>已离开...\n", (int)pClient->sockfd());
			return -1;
		}

		memcpy(pClient->msgBuf() + pClient->getLastPos(), _szRecv, nLen); //将收取到的数据拷贝到消息缓冲区
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
			//printf("收到<Socket=%d>请求: CMD_LOGIN, 数据长度: %d, userName: %s, Password: %s \n", (int)cSock, login->dataLength, login->userName, login->PassWord);
			//LoginResult ret;
			//pClient->SendData(&ret);
		}
		break;
		case CMD_LOGOUT:
		{
			Logout* logout = (Logout*)header;
			//printf("收到<Socket=%d>请求: CMD_LOGOUT, 数据长度: %d, userName: %s \n", (int)cSock, logout->dataLength, logout->userName);
			//LogoutResult ret;
			//pClient->SendData(&ret);
		}
		break;
		default:
		{
			printf("<socket=%d>收到未定义消息，数据长度：%d\n", (int)pClient->sockfd(), header->dataLength);
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
	std::vector<ClientSocket*> _clients; //正式队列
	SOCKET _sock;
	std::vector<ClientSocket*> _clientsBuff; //缓冲队列
	std::mutex _mutex;  //缓冲队列的锁
	std::thread* _pThread;
	INetEvent* _pNetEvent; //网络事件对象
};


class EasyTcpServer : public INetEvent
{
private:
	std::vector<CellServer*> _cellServers;  //消息处理对象，内部会创建线程
	SOCKET _sock;
	CELLTimestamp _tTime;  //每秒数据包数量的计时
	std::atomic<int> _recvCount; //收到数据包的计数
	std::atomic<int> _clientCount;  //接入客户端的计数
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
			printf("thread<%d>,time<%1f>,socket<%d>,clients<%d>,recvCount<%d>\n", (int)_cellServers.size(), t1, (int)_sock, (int)_clientCount,(int)(_recvCount/t1));
			_recvCount = 0;
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
	virtual void OnNetMsg(ClientSocket* pClient, DataHeader* header)
	{
		_recvCount++;
	}
};

#endif