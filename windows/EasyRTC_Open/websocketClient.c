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


static uint32_t ws_rand_mask(void)
{
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned)time(NULL));
        seeded = 1;
    }
    return (uint32_t)rand();
}

int rtc_websocket_write(
    int sockfd,
    int opcode,
    int role,
    const uint8_t* data,
    int len)
{
    if (len < 0 || len > 0x7FFFFFFFFFFFFFFFLL)
        return -1;

    uint8_t header[14];
    int hlen = 0;
    int mask_flag = (role == 1); // client must mask
    uint32_t mask_key = 0;

    /* byte 0: FIN + RSV + opcode */
    header[0] = 0x80 | (opcode & 0x0F);

    /* byte 1: payload len + mask bit */
    if (len < 126) {
        header[1] = len | (mask_flag ? 0x80 : 0x00);
        hlen = 2;
    }
    else if (len <= 0xFFFF) {
        header[1] = 126 | (mask_flag ? 0x80 : 0x00);
        uint16_t tmp = htons((uint16_t)len);
        memcpy(&header[2], &tmp, 2);
        hlen = 4;
    }
    else {
        header[1] = 127 | (mask_flag ? 0x80 : 0x00);
        uint64_t tmp = htobe64((uint64_t)len);
        memcpy(&header[2], &tmp, 8);
        hlen = 10;
    }

    /* mask key (only for client) */
    if (mask_flag) {
        mask_key = ws_rand_mask();
        memcpy(&header[hlen], &mask_key, 4);
        hlen += 4;
    }

    /* send header */
    if (send(sockfd, header, hlen, 0) != hlen)
        return -1;

    /* send payload */
    if (len > 0) {
        if (mask_flag) {
            uint8_t* buf = malloc(len);
            if (!buf) return -1;

            for (int i = 0; i < len; i++) {
                buf[i] = data[i] ^ ((uint8_t*)&mask_key)[i % 4];
            }

            int ret = send(sockfd, buf, len, 0);
            free(buf);
            return (ret == len) ? (hlen + len) : -1;
        }
        else {
            return send(sockfd, data, len, 0) == len
                ? hlen + len : -1;
        }
    }

    return hlen;
}

//
//int rtc_websocket_write(int sockfd, int opcode, int mask, char* data, int iLength)
//{
//	if (iLength < 126)
//	{
//		char buffer[256];
//		ws_0m_header* ws0header = (ws_0m_header*)buffer;
//		ws0header->opcode = opcode;
//		ws0header->rsv1 = 0;
//		ws0header->rsv2 = 0;
//		ws0header->rsv3 = 0;
//		ws0header->fin = 1;
//		ws0header->payloadlen = iLength;
//		ws0header->mask = mask;
//		ws0header->maskey[0] = 0x55;
//		ws0header->maskey[1] = 0x66;
//		ws0header->maskey[2] = 0x77;
//		ws0header->maskey[3] = 0x88;
//		if (iLength > 0)
//		{
//			if (mask == 0x01)
//			{
//				for (int i = 0; i < iLength; i++)
//				{
//					ws0header->payloaddata[i] = (data[i] ^ ws0header->maskey[i % 4]);
//				}
//			}
//			else
//			{
//				for (int i = 0; i < iLength; i++)
//				{
//					ws0header->payloaddata[i] = data[i];
//				}
//			}
//		}
//
//		return send(sockfd, buffer, sizeof(ws_0m_header) + iLength, 0);
//	}
//	else if (iLength < 65536)
//	{
//		char* buffer = (char*)malloc(iLength + sizeof(ws_1m_header));
//		ws_1m_header* ws1header = (ws_1m_header*)buffer;
//		ws1header->opcode = opcode;
//		ws1header->rsv1 = 0;
//		ws1header->rsv2 = 0;
//		ws1header->rsv3 = 0;
//		ws1header->fin = 1;
//		ws1header->payloadlen = 126;
//		ws1header->mask = mask;
//		ws1header->expayloadlen = htobe16(iLength);
//		if (mask == 0x01)
//		{
//			ws1header->maskey[0] = 0x55;
//			ws1header->maskey[1] = 0x66;
//			ws1header->maskey[2] = 0x77;
//			ws1header->maskey[3] = 0x88;
//			for (int i = 0; i < iLength; i++)
//			{
//				ws1header->payloaddata[i] = (data[i] ^ ws1header->maskey[i % 4]);
//			}
//		}
//		else
//		{
//			ws1header->maskey[0] = 0x00;
//			ws1header->maskey[1] = 0x00;
//			ws1header->maskey[2] = 0x00;
//			ws1header->maskey[3] = 0x00;
//			for (int i = 0; i < iLength; i++)
//			{
//				ws1header->payloaddata[i] = data[i];
//			}
//		}
//		
//
//		int ret = send(sockfd, buffer, sizeof(ws_1m_header) + iLength, 0);
//		free(buffer);
//		return ret;
//	}
//	else
//	{
//		char* buffer = (char*)malloc(iLength + sizeof(ws_2m_header));
//		ws_2m_header* ws2header = (ws_2m_header*)buffer;
//		ws2header->opcode = opcode;
//		ws2header->rsv1 = 0;
//		ws2header->rsv2 = 0;
//		ws2header->rsv3 = 0;
//		ws2header->fin = 1;
//		ws2header->payloadlen = 127;
//		ws2header->mask = mask;
//		ws2header->expayloadlen = htobe64(iLength);
//		ws2header->maskey[0] = 0x55;
//		ws2header->maskey[1] = 0x66;
//		ws2header->maskey[2] = 0x77;
//		ws2header->maskey[3] = 0x88;
//		if (mask == 1)
//		{
//			for (int i = 0; i < iLength; i++)
//			{
//				ws2header->payloaddata[i] = (data[i] ^ ws2header->maskey[i % 4]);
//			}
//		}
//		else
//		{
//			for (int i = 0; i < iLength; i++)
//			{
//				ws2header->payloaddata[i] = data[i];
//			}
//		}
//
//		int ret = send(sockfd, buffer, sizeof(ws_2m_header) + iLength, 0);
//		free(buffer);
//		return ret;
//	}
//
//	return -1;
//}
