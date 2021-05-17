#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include<thread>
#include"EasyTcpServer.hpp"

//#pragma comment(lib,"ws2_32.lib")

class myServer :public EasyTcpServer
{
	virtual void OnJoin(ClientSocket* pClient)
	{
		EasyTcpServer::OnJoin(pClient);
	}

	virtual void OnLeave(ClientSocket* pClient)
	{
		EasyTcpServer::OnLeave(pClient);
	}

	virtual void OnNetMsg(CellServer* pCellServer, ClientSocket* pClient, DataHeader* header)
	{
		EasyTcpServer::OnNetMsg(pCellServer, pClient, header);

		switch (header->cmd)
		{
		case CMD_LOGIN:
		{
			Login* login = (Login*)header;
			//printf("收到<Socket=%d>请求: CMD_LOGIN, 数据长度: %d, userName: %s, Password: %s \n", (int)cSock, login->dataLength, login->userName, login->PassWord);
			//LoginResult ret;
			//pClient->SendData(&ret);
			LoginResult* ret = new LoginResult();
			pCellServer->addSendTask(ret, pClient);
		}
		break;
		case CMD_LOGOUT:
		{
			Logout* logout = (Logout*)header;
			//printf("收到<Socket=%d>请求: CMD_LOGOUT, 数据长度: %d, userName: %s \n", (int)cSock, logout->dataLength, logout->userName);
			LogoutResult ret;
			pClient->SendData(&ret);
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

	virtual void OnNetRecv(ClientSocket* pClient)
	{
		EasyTcpServer::OnNetRecv(pClient);
	}
};


bool g_bRun = true;
void cmdThread()
{
	while (true)
	{
		char cmdBuf[128] = {};
#ifdef _WIN32
		scanf_s("%s", cmdBuf, 128);
#else
		scanf("%s", cmdBuf);
#endif

		if (0 == strcmp(cmdBuf, "exit"))
		{
			g_bRun = false;
			printf("收到退出命令...\n");
			return;
		}
		else
		{
			printf("不支持的命令，请重新输入... \n");
		}
	}
}


int main()
{
	myServer server;
	server.InitSocket();
	server.Bind(nullptr, 4567);
	server.Listen(5);
	server.Start();

	//启动UI线程
	std::thread t1(cmdThread);
	t1.detach();


	while (g_bRun)
	{
		server.OnRun();
	}

	server.Close();
	printf("已退出\n");
	getchar();

	return 0;
}