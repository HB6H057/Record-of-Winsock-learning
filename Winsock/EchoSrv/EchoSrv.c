#include <stdio.h>
#include <winsock2.h>
#include <stdlib.h>
#pragma comment(lib, "ws2_32")

#define ECHO_DEF_PORT	7
#define ECHO_BUF_SIZE	256

int main(int argc, char **argv)
{
	WSADATA wsa_data;
	SOCKET echo_soc = 0,
			acpt_soc = 0;

	struct sockaddr_in serv_addr, 
							clnt_addr;
	unsigned short port = ECHO_DEF_PORT;
	int result = 0;
	int addr_len = sizeof(struct sockaddr_in);
	char recv_buf[ECHO_BUF_SIZE];

	if (argc == 2)
		port = atoi(argv[1]);

	WSAStartup(MAKEWORD(2, 0), &wsa_data);
	echo_soc = socket(AF_INET, SOCK_STREAM, 0);

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	result = bind(echo_soc, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	if (result == SOCKET_ERROR)
	{
		printf("[Echo Server] bind error: %d \n", WSAGetLastError());
		closesocket(echo_soc);
		return -1;
	}
	
	listen(echo_soc, SOMAXCONN);

	printf("[Echo Server] is running......\n");

	while (1)
	{
		acpt_soc = accept(echo_soc, (struct sockaddr *)&clnt_addr, &addr_len);
		if (acpt_soc == INVALID_SOCKET)
		{
			printf("[Echo Server] accept error: %d\n", WSAGetLastError());
			break;
		}

		result = recv(acpt_soc, recv_buf, ECHO_BUF_SIZE, 0);
		if (result > 0)
		{
			recv_buf[result] = 0;
			printf("[ECHO Server] receives:\"%s\", from %s\r\n",
					recv_buf, inet_ntoa(clnt_addr.sin_addr));

			result = send(acpt_soc, recv_buf, result, 0);
		}
		closesocket(acpt_soc);
	}
	closesocket(acpt_soc);
	WSACleanup();
	system("pause");
	return 0;
}