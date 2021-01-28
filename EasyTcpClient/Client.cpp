#include"EasyTcpClient.hpp"
#include<thread>

//#pragma comment(lib,"ws2_32.lib")


void cmdThread(EasyTcpClient* client)
{
	while (true)
	{
		char cmdBuf[128] = {};
		scanf_s("%s", cmdBuf, 128);

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
			strcpy_s(login.userName, "Allen");
			strcpy_s(login.PassWord, "4584");
			client->SendData(&login);
		}
		else if (0 == strcmp(cmdBuf, "logout"))
		{
			Logout logout;
			strcpy_s(logout.userName, "Allen");
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
	client.Connect("127.0.0.1", 4567);

	EasyTcpClient client2;
	client2.InitSocket();
	client2.Connect("127.0.0.1", 4567);

	//启动UI线程
	std::thread t1(cmdThread, &client);
	t1.detach();

	std::thread t2(cmdThread, &client2);
	t2.detach();

	while (client.isRun() || client2.isRun())
	{
		client.OnRun();
		client2.OnRun();
	}

	client.Close();
	client2.Close();
	printf("已退出 \n");
	getchar();

	return 0;
}