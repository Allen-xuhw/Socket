#include"EasyTcpClient.hpp"
#include<thread>

//#pragma comment(lib,"ws2_32.lib")


void cmdThread(EasyTcpClient* client)
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
			client->Close();
			printf("收到退出命令...\n");
			return;
		}
		else if (0 == strcmp(cmdBuf, "login"))
		{
			Login login;
#ifdef _WIN32
			strcpy_s(login.userName, "Allen");
			strcpy_s(login.PassWord, "4584");
#else
			strcpy(login.userName, "Allen");
			strcpy(login.PassWord, "4584");
#endif
			client->SendData(&login);
		}
		else if (0 == strcmp(cmdBuf, "logout"))
		{
			Logout logout;
#ifdef _WIN32
			strcpy_s(logout.userName, "Allen");
#else
			strcpy(logout.userName, "Allen");
#endif
			client->SendData(&logout);
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
	EasyTcpClient client;
	client.InitSocket();
	client.Connect("192.168.2.113", 4567);

	//EasyTcpClient client2;
	//client2.InitSocket();
	//client2.Connect("127.0.0.1", 4568);

	//启动UI线程
	std::thread t1(cmdThread, &client);
	t1.detach();

	//std::thread t2(cmdThread, &client2);
	//t2.detach();

	while (client.isRun())
	{
		client.OnRun();
		//client2.OnRun();
	}

	client.Close();
	//client2.Close();
	printf("已退出 \n");
	getchar();

	return 0;
}