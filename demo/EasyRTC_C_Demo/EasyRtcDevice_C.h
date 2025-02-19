#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "EasyRTC.h"
#include "osmutex.h"
#include "osthread.h"
#include <time.h>

#ifdef _WIN32
#pragma comment(lib, "EasyRTCSDK/EasyRTC.lib")
#endif

#ifdef _WIN32
#define MAX_WEBRTC_PEERS_COUNT 8	//8 //���ܴ���128,��Ϊsdk���������������
#else
#define MAX_WEBRTC_PEERS_COUNT 1	//8 //���ܴ���128,��Ϊsdk���������������
#endif


typedef struct __WEBRTC_PEER_T
{
	time_t		createTime;		// ����ʱ��
	int connected;
	int disconnected;
	int noneedsI;

	PRtcPeerConnection peer_;
	PRtcDataChannel dc_;

	PRtcRtpTransceiver videoTransceiver_;
	PRtcRtpTransceiver audioTransceiver_;

	void* userptr;		// ָ��WEBRTC_DEVICE_T
}WEBRTC_PEER_T;

typedef struct __WEBRTC_DEVICE_T
{
	int audioCodecID;
	int videoCodecID;
	char strUUID[128];
	BOOL  enableLocalService;
	int		webRTCPeerNum;
	WEBRTC_PEER_T* webrtcPeer[MAX_WEBRTC_PEERS_COUNT];
	OSTHREAD_OBJ_T* checkThread;		// ��������߳�

	void* callback;
	void* userptr;
}WEBRTC_DEVICE_T;

typedef int (*EasyRTC_DataChannel_Callback)(void* userptr, WEBRTC_PEER_T *webrtcPeer,BOOL isBinary, PBYTE msgData, UINT32 msgLen);

// ��ʼ��
int		EasyRTC_Device_Init();
// ����ʼ��
void	EasyRTC_Device_Deinit();

// ����EasyRTC
int		EasyRTC_Device_Start(WEBRTC_DEVICE_T *pDevice, int videoCodecID, int audioCodecID, const char *uuid, BOOL enableLocalService, EasyRTC_DataChannel_Callback callback, void *userptr);
// ֹͣEasyRTC
int		EasyRTC_Device_Stop(WEBRTC_DEVICE_T* pDevice);

// ������Ƶ֡
int		EasyRTC_Device_SendVideoFrame(WEBRTC_DEVICE_T* pDevice, char *framedata, const int framesize, int keyframe, uint64_t pts/*ʱ�����λ��:����*/);
// ������Ƶ֡
int		EasyRTC_Device_SendAudioFrame(WEBRTC_DEVICE_T* pDevice, char* framedata, const int framesize, uint64_t pts/*ʱ�����λ��:����*/);
// �����Զ�������
int		EasyRTC_Device_SendData(WEBRTC_PEER_T* webrtcPeer, BOOL isBinary, PBYTE msgData, UINT32 msgLen);
