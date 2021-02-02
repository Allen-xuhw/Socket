#include"EasyTcpClient.hpp"
#include<thread>

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

		//address the command
		if (0 == strcmp(cmdBuf, "exit"))
		{
			printf("收到退出命令...\n");
			g_bRun = false;
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
	//创建类对象
	const int cCount = 100;
	EasyTcpClient* client[cCount];

	for (int n = 0; n < cCount; n++)
	{
		if (!g_bRun)
		{
			return 0;
		}
		client[n] = new EasyTcpClient();
	}

	for (int n = 0; n < cCount; n++)
	{
		if (!g_bRun)
		{
			return 0;
		}
		//client[n]->InitSocket();
		client[n]->Connect("127.0.0.1", 4567); //192.168.2.113 172.27.131.5 127.0.0.1
		printf("Connect=%d\n", n);
	}


	//启动UI线程
	std::thread t1(cmdThread);
	t1.detach();


	Login login;
	strcpy_s(login.userName, "Allen");
	strcpy_s(login.PassWord, "4584");

	while (g_bRun)
	{
		for (int n = 0; n < cCount; n++)
		{
			client[n]->SendData(&login);
			//client[n]->OnRun();
		}
	}

	for (int n = 0; n < cCount; n++)
	{
		client[n]->Close();
	}
	printf("已退出 \n");

	return 0;
}