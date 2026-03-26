#ifndef __EASYRTC_DEVICE_H__
#define __EASYRTC_DEVICE_H__



#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "osmutex.h"
#include "osthread.h"
#include "buff.h"
#include <time.h>
#include "wsProtocol.h"
#include "EasyRTCDeviceAPI.h"
#include "EasyRTC_websocket.h"
#include "RTCPeer.h"
#include <stdbool.h>

#if defined(SUPPORT_WEBRTC_AEC)
#include "echo_control_mobile.h"
#endif

#ifndef _WIN32
#define Sleep(x)	usleep(x*1000)
#endif

// WebRTC 服务器配置
typedef struct {
	char stun_servers[10][256];  // STUN 服务器列表
	int stun_server_count;
	char turn_servers[10][256];  // TURN 服务器列表
	int turn_server_count;
	char turn_username[256];     // TURN 服务器用户名
	char turn_password[256];     // TURN 服务器密码
	char ice_server_urls[10][256]; // ICE 服务器 URL 列表
	int ice_server_count;
}EasyRTCServerConfig;


#define		TRACK_VIDEO_CID		"webrtc_track1"
#define		TRACK_VIDEO_NAME	"webrtc_stream1"


#define EASYRTC_MDIA_TYPE_VIDEO	0x01
#define EASYRTC_MDIA_TYPE_AUDIO	0x02


// H.265 NAL单元类型枚举
typedef enum {
	NAL_UNIT_TRAIL_N = 0,
	NAL_UNIT_TRAIL_R = 1,
	NAL_UNIT_TSA_N = 2,
	NAL_UNIT_TSA_R = 3,
	NAL_UNIT_STSA_N = 4,
	NAL_UNIT_STSA_R = 5,
	NAL_UNIT_RADL_N = 6,
	NAL_UNIT_RADL_R = 7,
	NAL_UNIT_RASL_N = 8,
	NAL_UNIT_RASL_R = 9,
	NAL_UNIT_BLA_W_LP = 16,
	NAL_UNIT_BLA_W_RADL = 17,
	NAL_UNIT_BLA_N_LP = 18,
	NAL_UNIT_IDR_W_RADL = 19,
	NAL_UNIT_IDR_N_LP = 20,
	NAL_UNIT_CRA = 21,
	NAL_UNIT_VPS = 32,
	NAL_UNIT_SPS = 33,
	NAL_UNIT_PPS = 34,
	NAL_UNIT_ACCESS_UNIT_DELIMITER = 35,
	NAL_UNIT_EOS = 36,
	NAL_UNIT_EOB = 37,
	NAL_UNIT_FILLER_DATA = 38,
	NAL_UNIT_SEI = 39,
	NAL_UNIT_SEI_SUFFIX = 40
} NalUnitType;


typedef struct __CHANNEL_INFO_T
{
	char	code[32];
	unsigned int audioCodecID;
	unsigned int videoCodecID;

	unsigned int	mediaType;

	EasyRTC_RTP_TRANSCEIVER_DIRECTION direction;

	BUFF_T	bufAudioFrame;
#if defined(SUPPORT_WEBRTC_AEC)
	void* aecmInstHandler;
#endif
}CHANNEL_INFO_T;



typedef struct __EASYRTC_DEVICE_T
{
	char	serverAddr[64];
	int		serverPort;
	int		isSecure;

	EasyRTCServerConfig	serverConfig;
	EasyRTC_Configuration rtcConfiguration;

	//===================================================
	int		sockfd;

	// 公共部分
	EASYRTC_PEER_T* peerList;
	OSMutex* peerMutex;

	void* websocket;
	//OSTHREAD_OBJ_T* workerThread;		// 检查连接线程
	EasyRTC_Data_Callback	dataCallback;
	void* dataUserptr;
	void* dataChannelUserptr;



	bool	enablePingPong;
	//===================================================

	unsigned int	transactionID;				// 事务id

	char local_id[128];
	unsigned int	local_uuid[4];
	char peer_id[128];							// 当前仅为caller时使用
	unsigned int	peer_uuid[4];
	CHANNEL_INFO_T	channelInfo;
	//PASSIVE_CALL_T	passiveCallList[MAX_CHANNEL_NUM];

	int		registerStatus;

	//EasyRTC_Device_Callback deviceCallback;
	//void* deviceUserptr;

	time_t	lastCheckTime;

	void* pThis;				// 指向EasyRTCDevice

	int		spsLen;
	int		ppsLen;
	char	sps[1024];
	char	pps[128];
	//BUFF_T  bufFullFrame;
	//BUFF_T	bufAudioFrame;
}EASYRTC_DEVICE_T;

// ===============================================
// ===============================================
// =================设备端接口====================
// ===============================================
// ===============================================


int RTC_Device_Start(EASYRTC_DEVICE_T *pDevice, const char* local_id, EasyRTC_Data_Callback callback, void* userptr);
int RTC_Device_SetChannelInfo(EASYRTC_DEVICE_T* pDevice, EASYRTC_CODEC videoCodecID, EASYRTC_CODEC audioCodecID);

// 被动呼叫响应(decline: 1为拒绝呼叫   0为接受呼叫)
int RTC_Device_PassiveCallResponse(EASYRTC_DEVICE_T* pDevice, const char *peer_id, const int decline);

int RTC_Device_SendVideoFrame(EASYRTC_DEVICE_T* pDevice, char* framedata, const int framesize, bool keyframe, unsigned long long pts/*时间戳单位是:毫秒*/);
int RTC_Device_SendAudioFrame(EASYRTC_DEVICE_T* pDevice, char* framedata, const int framesize, unsigned long long pts/*时间戳单位是:毫秒*/);
int RTC_Device_SendCustomData(EASYRTC_DEVICE_T* pDevice, const char *peerUUID, const int isBinary, const char* data, const int size);
int RTC_Device_Hangup(EASYRTC_DEVICE_T* pDevice, const char* peerUUID);

int RTC_Device_aecm_process(EASYRTC_DEVICE_T* pDevice, short* nearendNoisy, short* outPcmData, int pcmSize);




void WebsocketDataHandler(EASYRTC_DEVICE_T* pDevice, const unsigned char* data, size_t len, int* errCode);


// ===============================================
// ===============================================
// =================播放端接口====================
// ===============================================
// ===============================================
// 连接设备
int RTC_Caller_Connect(EASYRTC_DEVICE_T* pDevice, const char * peer_id);


// ===============================================
// ===============================================
// ==================公共接口=====================
// ===============================================
// ===============================================

EASYRTC_DEVICE_T* RTC_Device_Create(const char* serverAddr, const int serverPort, const int isSecure);

int RTC_Device_Stop(EASYRTC_DEVICE_T* pDevice);
int RTC_Device_Release(EASYRTC_DEVICE_T** ppDevice);


int  CallbackData(EASYRTC_DEVICE_T* pDevice, const char* peerUUID, EASYRTC_DATA_TYPE_ENUM_E dataType, int codecID, int isBinary, char* data, int size, int keyframe, unsigned long long pts);

int	__Print__(void* userptr, const char* functionName, const int lineNum, bool dataChannelCallback, const char* szFormat, ...);

int LockPeerList(EASYRTC_DEVICE_T* pDevice, const char* functionName, const int lineNum);
int UnlockPeerList(EASYRTC_DEVICE_T* pDevice, const char* functionName, const int lineNum);

int ReleasePeerList(EASYRTC_DEVICE_T* pDevice);		// 释放pDevice中的所有列表

//int CheckTypeAndId(EASYRTC_DEVICE_T* pDevice, const char* type, const char* id, const char *channelId);
int EasyRTC_Build_TransactionID(EASYRTC_DEVICE_T* pDevice, char *outTransactionID);
int SendRegister(EASYRTC_DEVICE_T* pDevice);
int Hangup(EASYRTC_DEVICE_T* pDevice, EASYRTC_PEER_T* peer, const char *channelIdStr);


int	EasyRTC_CreatePeer(EASYRTC_DEVICE_T* pDevice, EASYRTC_PEER_T *peer, EASYRTC_PEER_TYPE_E type, const char *offerSDP);
int EasyRTC_ReleasePeer(EASYRTC_PEER_T* peer);

void trim(char* strIn, char* strOut);
int GetUUIDSFromString(char* struuid, uint32_t* myids);

// ICE canidate 回调函数
int __EasyRTC_IceCandidate_Callback(void* userPtr, const int isOffer, const char* sdp);

int GetOnlineDevices(EASYRTC_DEVICE_T* pDevice, EasyRTC_Data_Callback callback, void *userptr);

#endif
