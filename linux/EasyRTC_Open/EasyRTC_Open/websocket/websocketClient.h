#ifndef __WEBSOCKET_CLIENT_H__
#define __WEBSOCKET_CLIENT_H__

#include <stdint.h>

#define USE_MBEDTLS

#include "mbedtls/net_sockets.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

#ifdef _WIN32
#include <winsock2.h>
#include <mstcpip.h>
#pragma comment(lib, "ws2_32")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#ifdef ANDROID
#ifndef TCP_KEEPINTVL
# define TCP_KEEPINTVL 79
#endif

#ifndef TCP_KEEPCNT
# define TCP_KEEPCNT   80
#endif

#ifndef TCP_KEEPIDLE
# define TCP_KEEPIDLE  75  // 注意：有些地方是 4, 但 Android 推荐用 75
#endif
#endif



//websocket
#define htobe16(x) htons(x)
#define htobe64(x) (((uint64_t)htonl(((uint32_t)(((uint64_t)(x)) >> 32)))) | (((uint64_t)htonl(((uint32_t)(x)))) << 32))
#define be16toh(x) ntohs(x)
#define be32toh(x) ntohl(x)
#define be64toh(x) (((uint64_t)ntohl(((uint32_t)(((uint64_t)(x)) >> 32)))) | (((uint64_t)ntohl(((uint32_t)(x)))) << 32))

enum ws_opcode
{
	WS_OPCODE_CONTINUATION = 0x00,
	WS_OPCODE_TEXT = 0x01,
	WS_OPCODE_BIN = 0x02,
	WS_OPCODE_END = 0x03,
	WS_OPCODE_CLOSE = 0x08,
	WS_OPCODE_PING = 0x09,
	WS_OPCODE_PONG = 0x0A,
};

//#if defined(_MSC_VER) // Microsoft compiler
//#define __attribute__(x)
//#elif defined(__GNUC__) // GNU compiler
//#else
//#define __attribute__(x)
//#endif
//
//#if defined(_MSC_VER) // Microsoft compiler
//#pragma pack(1)
//#endif

//websocket
typedef struct
{
	uint8_t opcode : 4;
	uint8_t rsv3 : 1;
	uint8_t rsv2 : 1;
	uint8_t rsv1 : 1;
	uint8_t fin : 1;
	uint8_t payloadlen : 7;
	uint8_t mask : 1;
}ws_comm_header;

typedef struct
{
	uint8_t opcode : 4;
	uint8_t rsv3 : 1;
	uint8_t rsv2 : 1;
	uint8_t rsv1 : 1;
	uint8_t fin : 1;
	uint8_t payloadlen : 7;
	uint8_t mask : 1;
	unsigned char payloaddata[0];
}ws_0_header;

typedef struct
{
	uint8_t opcode : 4;
	uint8_t rsv3 : 1;
	uint8_t rsv2 : 1;
	uint8_t rsv1 : 1;
	uint8_t fin : 1;
	uint8_t payloadlen : 7;
	uint8_t mask : 1;
	unsigned char maskey[4];
	unsigned char payloaddata[0];
}ws_0m_header;

typedef struct
{
	uint8_t opcode : 4;
	uint8_t rsv3 : 1;
	uint8_t rsv2 : 1;
	uint8_t rsv1 : 1;
	uint8_t fin : 1;
	uint8_t payloadlen : 7;
	uint8_t mask : 1;
	uint16_t expayloadlen;
	unsigned char payloaddata[0];
}ws_1_header;

typedef struct
{
	uint8_t opcode : 4;
	uint8_t rsv3 : 1;
	uint8_t rsv2 : 1;
	uint8_t rsv1 : 1;
	uint8_t fin : 1;
	uint8_t payloadlen : 7;
	uint8_t mask : 1;
	uint16_t expayloadlen;
	unsigned char maskey[4];
	unsigned char payloaddata[0];
}ws_1m_header;

typedef struct
{
	uint8_t opcode : 4;
	uint8_t rsv3 : 1;
	uint8_t rsv2 : 1;
	uint8_t rsv1 : 1;
	uint8_t fin : 1;
	uint8_t payloadlen : 7;
	uint8_t mask : 1;
	uint64_t expayloadlen;
	unsigned char payloaddata[0];
}ws_2_header;

typedef struct
{
	uint8_t opcode : 4;
	uint8_t rsv3 : 1;
	uint8_t rsv2 : 1;
	uint8_t rsv1 : 1;
	uint8_t fin : 1;
	uint8_t payloadlen : 7;
	uint8_t mask : 1;
	uint64_t expayloadlen;
	unsigned char maskey[4];
	unsigned char payloaddata[0];
}ws_2m_header;


int easyrtc_socket_set_keepalive(int sockfd);
int easyrtc_websocket_write(mbedtls_ssl_context* ssl, int sockfd, int opcode, char* data, int iLength);
int easyrtc_websocket_send_raw(mbedtls_ssl_context* ssl, int sockfd, char* buf, int len);

#endif
