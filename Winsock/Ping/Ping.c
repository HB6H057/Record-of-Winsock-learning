#include <stdio.h>
#include <winsock2.h>

#pragma comment (lib, "ws2_32.lib") /* Winsock使用的库函数 */

/* ICMP类型 */
#define ICMP_TYPE_ECHO			8
#define ICMP_TYPE_ECHO_REPLY	0

#define ICMP_MIN_LEN		8		 /* ICMP最小长度，只有首部 */
#define ICMP_DEF_COUNT		4		 /* 默认数据次数 */
#define ICMP_DEF_SIZE		32		 /* 默认数据长度 */
#define ICMP_DEF_TIMEOUT	1000	/* 默认超时时间，毫秒 */
#define ICMP_MAX_SIZE		65500	/* 最大数据长度 */

	/* IP首部 —— RFC791 */
struct ip_hdr
{
	unsigned char vers_len;		/* 版本和首部长度 */
	unsigned char tos;			/* 服务类型 */
	unsigned short total_len;	/* 数据包的总长度 */
	unsigned short id;			/* 标识符 */
	unsigned short frag;			/* 标志和片偏移 */ 
	unsigned char ttl;			/* 生产时间 */
	unsigned char proto;		/* 协议 */
	unsigned short checksum;		/* 校验和 */
	unsigned int sour;			/* 源IP地址 */
	unsigned int dest;			/* 目的IP地址 */
};

	/* ICMP首部——792 */
struct icmp_hdr
{
	unsigned char type;			/* 类型 */
	unsigned char code;			/* 代码 */
	unsigned short checksum;	/* 校验和 */
	unsigned short id;			/* 标识符 */
	unsigned short seq;			/* 序列号 */

	/* 不是标志的ICMP首部 用于记录时间 */
	unsigned long timestamp;
};

struct icmp_user_opt
{
	unsigned int persist;		/* 一直PING */
	unsigned int count;			/* 发送Echo请求的数量 */
	unsigned int size;			/* 发送数据的大小 */
	unsigned int timeout;		/* 等待答复的超时时间 */
	char		 *host;			/* 主机地址 */
	unsigned int send;			/* 发送数量 */
	unsigned int recv;			/* 接收数量 */
	unsigned int min_t;			/* 最短时间 */
	unsigned int max_t;			/* 最长时间 */
	unsigned int total_t;		/* 总的累计时间 */
};

	/* 随机数据 */
const char icmp_rand_data[] = "abcdefghijklmnopqrstuvwxyz0123456"\
								"ABCDEFGHIJKLMNOPQRSTUVWXYZ";

struct icmp_user_opt user_opt_g = {
	0, ICMP_DEF_COUNT, ICMP_DEF_SIZE, ICMP_DEF_TIMEOUT, NULL,
	0, 0, 0XFFFF, 0
};

unsigned short ip_checksum(unsigned short *buf, int buf_len);

void icmp_make_data(char *icmp_data, int data_size, int sequence)
{
	struct icmp_hdr *icmp_hdr;
	char *data_buf;
	int data_len;
	int fill_count = sizeof(icmp_rand_data)	/ sizeof(icmp_rand_data[0]);

	/* 填写ICMP数据 */
	data_buf = icmp_data + sizeof(struct icmp_hdr);
	data_len = data_size - sizeof(struct icmp_hdr);

	while (data_len > fill_count)
	{
		memcpy(data_buf, icmp_rand_data, fill_count);
		data_len -= fill_count;
	}

	if (data_len > 0)
		memcpy(data_buf, icmp_rand_data, data_len);

	/* 填写ICMP首部 */
	icmp_hdr = (struct icmp_hdr *)icmp_data;

	icmp_hdr->type = ICMP_TYPE_ECHO;
	icmp_hdr->code = 0;
	icmp_hdr->id   = (unsigned short)GetCurrentProcessId();
	icmp_hdr->seq  = sequence;
	icmp_hdr->checksum = 0;
	icmp_hdr->timestamp = GetTickCount();
	
	icmp_hdr->checksum = ip_checksum((unsigned short *)icmp_data, data_size);
}

int icmp_parse_reply(char *buf, int buf_len, struct sockaddr_in *from)
{
	struct ip_hdr *ip_hdr;
	struct icmp_hdr *icmp_hdr;
	unsigned short hdr_len;
	int icmp_len;
	unsigned long trip_t;

	ip_hdr = (struct ip_hdr *)buf;
	hdr_len = (ip_hdr->vers_len & 0xf) << 2; /* IP首部长度 */

	if (buf_len < hdr_len + ICMP_MIN_LEN)
	{
		printf("[Ping] Too few bytes from %s\n", inet_ntoa(from->sin_addr));
		return -1;
	}

	icmp_hdr = (struct icmp_hdr *)(buf + hdr_len);
	icmp_len = ntohs(ip_hdr->total_len) - hdr_len;

	/* 检查校验和 */
	if (ip_checksum((unsigned short *)icmp_hdr, icmp_len))
	{
		printf("[Ping] icmp checksum error!\n");
		return -1;
	}

	/* 检查ICMP类型 */
	if (icmp_hdr->type != ICMP_TYPE_ECHO_REPLY)
	{
		printf("[Ping] not echo reply: %d\n", icmp_hdr->type);
		return -1;
	}

	/* 检查ICMP的ID */
	if (icmp_hdr->id != (unsigned short)GetCurrentProcessId())
	{
		printf("[Ping] someone else's message!\n");
		return -1;
	}

	/* 输出响应信息 */
	trip_t = GetTickCount() - icmp_hdr->timestamp;
	buf_len = ntohs(ip_hdr->total_len) - hdr_len - ICMP_MIN_LEN;
	printf("%d bytes from %s:", buf_len, inet_ntoa(from->sin_addr));
	printf(" icmp_seq = %d time = %d ms\n ", icmp_hdr->seq, trip_t);

	user_opt_g.recv++;
	user_opt_g.total_t += trip_t;

	/* 记录返回时间 */
	if (user_opt_g.min_t > trip_t)
		user_opt_g.min_t = trip_t;

	if (user_opt_g.max_t < trip_t )
		user_opt_g.max_t = trip_t;

	return 0;
}

int icmp_process_reply(SOCKET icmp_soc)
{
	struct sockaddr_in from_addr;
	int result, data_size = user_opt_g.size;
	int from_len = sizeof(from_addr);
	char *recv_buf;

	data_size += sizeof(struct ip_hdr) + sizeof(struct icmp_hdr);
	recv_buf = malloc(data_size);

	/* 接收数据 */
	result = recvfrom(icmp_soc, recv_buf, data_size, 0,\
		(struct sockaddr*) &from_addr, &from_len);
	if (result == SOCKET_ERROR)
	{
		if (WSAGetLastError() == WSAETIMEDOUT)
			printf("timed out\n");
		else
			printf("[Ping] recvfrom failed: %d\n", WSAGetLastError());

		return -1;
	}
	
	result = icmp_parse_reply(recv_buf, result &from_addr);
	free(recv_buf);

	return result;
}

void icmp_help(char *prog_name)
{
	char *file_name;

	file_name = strrchr(prog_name, '//');
	if (file_name != NULL)
		file_name++;
	else
		file_name = prog_name;

	/* 显示帮助信息 */
	printf(" usage:		%s host_address [-t] [-n count] [-l size] "\
			"[-w timeout]\n", file_name);
	printf(" -t		ping the host until stoppped.\n");
	printf(" -n count	the count to send ECHO\n");
	printf(" -l size	the size to send data\n");
	printf(" -w timeout timeout to wait the reply\n");
	exit(1);
}

void icmp_parse_param(int argc, char **argv)
{
	int i;

	for (i = 1; i< argc; i++)
	{
		if ((argv[i][0] != '-') && (argv[i][0] != '/'))
		{
			/* 处理主机名 */
			if (user_opt_g.host)
				icmp_help(argv[0]);
			else
			{
				user_opt_g.host = argv[i];
				continue;
			}
		}

		switch (tolower(argv[i][1]))
		{
		case 't': /* 持续ping */
			user_opt_g.persist = 1;
			break;

		case 'n': /* 发送请求的数量 */
			i++;
			user_opt_g.count = atoi(argv[i]);
			break;

		case 'l': /* 发送数据的大小 */
			i++;
			user_opt_g.size = atoi(argv[i]);
			if (user_opt_g.size > ICMP_MAX_SIZE)
				user_opt_g.size = ICMP_MAX_SIZE;
			break;

		case 'w': /* 等待接收的超时时间 */
			i++;
			user_opt_g.timeout = atoi(argv[i]);
			break;
		default:
			icmp_help(argv[0]);
			break;
		}
	}
}

int main(int argc, char **argv)
{
	WSADATA wsaData;
	SOCKET icmp_soc;
	struct sockaddr_in dest_addr;
	struct hostent *host_ent = NULL;

	int result, data_size, send_len;
	unsigned int i, timeout, lost;
	char *icmp_dat;
	unsigned int ip_addr = 0;
	unsigned short seq_no = 0;

	if (argc < 2)
		icmp_help(argv[0]);

	icmp_parse_param(argc, argv);
	WSAStartup(MAKEWORD(2, 0), &wsaData);

	/* 解析主机地址 */
	



	return 0;
}