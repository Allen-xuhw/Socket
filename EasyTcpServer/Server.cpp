#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include<thread>
#include"EasyTcpServer.hpp"

//#pragma comment(lib,"ws2_32.lib")


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
	EasyTcpServer server;
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