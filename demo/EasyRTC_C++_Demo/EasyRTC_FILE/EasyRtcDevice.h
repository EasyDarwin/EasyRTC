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
#define MAX_WEBRTC_PEERS_COUNT 8	//8 //不能大于128,因为sdk里面做了这个限制
#else
#define MAX_WEBRTC_PEERS_COUNT 1	//8 //不能大于128,因为sdk里面做了这个限制
#endif

// DataChannel 数据回调
typedef int (*EasyRTC_DataChannel_Callback)(void* userptr, void* webrtcPeer, BOOL isBinary, PBYTE msgData, UINT32 msgLen);

// 状态信息回调
typedef int (*EasyRTC_Device_Callback)(void* userptr, const char *pbuf, const int bufsize);

class EasyRtcDevice
{
public:
	typedef struct __WEBRTC_PEER_T
	{
		time_t		createTime;		// 创建时间
		int connected;
		int disconnected;
		int noneedsI;

		PRtcPeerConnection peer_;
		PRtcDataChannel dc_;

		PRtcRtpTransceiver videoTransceiver_;
		PRtcRtpTransceiver audioTransceiver_;
		void* userptr;		// 指向WEBRTC_DEVICE_T
	}WEBRTC_PEER_T;

	typedef map<uint64_t, WEBRTC_PEER_T*>		WEBRTC_PEER_MAP;
	typedef struct __WEBRTC_DEVICE_T
	{
		int audioCodecID;
		int videoCodecID;
		char strUUID[128];
		bool enableLocalService;
		WEBRTC_PEER_MAP	webrtcPeerMap;
		OSTHREAD_OBJ_T* checkThread;		// 检查连接线程
		EasyRTC_DataChannel_Callback	dataChannelCallback;
		void* dataChannelUserptr;

		EasyRTC_Device_Callback deviceCallback;
		void* deviceUserptr;

		void* pThis;				// 指向EasyRtcDevice

		__WEBRTC_DEVICE_T()
		{
			audioCodecID = 0;
			videoCodecID = 0;
			memset(strUUID, 0x00, sizeof(strUUID));
			enableLocalService = false;
			webrtcPeerMap.clear();

			checkThread = NULL;

			dataChannelCallback = NULL;
			dataChannelUserptr = NULL;
			deviceCallback = NULL;
			deviceUserptr = NULL;
		};

	}WEBRTC_DEVICE_T;

public:
    EasyRtcDevice();
    ~EasyRtcDevice();

	// 开始
	int		Start(int videoCodecID, int audioCodecID, const char* uuid, bool enableLocalService, EasyRTC_DataChannel_Callback callback, void* userptr);
	// 停止
	int		Stop();

	// 设置事件回调, 现用于回调输出的日志
	int		SetCallback(EasyRTC_Device_Callback callback, void* userptr);

	// 发送视频帧
	int		SendVideoFrame(char *framedata, const int framesize, bool keyframe, uint64_t pts/*时间戳单位是:毫秒*/);
	// 发送音频帧
	int		SendAudioFrame(char* framedata, const int framesize, uint64_t pts/*时间戳单位是:毫秒*/);

	// 发送自定义数据
	int		SendData(WEBRTC_PEER_T* webrtcPeer, BOOL isBinary, PBYTE msgData, UINT32 msgLen);


	//=======================================================
	// 以下部分为内部调用
public:		
	void	CheckWebRtcPeerQueue();			// 检查webrtcPeerMap是否超过最大值
	void	AddWebRtcPeer2Map(WEBRTC_PEER_T *peer);		// 添加WEBRTC_PEER_T到列表
	void	CheckWebRtcPeerConnection();	// 检查连接状态

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

