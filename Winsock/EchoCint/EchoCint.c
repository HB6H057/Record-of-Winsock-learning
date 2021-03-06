#include <stdio.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32")

#define ECHO_DEF_PORT	7
#define ECHO_BUF_SIZE	256

int main(int argc, char **argv)
{
	WSADATA wsa_data;
	SOCKET echo_soc = 0;
	struct sockaddr_in serv_addr;	

	unsigned short port = ECHO_DEF_PORT;
	int result = 0, send_len = 0;
	char *test_data = "Hello World!", recv_buf[ECHO_BUF_SIZE];

	if (argc < 2)
	{
		printf("input %s server_address [porn]\n", argv[0]);
		return -1;
	}

	if (argc >= 3)
		port = atoi(argv[2]);

	WSAStartup(MAKEWORD(2, 0), &wsa_data);
	send_len = strlen(test_data);

	/*��������ַ*/
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);

	if (serv_addr.sin_addr.s_addr == INADDR_NONE)
	{
		printf("[ECHO] invalid address\n");
		return -1;
	}

	echo_soc = socket(AF_INET, SOCK_STREAM, 0);
	result = connect(echo_soc, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	
	if (result == 0)
	{
		result = send(echo_soc, test_data, send_len, 0);
		result = recv(echo_soc, recv_buf, ECHO_BUF_SIZE, 0);
	}

	if (result > 0)
	{
		recv_buf[result] = 0;
		printf("[Echo Client] receives: \"%s\"\r\n", recv_buf);
	}
	else
		printf("[Echo Client] error : %d.\r\n", WSAGetLastError());

	closesocket(echo_soc);
	WSACleanup();

	return 0;
}