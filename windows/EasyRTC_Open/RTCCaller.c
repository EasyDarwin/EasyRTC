#include "RTCDevice.h"
#include "g711.h"
#ifdef ANDROID
#include "jni/JNI_EasyRTCDevice.h"
#endif
#include "websocketClient.h"


int RTC_Caller_Connect(EASYRTC_DEVICE_T* pDevice, const char* peer_id)
{
	if (NULL == pDevice)	return -1;
	//if (NULL == peer_id)	return -1;

	if (NULL != peer_id)		// 外部调用时peer_id不为空, 所以仅赋值给pDevice->peer_id
	{
		int id_len = (int)strlen(peer_id);
		if (id_len != 36)		return -1;

		strcpy(pDevice->peer_id, peer_id);
		return 0;
	}

	// 内部调用
	if (0 == strcmp(pDevice->peer_id, "\0"))	return 0;

	char hisidOut[64] = { 0 };
	trim((char*)pDevice->peer_id, hisidOut);

	uint32_t hisids[4] = { 0 };
	if (GetUUIDSFromString(hisidOut, hisids) != 0) return -3;

	int len = sizeof(REQ_CONNECTUSER_INFO);
	REQ_CONNECTUSER_INFO* reqInfo = (REQ_CONNECTUSER_INFO*)malloc(len);
	if (NULL == reqInfo)		return EASYRTC_ERROR_NOT_ENOUGH_MEMORY;
	memset(reqInfo, 0x00, sizeof(REQ_CONNECTUSER_INFO));
	reqInfo->length = len;
	reqInfo->msgtype = HP_REQCONNECTUSER_INFO;
	reqInfo->hisid[0] = hisids[0];
	reqInfo->hisid[1] = hisids[1];
	reqInfo->hisid[2] = hisids[2];
	reqInfo->hisid[3] = hisids[3];

#ifdef _DEBUG
	FILE* f = fopen("caller.txt", "wb");
	if (f)
	{
		fwrite(reqInfo, 1, reqInfo->length, f);
		fclose(f);
	}
#endif

	websocketSendData(pDevice->websocket, WS_OPCODE_BIN,1, (char*)reqInfo, reqInfo->length);
	free(reqInfo);

    return 0;
}

// 连接局域网设备
int RTC_Lan_Connect(EASYRTC_DEVICE_T* pDevice, const char* peerIP, const int peerPort)
{
	if (NULL == pDevice)	return -1;
	if (NULL == peerIP)		return -1;
	if (peerPort < 1 || peerPort > 65535)	return -1;

	char hisidOut[64] = { 0 };
	strcpy(pDevice->peer_id, "00000000-0000-0000-0000-000000000000");
	trim((char*)pDevice->peer_id, hisidOut);

	uint32_t hisids[4] = { 0 };
	if (GetUUIDSFromString(hisidOut, hisids) != 0) return -3;

	int len = sizeof(REQ_GETWEBRTCOFFER_INFO);
	REQ_GETWEBRTCOFFER_INFO* reqInfo = (REQ_GETWEBRTCOFFER_INFO*)malloc(len);
	if (NULL == reqInfo)		return EASYRTC_ERROR_NOT_ENOUGH_MEMORY;
	memset(reqInfo, 0x00, sizeof(REQ_GETWEBRTCOFFER_INFO));
	reqInfo->length = len;
	reqInfo->msgtype = HP_REQGETWEBRTCOFFER_INFO;
	reqInfo->hisid[0] = hisids[0];
	reqInfo->hisid[1] = hisids[1];
	reqInfo->hisid[2] = hisids[2];
	reqInfo->hisid[3] = hisids[3];

#ifdef _DEBUG
	FILE* f = fopen("caller.txt", "wb");
	if (f)
	{
		fwrite(reqInfo, 1, reqInfo->length, f);
		fclose(f);
	}
#endif

	websocketSendData(pDevice->websocket, WS_OPCODE_BIN, 1, (char*)reqInfo, reqInfo->length);
	free(reqInfo);

	return 0;

}
