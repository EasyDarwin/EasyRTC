
#ifndef __WS_PROTOCOL_H__
#define __WS_PROTOCOL_H__

#if defined(_WIN32)
#include <WinSock2.h>
#include <mstcpip.h>
#include <stdint.h>
#include <process.h>
#else
#include <ctype.h>
#include <arpa/inet.h>
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
#define Sleep(x) usleep(1000 * x)

typedef struct
{
	pthread_mutex_t m;
	pthread_t t;
} __attribute__((packed)) CRITICAL_SECTION;

#if (1)
#define InitializeCriticalSection(x) {(x)->t = 0; pthread_mutex_init(&((x)->m),NULL);}
#define EnterCriticalSection(x) while (1)\
{\
	if ((x)->t == 0)\
{\
	pthread_mutex_lock(&((x)->m));\
	(x)->t = pthread_self();\
	break;\
}\
	if (pthread_self() == (x)->t)\
	break;\
	pthread_mutex_lock(&((x)->m));\
	(x)->t = pthread_self();\
	break;\
}
#else
#define InitializeCriticalSection(x) {(x)->t = 0;pthread_mutexattr_t mutexattr;pthread_mutexattr_init(&mutexattr);pthread_mutexattr_settype(&mutexattr,PTHREAD_MUTEX_NORMAL);pthread_mutex_init(&((x)->m),&mutexattr);pthread_mutexattr_destroy(&mutexattr);}
#define EnterCriticalSection(x) {pthread_mutex_lock(&((x)->m));}
#endif
#define LeaveCriticalSection(x) {pthread_mutex_unlock(&((x)->m));(x)->t = 0;}
#define DeleteCriticalSection(x) {pthread_mutex_destroy(&((x)->m));}
#endif



#define HP_REQLOGINUSER_INFO                 0x10001
#define HP_ACKLOGINUSER_INFO                 0x10002
#define HP_REQCONNECTUSER_INFO               0x10003
#define HP_ACKCONNECTUSER_INFO               0x10004
#define HP_REQGETWEBRTCOFFER_INFO            0x10005
#define HP_NTIWEBRTCOFFER_INFO               0x10006
#define HP_NTIWEBRTCOFFER_INFO2              0x10007
#define HP_NTIWEBRTCANSWER_INFO              0x10008

#define HP_REQPRIVCONNECTUSER_INFO           0x11001
#define HP_ACKPRIVCONNECTUSER_INFO           0x11002

#define HP_REQGETONLINEDEVICES_INFO          0x20001
#define HP_ACKGETONLINEDEVICES_INFO          0x20002




#if defined(_MSC_VER) // Microsoft compiler
#define __attribute__(x)
#elif defined(__GNUC__) // GNU compiler
#else
#define __attribute__(x)
#endif

#if defined(_MSC_VER) // Microsoft compiler
#pragma pack(1)
#endif




//webrtc
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
	uint16_t sturndataslen;
	int8_t   sturncount;
	int8_t   sturntypes[8];
	char     sturndatas[0];
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
	uint16_t sturndataslen;
	int8_t   sturncount;
	int8_t   sturntypes[8];
	char     sturndatas[0]; //在这之后是 offersdp
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


//下面是设备端收到的数据包格式
typedef struct
{
	uint32_t length;
	uint32_t msgtype;
	uint32_t myid[4];
	int32_t  status;
}__attribute__((packed)) SDK_ACK_LOGINUSER_INFO;

typedef struct
{
	uint32_t length;
	uint32_t msgtype;
	uint32_t hisid[4];
	int32_t  sockfd;
	uint16_t extradatalen0;
	uint16_t sturndataslen;
	int8_t   sturncount;
	int8_t   sturntypes[8];
	char     sturndatas[0];
	//在这之后是 extradata0
}__attribute__((packed)) SDK_REQ_GETWEBRTCOFFER_INFO;

typedef struct
{
	uint32_t length;
	uint32_t msgtype;
	uint32_t hisid[4];
	uint16_t sdplen;
	char answersdp[0];
} __attribute__((packed)) SDK_NTI_WEBRTCANSWER_INFO;

//下面是客户端(网页端)收到的数据包格式(与ACK_CONNECTWEBRTCUSER_INFO保持完全一致的定义)
typedef struct
{
	uint32_t length;
	uint32_t msgtype;
	uint32_t hisid[4];
	uint32_t myid[4];
	int32_t  status;
	uint16_t extradatalen0;
	char     extradata0[0]; //这是另一端发送过来的
}__attribute__((packed)) SDK_ACK_CONNECTUSER_INFO;

typedef struct
{
	uint32_t length;
	uint32_t msgtype;
	uint32_t hisid[4];
	int32_t  sockfd;
	uint16_t sdplen;
	uint16_t sturndataslen;
	int8_t   sturncount;
	int8_t   sturntypes[8];
	char     sturndatas[0]; //在这之后是 offersdp
	//char     offersdp[0];
} __attribute__((packed)) SDK_NTI_WEBRTCOFFER_INFO;


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






#endif

