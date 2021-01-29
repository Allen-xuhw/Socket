#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include"EasyTcpServer.hpp"

//#pragma comment(lib,"ws2_32.lib")




int main()
{
	EasyTcpServer server;
	server.InitSocket();
	server.Bind(nullptr, 4567);
	server.Listen(5);


	while (server.isRun())
	{
		server.OnRun();
	}

	server.Close();
	printf("已退出\n");
	getchar();

	return 0;
}