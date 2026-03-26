#ifndef __EASYRTC_WS_H__
#define __EASYRTC_WS_H__

//#include <winsock2.h>
//#include "amazon-kinesis-video-streams-pic/src/common/include/com/amazonaws/kinesis/video/common/CommonDefs.h"

//#ifdef _WIN32
//#define WIN32_LEAN_AND_MEAN
//#include <winsock2.h>
//#define EASYRTC_WS_API  __declspec(dllexport)
//#else
//#include <stdlib.h>
//#include <stdio.h>
//#include <string.h>
//#define EASYRTC_WS_API   __attribute__ ((visibility("default")))
//#define	CALLBACK
//#endif



typedef enum __CONNECT_STATUS_E
{
    EASYRTC_WS_DNS_FAIL =   0x01,                   // DNS解析失败
    EASYRTC_WS_CONNECTING,                          // 连接中
    EASYRTC_WS_CONNECTED,                           // 连接成功
    EASYRTC_WS_CONNECT_FAIL,                        // 连接失败
    EASYRTC_WS_DISCONNECT,                          // 连接断开
}CONNECT_STATUS_E;


#ifdef __cplusplus
extern "C" {
#endif

typedef int (*ws_connect_callback)(void* userptr, CONNECT_STATUS_E status);
typedef int (*ws_register_callback)(void* userptr);
typedef int (*ws_idle_callback)(void* userptr);
typedef int (*ws_data_callback)(void* userptr, char *data, int size);

int websocketCreate(const char* serverAddr, const int serverPort, const int isSecure, void* userptr, ws_connect_callback connectCallback, ws_register_callback registerCallback, ws_data_callback dataCallback, ws_idle_callback idleCallback, void** ppWssClient);
int websocketSetRegisterStatus(void* pWssClient, int registerStatus);
int websocketSendData(void* pWssClient, int opcode, char* data, int size);
void websocketRelease(void** ppWssClient);

int websocketGetVersion(char* version);

#ifdef __cplusplus
}
#endif
#endif
