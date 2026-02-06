#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <uuid/uuid.h>
#include <list>
#include <map>
#include "dictionary.h"
#include "iniparser.h"
#include "llhttp.h"
#include "sha.h"
#include "Sdp.h"

#include "HPSocket4C.h"
#include "HPSocket4C-SSL.h"

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close

typedef struct
{
	pthread_mutex_t m;
	pthread_t t;
} __attribute__((packed)) CRITICAL_SECTION;

#if (1)
#define InitializeCriticalSection(x)         \
	{                                        \
		(x)->t = 0;                          \
		pthread_mutex_init(&((x)->m), NULL); \
	}
#define EnterCriticalSection(x)            \
	while (1)                              \
	{                                      \
		if ((x)->t == 0)                   \
		{                                  \
			pthread_mutex_lock(&((x)->m)); \
			(x)->t = pthread_self();       \
			break;                         \
		}                                  \
		if (pthread_self() == (x)->t)      \
			break;                         \
		pthread_mutex_lock(&((x)->m));     \
		(x)->t = pthread_self();           \
		break;                             \
	}
#else
#define InitializeCriticalSection(x)                                 \
	{                                                                \
		(x)->t = 0;                                                  \
		pthread_mutexattr_t mutexattr;                               \
		pthread_mutexattr_init(&mutexattr);                          \
		pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_NORMAL); \
		pthread_mutex_init(&((x)->m), &mutexattr);                   \
		pthread_mutexattr_destroy(&mutexattr);                       \
	}
#define EnterCriticalSection(x)        \
	{                                  \
		pthread_mutex_lock(&((x)->m)); \
	}
#endif
#define LeaveCriticalSection(x)          \
	{                                    \
		pthread_mutex_unlock(&((x)->m)); \
		(x)->t = 0;                      \
	}
#define DeleteCriticalSection(x)          \
	{                                     \
		pthread_mutex_destroy(&((x)->m)); \
	}

#define CONFIG_FILE_NAME "./signalserver.ini"

#define MAX_SUPPORT_KEYVALUECOUNT 48
#define MAX_SUPPORT_HTMLREQ 8192

#define WS_ACCEPT_MAGIC_KEY "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_HANDSHAKE_REPLY_BLUEPRINT "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n"

#define WEBRTC_ROLE_INVALID                     0
#define WEBRTC_ROLE_OFFER                       1
#define WEBRTC_ROLE_ANSWER                      2

#define HP_REQLOGINUSER_INFO                 0x10001
#define HP_ACKLOGINUSER_INFO                 0x10002
#define HP_REQCONNECTUSER_INFO               0x10003
#define HP_ACKCONNECTUSER_INFO               0x10004
#define HP_REQGETWEBRTCOFFER_INFO            0x10005
#define HP_NTIWEBRTCOFFER_INFO               0x10006
#define HP_NTIWEBRTCOFFER_INFO2              0x10007
#define HP_NTIWEBRTCANSWER_INFO              0x10008

#define HP_REQGETONLINEDEVICES_INFO          0x20001
#define HP_ACKGETONLINEDEVICES_INFO          0x20002

#if defined(_MSC_VER)
#define __attribute__(x)
#elif defined(__GNUC__)
#else
#define __attribute__(x)
#endif

#if defined(_MSC_VER)
#pragma pack(1)
#endif

typedef struct
{
	uint32_t length;
	uint32_t msgtype;
}__attribute__((packed)) BASE_MSG_INFO;

typedef struct
{
	uint32_t length;
	uint32_t msgtype;
	uint32_t myid[4];
	uint32_t mysn[4];
	uint8_t  mykey[32];
	uint16_t extradatalen0;
	uint16_t extradatalen1;
	char     extradata0[0]; //这是发给另一端的
	//在这之后是 extradata1; //这是发给信令服务器的
}__attribute__((packed)) REQ_LOGINUSER_INFO;

typedef struct
{
	uint32_t length;
	uint32_t msgtype;
	uint32_t myid[4];
	int32_t  status;
}__attribute__((packed)) ACK_LOGINUSER_INFO;

typedef struct
{
	uint32_t length;
	uint32_t msgtype;
	uint32_t hisid[4];
	uint8_t  hiskey[32];
	uint16_t extradatalen0;
	uint16_t extradatalen1;
	char     extradata0[0]; //这是发给另一端的
	//在这之后是 extradata1; //这是发给信令服务器的
}__attribute__((packed)) REQ_CONNECTUSER_INFO;

typedef struct
{
	uint32_t length;
	uint32_t msgtype;
	uint32_t hisid[4]; //设备端uuid
	uint32_t myid[4]; //在信令服务器上动态分配的纯主动连接端的uuid,也就是纯客户端
	int32_t  status;
	uint16_t extradatalen0;
	char     extradata0[0]; //这是另一端发送过来的
}__attribute__((packed)) ACK_CONNECTUSER_INFO;

typedef struct
{
	uint32_t length;
	uint32_t msgtype;
	uint32_t hisid[4];
	uint16_t extradatalen0;
	uint16_t strdataslen;
	int8_t   strcount;
	int8_t   strtypes[8];
	char     strdatas[0];
	//在这之后是 extradata0
}__attribute__((packed)) REQ_GETWEBRTCOFFER_INFO;

typedef struct
{
	uint32_t length;
	uint32_t msgtype;
	uint32_t hisid[4];
	uint16_t sdplen;
	char offersdp[0];
}__attribute__((packed)) NTI_WEBRTCOFFER_INFO;

typedef struct
{
	uint32_t length;
	uint32_t msgtype;
	uint32_t hisid[4];
	uint16_t sdplen;
	uint16_t strdataslen;
	int8_t   strcount;
	int8_t   strtypes[8];
	char     strdatas[0]; //在这之后是 offersdp
	//char   offersdp[0];
}__attribute__((packed)) NTI_WEBRTCOFFER_INFO2;

typedef struct
{
	uint32_t length;
	uint32_t msgtype;
	uint32_t hisid[4];
	uint16_t sdplen;
	char answersdp[0];
}__attribute__((packed)) NTI_WEBRTCANSWER_INFO;

typedef struct
{
	uint32_t length;
	uint32_t msgtype;
}__attribute__((packed)) REQ_GETONLINEDEVICES_INFO;

typedef struct
{
	uint32_t length;
	uint32_t msgtype;
	uint32_t idscount;
	uint32_t hisids[][4];
}__attribute__((packed)) ACK_GETONLINEDEVICES_INFO;

#if defined(_MSC_VER)
#pragma pack()
#endif

//////////////////////////////////////////////////////////////////////////////////////
enum ws_state
{
	WS_STATE_HANDSHAKE,
	WS_STATE_CONNECTED,
	WS_STATE_CLOSED
};

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

// Frame format of a websocket:
//​​
//      0                   1                   2                   3
//      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//     +-+-+-+-+-------+-+-------------+-------------------------------+
//     |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
//     |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
//     |N|V|V|V|       |S|             |   (if payload len==126/127)   |
//     | |1|2|3|       |K|             |                               |
//     +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
//     |     Extended payload length continued, if payload len == 127  |
//     + - - - - - - - - - - - - - - - +-------------------------------+
//     |                               |Masking-key, if MASK set to 1  |
//     +-------------------------------+-------------------------------+
//     | Masking-key (continued)       |          Payload Data         |
//     +-------------------------------- - - - - - - - - - - - - - - - +
//     :                     Payload Data continued ...                :
//     + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
//     |                     Payload Data continued ...                |
//     +---------------------------------------------------------------+
//
// copied form
// https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API/Writing_WebSocket_servers
// licensed under CC-BY-SA 2.5.

#if defined(_MSC_VER)
#pragma pack(1)
#endif

struct ws_comm_header
{
	uint8_t opcode : 4;
	uint8_t rsv3 : 1;
	uint8_t rsv2 : 1;
	uint8_t rsv1 : 1;
	uint8_t fin : 1;
	uint8_t payloadlen : 7;
	uint8_t mask : 1;
};

struct ws_0_header
{
	uint8_t opcode : 4;
	uint8_t rsv3 : 1;
	uint8_t rsv2 : 1;
	uint8_t rsv1 : 1;
	uint8_t fin : 1;
	uint8_t payloadlen : 7;
	uint8_t mask : 1;
	unsigned char payloaddata[0];
};

struct ws_0m_header
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
};

struct ws_1_header
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
};

struct ws_1m_header
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
};

struct ws_2_header
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
};

struct ws_2m_header
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
};

typedef struct
{
	uint16_t strdataslen;
	int8_t   strcount;
	int8_t   strtypes[8];
	char     strdatas[0];
}__attribute__((packed)) STRDATAS_INFO;

#if defined(_MSC_VER)
#pragma pack()
#endif

class uint128_t {
public:
	uint128_t();
	uint128_t(uint64_t hi, uint64_t lo);
	uint128_t(const uint128_t& val);

	void Initialize(uint64_t hi, uint64_t low);
	void Initialize(uint32_t hi, uint32_t m1, uint32_t m2, uint32_t low);

	bool operator==(const uint128_t& b) const;
	bool operator!=(const uint128_t& b) const;
	uint128_t& operator=(const uint128_t& b);

	bool operator<(const uint128_t& b) const;
	bool operator>(const uint128_t& b) const;
	bool operator<=(const uint128_t& b) const;
	bool operator>=(const uint128_t& b) const;

private:
	uint64_t      lo_;
	uint64_t      hi_;
};

inline bool uint128_t::operator==(const uint128_t& b) const {
	return (lo_ == b.lo_) && (hi_ == b.hi_);
}
inline bool uint128_t::operator!=(const uint128_t& b) const {
	return (lo_ != b.lo_) || (hi_ != b.hi_);
}
inline uint128_t& uint128_t::operator=(const uint128_t& b) {
	lo_ = b.lo_;
	hi_ = b.hi_;
	return *this;
}

inline uint128_t::uint128_t() : lo_(0), hi_(0) { }
inline uint128_t::uint128_t(uint64_t hi, uint64_t lo) : lo_(lo), hi_(hi) { }
inline uint128_t::uint128_t(const uint128_t& v) : lo_(v.lo_), hi_(v.hi_) { }
inline void uint128_t::Initialize(uint64_t hi, uint64_t lo) {
	hi_ = hi;
	lo_ = lo;
}

inline void uint128_t::Initialize(uint32_t hi, uint32_t m1, uint32_t m2, uint32_t low) {
	hi_ = (((uint64_t)hi) << 32) | m1;
	lo_ = (((uint64_t)m2) << 32) | low;
}

#define CMP128(op)														\
	inline bool uint128_t::operator op(const uint128_t& b) const {      \
	return (hi_ == b.hi_) ? (lo_ op b.lo_) : (hi_ op b.hi_);			\
}

CMP128(< )
CMP128(> )
CMP128(>= )
CMP128(<= )
#undef CMP128

typedef struct
{
	char* url;
	char* keys[MAX_SUPPORT_KEYVALUECOUNT];
	char* values[MAX_SUPPORT_KEYVALUECOUNT];

	int nPos;
	int keycount;
	int valuecount;
	int headercp;
	int messagecp;

	char htmlREQ[MAX_SUPPORT_HTMLREQ];
}HTML_INFO;

typedef struct
{
	HP_Server pSender;
	HP_CONNID dwConnID;
	UINT_PTR pClient;
	enum ws_state state;

	int remotePort;
	char remoteAddress[32];

	int role;
	int alive;

	uint32_t myid[4];
	uint32_t mysn[4];
	uint8_t  mykey[32];

	uint16_t extradatalen0;
	uint8_t* extradata0; //offer提交上来,用于在answer的时候发送给纯客户端
}HP_WBSPEER_INFO;

typedef struct
{
	struct
	{
		int localport;
		int stuncount;
		int turncount;
		int supportssl;
	}baseinfo;

	struct
	{
		char url[128];
	}stun[2];

	struct
	{
		char url[128];
		char username[128];
		char credential[128];
	}turn[2];

	struct
	{
		int  localport;
		char pemcertfile[256];
		char pemkeyfile[256];
		char keypassword[128];
	}ssl;
}SERVER_CONFIG;

SERVER_CONFIG g_sconfig = { 0 };
STRDATAS_INFO* g_strdatas_info = NULL;

CRITICAL_SECTION g_csHPWBSPeerInfo;
std::map<uint128_t, HP_WBSPEER_INFO*> g_HPWBSPeerInfo_map;

CRITICAL_SECTION g_csSessionDescription;
SessionDescription* g_pSessionDescription = NULL;

#if 0
void WriteToConsoleWindow(char* szFormat, ...)
{
	char szBuffer[8192] = { 0 }; //因为sdp内容可能很长,所以这里分配较大空间
	va_list pArgList;
	va_start(pArgList, szFormat);
	_vsnprintf(szBuffer, sizeof(szBuffer) / sizeof(char), szFormat, pArgList);
	va_end(pArgList);

	SYSTEMTIME st;
	GetLocalTime(&st);
	printf("[time]%04d-%02d-%02d %02d:%02d:%02d [MSG]%s\n\0", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, szBuffer);

	return;
}
#else
void WriteToConsoleWindow(const char *szFormat, ...)
{
	char szBuffer[8192] = {0}; // 保留大缓冲区以容纳长消息

	// 处理可变参数
	va_list pArgList;
	va_start(pArgList, szFormat);
	vsnprintf(szBuffer, sizeof(szBuffer), szFormat, pArgList); // Linux 使用 vsnprintf
	va_end(pArgList);

	// 获取本地时间（Linux 替代 GetLocalTime）
	struct timeval tv;
	gettimeofday(&tv, NULL);
	struct tm *tm_info = localtime(&tv.tv_sec);

	// 输出带时间戳的日志
	printf("[time]%04d-%02d-%02d %02d:%02d:%02d [MSG]%s\n",
		   tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
		   tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
		   szBuffer);

	//fflush(stdout); // 确保立即刷新输出缓冲区
}
#endif

//把strIn字符串去掉前后空格之后复制到strOut
void trim(char* strIn, char* strOut)
{
	int len;
	char* start, * end, * temp; //定义去除空格后字符串的头尾指针和遍历指针

	if (strIn == NULL)
	{
		strOut[0] = 0;
		return;
	}

	len = strlen(strIn);
	temp = strIn;

	while (*temp == ' ')
	{
		++temp;
	}

	start = temp; //求得头指针
	temp = strIn + len - 1; //得到原字符串最后一个字符的指针(不是'\0')

	while (*temp == ' ')
	{
		--temp;
	}

	end = temp; //求得尾指针
	for (strIn = start; strIn <= end;)
	{
		*strOut++ = *strIn++;
	}

	*strOut = '\0';
}

int GetUUIDSFromString(char* struuid, uint32_t* myids)
{
	int isR = 1; //默认格式正确,下面为0则代表格式不正确
	do
	{
		if (strlen(struuid) != 36) { isR = 0; break; }

		int sepcount = 0;
		unsigned char c = 0;

		for (int i = 0; i < 36; i++)
		{
			c = struuid[i];
			if (((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'f')))
			{
			}
			else if ((c >= 'A') && (c <= 'F'))
			{
				struuid[i] = c + 'a' - 'A';
			}
			else if (c == '-')
			{
				if ((i != 8) && (i != 13) && (i != 18) && (i != 23))
				{
					isR = 0;
					break;
				}
				sepcount++;
			}
			else
			{
				isR = 0;
				break;
			}
		}

		if (sepcount != 4) isR = 0;
	} while (0);

	if (isR == 0) return -1;

	{
		//下面是把类似 AE01A752-AD94-4107-B137-643ACC93756E 这样的字符串转为4个DWORD
		int i;
		unsigned char dataSZ[8] = { 0 };
		uint32_t data1_0 = 0;
		uint32_t data2_0 = 0;
		uint32_t data3_0 = 0;
		uint32_t data4_0 = 0;
		unsigned char* uuid = (unsigned char*)&struuid[0];

		{
			dataSZ[0] = uuid[0];
			dataSZ[1] = uuid[1];
			dataSZ[2] = uuid[2];
			dataSZ[3] = uuid[3];
			dataSZ[4] = uuid[4];
			dataSZ[5] = uuid[5];
			dataSZ[6] = uuid[6];
			dataSZ[7] = uuid[7];

			for (i = 0; i < 8; i++)
			{
				if (dataSZ[i] >= 'a')
					data1_0 = (data1_0 << 4) + (10 + dataSZ[i] - 'a');
				else
					data1_0 = (data1_0 << 4) + (dataSZ[i] - '0');
			}
		}

		{
			dataSZ[0] = uuid[9];
			dataSZ[1] = uuid[10];
			dataSZ[2] = uuid[11];
			dataSZ[3] = uuid[12];
			dataSZ[4] = uuid[14];
			dataSZ[5] = uuid[15];
			dataSZ[6] = uuid[16];
			dataSZ[7] = uuid[17];

			for (i = 0; i < 8; i++)
			{
				if (dataSZ[i] >= 'a')
					data2_0 = (data2_0 << 4) + (10 + dataSZ[i] - 'a');
				else
					data2_0 = (data2_0 << 4) + (dataSZ[i] - '0');
			}
		}

		{
			dataSZ[0] = uuid[19];
			dataSZ[1] = uuid[20];
			dataSZ[2] = uuid[21];
			dataSZ[3] = uuid[22];
			dataSZ[4] = uuid[24];
			dataSZ[5] = uuid[25];
			dataSZ[6] = uuid[26];
			dataSZ[7] = uuid[27];

			for (i = 0; i < 8; i++)
			{
				if (dataSZ[i] >= 'a')
					data3_0 = (data3_0 << 4) + (10 + dataSZ[i] - 'a');
				else
					data3_0 = (data3_0 << 4) + (dataSZ[i] - '0');
			}
		}

		{
			dataSZ[0] = uuid[28];
			dataSZ[1] = uuid[29];
			dataSZ[2] = uuid[30];
			dataSZ[3] = uuid[31];
			dataSZ[4] = uuid[32];
			dataSZ[5] = uuid[33];
			dataSZ[6] = uuid[34];
			dataSZ[7] = uuid[35];

			for (i = 0; i < 8; i++)
			{
				if (dataSZ[i] >= 'a')
					data4_0 = (data4_0 << 4) + (10 + dataSZ[i] - 'a');
				else
					data4_0 = (data4_0 << 4) + (dataSZ[i] - '0');
			}
		}

		myids[0] = data1_0;
		myids[1] = data2_0;
		myids[2] = data3_0;
		myids[3] = data4_0;
		return 0;
	}
}

int isValidSDP(const char* sdp)
{
	// Check if SDP starts with "v=0"
	if (strncmp(sdp, "v=0", 3) != 0)
	{
		return 0;
	}

	// Check if SDP contains "o=", "s=", "t=" and "m="
	if (strstr(sdp, "o=") == NULL || strstr(sdp, "s=") == NULL || strstr(sdp, "t=") == NULL || strstr(sdp, "m=") == NULL)
	{
		return 0;
	}

	// If all checks passed, return 1
	return 1;
}

int socket_set_keepalive(int sockfd)
{
#if defined(_WIN32)
	int enableKeepAlive = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (const char *)&enableKeepAlive, sizeof(enableKeepAlive)) != 0)
	{
		// return -1;
	}

	struct tcp_keepalive keepAlive = {0};
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
	int keepIdle = 10;	  // Idle time before starting keepalive probes
	int keepInterval = 5; // Interval between keepalive probes
	int keepCount = 3;	  // Number of keepalive probes before declaring the connection dead

	if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &enableKeepAlive, sizeof(enableKeepAlive)) < 0)
	{
		// perror("Cannot set SO_KEEPALIVE");
	}

#if defined(TCP_KEEPIDLE)
	if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(keepIdle)) < 0)
	{
		// perror("Cannot set TCP_KEEPIDLE");
	}
#endif

	if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(keepInterval)) < 0)
	{
		// perror("Cannot set TCP_KEEPINTVL");
	}

	if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(keepCount)) < 0)
	{
		// perror("Cannot set TCP_KEEPCNT");
	}
#endif

	return 0;
}

void LoadConfigIniFile()
{
	char subkey[64] = { 0 };
	dictionary* Dictionary = NULL;
	SERVER_CONFIG sconfig = { 0 };
	Dictionary = iniparser_load(CONFIG_FILE_NAME);

	do
	{
		if (Dictionary == NULL) break;

		int PortNumber = iniparser_getint(Dictionary, "base:localport", 0);
		sconfig.baseinfo.localport = PortNumber;

		int stuncount = iniparser_getint(Dictionary, "base:stuncount", 0);
		int turncount = iniparser_getint(Dictionary, "base:turncount", 0);
		int supportssl = iniparser_getint(Dictionary, "base:supportssl", 0);

		if (stuncount > 2) stuncount = 2;
		if (turncount > 2) turncount = 2;

		sconfig.baseinfo.stuncount = stuncount;
		sconfig.baseinfo.turncount = turncount;
		sconfig.baseinfo.supportssl = supportssl;

		const char* pVal = NULL;
		for (int i = 0; i < stuncount; i++)
		{
			sprintf(subkey, "stun%d:url", i);
			pVal = iniparser_getstring(Dictionary, subkey, NULL);
			if (pVal != NULL) trim((char*)pVal, sconfig.stun[i].url);
		}

		for (int i = 0; i < turncount; i++)
		{
			sprintf(subkey, "turn%d:url", i);
			pVal = iniparser_getstring(Dictionary, subkey, NULL);
			if (pVal != NULL) trim((char*)pVal, sconfig.turn[i].url);

			sprintf(subkey, "turn%d:username", i);
			pVal = iniparser_getstring(Dictionary, subkey, NULL);
			if (pVal != NULL) trim((char*)pVal, sconfig.turn[i].username);

			sprintf(subkey, "turn%d:credential", i);
			pVal = iniparser_getstring(Dictionary, subkey, NULL);
			if (pVal != NULL) trim((char*)pVal, sconfig.turn[i].credential);
		}

		if (supportssl == 1)
		{
			PortNumber = iniparser_getint(Dictionary, "ssl:localport", 0);
			sconfig.ssl.localport = PortNumber;

			pVal = iniparser_getstring(Dictionary, "ssl:pemcertfile", NULL);
			if (pVal != NULL) trim((char*)pVal, sconfig.ssl.pemcertfile);

			pVal = iniparser_getstring(Dictionary, "ssl:pemkeyfile", NULL);
			if (pVal != NULL) trim((char*)pVal, sconfig.ssl.pemkeyfile);

			pVal = iniparser_getstring(Dictionary, "ssl:keypassword", NULL);
			if (pVal != NULL) trim((char*)pVal, sconfig.ssl.keypassword);
		}

		iniparser_freedict(Dictionary); //这里释放后,pVal就不是有效的内存地址了
		memcpy(&g_sconfig, &sconfig, sizeof(SERVER_CONFIG));

		if (g_strdatas_info != NULL) free(g_strdatas_info);
		g_strdatas_info = (STRDATAS_INFO*)malloc(2048);
		memset(g_strdatas_info, 0, 2048);

		int iPos = 0;
		char* pStrDatas = g_strdatas_info->strdatas;
		for (int i = 0; i < g_sconfig.baseinfo.stuncount; i++)
		{
			g_strdatas_info->strtypes[g_strdatas_info->strcount] = 0x00;
			strcpy(pStrDatas + iPos, g_sconfig.stun[i].url);
			iPos += (strlen(g_sconfig.stun[i].url) + 1);
			g_strdatas_info->strcount++;
		}

		for (int i = 0; i < g_sconfig.baseinfo.turncount; i++)
		{
			g_strdatas_info->strtypes[g_strdatas_info->strcount] = 0x04; //juice=0x04,别的=0x01;
			strcpy(pStrDatas + iPos, g_sconfig.turn[i].url);
			iPos += (strlen(g_sconfig.turn[i].url) + 1);
			g_strdatas_info->strcount++;

			g_strdatas_info->strtypes[g_strdatas_info->strcount] = 0x02;
			strcpy(pStrDatas + iPos, g_sconfig.turn[i].username);
			iPos += (strlen(g_sconfig.turn[i].username) + 1);
			g_strdatas_info->strcount++;

			g_strdatas_info->strtypes[g_strdatas_info->strcount] = 0x03;
			strcpy(pStrDatas + iPos, g_sconfig.turn[i].credential);
			iPos += (strlen(g_sconfig.turn[i].credential) + 1);
			g_strdatas_info->strcount++;
		}
		g_strdatas_info->strdataslen = iPos;

		return;
	} while (0);
}

int mg_websocket_write(HP_WBSPEER_INFO* pHPPeerInfo, int opcode, char* data, int iLength)
{
	if (iLength < 126)
	{
		unsigned char buffer[256];
		ws_0_header* ws0header = (ws_0_header*)buffer;
		ws0header->opcode = opcode;
		ws0header->rsv1 = 0;
		ws0header->rsv2 = 0;
		ws0header->rsv3 = 0;
		ws0header->fin = 1;
		ws0header->payloadlen = iLength;
		ws0header->mask = 0;
		if (iLength > 0) memcpy(ws0header->payloaddata, data, iLength);

		if (!HP_Server_Send(pHPPeerInfo->pSender, pHPPeerInfo->dwConnID, buffer, sizeof(ws_0_header) + iLength)) return HR_ERROR;
	}
	else if (iLength < 65536)
	{
		unsigned char buffer[65 * 1024];
		ws_1_header* ws1header = (ws_1_header*)buffer;
		ws1header->opcode = opcode;
		ws1header->rsv1 = 0;
		ws1header->rsv2 = 0;
		ws1header->rsv3 = 0;
		ws1header->fin = 1;
		ws1header->payloadlen = 126;
		ws1header->mask = 0;
		ws1header->expayloadlen = htobe16(iLength);
		memcpy(ws1header->payloaddata, data, iLength);

		if (!HP_Server_Send(pHPPeerInfo->pSender, pHPPeerInfo->dwConnID, buffer, sizeof(ws_1_header) + iLength)) return HR_ERROR;
	}
	else
	{
		unsigned char* buffer = (unsigned char*)malloc(iLength + sizeof(ws_2_header));
		ws_2_header* ws2header = (ws_2_header*)buffer;
		ws2header->opcode = opcode;
		ws2header->rsv1 = 0;
		ws2header->rsv2 = 0;
		ws2header->rsv3 = 0;
		ws2header->fin = 1;
		ws2header->payloadlen = 127;
		ws2header->mask = 0;
		ws2header->expayloadlen = htobe64(iLength);
		memcpy(ws2header->payloaddata, data, iLength);

		if (!HP_Server_Send(pHPPeerInfo->pSender, pHPPeerInfo->dwConnID, buffer, sizeof(ws_2_header) + iLength)) { free(buffer); return HR_ERROR; }
		free(buffer);
	}

	return 0;
}

int request_url_cb(llhttp_t* p, const char* buf, size_t len)
{
	HTML_INFO* html = (HTML_INFO*)p->data;
	memcpy(html->htmlREQ + html->nPos, buf, len);
	html->url = html->htmlREQ + html->nPos;
	html->nPos += (len + 1);
	return 0;
}

int header_field_cb(llhttp_t* p, const char* buf, size_t len)
{
	HTML_INFO* html = (HTML_INFO*)p->data;
	if (html->keycount >= MAX_SUPPORT_KEYVALUECOUNT) return 0;

	memcpy(html->htmlREQ + html->nPos, buf, len);
	html->keys[html->keycount] = html->htmlREQ + html->nPos;
	html->nPos += (len + 1);
	html->keycount++;
	return 0;
}

int header_value_cb(llhttp_t* p, const char* buf, size_t len)
{
	HTML_INFO* html = (HTML_INFO*)p->data;
	if (html->valuecount >= MAX_SUPPORT_KEYVALUECOUNT) return 0;

	memcpy(html->htmlREQ + html->nPos, buf, len);
	html->values[html->valuecount] = html->htmlREQ + html->nPos;
	html->nPos += (len + 1);
	html->valuecount++;
	return 0;
}

int headers_complete_cb(llhttp_t* p)
{
	HTML_INFO* html = (HTML_INFO*)p->data;
	html->headercp = 1;
	return 0;
}

int message_complete_cb(llhttp_t* p)
{
	HTML_INFO* html = (HTML_INFO*)p->data;
	html->messagecp = 1;
	return 0;
}

int WebsocketDataHandler(HP_WBSPEER_INFO* pHPPeerInfo, ws_comm_header* wsheader, unsigned char* data, int iLength)
{
	if ((wsheader->opcode == WS_OPCODE_PING) || (wsheader->opcode == WS_OPCODE_PONG)) return 0;
	if ((wsheader->opcode == WS_OPCODE_CLOSE) || (wsheader->opcode != WS_OPCODE_BIN)) return -1;

	BASE_MSG_INFO* pBaseInfo = (BASE_MSG_INFO*)data;
	if ((iLength < sizeof(BASE_MSG_INFO)) || (iLength != pBaseInfo->length)) return -1;

	//WS_OPCODE_TEXT
	//WS_OPCODE_PING
	//WS_OPCODE_PONG
	//WS_OPCODE_CONTINUATION
	//WS_OPCODE_END

	switch (pBaseInfo->msgtype)
	{
	case HP_REQLOGINUSER_INFO:
	{
		if (iLength < sizeof(REQ_LOGINUSER_INFO)) return -1;

		REQ_LOGINUSER_INFO* pRecvInfo = (REQ_LOGINUSER_INFO*)data;
		if ((pRecvInfo->myid[0] == 0) && (pRecvInfo->myid[1] == 0) && (pRecvInfo->myid[2] == 0) && (pRecvInfo->myid[3] == 0)) return -1;
		if ((sizeof(REQ_LOGINUSER_INFO) + pRecvInfo->extradatalen0 + pRecvInfo->extradatalen1) != pRecvInfo->length) return -1;

		uint128_t uuid128;
		uuid128.Initialize(pRecvInfo->myid[0], pRecvInfo->myid[1], pRecvInfo->myid[2], pRecvInfo->myid[3]);

		HP_WBSPEER_INFO* pHPPeerInfo2 = NULL;
		std::map<uint128_t, HP_WBSPEER_INFO*>::iterator itl;
		EnterCriticalSection(&g_csHPWBSPeerInfo);
		itl = g_HPWBSPeerInfo_map.find(uuid128);
		if (itl == g_HPWBSPeerInfo_map.end())
		{
			g_HPWBSPeerInfo_map.insert(std::map<uint128_t, HP_WBSPEER_INFO*>::value_type(uuid128, pHPPeerInfo));
		}
		else
		{
			pHPPeerInfo2 = (*itl).second;
			if (pHPPeerInfo != pHPPeerInfo2)
			{
				//在我的win8虚拟机上,首先连接上,然后修改网卡ip地址,就会很快执行到这里
				(*itl).second = pHPPeerInfo;
				pHPPeerInfo2->alive = 0;
				HP_Server_Disconnect(pHPPeerInfo2->pSender, pHPPeerInfo2->dwConnID, TRUE);
			} //这里的else存在一种情况是:客户端先使用了 RTC_connectClient(也就是信令服务器端分配的临时uuid), 后使用了 RTC_loginSignalingServer
		}

		pHPPeerInfo->alive = 1;
		pHPPeerInfo->role = WEBRTC_ROLE_OFFER;
		pHPPeerInfo->myid[0] = pRecvInfo->myid[0];
		pHPPeerInfo->myid[1] = pRecvInfo->myid[1];
		pHPPeerInfo->myid[2] = pRecvInfo->myid[2];
		pHPPeerInfo->myid[3] = pRecvInfo->myid[3];

		pHPPeerInfo->mysn[0] = pRecvInfo->mysn[0];
		pHPPeerInfo->mysn[1] = pRecvInfo->mysn[1];
		pHPPeerInfo->mysn[2] = pRecvInfo->mysn[2];
		pHPPeerInfo->mysn[3] = pRecvInfo->mysn[3];

		for (int i = 0; i < 32; i++)
		{
			pHPPeerInfo->mykey[i] = pRecvInfo->mykey[i];
		}
		if (((pHPPeerInfo->extradata0 == NULL) && (pRecvInfo->extradatalen0 == 0)) || ((pHPPeerInfo->extradata0 != NULL) && (pHPPeerInfo->extradatalen0 == pRecvInfo->extradatalen0) && (memcmp(pHPPeerInfo->extradata0, pRecvInfo->extradata0, pRecvInfo->extradatalen0) == 0)))
		{
			//不必做任何处理
		}
		else
		{
			if (pHPPeerInfo->extradata0 != NULL)
			{
				free(pHPPeerInfo->extradata0);
				pHPPeerInfo->extradatalen0 = 0;
			}
			if (pRecvInfo->extradatalen0 != 0)
			{
				pHPPeerInfo->extradata0 = (uint8_t*)malloc(pRecvInfo->extradatalen0);
				memcpy(pHPPeerInfo->extradata0, pRecvInfo->extradata0, pRecvInfo->extradatalen0);
				pHPPeerInfo->extradatalen0 = pRecvInfo->extradatalen0;
			}
		}
		//特别注意:目前还没处理 pRecvInfo->extradatalen1这些数据
		LeaveCriticalSection(&g_csHPWBSPeerInfo);

		ACK_LOGINUSER_INFO ackInfo = { 0 };
		ackInfo.length = sizeof(ACK_LOGINUSER_INFO);
		ackInfo.msgtype = HP_ACKLOGINUSER_INFO;
		ackInfo.myid[0] = pHPPeerInfo->myid[0];
		ackInfo.myid[1] = pHPPeerInfo->myid[1];
		ackInfo.myid[2] = pHPPeerInfo->myid[2];
		ackInfo.myid[3] = pHPPeerInfo->myid[3];
		ackInfo.status = 0;
		mg_websocket_write(pHPPeerInfo, WS_OPCODE_BIN, (char*)&ackInfo, ackInfo.length);
		WriteToConsoleWindow("OnLogin %s:%d uuid:%08X-%04X-%04X-%04X-%04X%08X", pHPPeerInfo->remoteAddress, pHPPeerInfo->remotePort, pHPPeerInfo->myid[0], pHPPeerInfo->myid[1]>>16, pHPPeerInfo->myid[1]&0xFFFF, pHPPeerInfo->myid[2]>>16, pHPPeerInfo->myid[2]&0xFFFF, pHPPeerInfo->myid[3]);
	}
	break;
	case HP_REQCONNECTUSER_INFO:
	{
		if (iLength < sizeof(REQ_CONNECTUSER_INFO)) return -1;
		REQ_CONNECTUSER_INFO* pRecvInfo = (REQ_CONNECTUSER_INFO*)data;
		if ((sizeof(REQ_CONNECTUSER_INFO) + pRecvInfo->extradatalen0 + pRecvInfo->extradatalen1) != pRecvInfo->length) return -1;

		uint128_t uuid128;
		pHPPeerInfo->alive = 1;
		if ((pHPPeerInfo->myid[0] == 0) && (pHPPeerInfo->myid[1] == 0) && (pHPPeerInfo->myid[2] == 0) && (pHPPeerInfo->myid[3] == 0))
		{
			// 将原有的CoCreateGuid部分替换为以下代码：
			uuid_t uuid;
			uuid_generate(uuid);

			// 拆分UUID为标准GUID字段（大端序解析）
			uint32_t Data1 = ((uint32_t)uuid[0] << 24) | ((uint32_t)uuid[1] << 16) | ((uint32_t)uuid[2] << 8) | (uint32_t)uuid[3];
			uint16_t Data2 = ((uint16_t)uuid[4] << 8) | (uint16_t)uuid[5];
			uint16_t Data3 = ((uint16_t)uuid[6] << 8) | (uint16_t)uuid[7];
			uint8_t *Data4 = &uuid[8]; // 或者直接使用uuid[8]到uuid[15]

			{
				pHPPeerInfo->role = WEBRTC_ROLE_ANSWER;
				// 按照原Windows代码的规则赋值
				pHPPeerInfo->myid[0] = Data1;
				pHPPeerInfo->myid[1] = ((uint32_t)Data2 << 16) | Data3;
				pHPPeerInfo->myid[2] = ((uint32_t)Data4[0] << 24) | ((uint32_t)Data4[1] << 16) | ((uint32_t)Data4[2] << 8) | (uint32_t)Data4[3];
				pHPPeerInfo->myid[3] = ((uint32_t)Data4[4] << 24) | ((uint32_t)Data4[5] << 16) | ((uint32_t)Data4[6] << 8) | (uint32_t)Data4[7];
				
				uuid128.Initialize(pHPPeerInfo->myid[0], pHPPeerInfo->myid[1], pHPPeerInfo->myid[2], pHPPeerInfo->myid[3]);
				EnterCriticalSection(&g_csHPWBSPeerInfo);
				g_HPWBSPeerInfo_map.insert(std::map<uint128_t, HP_WBSPEER_INFO*>::value_type(uuid128, pHPPeerInfo));
				LeaveCriticalSection(&g_csHPWBSPeerInfo);
			}
		}

		ACK_CONNECTUSER_INFO ackInfo = { 0 };
		ackInfo.length = sizeof(ACK_CONNECTUSER_INFO);
		ackInfo.msgtype = HP_ACKCONNECTUSER_INFO;
		ackInfo.hisid[0] = pRecvInfo->hisid[0];
		ackInfo.hisid[1] = pRecvInfo->hisid[1];
		ackInfo.hisid[2] = pRecvInfo->hisid[2];
		ackInfo.hisid[3] = pRecvInfo->hisid[3];
		ackInfo.extradatalen0 = 0;
		ackInfo.status = -1; //账号不存在

		// 特别注意:要预留一个status来表示被控端版本过低,需要升级后才能使用

		uuid128.Initialize(pRecvInfo->hisid[0], pRecvInfo->hisid[1], pRecvInfo->hisid[2], pRecvInfo->hisid[3]);
		WriteToConsoleWindow("OnConnect %s:%d uuid:%08X-%04X-%04X-%04X-%04X%08X", pHPPeerInfo->remoteAddress, pHPPeerInfo->remotePort, pRecvInfo->hisid[0], pRecvInfo->hisid[1] >> 16, pRecvInfo->hisid[1] & 0xFFFF, pRecvInfo->hisid[2] >> 16, pRecvInfo->hisid[2] & 0xFFFF, pRecvInfo->hisid[3]);

		HP_WBSPEER_INFO* pHPPeerInfo2 = NULL;
		std::map<uint128_t, HP_WBSPEER_INFO*>::iterator itl;
		EnterCriticalSection(&g_csHPWBSPeerInfo);
		itl = g_HPWBSPeerInfo_map.find(uuid128);
		if (itl != g_HPWBSPeerInfo_map.end())
		{
			pHPPeerInfo2 = (*itl).second;
			if (memcmp(pRecvInfo->hiskey, pHPPeerInfo2->mykey, 32) != 0)
			{
				ackInfo.status = -2; //密码不对
			}
			else if (pHPPeerInfo2->alive != 1)
			{
				ackInfo.status = -3; //不在线
			}
			else if (pHPPeerInfo2->role != WEBRTC_ROLE_OFFER)
			{
				ackInfo.status = -4; //不是能够被连接的客户端(也就是不是连接的设备,我们不允许这种不是使用的我们rtcsdk原生动态库来开发的设备端)
			}
			else
			{
				int iPos = g_strdatas_info->strdataslen;
				int nSize = sizeof(ACK_CONNECTUSER_INFO) + iPos + pHPPeerInfo2->extradatalen0;
				ACK_CONNECTUSER_INFO* ackInfo2 = (ACK_CONNECTUSER_INFO*)malloc(nSize);
				ackInfo2->length = nSize;
				ackInfo2->msgtype = HP_ACKCONNECTUSER_INFO;
				ackInfo2->hisid[0] = pRecvInfo->hisid[0];
				ackInfo2->hisid[1] = pRecvInfo->hisid[1];
				ackInfo2->hisid[2] = pRecvInfo->hisid[2];
				ackInfo2->hisid[3] = pRecvInfo->hisid[3];
				ackInfo2->myid[0] = pHPPeerInfo->myid[0];
				ackInfo2->myid[1] = pHPPeerInfo->myid[1];
				ackInfo2->myid[2] = pHPPeerInfo->myid[2];
				ackInfo2->myid[3] = pHPPeerInfo->myid[3];
				ackInfo2->status = 0;
				ackInfo2->extradatalen0 = pHPPeerInfo2->extradatalen0;
				if (pHPPeerInfo2->extradatalen0 > 0) memcpy(ackInfo2->extradata0, pHPPeerInfo2->extradata0, pHPPeerInfo2->extradatalen0);
				mg_websocket_write(pHPPeerInfo, WS_OPCODE_BIN, (char*)ackInfo2, ackInfo2->length);

				nSize = sizeof(REQ_GETWEBRTCOFFER_INFO) + iPos + pRecvInfo->extradatalen0;
				REQ_GETWEBRTCOFFER_INFO* notiInfo = (REQ_GETWEBRTCOFFER_INFO*)malloc(nSize);
				notiInfo->length = nSize;
				notiInfo->msgtype = HP_REQGETWEBRTCOFFER_INFO;
				notiInfo->hisid[0] = pHPPeerInfo->myid[0];
				notiInfo->hisid[1] = pHPPeerInfo->myid[1];
				notiInfo->hisid[2] = pHPPeerInfo->myid[2];
				notiInfo->hisid[3] = pHPPeerInfo->myid[3];
				notiInfo->extradatalen0 = pRecvInfo->extradatalen0;
				notiInfo->strdataslen = g_strdatas_info->strdataslen;
				notiInfo->strcount = g_strdatas_info->strcount;
				for (int i = 0; i < 8; i++)
				{
					notiInfo->strtypes[i] = g_strdatas_info->strtypes[i];
				}
				if (iPos > 0) memcpy(notiInfo->strdatas, g_strdatas_info->strdatas, iPos);
				if (pRecvInfo->extradatalen0 > 0) memcpy(notiInfo->strdatas + iPos, pRecvInfo->extradata0, pRecvInfo->extradatalen0);
				mg_websocket_write(pHPPeerInfo2, WS_OPCODE_BIN, (char*)notiInfo, notiInfo->length);
				free(ackInfo2);
				free(notiInfo);

				LeaveCriticalSection(&g_csHPWBSPeerInfo);
				break;
			}
		}
		LeaveCriticalSection(&g_csHPWBSPeerInfo);

		//if (ackInfo.status != 0) mg_websocket_write(pHPPeerInfo, WS_OPCODE_BIN, (char*)&ackInfo, ackInfo.length); //可以选择只在不能连接的时候才发送这个消息
		mg_websocket_write(pHPPeerInfo, WS_OPCODE_BIN, (char*)&ackInfo, ackInfo.length);
	}
	break;
	case HP_NTIWEBRTCOFFER_INFO:
	{
		if (pHPPeerInfo->alive != 1) return -1;
		if (iLength <= sizeof(NTI_WEBRTCOFFER_INFO)) return -1;
		NTI_WEBRTCOFFER_INFO* pRecvInfo = (NTI_WEBRTCOFFER_INFO*)data;
		if ((pRecvInfo->sdplen == 0) || (pRecvInfo->length != (sizeof(NTI_WEBRTCOFFER_INFO) + pRecvInfo->sdplen + 1)) || (pRecvInfo->offersdp[0] == 0)) return -1;
		pRecvInfo->offersdp[pRecvInfo->sdplen] = 0;
		if (strlen(pRecvInfo->offersdp) < 64) return -1; //太短,应该是不正常的

		EnterCriticalSection(&g_csSessionDescription);
		memset(g_pSessionDescription, 0, sizeof(SessionDescription));
		STATUS status = deserializeSessionDescription(g_pSessionDescription, pRecvInfo->offersdp);
		LeaveCriticalSection(&g_csSessionDescription);
		if (status != STATUS_SUCCESS) return -1;

		uint128_t uuid128;
		uuid128.Initialize(pRecvInfo->hisid[0], pRecvInfo->hisid[1], pRecvInfo->hisid[2], pRecvInfo->hisid[3]);

		HP_WBSPEER_INFO* pHPPeerInfo2 = NULL;
		std::map<uint128_t, HP_WBSPEER_INFO*>::iterator itl;
		EnterCriticalSection(&g_csHPWBSPeerInfo);
		itl = g_HPWBSPeerInfo_map.find(uuid128);
		if (itl != g_HPWBSPeerInfo_map.end())
		{
			pHPPeerInfo2 = (*itl).second;
			if (pHPPeerInfo2->alive == 1)
			{
				//在这里要换成 NTI_WEBRTCOFFER_INFO2 结构体
				int nSize = sizeof(NTI_WEBRTCOFFER_INFO2) + pRecvInfo->sdplen + g_strdatas_info->strdataslen;
				NTI_WEBRTCOFFER_INFO2* notiInfo = (NTI_WEBRTCOFFER_INFO2*)malloc(nSize);
				notiInfo->length = nSize;
				notiInfo->msgtype = HP_NTIWEBRTCOFFER_INFO2;
				notiInfo->hisid[0] = pHPPeerInfo->myid[0];
				notiInfo->hisid[1] = pHPPeerInfo->myid[1];
				notiInfo->hisid[2] = pHPPeerInfo->myid[2];
				notiInfo->hisid[3] = pHPPeerInfo->myid[3];
				notiInfo->sdplen = pRecvInfo->sdplen;
				notiInfo->strdataslen = g_strdatas_info->strdataslen;
				notiInfo->strcount = g_strdatas_info->strcount;
				for (int i = 0; i < 8; i++)
				{
					notiInfo->strtypes[i] = g_strdatas_info->strtypes[i];
				}
				if (g_strdatas_info->strdataslen > 0) memcpy(notiInfo->strdatas, g_strdatas_info->strdatas, g_strdatas_info->strdataslen);
				if (pRecvInfo->sdplen > 0) memcpy(notiInfo->strdatas + g_strdatas_info->strdataslen, pRecvInfo->offersdp, pRecvInfo->sdplen);
				mg_websocket_write(pHPPeerInfo2, WS_OPCODE_BIN, (char*)notiInfo, notiInfo->length);
			}
			//printf("================\n");
			//WriteToConsoleWindow("sdptype: \n%s", pRecvInfo->sdptype);
			//WriteToConsoleWindow("offersdp: \n%s", pRecvInfo->offersdp);
			//printf("================\n");
		}
		LeaveCriticalSection(&g_csHPWBSPeerInfo);
	}
	break;
	case HP_NTIWEBRTCANSWER_INFO:
	{
		if (pHPPeerInfo->alive != 1) return -1;
		if (iLength <= sizeof(NTI_WEBRTCANSWER_INFO)) return -1;
		NTI_WEBRTCANSWER_INFO* pRecvInfo = (NTI_WEBRTCANSWER_INFO*)data;
		if ((pRecvInfo->sdplen == 0) || (pRecvInfo->length != (sizeof(NTI_WEBRTCANSWER_INFO) + pRecvInfo->sdplen + 1)) || (pRecvInfo->answersdp[0] == 0)) return -1;
		pRecvInfo->answersdp[pRecvInfo->sdplen] = 0;
		if (strlen(pRecvInfo->answersdp) < 64) return -1; //太短,应该是不正常的

		EnterCriticalSection(&g_csSessionDescription);
		memset(g_pSessionDescription, 0, sizeof(SessionDescription));
		STATUS status = deserializeSessionDescription(g_pSessionDescription, pRecvInfo->answersdp);
		LeaveCriticalSection(&g_csSessionDescription);
		if (status != STATUS_SUCCESS) return -1;

		uint128_t uuid128;
		uuid128.Initialize(pRecvInfo->hisid[0], pRecvInfo->hisid[1], pRecvInfo->hisid[2], pRecvInfo->hisid[3]);

		HP_WBSPEER_INFO* pHPPeerInfo2 = NULL;
		std::map<uint128_t, HP_WBSPEER_INFO*>::iterator itl;
		EnterCriticalSection(&g_csHPWBSPeerInfo);
		itl = g_HPWBSPeerInfo_map.find(uuid128);
		if (itl != g_HPWBSPeerInfo_map.end())
		{
			pHPPeerInfo2 = (*itl).second;
			if (pHPPeerInfo2->alive == 1)
			{
				pRecvInfo->hisid[0] = pHPPeerInfo->myid[0];
				pRecvInfo->hisid[1] = pHPPeerInfo->myid[1];
				pRecvInfo->hisid[2] = pHPPeerInfo->myid[2];
				pRecvInfo->hisid[3] = pHPPeerInfo->myid[3];
				mg_websocket_write(pHPPeerInfo2, WS_OPCODE_BIN, (char*)pRecvInfo, iLength);
			}
			//WriteToConsoleWindow("sdptype: \n%s", pRecvInfo->sdptype);
			//WriteToConsoleWindow("answersdp: \n%s", pRecvInfo->answersdp);
		}
		LeaveCriticalSection(&g_csHPWBSPeerInfo);
	}
	break;
	case HP_REQGETONLINEDEVICES_INFO:
	{
		int count = 0;
		HP_WBSPEER_INFO* pHPPeerInfo2 = NULL;
		std::map<uint128_t, HP_WBSPEER_INFO*>::iterator itl;

		EnterCriticalSection(&g_csHPWBSPeerInfo);
		int nSize = sizeof(ACK_GETONLINEDEVICES_INFO) + g_HPWBSPeerInfo_map.size() * sizeof(uint32_t) * 4;
		ACK_GETONLINEDEVICES_INFO* pAckInfo = (ACK_GETONLINEDEVICES_INFO*)malloc(nSize);

		for (itl = g_HPWBSPeerInfo_map.begin(); itl != g_HPWBSPeerInfo_map.end(); itl++)
		{
			pHPPeerInfo2 = (*itl).second;
			if ((pHPPeerInfo2->role != WEBRTC_ROLE_OFFER) || (pHPPeerInfo2->alive != 1)) continue; //不是设备
			if ((pHPPeerInfo2->myid[0] == pHPPeerInfo->myid[0]) && (pHPPeerInfo2->myid[1] == pHPPeerInfo->myid[1]) && (pHPPeerInfo2->myid[2] == pHPPeerInfo->myid[2]) && (pHPPeerInfo2->myid[3] == pHPPeerInfo->myid[3])) continue; //排除自己这个uuid
			pAckInfo->hisids[count][0] = pHPPeerInfo2->myid[0];
			pAckInfo->hisids[count][1] = pHPPeerInfo2->myid[1];
			pAckInfo->hisids[count][2] = pHPPeerInfo2->myid[2];
			pAckInfo->hisids[count][3] = pHPPeerInfo2->myid[3];
			count++;
		}
		LeaveCriticalSection(&g_csHPWBSPeerInfo);

		nSize = sizeof(ACK_GETONLINEDEVICES_INFO) + count * sizeof(uint32_t) * 4;
		pAckInfo->length = nSize;
		pAckInfo->msgtype = HP_ACKGETONLINEDEVICES_INFO;
		pAckInfo->idscount = count;
		mg_websocket_write(pHPPeerInfo, WS_OPCODE_BIN, (char*)pAckInfo, pAckInfo->length);
		free(pAckInfo);
	}
	break;
	default:
		WriteToConsoleWindow("!!!!!! pBaseInfo->msgtype:%u", pBaseInfo->msgtype);
		break;
	}

	return 0;
}

En_HP_HandleResult __HP_CALL OnWBSPrepareListen(HP_Server pSender, UINT_PTR soListen)
{
	return HR_OK;
}

En_HP_HandleResult __HP_CALL OnWBSAccept(HP_Server pSender, HP_CONNID dwConnID, UINT_PTR pClient)
{
	HP_WBSPEER_INFO* pHPPeerInfo = (HP_WBSPEER_INFO*)malloc(sizeof(HP_WBSPEER_INFO));
	memset(pHPPeerInfo, 0, sizeof(HP_WBSPEER_INFO));
	pHPPeerInfo->pSender = pSender;
	pHPPeerInfo->dwConnID = dwConnID;
	pHPPeerInfo->state = WS_STATE_HANDSHAKE;
	pHPPeerInfo->pClient = pClient;
	pHPPeerInfo->role = WEBRTC_ROLE_INVALID;
	pHPPeerInfo->alive = 0;

	USHORT usPort;
	TCHAR szAddress[65] = { 0 };
	int iAddressLen = sizeof(szAddress) / sizeof(TCHAR);
	HP_Server_GetRemoteAddress(pSender, dwConnID, szAddress, &iAddressLen, &usPort);
	pHPPeerInfo->remotePort = usPort;
	strcpy(pHPPeerInfo->remoteAddress, szAddress);
	HP_Server_SetConnectionExtra(pSender, dwConnID, pHPPeerInfo);
	socket_set_keepalive(pClient);

	WriteToConsoleWindow("OnWBSAccept %s:%d", pHPPeerInfo->remoteAddress, pHPPeerInfo->remotePort);
	return HR_OK;
}

En_HP_HandleResult __HP_CALL OnWBSHandShake(HP_Server pSender, HP_CONNID dwConnID)
{
	return HR_OK;
}

En_HP_HandleResult __HP_CALL OnWBSSend(HP_Server pSender, HP_CONNID dwConnID, const BYTE* pData, int iLength)
{
	return HR_OK;
}

En_HP_HandleResult __HP_CALL OnWBSPullReceive(HP_Server pSender, HP_CONNID dwConnID, int iLength)
{
	int i;
	HP_WBSPEER_INFO* pHPPeerInfo = NULL;
	HP_Server_GetConnectionExtra(pSender, dwConnID, (PVOID*)&pHPPeerInfo);
	if ((pHPPeerInfo->state == WS_STATE_HANDSHAKE) && (iLength >= MAX_SUPPORT_HTMLREQ)) return HR_ERROR;

	if (pHPPeerInfo->state != WS_STATE_HANDSHAKE)
	{
		int frameLen = 0;
		int HeaderLen = 0;
		int remain = iLength;

		unsigned char buffer[65 * 1024];
		ws_comm_header* wsheader = (ws_comm_header*)buffer;
		ws_0m_header* ws0header = (ws_0m_header*)buffer;
		ws_1m_header* ws1header = (ws_1m_header*)buffer;

		while (1)
		{
			if (remain < sizeof(ws_0m_header)) return HR_OK;

			HeaderLen = sizeof(ws_0m_header);
			if (remain >= sizeof(ws_2m_header)) HeaderLen = sizeof(ws_2m_header); else if (remain >= sizeof(ws_1m_header)) HeaderLen = sizeof(ws_1m_header);
			if (HP_TcpPullServer_Peek(pSender, dwConnID, buffer, HeaderLen) != FR_OK) return HR_ERROR;
			if ((wsheader->fin == 0) || (wsheader->rsv1 != 0) || (wsheader->rsv2 != 0) || (wsheader->rsv3 != 0) || (wsheader->mask == 0)) return HR_ERROR;

			if (wsheader->payloadlen < 126)
			{
				frameLen = sizeof(ws_0m_header) + wsheader->payloadlen;
				if (remain < frameLen) return HR_OK;

				if (HP_TcpPullServer_Fetch(pSender, dwConnID, buffer, frameLen) != FR_OK) return HR_ERROR;
				remain -= frameLen;

				for (i = 0; i < ws0header->payloadlen; i++)
				{
					ws0header->payloaddata[i] ^= ws0header->maskey[i % 4];
				}
				buffer[frameLen] = 0;
				if (WebsocketDataHandler(pHPPeerInfo, wsheader, ws0header->payloaddata, ws0header->payloadlen) != 0)
				{
					return HR_ERROR;
				}
			}
			else if (wsheader->payloadlen == 126)
			{
				if (remain < (sizeof(ws_1m_header) + 126)) return HR_OK;

				uint16_t expayloadlen = be16toh(ws1header->expayloadlen);
				frameLen = sizeof(ws_1m_header) + expayloadlen;
				if (remain < frameLen) return HR_OK;

				if (HP_TcpPullServer_Fetch(pSender, dwConnID, buffer, frameLen) != FR_OK) return HR_ERROR;
				remain -= frameLen;

				for (i = 0; i < expayloadlen; i++)
				{
					ws1header->payloaddata[i] ^= ws1header->maskey[i % 4];
				}
				buffer[frameLen] = 0;
				if (WebsocketDataHandler(pHPPeerInfo, wsheader, ws1header->payloaddata, expayloadlen) != 0)
				{
					return HR_ERROR;
				}
			}
			else if (wsheader->payloadlen == 127)
			{
				return HR_ERROR;
			}
		}
	}
	else
	{
		char htmlBuffer[MAX_SUPPORT_HTMLREQ] = { 0 };
		if (HP_TcpPullServer_Peek(pSender, dwConnID, (BYTE*)htmlBuffer, iLength) != FR_OK) return HR_ERROR;

		char htmlBuffer2[sizeof(HTML_INFO)] = { 0 };
		HTML_INFO* html = (HTML_INFO*)&htmlBuffer2;

		llhttp_t parser = { 0 };
		llhttp_settings_t settings = { 0 };

		llhttp_settings_init(&settings);
		settings.on_url = request_url_cb;
		settings.on_header_field = header_field_cb;
		settings.on_header_value = header_value_cb;
		settings.on_headers_complete = headers_complete_cb;
		settings.on_message_complete = message_complete_cb;

		llhttp_init(&parser, HTTP_REQUEST, &settings);
		parser.data = html;

		size_t nparsed = llhttp_execute(&parser, htmlBuffer, iLength);
		if (html->headercp == 0) return HR_OK; //还没有接收完毕
		if (html->url[0] != '/') return HR_ERROR;

		if (html->keycount != html->valuecount) return HR_ERROR;
		if ((parser.http_major <= 1) && (parser.http_minor < 1)) return HR_ERROR;

		int bIs1 = 0;
		int bIs2 = 0;
		int bIs3 = 0;
		int bIs4 = 0;
		int bIs5 = 0;
		int keyIndex = 0;
		for (i = 0; i < html->keycount; i++)
		{
			if ((bIs1 + bIs2 + bIs3 + bIs4 + bIs5) == 5) break;

			if ((bIs1 == 0) && (strcmp(html->keys[i], "Host") == 0))
			{
				bIs1 = (strlen(html->values[i]) > 0);
				continue;
			}
			if ((bIs3 == 0) && (strcmp(html->keys[i], "Connection") == 0))
			{
				bIs3 = ((strstr(html->values[i], "upgrade") != NULL) || (strstr(html->values[i], "Upgrade") != NULL));
				continue;
			}
			if ((bIs2 == 0) && (strcmp(html->keys[i], "Upgrade") == 0))
			{
				bIs2 = (strcmp(html->values[i], "websocket") == 0);
				continue;
			}
			if ((bIs4 == 0) && (strcmp(html->keys[i], "Sec-WebSocket-Version") == 0))
			{
				bIs4 = (strcmp(html->values[i], "13") == 0);
				continue;
			}
			if ((bIs5 == 0) && (strcmp(html->keys[i], "Sec-WebSocket-Key") == 0))
			{
				keyIndex = i;
				bIs5 = (strlen(html->values[i]) == 24);
				continue;
			}
		}

		if ((bIs1 + bIs2 + bIs3 + bIs4 + bIs5) != 5) return HR_ERROR;
		if (HP_TcpPullServer_Fetch(pSender, dwConnID, (BYTE*)htmlBuffer, iLength) != FR_OK) return HR_ERROR;

		char concatString[64] = { 0 };
		unsigned char sha1Hash[21] = { 0 };

		snprintf(concatString, sizeof(concatString), "%s" WS_ACCEPT_MAGIC_KEY, html->values[keyIndex]);
		SHA1Context sha1context = { 0 };
		SHA1Reset(&sha1context);
		SHA1Input(&sha1context, (uint8_t*)concatString, strlen(concatString));
		SHA1Result(&sha1context, sha1Hash);

		DWORD nDestLen = 256;
		char replyHeader[1024] = { 0 };
		char replyKey[256] = { 0 };
		SYS_Base64Encode((BYTE*)sha1Hash, 20, (BYTE*)replyKey, &nDestLen);
		snprintf(replyHeader, sizeof(replyHeader), WS_HANDSHAKE_REPLY_BLUEPRINT, replyKey);
		if (!HP_Server_Send(pSender, dwConnID, (BYTE*)replyHeader, strlen(replyHeader))) return HR_ERROR;

		pHPPeerInfo->role = 0;
		pHPPeerInfo->state = WS_STATE_CONNECTED;
	}

	return HR_OK;
}

En_HP_HandleResult __HP_CALL OnWBSClose(HP_Server pSender, HP_CONNID dwConnID, En_HP_SocketOperation enOperation, int iErrorCode)
{
	//printf("OnWBSClose ========================\n");
	HP_WBSPEER_INFO* pHPPeerInfo = NULL;
	HP_Server_GetConnectionExtra(pSender, dwConnID, (PVOID*)&pHPPeerInfo);
	if (pHPPeerInfo == NULL) return HR_OK;
	WriteToConsoleWindow("OnWBSClose %s:%d", pHPPeerInfo->remoteAddress, pHPPeerInfo->remotePort);

	if ((pHPPeerInfo->myid[0] == 0) && (pHPPeerInfo->myid[1] == 0) && (pHPPeerInfo->myid[2] == 0) && (pHPPeerInfo->myid[3] == 0))
	{
		free(pHPPeerInfo);
		return HR_OK;
	}

	uint128_t uuid128;
	uuid128.Initialize(pHPPeerInfo->myid[0], pHPPeerInfo->myid[1], pHPPeerInfo->myid[2], pHPPeerInfo->myid[3]);

	HP_WBSPEER_INFO* pHPPeerInfo2 = NULL;
	std::map<uint128_t, HP_WBSPEER_INFO*>::iterator itl;
	EnterCriticalSection(&g_csHPWBSPeerInfo);
	itl = g_HPWBSPeerInfo_map.find(uuid128);
	if (itl != g_HPWBSPeerInfo_map.end())
	{
		pHPPeerInfo2 = (*itl).second;
		if (pHPPeerInfo2 == pHPPeerInfo)
		{
			g_HPWBSPeerInfo_map.erase(itl);
		}
	}
	LeaveCriticalSection(&g_csHPWBSPeerInfo);
	if (pHPPeerInfo->extradata0 != NULL) free(pHPPeerInfo->extradata0);
	free(pHPPeerInfo);

	return HR_OK;
}

En_HP_HandleResult __HP_CALL OnWBSShutdown(HP_Server pSender)
{
	return HR_OK;
}

En_HP_HandleResult __HP_CALL OnTcpAccept(HP_Server pSender, HP_CONNID dwConnID, UINT_PTR pClient);
En_HP_HandleResult __HP_CALL OnTcpPullReceive(HP_Server pSender, HP_CONNID dwConnID, int iLength);
En_HP_HandleResult __HP_CALL OnTcpClose(HP_Server pSender, HP_CONNID dwConnID, En_HP_SocketOperation enOperation, int iErrorCode);

int main(int argc, char* argv[])
{
	LoadConfigIniFile();
	int wbsport = g_sconfig.baseinfo.localport;
	if (wbsport == 0) wbsport = 6688;

	g_pSessionDescription = (SessionDescription*)malloc(sizeof(SessionDescription));
	InitializeCriticalSection(&g_csHPWBSPeerInfo);
	InitializeCriticalSection(&g_csSessionDescription);

	HP_TcpPullServer s_wbsserver = NULL;
	HP_TcpPullServerListener s_wbslistener = NULL;
	s_wbslistener = Create_HP_TcpPullServerListener();
	s_wbsserver = Create_HP_TcpPullServer(s_wbslistener);

	HP_Set_FN_Server_OnPrepareListen(s_wbslistener, OnWBSPrepareListen);
	HP_Set_FN_Server_OnAccept(s_wbslistener, OnWBSAccept);
	HP_Set_FN_Server_OnHandShake(s_wbslistener, OnWBSHandShake);
	HP_Set_FN_Server_OnSend(s_wbslistener, OnWBSSend);
	HP_Set_FN_Server_OnPullReceive(s_wbslistener, OnWBSPullReceive);
	HP_Set_FN_Server_OnClose(s_wbslistener, OnWBSClose);
	HP_Set_FN_Server_OnShutdown(s_wbslistener, OnWBSShutdown);

	HP_TcpServer_SetKeepAliveTime(s_wbsserver, 10 * 1000);
	HP_TcpServer_SetKeepAliveInterval(s_wbsserver, 5 * 1000);
	HP_Server_SetSendPolicy(s_wbsserver, SP_DIRECT);
	if (!HP_Server_Start(s_wbsserver, "0.0.0.0", wbsport))
	{
		printf("websocket server failed...................\n");
	}

	HP_TcpPullServer sslserver = NULL;
	HP_TcpPullServerListener ssllistener = NULL;
	if (g_sconfig.baseinfo.supportssl == 1)
	{
		int sslport = g_sconfig.ssl.localport;
		if (sslport == 0) sslport = 6689;

		ssllistener = Create_HP_TcpPullServerListener();
		sslserver = Create_HP_SSLPullServer(ssllistener);
		if (HP_SSLServer_SetupSSLContext(sslserver, SSL_VM_NONE, g_sconfig.ssl.pemcertfile, g_sconfig.ssl.pemkeyfile, g_sconfig.ssl.keypassword, "", NULL))
		{
			HP_Set_FN_Server_OnPrepareListen(ssllistener, OnWBSPrepareListen);
			HP_Set_FN_Server_OnAccept(ssllistener, OnWBSAccept);
			HP_Set_FN_Server_OnHandShake(ssllistener, OnWBSHandShake);
			HP_Set_FN_Server_OnSend(ssllistener, OnWBSSend);
			HP_Set_FN_Server_OnPullReceive(ssllistener, OnWBSPullReceive);
			HP_Set_FN_Server_OnClose(ssllistener, OnWBSClose);
			HP_Set_FN_Server_OnShutdown(ssllistener, OnWBSShutdown);

			HP_TcpServer_SetKeepAliveTime(sslserver, 10 * 1000);
			HP_TcpServer_SetKeepAliveInterval(sslserver, 5 * 1000);
			HP_Server_SetSendPolicy(sslserver, SP_DIRECT);
			if (!HP_Server_Start(sslserver, "0.0.0.0", sslport))
			{
				printf("websocket ssl server failed...................\n");
			}
		}
		else
		{
			printf("HP_SSLServer_SetupSSLContext failed...................\n");
		}
	}

	WriteToConsoleWindow("signal server started...................\n");

	HP_WBSPEER_INFO* pHPPeerInfo = NULL;
	std::map<uint128_t, HP_WBSPEER_INFO*>::iterator itl;

	while (1) usleep(10000000);

	HP_Server_Stop(s_wbsserver);
	Destroy_HP_TcpPullServer(s_wbsserver);
	Destroy_HP_TcpPullServerListener(s_wbslistener);

	if (g_sconfig.baseinfo.supportssl == 1)
	{
		HP_Server_Stop(sslserver);
		HP_SSLServer_CleanupSSLContext(sslserver);
		Destroy_HP_SSLPullServer(sslserver);
		Destroy_HP_TcpPullServerListener(ssllistener);
	}

	EnterCriticalSection(&g_csHPWBSPeerInfo);
	for (itl = g_HPWBSPeerInfo_map.begin(); itl != g_HPWBSPeerInfo_map.end(); )
	{
		pHPPeerInfo = (*itl).second;
		itl = g_HPWBSPeerInfo_map.erase(itl);
		free(pHPPeerInfo);
	}
	LeaveCriticalSection(&g_csHPWBSPeerInfo);
	DeleteCriticalSection(&g_csHPWBSPeerInfo);

	DeleteCriticalSection(&g_csSessionDescription);
	free(g_pSessionDescription);
	g_pSessionDescription = NULL;
	if (g_strdatas_info != NULL)
	{
		free(g_strdatas_info);
		g_strdatas_info = NULL;
	}

	return 0;
}
