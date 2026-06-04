#ifndef __EASYRTC_WS_H__
#define __EASYRTC_WS_H__

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

// websocket client 连接服务器
int websocketCreate(const char* serverAddr, const int serverPort, const int isSecure, void* userptr, ws_connect_callback connectCallback, ws_register_callback registerCallback, ws_data_callback dataCallback, ws_idle_callback idleCallback, void** ppWssClient);

// websocket 监听本地端口
int websocketBindLocalService(const int localListenPort, void* userptr, ws_data_callback dataCallback, ws_idle_callback idleCallback, void** ppWssClient);


int websocketSetRegisterStatus(void* pWssClient, int registerStatus);
int websocketSendData(void* pWssClient, int opcode, int mask, char* data, int size);
void websocketRelease(void** ppWssClient);

int websocketGetVersion(char* version);

#ifdef __cplusplus
}
#endif
#endif
