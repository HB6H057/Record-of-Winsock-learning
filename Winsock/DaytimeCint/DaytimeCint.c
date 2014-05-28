#include <stdio.h>
#include <conio.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib") /*Winsock 使用的库函数*/

#define DAYTIME_DEF_PORT	13 /* Daytime默认端口 */
#define DAYTIME_BUF_SIZE	64 /* 缓冲区 */
#define DAYTIME_DEF_COUNT	2  /* 发送的次数 */

int main(int argc, char **argv)
{
	WSADATA wsaData;
	SOCKET time_soc = 0; /* Daytime socket 句柄 */
	struct sockaddr_in peer_addr, serv_addr;
	int timeout = 2000; /* 接收超过，2秒 */
	int i, result, send_len, addr_len = sizeof(serv_addr);
	char *dest = "127.0.0.1", *send_data = "Hello, Daytime!";
	char recv_buf[DAYTIME_BUF_SIZE];
	
	if (argc == 2) /*服务器地址*/
		dest = argv[1];

	WSAStartup(WINSOCK_VERSION, &wsaData); /*初始化 Winsock 资源*/
	send_len = strlen(send_data);

	/*服务器地址*/
	serv_addr.sin_family			= AF_INET;
	serv_addr.sin_addr.s_addr		= inet_addr(dest);
	serv_addr.sin_port				= htons(DAYTIME_DEF_PORT);

	if (serv_addr.sin_addr.s_addr == INADDR_NONE)
	{
		printf("[Daytime] invaild address\r\n");
		return -1;
	}

	/* 创建Daytime使用的socket */
	time_soc = socket(AF_INET, SOCK_DGRAM, 0);
	result = setsockopt(time_soc, SOL_SOCKET, SO_RCVTIMEO,\
						(char *)&timeout, sizeof(timeout));

	for (i = 0; i < DAYTIME_DEF_COUNT; i++)
	{
		result = sendto(time_soc, send_data, send_len, 0,\
						(struct sockaddr *)&serv_addr, sizeof(serv_addr));
		result = recvfrom(time_soc, recv_buf, DAYTIME_BUF_SIZE, 0,\
						(struct sockaddr *)&peer_addr, &addr_len);

		if (result >= 0)
		{
			recv_buf[result] = 0;
			printf("[Daytime] recv:\"%s\", from %s \r\n",
				recv_buf, inet_ntoa(peer_addr.sin_addr));
		}

	}

	closesocket(time_soc);
	WSACleanup();

	getch();

	return 0;
}