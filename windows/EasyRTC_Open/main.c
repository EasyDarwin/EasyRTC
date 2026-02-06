
#ifdef _DEBUG
#include <vld.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "EasyRTCDeviceAPI.h"
#include "EasyRTCAPI.h"

#define SIGNALING_SERVER_NAME "rts.easyrtc.cn"
//#define SIGNALING_SERVER_NAME "127.0.0.1"
#define SIGNALING_SERVER_PORT 19000
#define SIGNALING_SERVER_ISSECURE   0



typedef struct __EASYRTC_PEER_T
{
	EASYRTC_HANDLE  handle;

}EASYRTC_PEER_T;

int __EasyRTC_Data_Callback(void* userptr, const char* peerUUID, EASYRTC_DATA_TYPE_ENUM_E dataType, int codecID, int isBinary, char* data, int size, int keyframe, unsigned long long pts)
{
	EASYRTC_PEER_T* peer = (EASYRTC_PEER_T*)userptr;

	if (EASYRTC_CALLBACK_TYPE_DNS_FAIL == dataType)
	{
		printf("DNS failed.\n");
	}
	else if (EASYRTC_CALLBACK_TYPE_CONNECTING == dataType)
	{
		printf("Connecting...\n");
	}
	else if (EASYRTC_CALLBACK_TYPE_CONNECTED == dataType)
	{
		printf("Connected.\n");
	}
	else if (EASYRTC_CALLBACK_TYPE_CONNECT_FAIL == dataType)
	{
		printf("Connect failed..\n");
	}
	else if (EASYRTC_CALLBACK_TYPE_DISCONNECT == dataType)
	{
		printf("Disconnect..\n");
	}

	else if (EASYRTC_CALLBACK_TYPE_PASSIVE_CALL == dataType)
	{
		printf("Passive call..  peerUUID[%s]\n", peerUUID);

		return 1;		// 返回1表示自动接受	如果返回0,则需要调用EasyRTC_Device_PassiveCallResponse来处理该请求: 接受或拒绝
	}

	else if (EASYRTC_CALLBACK_TYPE_START_VIDEO == dataType)
	{
		printf("Start Video..  peerUUID[%s]\n", peerUUID);

		// 此时有用户请求发送视频
	}
	else if (EASYRTC_CALLBACK_TYPE_START_AUDIO == dataType)
	{
		printf("Start Audio.. peerUUID[%s]\n", peerUUID);
	}
	else if (EASYRTC_CALLBACK_TYPE_STOP_VIDEO == dataType)
	{
		printf("Stop Video..  peerUUID[%s]\n", peerUUID);

		// 此时用户已关闭视频，停止发送
	}
	else if (EASYRTC_CALLBACK_TYPE_STOP_AUDIO == dataType)
	{
		printf("Stop Audio..\n");
	}
	else if (EASYRTC_CALLBACK_TYPE_PEER_VIDEO == dataType)
	{
#ifndef _DEBUG
		printf("OnPeerVideo..\n");
#endif

		static FILE* f = NULL;
		//if (NULL == f && keyframe == 0x01)	f = fopen("1recv.h264", "wb");
		if (NULL == f)	f = fopen("1recv.h264", "wb");
		if (f)
		{
			fwrite(data, 1, size, f);
		}
	}
	else if (EASYRTC_CALLBACK_TYPE_PEER_AUDIO == dataType)
	{
#ifndef _DEBUG
		printf("OnPeerAudio..\n");
#endif

		static FILE* f = NULL;
		if (NULL == f)	f = fopen("1recv.pcm", "wb");
		if (f)
		{
			fwrite(data, 1, size, f);
		}
	}
	else if (EASYRTC_CALLBACK_TYPE_LOCAL_AUDIO == dataType)
	{
		printf("Local audio..\n");
	}
	else if (EASYRTC_CALLBACK_TYPE_PEER_CONNECTED == dataType)
	{
		printf("Peer Connected..  [%s]\n", codecID == 1 ? "P2P" : "TURN");
	}
	else if (EASYRTC_CALLBACK_TYPE_PEER_CONNECT_FAIL == dataType)
	{
		printf("Peer Connect failed..\n");
	}
	else if (EASYRTC_CALLBACK_TYPE_PEER_DISCONNECT == dataType)
	{
		printf("Peer Disconnect..\n");
	}
	else if (EASYRTC_CALLBACK_TYPE_PEER_CLOSED == dataType)
	{
		printf("Peer Close..\n");
	}
	else if (EASYRTC_CALLBACK_TYPE_ONLINE_DEVICE == dataType)
	{
		printf("==============Device: %s\n", peerUUID);

	}

	return 0;
}

int main()
{

    EasyRTC_Device_Init();


    const char* local_uuid = "92092eea-be8d-4ec4-8ac5-123456789012";

	EASYRTC_PEER_T	easyRTC_Peer;
	memset(&easyRTC_Peer, 0x00, sizeof(EASYRTC_PEER_T));

    //EasyRTC_Device_Create(&easyRTC_Peer.handle, SIGNALING_SERVER_NAME, SIGNALING_SERVER_PORT, SIGNALING_SERVER_ISSECURE, local_uuid, __EasyRTC_Data_Callback, (void*)&easyRTC_Peer);
    //EasyRTC_Device_SetChannelInfo(easyRTC_Peer.handle, EasyRTC_CODEC_H264, EasyRTC_CODEC_MULAW);


	EasyRTC_Device_Create(&easyRTC_Peer.handle, SIGNALING_SERVER_NAME, SIGNALING_SERVER_PORT, SIGNALING_SERVER_ISSECURE, NULL, __EasyRTC_Data_Callback, (void*)&easyRTC_Peer);


	const char* peer_uuid = "92092eea-be8d-4ec4-8ac5-123456789001";
	//const char* peer_uuid = "62D37137-9669-48FB-A5A1-904215A35D86";		// 公路
	//const char* peer_uuid = "A48F1C8D-CF04-4AEF-9A51-D7E63B02B697";
	EasyRTC_Caller_Connect(easyRTC_Peer.handle, peer_uuid);

	//getchar();
	//EasyRTC_GetOnlineDevices(easyRTC_Peer.handle, __EasyRTC_Data_Callback, (void*)&easyRTC_Peer);

    getchar();

	RTC_Device_Stop(easyRTC_Peer.handle);
	RTC_Device_Release(&easyRTC_Peer.handle);

    EasyRTC_Device_Deinit();

    return 0;
}