#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "EasyRTC.h"
#include "osmutex.h"
#include "osthread.h"
#include <time.h>
#include <map>
using namespace std;

#ifdef _WIN32
#pragma comment(lib, "EasyRTCSDK/EasyRTC.lib")
#endif

#ifdef _WIN32
#define MAX_WEBRTC_PEERS_COUNT 8	//8 //���ܴ���128,��Ϊsdk���������������
#else
#define MAX_WEBRTC_PEERS_COUNT 1	//8 //���ܴ���128,��Ϊsdk���������������
#endif

typedef enum __EASYRTC_DATA_TYPE_ENUM_E
{
	EASYRTC_DATA_TYPE_VIDEO	=	0x01,				// �ص��Զ˵���Ƶ����
	EASYRTC_DATA_TYPE_AUDIO,						// �ص��Զ˵���Ƶ����
	EASYRTC_DATA_TYPE_METADATA,						// �ص��Զ˵�DataChannel����
}EASYRTC_DATA_TYPE_ENUM_E;

// DataChannel ���ݻص�
typedef int (*EasyRTC_Data_Callback)(void* userptr, void* webrtcPeer, EASYRTC_DATA_TYPE_ENUM_E dataType, int codecID, BOOL isBinary, PBYTE msgData, UINT32 msgLen);

// ״̬��Ϣ�ص�
typedef int (*EasyRTC_Device_Callback)(void* userptr, const char *pbuf, const int bufsize);

class EasyRtcDevice
{
public:
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

	typedef map<uint64_t, WEBRTC_PEER_T*>		WEBRTC_PEER_MAP;
	typedef struct __WEBRTC_DEVICE_T
	{
		int audioCodecID;
		int videoCodecID;
		char strUUID[128];
		bool enableLocalService;
		WEBRTC_PEER_MAP	webrtcPeerMap;
		OSTHREAD_OBJ_T* checkThread;		// ��������߳�
		EasyRTC_Data_Callback	dataCallback;
		void* dataChannelUserptr;

		EasyRTC_Device_Callback deviceCallback;
		void* deviceUserptr;

		void* pThis;				// ָ��EasyRtcDevice

		__WEBRTC_DEVICE_T()
		{
			audioCodecID = 0;
			videoCodecID = 0;
			memset(strUUID, 0x00, sizeof(strUUID));
			enableLocalService = false;
			webrtcPeerMap.clear();

			checkThread = NULL;

			dataCallback = NULL;
			dataChannelUserptr = NULL;
			deviceCallback = NULL;
			deviceUserptr = NULL;
		};

	}WEBRTC_DEVICE_T;

public:
    EasyRtcDevice();
    ~EasyRtcDevice();

	// ��ʼ
	int		Start(int videoCodecID, int audioCodecID, const char* uuid, bool enableLocalService, EasyRTC_Data_Callback callback, void* userptr);
	// ֹͣ
	int		Stop();

	// �����¼��ص�, �����ڻص��������־
	int		SetCallback(EasyRTC_Device_Callback callback, void* userptr);

	// ������Ƶ֡
	int		SendVideoFrame(char *framedata, const int framesize, bool keyframe, uint64_t pts/*ʱ�����λ��:����*/);
	// ������Ƶ֡
	int		SendAudioFrame(char* framedata, const int framesize, uint64_t pts/*ʱ�����λ��:����*/);

	// �����Զ�������
	int		SendData(WEBRTC_PEER_T* webrtcPeer, BOOL isBinary, PBYTE msgData, UINT32 msgLen);


	//=======================================================
	// ���²���Ϊ�ڲ�����
public:		
	void	CheckWebRtcPeerQueue();			// ���webrtcPeerMap�Ƿ񳬹����ֵ
	void	AddWebRtcPeer2Map(WEBRTC_PEER_T *peer);		// ���WEBRTC_PEER_T���б�
	void	CheckWebRtcPeerConnection();	// �������״̬

	static int		__Print__(void *userptr, const char* functionName, const int lineNum, bool callback, const char* szFormat, ...);

	void	CallbackLog(const char *pbuf, int bufsize);

	void	LockDevice(const char* functionName, const int lineNum);
	void	UnlockDevice(const char* functionName, const int lineNum);
	WEBRTC_DEVICE_T* GetDevice() { return &webrtcDevice; }

	void	LockLog(const char* functionName, const int lineNum);
	void	UnlockLog(const char* functionName, const int lineNum);
private:

	WEBRTC_DEVICE_T	webrtcDevice;
	OSMutex		mutex;
};

