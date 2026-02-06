#include "EasyRTC_websocket.h"


#include "websocketClient.h"
#include "osthread.h"
#include <stdio.h>



#ifdef _WIN32
#define socklen_t	size_t
#define __attribute__(x)
#else
#include <errno.h>      // 定义 EAGAIN, EWOULDBLOCK 等
#include <unistd.h>     // 可能需要（read/write）
#include <stdio.h>
#include <sys/random.h> 
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#define INVALID_SOCKET -1
#define closesocket(x)	close(x)
#define __attribute__(x)
#define Sleep(x)	usleep(x*1000)
#endif

typedef struct __WSS_CLIENT_T
{
	OSTHREAD_OBJ_T* workerThread;

	char	serverAddr[128];
	int		serverPort;
	int		isSecure;          // 1 = wss, 0 = ws


	int		sockfd;			// 仅用于 ws
	ws_connect_callback	connectCallback;
	ws_register_callback	registerCallback;
	ws_idle_callback		idleCallback;
	ws_data_callback	dataCallback;
	int		registerStatus;

	void*	userptr;
}WSS_CLIENT_T;


/**
 * 生成随机UUID
 * @param buf - 用于存储UUID字符串的缓冲区，大小至少为37字节
 * @return 指向buf的指针
 */
char* random_uuid(char buf[37]) {
	const char* c = "89ab";  // UUID版本4的特定位
	char* p = buf;
	int n;

	// 初始化随机数种子
	static int seeded = 0;
	if (!seeded) {
		srand((unsigned int)time(NULL));
		seeded = 1;
	}

	for (n = 0; n < 16; ++n) {
		int b = rand() % 255;

		switch (n) {
		case 6:  // 设置版本位为4
			sprintf(p, "4%x", b % 15);
			break;
		case 8:  // 设置变体位
			sprintf(p, "%c%x", c[rand() % strlen(c)], b % 15);
			break;
		default:
			sprintf(p, "%02x", b);
			break;
		}
		p += 2;

		// 在特定位置插入连字符
		switch (n) {
		case 3:
		case 5:
		case 7:
		case 9:
			*p++ = '-';
			break;
		}
	}
	*p = '\0';  // 字符串结束符
	return buf;
}


// IP地址验证函数
int is_valid_ip(const char* ip) {
	if (ip == NULL) return 0;

	int num = 0;
	int dotCount = 0;
	int len = strlen(ip);

	if (len < 7 || len > 15) return 0;

	for (int i = 0; i < len; i++) {
		if (ip[i] == '.') {
			dotCount++;
			if (dotCount > 3) return 0;
			if (i > 0 && ip[i - 1] == '.') return 0;
			if (num < 0 || num > 255) return 0;
			num = 0;
		}
		else if (isdigit(ip[i])) {
			num = num * 10 + (ip[i] - '0');
			if (num > 255) return 0;
		}
		else {
			return 0;
		}
	}

	if (dotCount != 3) return 0;
	if (num < 0 || num > 255) return 0;

	return 1;
}


// DNS 解析函数：输入域名，输出第一个 IPv4 地址（字符串形式）
// 返回 0 表示成功，-1 表示失败
int resolve_domain_to_ip(const char* hostname, char* ip_str, size_t ip_str_len) {
	struct addrinfo hints, * res = NULL;
	int status;

	// 初始化 hints
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;       // 仅 IPv4
	hints.ai_socktype = SOCK_STREAM; // TCP（也可以设为0）

	// 调用 getaddrinfo
	if ((status = getaddrinfo(hostname, NULL, &hints, &res)) != 0) {
		//fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
		printf("getaddrinfo error: %d\n", status);
		return -1;
	}

	// 获取第一个结果并转换为可读 IP
	struct sockaddr_in* addr = (struct sockaddr_in*)res->ai_addr;
	const char* ip = inet_ntop(AF_INET, &addr->sin_addr, ip_str, ip_str_len);
	if (ip == NULL) {
		//perror("inet_ntop");
		freeaddrinfo(res);
		return -1;
	}

	freeaddrinfo(res); // 释放资源
	return 0;
}


// 初始化网络上下文
static int net_init(WSS_CLIENT_T* ctx, int isSecure) {
	ctx->isSecure = isSecure;
	ctx->sockfd = INVALID_SOCKET;

	return 0;
}

// 连接服务器
static int net_connect(WSS_CLIENT_T* ctx, const char* host, int port) {
	char port_str[16];
	snprintf(port_str, sizeof(port_str), "%d", port);

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons((unsigned short)port);
	addr.sin_addr.s_addr = inet_addr(host);
	if (addr.sin_addr.s_addr == INADDR_NONE) return -1;

	ctx->sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ctx->sockfd == INVALID_SOCKET) return -1;

	if (connect(ctx->sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		closesocket(ctx->sockfd);
		ctx->sockfd = INVALID_SOCKET;
		return -1;
	}
	return 0;

}

int websocket_send(int sockfd, char* buf, int len)
{
	return send(sockfd, (const char*)buf, (int)len, 0);
}


// 发送数据
static int net_send(WSS_CLIENT_T* ctx, int opcode, const unsigned char* buf, size_t len) {

	return rtc_websocket_write(ctx->sockfd, opcode, (const char*)buf, (int)len);
}

// 接收数据
static int net_recv(WSS_CLIENT_T* ctx, unsigned char* buf, size_t len) {
	return recv(ctx->sockfd, (char*)buf, (int)len, 0);
}

// 关闭连接
static void net_close(WSS_CLIENT_T* ctx) {
	if (ctx->sockfd != INVALID_SOCKET) {
		closesocket(ctx->sockfd);
		ctx->sockfd = INVALID_SOCKET;
	}
}

// 获取底层 fd（用于 select）
static int net_get_fd(WSS_CLIENT_T* ctx) {
	return (int)ctx->sockfd;
}

#ifdef _WIN32
DWORD WINAPI __EasyRTC_Worker_Thread(void* lpParam)
#else
void* __EasyRTC_Worker_Thread(void* lpParam)
#endif
{
	OSTHREAD_OBJ_T* pThread = (OSTHREAD_OBJ_T*)lpParam;
	WSS_CLIENT_T* pWssClient = (WSS_CLIENT_T*)pThread->userPtr;

	pThread->flag = THREAD_STATUS_RUNNING;

	// 生成 Sec-WebSocket-Key
	static unsigned char shakeKey[32] = { 0 };
	if (memcmp(shakeKey, "\0\0\0\0", 4) == 0) {
		size_t len = 0;
		unsigned char tempKey[16] = { 0 };

#ifdef _WIN32
		GUID* guid = (GUID*)tempKey;
		CoCreateGuid(guid);
#else
		FILE* urandom = fopen("/dev/urandom", "rb");
		if (urandom) {
			fread(tempKey, 1, sizeof(tempKey), urandom);
			fclose(urandom);
		}
#endif
		mbedtls_base64_encode(shakeKey, 32, &len, tempKey, 16);
	}

	int maxrecvlen = 400 * 1024;
	char* recvbuf = (char*)malloc(maxrecvlen);
	if (!recvbuf) goto exit_thread;

	while (1) {
		if (pThread->flag == THREAD_STATUS_EXIT) break;

		int isDomainName = !is_valid_ip(pWssClient->serverAddr);
		char serverIP[64] = { 0 };
		const char* connectHost = pWssClient->serverAddr;

		if (isDomainName) {
			if (resolve_domain_to_ip(pWssClient->serverAddr, serverIP, sizeof(serverIP)) != 0) {
				pWssClient->connectCallback(pWssClient->userptr, EASYRTC_WS_DNS_FAIL);
				Sleep(3000);
				continue;
			}
			connectHost = serverIP;
		}

		pWssClient->connectCallback(pWssClient->userptr, EASYRTC_WS_CONNECTING);

		if (net_init(pWssClient, pWssClient->isSecure) != 0) {
			pWssClient->connectCallback(pWssClient->userptr, EASYRTC_WS_CONNECT_FAIL);
			Sleep(3000);
			continue;
		}

		if (net_connect(pWssClient, connectHost, pWssClient->serverPort) != 0) {
			net_close(pWssClient);
			pWssClient->connectCallback(pWssClient->userptr, EASYRTC_WS_CONNECT_FAIL);
			Sleep(3000);
			continue;
		}

		// 构造 Host 头（WSS 默认端口 443，WS 默认 80）
		char hostWithPort[128];
		if ((pWssClient->isSecure && pWssClient->serverPort == 443) ||
			(!pWssClient->isSecure && pWssClient->serverPort == 80)) {
			snprintf(hostWithPort, sizeof(hostWithPort), "%s", pWssClient->serverAddr);
		}
		else {
			snprintf(hostWithPort, sizeof(hostWithPort), "%s:%d", pWssClient->serverAddr, pWssClient->serverPort);
		}

		const char httpRequest[] =
			"GET / HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Connection: Upgrade\r\n"
			"Upgrade: websocket\r\n"
			"Sec-WebSocket-Key: %s\r\n"
			"Sec-WebSocket-Version: 13\r\n"
			"\r\n";

		char httpHead[2048];
		int httpHeadLen = snprintf(httpHead, sizeof(httpHead), httpRequest, hostWithPort, shakeKey);


		if (websocket_send(pWssClient->sockfd, (unsigned char*)httpHead, httpHeadLen) != httpHeadLen) {
			net_close(pWssClient);
			pWssClient->connectCallback(pWssClient->userptr, EASYRTC_WS_DISCONNECT);
			Sleep(3000);
			continue;
		}


		// 接收握手响应
		memset(recvbuf, 0, maxrecvlen);
		int total = 0;
		while (total < 4 || strstr(recvbuf, "\r\n\r\n") == NULL) {
			int rc = net_recv(pWssClient, (unsigned char*)(recvbuf + total), maxrecvlen - total - 1);
			if (rc <= 0) {
				net_close(pWssClient);
				pWssClient->connectCallback(pWssClient->userptr, EASYRTC_WS_DISCONNECT);
				Sleep(3000);
				goto reconnect;
			}
			total += rc;
			recvbuf[total] = '\0';
		}

		if (strncmp(recvbuf, "HTTP/1.1 101 ", 13) != 0 ||
			!strstr(recvbuf, "Sec-WebSocket-Accept:")) {
			net_close(pWssClient);
			pWssClient->connectCallback(pWssClient->userptr, EASYRTC_WS_DISCONNECT);
			Sleep(3000);
			goto reconnect;
		}

		pWssClient->connectCallback(pWssClient->userptr, EASYRTC_WS_CONNECTED);

		socket_set_keepalive(pWssClient->sockfd);

		// ========== WebSocket 数据循环 ==========
		int pos = 0;
		time_t registerTime = 0;
		pWssClient->registerStatus = 0;

		while (1) {
			if (pThread->flag == THREAD_STATUS_EXIT) break;

			if (pWssClient->registerStatus == 0) {
				time_t tCurrent = time(NULL);
				if (tCurrent - registerTime > 5) {
					pWssClient->registerCallback(pWssClient->userptr);
					registerTime = tCurrent;
				}
			}

			fd_set rfds;
			FD_ZERO(&rfds);
			int sockfd = net_get_fd(pWssClient);
			if (sockfd < 0) break;
			FD_SET(sockfd, &rfds);
			struct timeval timeout = { 1, 0 };
			int rc = select(sockfd + 1, &rfds, NULL, NULL, &timeout);
			if (rc == 0) {
				if (pWssClient->idleCallback) pWssClient->idleCallback(pWssClient->userptr);
				continue;
			}
			if (rc < 0 || !FD_ISSET(sockfd, &rfds)) break;

			rc = net_recv(pWssClient, (unsigned char*)(recvbuf + pos), maxrecvlen - pos - 1);
			if (rc <= 0) {
#ifdef _WIN32
				int err = WSAGetLastError();
				if (err == WSAEWOULDBLOCK) {
					continue;
				}
#else
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					continue;
				}
#endif


				//if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == EWOULDBLOCK || rc == EAGAIN) continue;
				break;
			}
			pos += rc;
			recvbuf[pos] = '\0';

			ws_comm_header* wsheader = (ws_comm_header*)recvbuf;
			ws_0_header* ws0header = (ws_0_header*)recvbuf;
			ws_1_header* ws1header = (ws_1_header*)recvbuf;
			ws_0m_header* ws0mheader = (ws_0m_header*)recvbuf;
			ws_1m_header* ws1mheader = (ws_1m_header*)recvbuf;

			int errCode = 0;
		NEXTLOOP1:
			if (pos < (int)sizeof(ws_comm_header)) continue;

			if ((wsheader->fin == 0) || (wsheader->rsv1 != 0) || (wsheader->rsv2 != 0) ||
				(wsheader->rsv3 != 0) || (wsheader->opcode == WS_OPCODE_CLOSE)) 
			{
				pWssClient->connectCallback(pWssClient->userptr, EASYRTC_WS_DISCONNECT);

				break;
			}
			else if (wsheader->opcode == WS_OPCODE_PING) {
				int payloadlen = ws0header->payloadlen;
				int framesize = (ws0header->mask == 0) ? (sizeof(ws_0_header) + payloadlen)
					: (sizeof(ws_0m_header) + payloadlen);
				if (framesize > pos) continue;

				// 发送 PONG
				unsigned char pong[2] = { 0x8A, 0x00 }; // FIN=1, opcode=10 (PONG), no payload
				net_send(pWssClient, WS_OPCODE_PONG, pong, 2);

				if (pos > framesize) memmove(recvbuf, recvbuf + framesize, pos - framesize);
				pos -= framesize;
				if (pos >= (int)sizeof(ws_comm_header)) goto NEXTLOOP1;
			}
			else {
				int payloadlen, framesize;
				if (wsheader->payloadlen < 126) {
					payloadlen = ws0header->payloadlen;
					framesize = (ws0header->mask == 0) ? (sizeof(ws_0_header) + payloadlen)
						: (sizeof(ws_0m_header) + payloadlen);
					if (framesize > pos) continue;
					if (ws0header->mask == 0) {
						ws0header->payloaddata[payloadlen] = '\0';
						errCode = pWssClient->dataCallback(pWssClient->userptr, ws0header->payloaddata, payloadlen);
					}
					else {
						for (int i = 0; i < payloadlen; i++) {
							ws0mheader->payloaddata[i] ^= ws0mheader->maskey[i % 4];
						}
						errCode = pWssClient->dataCallback(pWssClient->userptr, ws0mheader->payloaddata, payloadlen);
					}
				}
				else if (wsheader->payloadlen == 126) {
					payloadlen = be16toh(ws1header->expayloadlen);
					framesize = (ws1header->mask == 0) ? (sizeof(ws_1_header) + payloadlen)
						: (sizeof(ws_1m_header) + payloadlen);
					if (framesize > maxrecvlen) {
						int maxrecvlen2 = (framesize + 2048);// / 1024 * 1024;
						char* recvbuf2 = (char*)malloc(maxrecvlen2);
						memcpy(recvbuf2, recvbuf, maxrecvlen);
						free(recvbuf);

						maxrecvlen = maxrecvlen2;
						recvbuf = recvbuf2;

						wsheader = (ws_comm_header*)recvbuf;
						ws0header = (ws_0_header*)recvbuf;
						ws1header = (ws_1_header*)recvbuf;
						ws0mheader = (ws_0m_header*)recvbuf;
						ws1mheader = (ws_1m_header*)recvbuf;
					}
					if (framesize > pos) continue;
					if (ws1header->mask == 0) {
						ws1header->payloaddata[payloadlen] = '\0';
						errCode = pWssClient->dataCallback(pWssClient->userptr, ws1header->payloaddata, payloadlen);
					}
					else {
						for (int i = 0; i < payloadlen; i++) {
							ws1mheader->payloaddata[i] ^= ws1mheader->maskey[i % 4];
						}
						errCode = pWssClient->dataCallback(pWssClient->userptr, ws1mheader->payloaddata, payloadlen);
					}
				}
				else {
					break; // payloadlen == 127 not supported
				}

				if (errCode != 0) break;
				if (pos > framesize) memmove(recvbuf, recvbuf + framesize, pos - framesize);
				pos -= framesize;
				if (pos >= (int)sizeof(ws_comm_header)) goto NEXTLOOP1;
			}
		}

	reconnect:
		net_close(pWssClient);
		memset(recvbuf, 0, maxrecvlen);
		pos = 0;


		if (pThread->flag == THREAD_STATUS_EXIT) break;
		Sleep(3000);
	}

exit_thread:
	free(recvbuf);
	pThread->flag = THREAD_STATUS_INIT;
	return 0;
}


int websocketCreate(const char* serverAddr, const int serverPort, const int isSecure, void* userptr,
	ws_connect_callback connectCallback, ws_register_callback registerCallback, ws_data_callback dataCallback,
	ws_idle_callback idleCallback,
	void** ppWssClient)
{
	WSS_CLIENT_T* pWssClient = (WSS_CLIENT_T*)malloc(sizeof(WSS_CLIENT_T));
	if (NULL == pWssClient)	return -2;

	memset(pWssClient, 0x00, sizeof(WSS_CLIENT_T));
	strcpy(pWssClient->serverAddr, serverAddr);
	pWssClient->serverPort = serverPort;
	pWssClient->isSecure = isSecure;
	pWssClient->connectCallback = connectCallback;
	pWssClient->registerCallback = registerCallback;
	pWssClient->dataCallback = dataCallback;
	pWssClient->idleCallback = idleCallback;
	pWssClient->userptr = userptr;
	CreateOSThread(&pWssClient->workerThread, __EasyRTC_Worker_Thread, (void*)pWssClient, 0);

	if (NULL != pWssClient->workerThread)
	{
		if (NULL != ppWssClient)	*ppWssClient = pWssClient;
	}
	else
	{
		if (NULL != ppWssClient)	*ppWssClient = NULL;
		free(pWssClient);
	}

	return 0;
}

int websocketSetRegisterStatus(void* _pWssClient, int registerStatus)
{
	WSS_CLIENT_T* pWssClient = (WSS_CLIENT_T*)_pWssClient;

	if (NULL == pWssClient)	return -1;

	pWssClient->registerStatus = registerStatus;
	return 0;
}

int websocketSendData(void* _pWssClient, int opcode, char* data, int size)
{
	WSS_CLIENT_T* pWssClient = (WSS_CLIENT_T*)_pWssClient;
	if (NULL == pWssClient)	return -1;

	return net_send(pWssClient, opcode, data, size);
}

void websocketRelease(void** ppWssClient)
{
	if (NULL == ppWssClient)	return;
	if (NULL == *ppWssClient)	return;

	WSS_CLIENT_T* pWssClient = (WSS_CLIENT_T*)*ppWssClient;
	
	if (NULL != pWssClient->workerThread)
	{
		DeleteOSThread(&pWssClient->workerThread);
	}

	free(pWssClient);

	*ppWssClient = NULL;
}



/****************************************************
 * 函数: getBuildTime
 * 功能: 获取软件编译时间和日期
 ***************************************************/
void getBuildTime(int* _year, int* _month, int* _day, int* _hour, int* _minute, int* _second)
{
	char monthStr[12][5] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	char buf[16] = { 0 };
	char monthBuf[4] = { 0 };
	unsigned int i = 0;

	int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;

	strcpy(buf, __DATE__);
	memset(monthBuf, 0, 4);

	strncpy(monthBuf, buf, 3);
	day = buf[5] - '0';
	if (buf[4] != ' ')
	{
		day += (buf[4] - '0') * 10;
	}
	year = (buf[7] - '0') * 1000 + (buf[8] - '0') * 100 + (buf[9] - '0') * 10 + buf[10] - '0';
	strcpy(buf, __TIME__);
	hour = (buf[0] - '0') * 10 + buf[1] - '0';
	minute = (buf[3] - '0') * 10 + buf[4] - '0';
	second = (buf[6] - '0') * 10 + buf[7] - '0';
	for (i = 0; i < (sizeof(monthStr) / sizeof(monthStr[0])); i++)
	{
		if (0 == strcmp(monthStr[i], monthBuf))
		{
			month = i + 1;
			break;
		}
	}
	if (i >= 12)
	{
		month = 1;
	}

	if (_year)	*_year = year;
	if (_month)	*_month = month;
	if (_day)	*_day = day;
	if (_hour)	*_hour = hour;
	if (_minute)	*_minute = minute;
	if (_second)	*_second = second;
}

int websocketGetVersion(char* version)
{
	int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
	getBuildTime(&year, &month, &day, &hour, &minute, &second);

	if (NULL == version)	return -1;

	sprintf(version, "EasyRTC_Open v1.0.%d.%02d%02d", year - 2000, month, day);

	return 0;
}
