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


const int cCount = 10000; //客户端数量
const int tCount = 4; //线程数量
EasyTcpClient* client[cCount]; //客户端数组

void sendThread(int id)
{
	int c = cCount / tCount;
	int begin = (id - 1) * c;
	int end = id * c;

	for (int n = begin; n < end; n++)
	{
		client[n] = new EasyTcpClient();
	}

	for (int n = begin; n < end; n++)
	{
		//client[n]->InitSocket();
		client[n]->Connect("127.0.0.1", 4567); //192.168.2.113 172.27.131.5 127.0.0.1
		printf("Thread<%d>,Connect=%d\n", id, n);
	}

	std::chrono::milliseconds t(3000);
	std::this_thread::sleep_for(t);


	Login login;
	strcpy_s(login.userName, "Allen");
	strcpy_s(login.PassWord, "4584");

	while (g_bRun)
	{
		for (int n = begin; n < end; n++)
		{
			client[n]->SendData(&login);
			//client[n]->OnRun();
		}
	}

	for (int n = begin; n < end; n++)
	{
		client[n]->Close();
	}
}


int main()
{
	//启动UI线程
	std::thread t1(cmdThread);
	t1.detach();

	//启动发送线程
	for (int n = 0; n < tCount; n++)
	{
		std::thread t1(sendThread, n+1);
		t1.detach();
	}

	while (g_bRun)
		Sleep(100);

	printf("已退出 \n");

	return 0;
}