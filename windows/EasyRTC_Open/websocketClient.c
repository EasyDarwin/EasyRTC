#include "websocketClient.h"


int socket_set_keepalive(int sockfd)
{
#if defined(_WIN32)
	int enableKeepAlive = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (const char*)&enableKeepAlive, sizeof(enableKeepAlive)) != 0)
	{
		//return -1;
	}

	struct tcp_keepalive keepAlive = { 0 };
	keepAlive.onoff = 1;
	keepAlive.keepalivetime = 20000;
	keepAlive.keepaliveinterval = 10000;

	DWORD bytesReturned;
	if (WSAIoctl(sockfd, SIO_KEEPALIVE_VALS, &keepAlive, sizeof(keepAlive), NULL, 0, &bytesReturned, NULL, NULL) == SOCKET_ERROR)
	{
		return -1;
	}
#else
	int enableKeepAlive = 1;
	int keepIdle = 10;	// Idle time before starting keepalive probes
	int keepInterval = 5; // Interval between keepalive probes
	int keepCount = 3;	// Number of keepalive probes before declaring the connection dead

	if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &enableKeepAlive, sizeof(enableKeepAlive)) < 0)
	{
		//perror("Cannot set SO_KEEPALIVE");
	}

#if defined(TCP_KEEPIDLE)
	if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(keepIdle)) < 0)
	{
		//perror("Cannot set TCP_KEEPIDLE");
	}
#endif

#ifdef TCP_KEEPINTVL
	if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(keepInterval)) < 0)
	{
		//perror("Cannot set TCP_KEEPINTVL");
	}
#endif

#ifdef TCP_KEEPCNT
	if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(keepCount)) < 0)
	{
		//perror("Cannot set TCP_KEEPCNT");
	}
#endif

#endif

	return 0;
}




int rtc_websocket_write(int sockfd, int opcode, char* data, int iLength)
{
	if (iLength < 126)
	{
		char buffer[256];
		ws_0m_header* ws0header = (ws_0m_header*)buffer;
		ws0header->opcode = opcode;
		ws0header->rsv1 = 0;
		ws0header->rsv2 = 0;
		ws0header->rsv3 = 0;
		ws0header->fin = 1;
		ws0header->payloadlen = iLength;
		ws0header->mask = 1;
		ws0header->maskey[0] = 0x55;
		ws0header->maskey[1] = 0x66;
		ws0header->maskey[2] = 0x77;
		ws0header->maskey[3] = 0x88;
		if (iLength > 0)
		{
			for (int i = 0; i < iLength; i++)
			{
				ws0header->payloaddata[i] = (data[i] ^ ws0header->maskey[i % 4]);
			}
		}
		return send(sockfd, buffer, sizeof(ws_0m_header) + iLength, 0);
	}
	else if (iLength < 65536)
	{
		char* buffer = (char*)malloc(iLength + sizeof(ws_1m_header));
		ws_1m_header* ws1header = (ws_1m_header*)buffer;
		ws1header->opcode = opcode;
		ws1header->rsv1 = 0;
		ws1header->rsv2 = 0;
		ws1header->rsv3 = 0;
		ws1header->fin = 1;
		ws1header->payloadlen = 126;
		ws1header->mask = 1;
		ws1header->expayloadlen = htobe16(iLength);
		ws1header->maskey[0] = 0x55;
		ws1header->maskey[1] = 0x66;
		ws1header->maskey[2] = 0x77;
		ws1header->maskey[3] = 0x88;
		for (int i = 0; i < iLength; i++)
		{
			ws1header->payloaddata[i] = (data[i] ^ ws1header->maskey[i % 4]);
		}

		int ret = send(sockfd, buffer, sizeof(ws_1m_header) + iLength, 0);
		free(buffer);
		return ret;
	}
	else
	{
		char* buffer = (char*)malloc(iLength + sizeof(ws_2m_header));
		ws_2m_header* ws2header = (ws_2m_header*)buffer;
		ws2header->opcode = opcode;
		ws2header->rsv1 = 0;
		ws2header->rsv2 = 0;
		ws2header->rsv3 = 0;
		ws2header->fin = 1;
		ws2header->payloadlen = 127;
		ws2header->mask = 1;
		ws2header->expayloadlen = htobe64(iLength);
		ws2header->maskey[0] = 0x55;
		ws2header->maskey[1] = 0x66;
		ws2header->maskey[2] = 0x77;
		ws2header->maskey[3] = 0x88;
		for (int i = 0; i < iLength; i++)
		{
			ws2header->payloaddata[i] = (data[i] ^ ws2header->maskey[i % 4]);
		}

		int ret = send(sockfd, buffer, sizeof(ws_2m_header) + iLength, 0);
		free(buffer);
		return ret;
	}

	return -1;
}
