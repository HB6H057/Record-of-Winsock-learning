#include <stdio.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib") /* Winsock使用的库函数 */

/* 定义常量 */

#define HTTP_DEF_PORT		80		/* 连接的默认端口 */
#define HTTP_BUF_SIZE		1024	/* 缓冲区的大小 */
#define HTTP_FILENAME_LEN	265		/* 文件名长度 */

/* 定义文件类型对应的Content -Type */
struct doc_type
{
	char * suffix; /* 文件后缀 */
	char * type;   /* Content-Type */
};

struct doc_type file_type[] = 
{
	{"html",	"text/html"},
	{"gif",		"image/gif"},
	{"jepg",	"image/jepg"},
	{ NULL,		NULL		}
};

char *http_res_hdr_tmpl = "HTTP/1.1 200 OK \r\nServer: HB6's Server<0.1>\r\n"\
	"Accept-Ranges: bytes\r\nContent-Length: %d\r\nConnection: close\r\n"
	"Content-Type: %s\r\n\r\n";

char *http_get_type_by_suffix(const char * suffix)
{
	struct doc_type *type;

	for (type = file_type; type->suffix; type++)
	{
		if (strcmp(type->suffix, suffix) == 0)
			return type->type;
	}
	return NULL;
}

void http_parse_request_cmd(char *buf, int buflen, char *file_name, char *suffix)
{
	int length = 0;
	char *begin, *end, *bias;

	/* 查找URL的开始位置 */
	begin = strchr(begin, ' ');
	begin += 1;

	/* 查找URL的结束位置 */
	end = strchr(begin, ' ');
	*end = 0;

	bias = strrchr(begin, ' ');
	length = end - bias;

	/* 找到文件名的开始位置 */
	if ((*bias == '/') || (*bias == '\\'))
	{
		bias++;
		length++;
	}

	/* 得到文件名 */
	if (length > 0)
	{
		memcpy(file_name, bias, length);
		file_name[length] = 0;

		begin = strchr(file_name, '.');
		if (begin)
			strcpy(suffix, begin + 1);
	}
}

int http_send_response(SOCKET soc, char *buf, int buf_len)
{
	int read_len, file_len, hdr_len, send_len;
	char *type;
	char read_buf[HTTP_BUF_SIZE];
	char http_header[HTTP_BUF_SIZE];
	char file_name[HTTP_FILENAME_LEN] = "index.html", suffix[16] = "html";
	FILE *res_file;

	/* 得到文件名和后缀 */
	http_parse_request_cmd(buf, buf_len, file_name, suffix);

	res_file = fopen(file_name, "rb+"); /* 用二进制格式打开文件 */
	if (res_file == NULL)
	{
		printf("[web] the file [%s] is not existed\n", file_name);
		return 0;
	}

	fseek(res_file, 0, SEEK_END);
	file_len = ftell(res_file);
	fseek(res_file, 0, SEEK_END);

	type = http_get_type_by_suffix(suffix); /* 文件对应的 Content-Type */
	if (type == NULL)
	{
		printf("[Web] There is not the related content type\n");
		return 0;
	}

	/* 构造 HTTP 首部, 并发送 */
	hdr_len = sprintf(http_header, http_res_hdr_tmpl, file_len, type);
	send_len = send(soc, http_header, hdr_len, 0);

	if (send_len == SOCKET_ERROR)
	{
		fclose(res_file);
		printf("[Web] Fail to send, error = %d\n", WSAGetLastError);
		return 0;
	}

	do /* 发送文件, HTTP 的消息体 */
	{
		read_len = fread(read_buf, sizeof(char), HTTP_BUF_SIZE, res_file);
		if (read_len > 0)
		{
			send_len = send(soc, read_buf, read_len, 0);
			file_len -= read_len;
		}
	} while ((read_len > 0) && (file_len) > 0);

	fclose(res_file);

	return 1;
}

int main(int argc, char **argv)
{
	WSADATA wsa_Data;
	SOCKET	srv_soc = 0, acpt_soc; /* socket 句柄 */
	struct sockaddr_in serv_addr; /* 服务器地址 */
	struct sockaddr_in from_addr; /* 客户端地址 */
	char recv_buf[HTTP_BUF_SIZE];
	unsigned short port = HTTP_DEF_PORT;
	unsigned long from_len = sizeof(from_addr);
	int	result = 0, recv_len;

	if (argc == 2) /* 端口号 */
		port = atoi(argv[1]);

	WSAStartup(MAKEWORD(2, 0), &wsa_Data); /* 初始化Winsock资源 */

	srv_soc = socket(AF_INET, SOCK_STREAM, 0); /* 创建socket */
	if (srv_soc == INVALID_SOCKET)
	{
		printf("[Web] socket() Fails, error = %d\n", WSAGetLastError());
		return -1;
	}

	/* 服务器地址 */
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port	 = htons(port);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	result = bind(srv_soc, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
	if (result == SOCKET_ERROR) /* 绑定失败 */
	{
		closesocket(srv_soc);
		printf("[Web] Fail to bind, error = %d\n", WSAGetLastError());
		return -1;
	}

	result = listen(srv_soc, SOMAXCONN);
	printf("[Web] The server is running ... ...\n");

	while (1)
	{
		acpt_soc = accept(srv_soc, (struct sockaddr *) &from_addr,  &from_len); /* 这里有警告 */
		if (acpt_soc == INVALID_SOCKET) /* 接受失败 */
		{
			printf("[Web] Fail to accept, error = %d\n", WSAGetLastError());
			break;
		}

		printf("[Web] Accepted address:[%s], port:[%d]\n", \
				inet_ntoa(from_addr.sin_addr), ntohs(from_addr.sin_port));

		recv_len = recv(acpt_soc, recv_buf, HTTP_BUF_SIZE, 0);
		if (recv_len == SOCKET_ERROR) /* 接受失败 */
		{
			closesocket(acpt_soc);
			printf("[Web] Fail to recv, error = %d\n", WSAGetLastError());
			break;
		}

		recv_buf[recv_len] = 0;

		/* 向客户端发送响应数据 */
		result = http_send_response(acpt_soc, recv_buf, recv_len);
		closesocket(acpt_soc);
	}
	closesocket(acpt_soc);
	WSACleanup();
	printf("[Web] the server is stopped.\n");

	return 0;
}