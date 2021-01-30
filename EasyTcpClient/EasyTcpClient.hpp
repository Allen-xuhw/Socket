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

	//初始化socket
	SOCKET InitSocket()
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
		return _sock;
	}

	//连接服务器
	int Connect(const char* ip, unsigned short port)
	{
		if (_sock == INVALID_SOCKET) //检测是否已初始化
		{
			InitSocket();
		}
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(port);
#ifdef _WIN32
		_sin.sin_addr.S_un.S_addr = inet_addr(ip); //NADDR_ANY表示任意本机的任意ip地址
#else
		_sin.sin_addr.s_addr = inet_addr(ip);
#endif
		int ret = connect(_sock, (sockaddr*)&_sin, sizeof(sockaddr_in));
		if (SOCKET_ERROR == ret)
		{
			printf("<socket=%d>连接服务器<%s:%d>失败...\n", (int)_sock, ip, port);
		}
		else
		{
			printf("<socket=%d>成功连接服务器<%s:%d>...\n", (int)_sock, ip, port);
		}
		return ret;
	}

	//关闭socket
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

	//处理网络消息
	bool OnRun()
	{
		if (isRun()) //判断是否socket正在工作中
		{
			fd_set fdReads;
			FD_ZERO(&fdReads);
			FD_SET(_sock, &fdReads);
			timeval t = { 0,0 };
			int ret = select(_sock, &fdReads, 0, 0, &t);
			if (ret < 0)
			{
				printf("<socket=%d>select任务结束\n", (int)_sock);
				Close();
				return false;
			}

			if (FD_ISSET(_sock, &fdReads))
			{
				FD_CLR(_sock, &fdReads);

				if (-1 == RecvData())
				{
					printf("<socket=%d>select任务结束\n", (int)_sock);
					Close();
					return false;
				}
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


	//接收数据 处理粘包 拆分包
#ifndef RECV_BUFF_SIZE
	#define RECV_BUFF_SIZE 10240
#endif
	char _szRecv[RECV_BUFF_SIZE] = {}; //接收缓冲区
	char _szMsgBuf[10 * RECV_BUFF_SIZE] = {}; //消息缓冲区
	int _lastPos = 0; //消息缓冲区数据长度

	int RecvData()
	{
		//接收客户端数据
		int nLen = (int)recv(_sock, _szRecv, RECV_BUFF_SIZE, 0);
		if (nLen <= 0)
		{
			printf("<socket=%d>与服务端断开连接，任务结束...\n", (int)_sock);
			return -1;
		}

		memcpy(_szMsgBuf + _lastPos, _szRecv, nLen); //将收取到的数据拷贝到消息缓冲区
		_lastPos += nLen;

		while (_lastPos >= sizeof(DataHeader))
		{
			DataHeader* header = (DataHeader*)_szMsgBuf;
			if (_lastPos >= header->dataLength)
			{
				int nSize = _lastPos - header->dataLength; //剩余未处理消息缓冲区数据的长度
				OnNetMsg(header);
				memcpy(_szMsgBuf, _szMsgBuf + header->dataLength, nSize); //将消息缓冲区未处理数据前移
				_lastPos = nSize;
			}
			else
			{
				break;
			}
		}
		return 0;
	}

	//响应网络消息
	virtual void OnNetMsg(DataHeader* header)
	{
		switch (header->cmd)
		{
		case CMD_LOGIN_RESULT:
		{
			LoginResult* loginResult = (LoginResult*)header; //所有消息都是从DataHeader继承产生的
			//printf("<socket=%d>收到服务端消息: CMD_LOGIN_RESULT, 数据长度: %d \n", (int)_sock, loginResult->dataLength);

		}
		break;
		case CMD_LOGOUT_RESULT:
		{
			LogoutResult* logoutResult = (LogoutResult*)header;
			//printf("<socket=%d>收到服务端消息: CMD_LOGOUT_RESULT, 数据长度: %d \n", (int)_sock, logoutResult->dataLength);
		}
		break;
		case CMD_NEW_USER_JOIN:
		{
			NewUserJoin* userJoin = (NewUserJoin*)header;
			//printf("<socket=%d>收到服务端消息: CMD_NEW_USER_JOIN, 数据长度: %d \n", (int)_sock, userJoin->dataLength);
		}
		break;
		case CMD_ERROR:
		{
			printf("<socket=%d>收到服务端消息：CMD_ERROR, 数据长度: %d \n", (int)_sock, header->dataLength);
		}
		break;
		default:
		{
			printf("<socket=%d>收到未定义消息, 数据长度: %d \n", (int)_sock, header->dataLength);
		}
		}
	}

	//发送数据
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