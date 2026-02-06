#include "RTCDevice.h"
#include "g711.h"
#ifdef ANDROID
#include "jni/JNI_EasyRTCDevice.h"
#endif
#include "websocketClient.h"

#ifdef _WIN32
DWORD WINAPI __EasyRTC_Worker_Thread(void* lpParam);
#else
void* __EasyRTC_Worker_Thread(void* lpParam);
#endif


EASYRTC_DEVICE_T* RTC_Device_Create(const char* serverAddr, const int serverPort, const int isSecure)
{
	EASYRTC_DEVICE_T* pDevice = (EASYRTC_DEVICE_T*)malloc(sizeof(EASYRTC_DEVICE_T));

	do
	{
		if (NULL == pDevice)	break;

		memset(pDevice, 0x00, sizeof(EASYRTC_DEVICE_T));

		pDevice->peerMutex = (OSMutex*)malloc(sizeof(OSMutex));
		if (NULL != pDevice->peerMutex)
		{
			InitMutex(pDevice->peerMutex);
		}

		strncpy(pDevice->serverAddr, serverAddr, sizeof(pDevice->serverAddr) - 1);
		pDevice->serverAddr[sizeof(pDevice->serverAddr) - 1] = '\0';
		pDevice->serverPort = serverPort;
		pDevice->isSecure = isSecure;

	} while (0);

	return pDevice;
}

int RTC_Device_Release(EASYRTC_DEVICE_T** ppDevice)
{
	EASYRTC_DEVICE_T* pDevice = *ppDevice;
	if (NULL == ppDevice)		return 0;
	if (NULL == pDevice)		return 0;
	RTC_Device_Stop(pDevice);

	if (NULL != pDevice->peerMutex)
	{
		DeinitMutex(pDevice->peerMutex);
		free(pDevice->peerMutex);
		pDevice->peerMutex = NULL;
	}

	free(pDevice);
	*ppDevice = NULL;

	return 0;
}

// EasyRTC底层库的回调, 第二个参数为CONNECT_STATUS_E, 此处换成EASYRTC_DATA_TYPE_ENUM_E
// 表示EASYRTC_DATA_TYPE_ENUM_E中的前5个连接状态必须和底层库的CONNECT_STATUS_E保持一致
int __ws_connect_callback(void* userptr, EASYRTC_DATA_TYPE_ENUM_E status)
{
	EASYRTC_DEVICE_T* pDevice = (EASYRTC_DEVICE_T*)userptr;

	CallbackData(pDevice, NULL, status, 0, 0, NULL, 0, 0, 0);

	return 0;
}

// 
int __ws_register_callback(void* userptr)
{
	EASYRTC_DEVICE_T* pDevice = (EASYRTC_DEVICE_T*)userptr;

	return SendRegister(pDevice);
}
int __ws_data_callback(void* userptr, char* data, int size)
{
	EASYRTC_DEVICE_T* pDevice = (EASYRTC_DEVICE_T*)userptr;

	int errCode = 0;
	WebsocketDataHandler(pDevice, (unsigned char*)data, size, &errCode);

	return errCode;
}
int __ws_idle_callback(void* userptr)
{
	EASYRTC_DEVICE_T* pDevice = (EASYRTC_DEVICE_T*)userptr;

	// 检查是否有空闲的对象需删除

	time_t currTime = time(NULL);
	if (currTime - pDevice->lastCheckTime < 2)
	{
		return 0;
	}

	pDevice->lastCheckTime = currTime;

	LockPeerList(pDevice, __FUNCTION__, __LINE__);			// Lock

	EASYRTC_PEER_T* current = pDevice->peerList;
	while (current != NULL)
	{
		bool isDisconnect = false;

		if ((NULL!=current->peerConnection[EASYRTC_PEER_PUBLISHER].peer_) &&
			((current->peerConnection[EASYRTC_PEER_PUBLISHER].connected == 0 || current->peerConnection[EASYRTC_PEER_PUBLISHER].disconnected == 1) &&
				(currTime - current->createTime >= 30)))
		{
			isDisconnect = true;
		}

		if ((NULL != current->peerConnection[EASYRTC_PEER_SUBSCRIBER].peer_) &&
			((current->peerConnection[EASYRTC_PEER_SUBSCRIBER].connected == 0 || current->peerConnection[EASYRTC_PEER_SUBSCRIBER].disconnected == 1) &&
				(currTime - current->createTime >= 30)))
		{
			isDisconnect = true;
		}


		if (((current->operateType != OPERATE_TYPE_WORKING) && (currTime - current->createTime >= 30)) ||
			(current->operateType == OPERATE_TYPE_DELETE) ||
			(isDisconnect))
		{
			// 此处释放相应对象
			EasyRTC_ReleasePeer(current);

			// 从队列中删除
			deletePeerByUUID(&pDevice->peerList, current->uuid);

			current = pDevice->peerList;
		}
		else
		{
			current = current->next;
		}
	}

	UnlockPeerList(pDevice, __FUNCTION__, __LINE__);			// Unlock

	return 0;
}


int RTC_Device_Start(EASYRTC_DEVICE_T* pDevice, const char* local_id, EasyRTC_Data_Callback callback, void* userptr)
{
	if (NULL == pDevice->websocket)
	{
		if (NULL != local_id)
		{
			memset(pDevice->local_id, 0x00, sizeof(pDevice->local_id));
			strcpy(pDevice->local_id, local_id);
		}

		memset(&pDevice->channelInfo, 0x00, sizeof(CHANNEL_INFO_T));


#if defined(SUPPORT_WEBRTC_AEC)
		WebRtcAecm_Create(&pDevice->channelInfo.aecmInstHandler);
		WebRtcAecm_Init(pDevice->channelInfo.aecmInstHandler, 8000);

		AecmConfig aecmConfig;
		aecmConfig.cngMode = AecmTrue;
		aecmConfig.echoMode = 4; //3; //4;
		WebRtcAecm_set_config(pDevice->channelInfo.aecmInstHandler, aecmConfig);
#endif

		pDevice->dataCallback = callback;
		pDevice->dataUserptr = userptr;

		websocketCreate(pDevice->serverAddr, pDevice->serverPort, pDevice->isSecure, (void*)pDevice, __ws_connect_callback, __ws_register_callback, __ws_data_callback, __ws_idle_callback, &pDevice->websocket);
	}

	return 0;
}

int RTC_Device_SetChannelInfo(EASYRTC_DEVICE_T* pDevice, EASYRTC_CODEC videoCodecID, EASYRTC_CODEC audioCodecID)
{
	if (NULL == pDevice)		return -1;

	pDevice->channelInfo.videoCodecID = videoCodecID;
	pDevice->channelInfo.audioCodecID = audioCodecID;

	return 0;
}


int __EasyRTC_IceCandidate_Callback(void* userPtr, const int isOffer, const char* sdp)
{
	EASY_PEER_CONNECTION_T* peerConnection = (EASY_PEER_CONNECTION_T*)userPtr;
	EASYRTC_PEER_T* peer = (EASYRTC_PEER_T*)peerConnection->peerPtr;
	EASYRTC_DEVICE_T* pDevice = (EASYRTC_DEVICE_T*)peer->devicePtr;

	if (NULL == pDevice)	return 0;

	int ret = 0;

	if (isOffer == 1)
	{
		int offersdplen = (int)strlen(sdp);
		if (offersdplen == 0) return 0;
		int iLength = sizeof(NTI_WEBRTCOFFER_INFO) + offersdplen + 1;
		NTI_WEBRTCOFFER_INFO* pInfo = (NTI_WEBRTCOFFER_INFO*)malloc(iLength);
		if (NULL == pInfo)		return EASYRTC_ERROR_NOT_ENOUGH_MEMORY;
		memset(pInfo, 0, iLength);
		pInfo->length = iLength;
		pInfo->msgtype = HP_NTIWEBRTCOFFER_INFO;
		pInfo->sdplen = offersdplen;
		//pInfo->hisid[0] = pDevice->local_uuid[0];
		//pInfo->hisid[1] = pDevice->local_uuid[1];
		//pInfo->hisid[2] = pDevice->local_uuid[2];
		//pInfo->hisid[3] = pDevice->local_uuid[3];

		pInfo->hisid[0] = peer->caller_id[0];
		pInfo->hisid[1] = peer->caller_id[1];
		pInfo->hisid[2] = peer->caller_id[2];
		pInfo->hisid[3] = peer->caller_id[3];

		strcpy(pInfo->offersdp, sdp);
		ret = websocketSendData(pDevice->websocket, WS_OPCODE_BIN, (char*)pInfo, iLength);

		free(pInfo);
	}
	else
	{
		int offersdplen = (int)strlen(sdp);
		if (offersdplen == 0) return 0;
		int iLength = sizeof(NTI_WEBRTCANSWER_INFO) + offersdplen + 1;
		NTI_WEBRTCANSWER_INFO* pInfo = (NTI_WEBRTCANSWER_INFO*)malloc(iLength);
		if (NULL == pInfo)        return EASYRTC_ERROR_NOT_ENOUGH_MEMORY;

		memset(pInfo, 0, iLength);
		pInfo->length = iLength;
		pInfo->msgtype = HP_NTIWEBRTCANSWER_INFO;
		pInfo->sdplen = offersdplen;
		strcpy(pInfo->answersdp, sdp);

		pInfo->hisid[0] = peer->hisid[0];
		pInfo->hisid[1] = peer->hisid[1];
		pInfo->hisid[2] = peer->hisid[2];
		pInfo->hisid[3] = peer->hisid[3];

		ret = websocketSendData(pDevice->websocket, WS_OPCODE_BIN, (char*)pInfo, iLength);

		free(pInfo);
	}

	return 0;
}

int RTC_Device_PassiveCallResponse(EASYRTC_DEVICE_T* pDevice, const char* peer_id, const int decline)
{
	int ret = -1;
	EASYRTC_PEER_T* peer = NULL;

	if (NULL == pDevice)		return -1;
	if (NULL == peer_id)		return -1;
	if (0 == strcmp(peer_id, "\0"))	return -1;


	LockPeerList(pDevice, __FUNCTION__, __LINE__);			// Lock
	peer = findPeerByUUID(pDevice->peerList, peer_id);

	do
	{
		if (NULL == peer)			// 两种情况: 1.传入的peerUUID错误  2. 响应时距离请求时间已超过30秒,导致已被自动删除
		{
			ret = -100;
			break;
		}

		if (decline)		// 如果拒绝
		{
			break;
		}


		unsigned int mediaType = EASYRTC_MDIA_TYPE_VIDEO | EASYRTC_MDIA_TYPE_AUDIO;
		EasyRTC_RTP_TRANSCEIVER_DIRECTION direction = EasyRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;

		pDevice->channelInfo.mediaType = mediaType;
		pDevice->channelInfo.direction = direction;

		EasyRTC_CreatePeer(pDevice, peer, EASYRTC_PEER_PUBLISHER, NULL);
		ret = EasyRTC_CreateOffer(peer->peerConnection[EASYRTC_PEER_PUBLISHER].peer_, __EasyRTC_IceCandidate_Callback, &peer->peerConnection[EASYRTC_PEER_PUBLISHER]);
		if (ret == 0)
		{
			peer->operateType = OPERATE_TYPE_WORKING;
		}

	} while (0);
	UnlockPeerList(pDevice, __FUNCTION__, __LINE__);			// Unlock

	return ret;
}

int RTC_Device_SendVideoFrame(EASYRTC_DEVICE_T* pDevice, char* framedata, const int framesize, bool keyframe, unsigned long long pts/*时间戳单位是:毫秒*/)
{
	if (NULL == framedata)		return -1;
	if (framesize < 1)			return -1;


	EasyRTC_Frame frame = { 0 };
	frame.frameData = (PBYTE)framedata;
	frame.size = framesize;
	frame.presentationTs = pts * 10000;
	if (keyframe)	frame.flags = EASYRTC_FRAME_FLAG_KEY_FRAME;

	LockPeerList(pDevice, __FUNCTION__, __LINE__);			// Lock

	EASYRTC_PEER_T* current = pDevice->peerList;
	while (current != NULL)
	{
		if (current->operateType == OPERATE_TYPE_DELETE)		break;

		LockPeer(current, __FUNCTION__, __LINE__);

		if ((NULL != current->peerConnection[EASYRTC_PEER_PUBLISHER].videoTransceiver_) &&
			(current->peerConnection[EASYRTC_PEER_PUBLISHER].connected == 1))
		{
			EasyRTC_SendFrame(current->peerConnection[EASYRTC_PEER_PUBLISHER].videoTransceiver_, &frame);
		}

		if ((NULL != current->peerConnection[EASYRTC_PEER_SUBSCRIBER].videoTransceiver_) &&
			(current->peerConnection[EASYRTC_PEER_SUBSCRIBER].connected == 1))
		{
			EasyRTC_SendFrame(current->peerConnection[EASYRTC_PEER_SUBSCRIBER].videoTransceiver_, &frame);
		}

		UnlockPeer(current, __FUNCTION__, __LINE__);

		current = current->next;
	}

	UnlockPeerList(pDevice, __FUNCTION__, __LINE__);			// Unlock

	return 0;
}

int RTC_Device_SendAudioFrame(EASYRTC_DEVICE_T* pDevice, char* framedata, const int framesize, unsigned long long pts/*时间戳单位是:毫秒*/)
{
	if (NULL == framedata)		return -1;
	if (framesize < 1)			return -1;

	EasyRTC_Frame frame = { 0 };
	frame.frameData = (PBYTE)framedata;
	frame.size = framesize;
	frame.presentationTs = pts * 10000;

	LockPeerList(pDevice, __FUNCTION__, __LINE__);			// Lock


	EASYRTC_PEER_T* current = pDevice->peerList;
	while (current != NULL)
	{
		if (current->operateType == OPERATE_TYPE_DELETE)		break;

		LockPeer(current, __FUNCTION__, __LINE__);
		if ((NULL != current->peerConnection[EASYRTC_PEER_PUBLISHER].audioTransceiver_) &&
			(current->peerConnection[EASYRTC_PEER_PUBLISHER].connected == 1))
		{
			EasyRTC_SendFrame(current->peerConnection[EASYRTC_PEER_PUBLISHER].audioTransceiver_, &frame);
		}

		if ((NULL != current->peerConnection[EASYRTC_PEER_SUBSCRIBER].audioTransceiver_) &&
			(current->peerConnection[EASYRTC_PEER_SUBSCRIBER].connected == 1))
		{
			EasyRTC_SendFrame(current->peerConnection[EASYRTC_PEER_SUBSCRIBER].audioTransceiver_, &frame);
		}

		UnlockPeer(current, __FUNCTION__, __LINE__);

		current = current->next;
	}


	UnlockPeerList(pDevice, __FUNCTION__, __LINE__);			// Unlock

	return 0;
}

int RTC_Device_SendCustomData(EASYRTC_DEVICE_T* pDevice, const char* peerUUID, const int isBinary, const char* data, const int size)
{
	if (NULL == data)		return -1;
	if (size < 1)			return -1;

	LockPeerList(pDevice, __FUNCTION__, __LINE__);			// Lock

	EASYRTC_PEER_T* current = pDevice->peerList;
	while (current != NULL)
	{
		if (current->operateType == OPERATE_TYPE_DELETE)		break;

		LockPeer(current, __FUNCTION__, __LINE__);

		if ((NULL != current->peerConnection[EASYRTC_PEER_PUBLISHER].dc_) &&
			(current->peerConnection[EASYRTC_PEER_PUBLISHER].connected == 1) &&
			((NULL == peerUUID) || (0 == strcmp(peerUUID, "\0")) || (0 == strcmp(peerUUID, current->uuid))))
		{
			EasyRTC_DataChannelSend(current->peerConnection[EASYRTC_PEER_PUBLISHER].dc_, isBinary, (PBYTE)data, (UINT32)size);
		}

		if ((NULL != current->peerConnection[EASYRTC_PEER_SUBSCRIBER].dc_) &&
			(current->peerConnection[EASYRTC_PEER_SUBSCRIBER].connected == 1) &&
			((NULL == peerUUID) || (0 == strcmp(peerUUID, "\0")) || (0 == strcmp(peerUUID, current->uuid))))
		{
			EasyRTC_DataChannelSend(current->peerConnection[EASYRTC_PEER_SUBSCRIBER].dc_, isBinary, (PBYTE)data, (UINT32)size);
		}

		UnlockPeer(current, __FUNCTION__, __LINE__);

		current = current->next;
	}

	UnlockPeerList(pDevice, __FUNCTION__, __LINE__);			// Unlock

	return 0;
}


int RTC_Device_Hangup(EASYRTC_DEVICE_T* pDevice, const char* peerUUID)
{
	if (NULL == pDevice)		return -1;
	if (NULL == peerUUID)		return -1;
	if (0 == strcmp(peerUUID, "\0"))	return -1;

	int ret = -2;

	LockPeerList(pDevice, __FUNCTION__, __LINE__);			// Lock

	EASYRTC_PEER_T* peer = findPeerByUUID(pDevice->peerList, peerUUID);

	if (NULL!=peer)
	{
		// 此处释放相应对象
		EasyRTC_ReleasePeer(peer);

		// 从队列中删除
		deletePeerByUUID(&pDevice->peerList, peerUUID);

		ret = 0;

	}

	UnlockPeerList(pDevice, __FUNCTION__, __LINE__);			// Unlock

	return ret;
}

int RTC_Device_aecm_process(EASYRTC_DEVICE_T* pDevice, short* nearendNoisy, short* outPcmData, int pcmSize)
{
	int ret = -1;
	if (NULL == nearendNoisy)		return -1;
	if (NULL == outPcmData)			return -1;

#if defined(SUPPORT_WEBRTC_AEC)

	char uuid[64] = { 0 };
	CHANNEL_INFO_T* pChannel = &pDevice->channelInfo;

	if (NULL != pChannel->aecmInstHandler)
	{
		LockPeerList(pDevice, __FUNCTION__, __LINE__);			// Lock

		EASYRTC_PEER_T* current = pDevice->peerList;
		while (current != NULL)
		{
			if (current->channelId == channelId)
			{
				strcpy(uuid, current->uuid);

				do
				{
					//回声消除  可以试试30和40,看哪一个值消除回声效果更好
					if (WebRtcAecm_Process(pChannel->aecmInstHandler, (int16_t*)nearendNoisy, NULL, (int16_t*)outPcmData, pcmSize, 40) < 0)
					{
						break;
					}

					ret = 0;

				} while (0);
			}

			current = current->next;
		}


		UnlockPeerList(pDevice, __FUNCTION__, __LINE__);			// Unlock

		if (ret == 0)		// 处理成功
		{
			// 因webrtc交互时会协商一个双方都支持的格式,所以此处对方也是回调相同的编码格式数据, 故此处直接填写pDevice->audioCodecID
			CallbackData(pDevice, uuid, EASYRTC_CALLBACK_TYPE_LOCAL_AUDIO, 65536/*PCM*/, 1, (char*)outPcmData, pcmSize * 2, 0, 0);

			//(void* userptr, const char* peerUUID, EASYRTC_DATA_TYPE_ENUM_E dataType, int codecID, int isBinary, char* data, int size, int keyframe, unsigned long long pts);
			//pDevice->dataCallback(pDevice->dataChannelUserptr, webrtcpeer, EASYRTC_CALLBACK_TYPE_AUDIO, pDevice->audioCodecID, true, (char*)pcmBuffer, (int)frame->size * 2);
			//webrtcDevice.dataCallback(webrtcDevice.dataChannelUserptr, NULL, EASYRTC_CALLBACK_TYPE_LOCAL_AUDIO, 65536/*PCM*/, true, (char*)outPcmData, pcmSize * 2);
		}

	}
#endif

	return ret;
}

int RTC_Device_Stop(EASYRTC_DEVICE_T* pDevice)
{
	if (NULL == pDevice)		return 0;

	websocketRelease(&pDevice->websocket);
	ReleasePeerList(pDevice);

#if defined(SUPPORT_WEBRTC_AEC)
	WebRtcAecm_Free(pDevice->channelInfo.aecmInstHandler);
	pDevice->channelInfo.aecmInstHandler = NULL;
#endif
	BUFF_FREE(&pDevice->channelInfo.bufAudioFrame);

    return 0;
}

int ReleasePeerList(EASYRTC_DEVICE_T* pDevice)
{
	LockPeerList(pDevice, __FUNCTION__, __LINE__);			// Lock

	if (NULL != pDevice->peerList)
	{
		EASYRTC_PEER_T* current = pDevice->peerList;
		while (current != NULL)
		{
			EasyRTC_ReleasePeer(current);

			current = current->next;
		}

		freePeerList(&pDevice->peerList);
	}

	UnlockPeerList(pDevice, __FUNCTION__, __LINE__);			// Unlock

	return 0;
}

int LockPeerList(EASYRTC_DEVICE_T* pDevice, const char* functionName, const int lineNum)
{
	if (NULL == pDevice)		return 0;
	if (NULL == pDevice->peerMutex)	return 0;
	
	//__Print__(pDevice, __FUNCTION__, __LINE__, false, "%s line[%d] LockMutex Begin.\n", functionName, lineNum);

	LockMutex(pDevice->peerMutex);

	//__Print__(pDevice, __FUNCTION__, __LINE__, false, "%s line[%d] LockMutex End.\n", functionName, lineNum);

	return 0;
}
int UnlockPeerList(EASYRTC_DEVICE_T* pDevice, const char* functionName, const int lineNum)
{
	if (NULL == pDevice)		return 0;
	if (NULL == pDevice->peerMutex)	return 0;

	//__Print__(pDevice, __FUNCTION__, __LINE__, false, "%s line[%d] UnlockMutex Begin.\n", functionName, lineNum);

	UnlockMutex(pDevice->peerMutex);

	//__Print__(pDevice, __FUNCTION__, __LINE__, false, "%s line[%d] UnlockMutex End.\n", functionName, lineNum);

	return 0;
}

void    ConvertLocalTime(struct tm* _tm, time_t* _tt)
{
#ifdef _WIN32
	localtime_s(_tm, _tt);
#else
	localtime_r(_tt, _tm);
#endif
}
int		__Print__(void* userptr, const char* functionName, const int lineNum, bool dataChannelCallback, const char* szFormat, ...)
{
	char szBuff[8192] = { 0 };

	char szTime[32] = { 0 };
	time_t tt = time(NULL);
	struct tm _timetmp;
	ConvertLocalTime(&_timetmp, &tt);
	strftime(szBuff, 32, "[%Y-%m-%d %H:%M:%S] ", &_timetmp);

	int offset = (int)strlen(szBuff);

#ifndef _MBCS
	va_list args;
	va_start(args, szFormat);
#ifdef _WIN32
	_vsnprintf(szBuff + offset, sizeof(szBuff) - offset - 1, szFormat, args);
#else
	vsnprintf(szBuff + offset, sizeof(szBuff) - offset - 1, szFormat, args);
#endif
	va_end(args);
#else
	va_list args;
	va_start(args, szFormat);
	vsnprintf(szBuff + offset, sizeof(szBuff) - offset - 1, szFormat, args);
	va_end(args);
#endif

#ifdef ANDROID

	char szLog[8192] = { 0 };
	sprintf(szLog, "%s line[%d]: %s\n", functionName, lineNum, szBuff);

	LOGD("%s", szLog);
#endif

	if (userptr)
	{
		//EasyRTCDevice* pThis = (EasyRTCDevice*)userptr;

		//pThis->CallbackLog(szBuff, (int)strlen(szBuff));
	}
#ifdef _DEBUG__
	printf("%s", szBuff);
#endif

	printf("%s", szBuff);

	return 0;
}


// 回调函数
int __EasyRTC_ConnectionStateChange_Callback(void* userPtr, EASYRTC_PEER_CONNECTION_STATE newState)
//VOID onConnectionStateChange(UINT64 customData, RTC_PEER_CONNECTION_STATE newState) 
{
	EASY_PEER_CONNECTION_T* peerConnection = (EASY_PEER_CONNECTION_T*)userPtr;
	EASYRTC_PEER_T* peer = (EASYRTC_PEER_T*)peerConnection->peerPtr;
	EASYRTC_DEVICE_T* pDevice = (EASYRTC_DEVICE_T*)peer->devicePtr;

	__Print__(NULL, __FUNCTION__, __LINE__, false, "%s: Connection state changed to: %d\n", 
		peerConnection->type == EASYRTC_PEER_PUBLISHER ? "PUBLISHER" : "SUBSCRIBER", newState);

	if (newState == EASYRTC_PEER_CONNECTION_STATE_CONNECTED)
	{
		__Print__(NULL, __FUNCTION__, __LINE__, false, "%s: RTC_PEER_CONNECTION_STATE_CONNECTED\n", 
			peerConnection->type == EASYRTC_PEER_PUBLISHER ? "PUBLISHER" : "SUBSCRIBER");

		// 获取Ice连接类型
		EasyRTC_IceCandidatePairStats rtcIceCandidatePairStats;
		memset(&rtcIceCandidatePairStats, 0x00, sizeof(EasyRTC_IceCandidatePairStats));
		EasyRTC_GetIceCandidatePairStats(peerConnection->peer_, &rtcIceCandidatePairStats);
		//printf("local ice type: %d\n", RtcIceCandidatePairStats.local_iceCandidateType);
		//printf("remote ice type: %d\n", RtcIceCandidatePairStats.remote_iceCandidateType);

		//printf("local ice port: %d\n", my_ntohs(RtcIceCandidatePairStats.local_ipAddress.port));
		//printf("remote ice port: %d\n", my_ntohs(RtcIceCandidatePairStats.remote_ipAddress.port));

		//printf("local ice address: %s\n", RtcIceCandidatePairStats.local_ipAddress.strAddress);
		//printf("remote ice address: %s\n", RtcIceCandidatePairStats.remote_ipAddress.strAddress);

		int iceCandidateType = 0;
		if ((rtcIceCandidatePairStats.local_iceCandidateType == EasyRTC_ICE_CANDIDATE_TYPE_RELAYED) || (rtcIceCandidatePairStats.remote_iceCandidateType == EasyRTC_ICE_CANDIDATE_TYPE_RELAYED))
		{
			//这是中转的方式建立起来的连接
			iceCandidateType = 2;
		}
		else
		{
			//这是直连的方式建立起来的连接
			iceCandidateType = 1;
		}

		peerConnection->connected = 1;
		peer->operateType = OPERATE_TYPE_WORKING;
		CallbackData(pDevice, peer->uuid, EASYRTC_CALLBACK_TYPE_PEER_CONNECTED, iceCandidateType, 0, NULL, 0, 0, 0);
	}
	else if (newState == EASYRTC_PEER_CONNECTION_STATE_FAILED)
	{
		peerConnection->connected = 0;
		peer->operateType = OPERATE_TYPE_WAITING;
		__Print__(NULL, __FUNCTION__, __LINE__, false, "%s: RTC_PEER_CONNECTION_STATE_FAILED\n", 
			peerConnection->type == EASYRTC_PEER_PUBLISHER ? "PUBLISHER" : "SUBSCRIBER");

		CallbackData(pDevice, peer->uuid, EASYRTC_CALLBACK_TYPE_PEER_CONNECT_FAIL, 0, 0, NULL, 0, 0, 0);
	}
	else if (newState == EASYRTC_PEER_CONNECTION_STATE_DISCONNECTED)
	{
		peerConnection->connected = 0;
		peer->operateType = OPERATE_TYPE_WAITING;
		// 回调: 通知上层停止发送视频和音频
		CallbackData(pDevice, peer->uuid, EASYRTC_CALLBACK_TYPE_STOP_VIDEO, 0, 0, NULL, 0, 0, 0);
		CallbackData(pDevice, peer->uuid, EASYRTC_CALLBACK_TYPE_STOP_AUDIO, 0, 0, NULL, 0, 0, 0);

		CallbackData(pDevice, peer->uuid, EASYRTC_CALLBACK_TYPE_PEER_DISCONNECT, 0, 0, NULL, 0, 0, 0);
	}
	else if (newState == EASYRTC_PEER_CONNECTION_STATE_CLOSED)
	{
		peerConnection->connected = 0;
		peer->operateType = OPERATE_TYPE_WAITING;

		// 回调: 通知上层停止发送视频和音频
		CallbackData(pDevice, peer->uuid, EASYRTC_CALLBACK_TYPE_STOP_VIDEO, 0, 0, NULL, 0, 0, 0);
		CallbackData(pDevice, peer->uuid, EASYRTC_CALLBACK_TYPE_STOP_AUDIO, 0, 0, NULL, 0, 0, 0);

		CallbackData(pDevice, peer->uuid, EASYRTC_CALLBACK_TYPE_PEER_CLOSED, 0, 0, NULL, 0, 0, 0);
	}
	else {
		peerConnection->connected = 0;
	}

	return 0;
}

void func_audioRtcOnFrame(void* customData, EasyRTC_CODEC codecID, EasyRTC_Frame* frame)
{
	EASY_PEER_CONNECTION_T* peerConnection = (EASY_PEER_CONNECTION_T*)customData;
	EASYRTC_PEER_T* peer = (EASYRTC_PEER_T*)peerConnection->peerPtr;
	EASYRTC_DEVICE_T* pDevice = (EASYRTC_DEVICE_T*)peer->devicePtr;

	CHANNEL_INFO_T* pChannel = (CHANNEL_INFO_T*)&pDevice->channelInfo;

	pChannel->audioCodecID = codecID;

	if (pChannel->audioCodecID == EASYRTC_CODEC_ALAW ||
		pChannel->audioCodecID == EASYRTC_CODEC_MULAW)
	{
		if (pChannel->bufAudioFrame.bufsize < (int)frame->size)
		{
			BUFF_MALLOC(&pChannel->bufAudioFrame, frame->size * 2 + 1024);
		}
		BUFF_ENQUEUE(&pChannel->bufAudioFrame, (char*)frame->frameData, frame->size);

		int procBytes = 0;
#define G711_DATA_SIZE		160
		while (procBytes < pChannel->bufAudioFrame.bufpos)
		{
			if (procBytes + G711_DATA_SIZE > pChannel->bufAudioFrame.bufpos)		break;

			int offset = 0;
			char pcmBuffer[320] = { 0 };
			for (int i = 0; i < G711_DATA_SIZE; i++) {
				unsigned short s = 0;
				if (pChannel->audioCodecID == EASYRTC_CODEC_ALAW)
				{
					s = g711_alaw2linear(pChannel->bufAudioFrame.pbuf[i + procBytes]);
				}
				else if (pChannel->audioCodecID == EASYRTC_CODEC_MULAW)
				{
					s = g711_ulaw2linear(pChannel->bufAudioFrame.pbuf[i + procBytes]);
				}
				pcmBuffer[(i * 2)] = s & 0xFF;
				pcmBuffer[(i * 2 + 1)] = (s >> 8) & 0xFF;
			}

#if defined(SUPPORT_WEBRTC_AEC)
			WebRtcAecm_BufferFarend(pChannel->aecmInstHandler, (int16_t*)pcmBuffer, G711_DATA_SIZE);
#endif
			if (NULL != pDevice->dataCallback)
			{
				// 因webrtc交互时会协商一个双方都支持的格式,所以此处对方也是回调相同的编码格式数据, 故此处直接填写pDevice->audioCodecID
				//pDevice->dataCallback(pDevice->dataChannelUserptr, webrtcpeer, EASYRTC_CALLBACK_TYPE_AUDIO, pDevice->audioCodecID, true, (char*)pcmBuffer, (int)frame->size * 2);
				//pDevice->dataCallback(pDevice->dataChannelUserptr, webrtcpeer->uuid, EASYRTC_CALLBACK_TYPE_PEER_AUDIO, 65536/*PCM*/, true, (char*)pcmBuffer, (int)G711_DATA_SIZE << 1);
				CallbackData(pDevice, peer->uuid, EASYRTC_CALLBACK_TYPE_PEER_AUDIO, 65536/*PCM*/, 1, (char*)pcmBuffer, (int)G711_DATA_SIZE << 1, 0, frame->presentationTs / 1000);
			}

			procBytes += G711_DATA_SIZE;
		}

		if (pChannel->bufAudioFrame.bufpos - procBytes > 0)
		{
			memmove(pChannel->bufAudioFrame.pbuf, pChannel->bufAudioFrame.pbuf + pChannel->bufAudioFrame.bufpos, pChannel->bufAudioFrame.bufpos - procBytes);
			pChannel->bufAudioFrame.bufpos -= procBytes;
		}
		else
		{
			pChannel->bufAudioFrame.bufpos = 0;
		}
	}
	else
	{
		CallbackData(pDevice, peer->uuid, EASYRTC_CALLBACK_TYPE_PEER_AUDIO, pChannel->audioCodecID, 1, (char*)frame->frameData, frame->size, 0, frame->presentationTs / 1000);
	}
}


int __EasyRTC_Device_Transceiver_Callback(void* userPtr, EASYRTC_TRANSCEIVER_CALLBACK_TYPE_E type, EasyRTC_CODEC codecID, EasyRTC_Frame* frame, double bandwidthEstimation)
{
	EASY_PEER_CONNECTION_T* peerConnection = (EASY_PEER_CONNECTION_T*)userPtr;
	EASYRTC_PEER_T* peer = (EASYRTC_PEER_T*)peerConnection->peerPtr;
	EASYRTC_DEVICE_T* pDevice = (EASYRTC_DEVICE_T*)peer->devicePtr;

	if (EASYRTC_TRANSCEIVER_CALLBACK_VIDEO_FRAME == type)
	{
		CallbackData(pDevice, peer->uuid, EASYRTC_CALLBACK_TYPE_PEER_VIDEO, codecID, 1, (char*)frame->frameData, frame->size, frame->flags, frame->presentationTs);
	}
	else if (EASYRTC_TRANSCEIVER_CALLBACK_AUDIO_FRAME == type)
	{
		func_audioRtcOnFrame(userPtr, codecID, frame);
	}
	else if (EASYRTC_TRANSCEIVER_CALLBACK_BANDWIDTH == type)
	{

	}
	else if (EASYRTC_TRANSCEIVER_CALLBACK_KEY_FRAME_REQ == type)
	{
		// 当前有丢帧, 现请求关键帧
	}

	return 0;
}


int __EasyRTC_Caller_Transceiver_Callback(void* userPtr, EASYRTC_TRANSCEIVER_CALLBACK_TYPE_E type, EasyRTC_CODEC codecID, EasyRTC_Frame* frame, double bandwidthEstimation)
{
	EASY_PEER_CONNECTION_T* peerConnection = (EASY_PEER_CONNECTION_T*)userPtr;
	EASYRTC_PEER_T* peer = (EASYRTC_PEER_T*)peerConnection->peerPtr;
	EASYRTC_DEVICE_T* pDevice = (EASYRTC_DEVICE_T*)peer->devicePtr;

	if (EASYRTC_TRANSCEIVER_CALLBACK_VIDEO_FRAME == type)
	{
		CallbackData(pDevice, peer->uuid, EASYRTC_CALLBACK_TYPE_PEER_VIDEO, codecID, 1, (char*)frame->frameData, frame->size, frame->flags, frame->presentationTs);

		if (frame->flags == 0x01)
		{
			__Print__(NULL, __FUNCTION__, __LINE__, false, "############## Caller callback video.\n");
		}
	}
	else if (EASYRTC_TRANSCEIVER_CALLBACK_AUDIO_FRAME == type)
	{
		func_audioRtcOnFrame(userPtr, codecID, frame);
	}
	//else if (EASYRTC_TRANSCEIVER_CALLBACK_BANDWIDTH == type)
	//{

	//}
	//else if (EASYRTC_TRANSCEIVER_CALLBACK_KEY_FRAME_REQ == type)
	//{
	//	// 当前有丢帧, 现请求关键帧
	//}

	return 0;
}


int __EasyRTC_DataChannel_Callback(void* userPtr, EASYRTC_DATACHANNEL_CALLBACK_TYPE_E type, BOOL isBinary, const char* msgData, int msgLen)
{
	EASY_PEER_CONNECTION_T* peerConnection = (EASY_PEER_CONNECTION_T*)userPtr;
	EASYRTC_PEER_T* peer = (EASYRTC_PEER_T*)peerConnection->peerPtr;
	EASYRTC_DEVICE_T* pDevice = (EASYRTC_DEVICE_T*)peer->devicePtr;

	if (EASYRTC_DATACHANNEL_CALLBACK_DATA == type)
	{
		//__Print__(NULL, __FUNCTION__, __LINE__, false, "############## ptr2222:%p\n", userPtr);

		// 对方发过来的视频
		//__device_Print__(NULL, __FUNCTION__, __LINE__, false, "%s line[%d] [%s] peer Video codecId[%u] videoCodecID[%u]\n", __FUNCTION__, __LINE__, 
		//	peerConnection->type == EASYRTC_PEER_PUBLISHER ? "PUBLISHER" : "SUBSCRIBER", peerConnection->video_track_.codec, peerConnection->videoCodecID);

		CallbackData(pDevice, peer->uuid, EASYRTC_CALLBACK_TYPE_PEER_CUSTOM_DATA, 0, isBinary, (char*)msgData, msgLen, 0, 0);
	}
	else if (EASYRTC_DATACHANNEL_CALLBACK_OPENED == type)
	{
		CallbackData(pDevice, peer->uuid, EASYRTC_CALLBACK_TYPE_PEER_ENABLED_CUSTOM_DATA, 0, isBinary, (char*)msgData, msgLen, 0, 0);
	}

	return 0;
}


int	EasyRTC_CreatePeer(EASYRTC_DEVICE_T* pDevice, EASYRTC_PEER_T* peer, EASYRTC_PEER_TYPE_E type, const char* offerSDP)
{
	int retStatus = -1;

	CHANNEL_INFO_T* pChannel = (CHANNEL_INFO_T*)&pDevice->channelInfo;

	// 每次连接都使用新的配置
	memset(&pDevice->rtcConfiguration, 0x00, sizeof(EasyRTC_Configuration));

	// 配置 WebRTC
	if (pDevice->rtcConfiguration.iceTransportPolicy == 0)
	{
		pDevice->rtcConfiguration.iceTransportPolicy = EasyRTC_ICE_TRANSPORT_POLICY_ALL;		// 先NAT穿透，不通再转为turn, 如需直接使用turn则置为ICE_TRANSPORT_POLICY_RELAY

		peer->peerConnection[type].turncount = 0;
		peer->peerConnection[type].icecount = 0;
		peer->peerConnection[type].icesended = 0;

		// 设置 STUN/TURN 服务器
		int serverIdx = 0;
		for (int i = 0; i < pDevice->serverConfig.stun_server_count; i++)
		{
			if (0 == strcmp(pDevice->serverConfig.stun_servers[i], "\0"))	break;

			strcpy(pDevice->rtcConfiguration.iceServers[serverIdx++].urls, pDevice->serverConfig.stun_servers[i]);
		}

		int turnServerNum = sizeof(pDevice->serverConfig.turn_servers) / sizeof(pDevice->serverConfig.turn_servers[0]);
		for (int i = 0; i < turnServerNum; i++)
		{
			if (0 == strcmp(pDevice->serverConfig.turn_servers[i], "\0"))	continue;

			strcpy(pDevice->rtcConfiguration.iceServers[serverIdx].urls, pDevice->serverConfig.turn_servers[i]);
			strcpy(pDevice->rtcConfiguration.iceServers[serverIdx].username, pDevice->serverConfig.turn_username);
			strcpy(pDevice->rtcConfiguration.iceServers[serverIdx].credential, pDevice->serverConfig.turn_password);

			serverIdx++;
			peer->peerConnection[type].turncount++;
		}
	}

	

	// 创建 Peer Connection
	retStatus = EasyRTC_CreatePeerConnection(&peer->peerConnection[type].peer_, &pDevice->rtcConfiguration, __EasyRTC_ConnectionStateChange_Callback, &peer->peerConnection[type]);
	
	if (retStatus != 0) {
		__Print__(NULL, __FUNCTION__, __LINE__, false, "Failed to create peer connection with status: %d\n", retStatus);
		return retStatus;
	}

	peer->peerConnection[type].type = type;				// 更新类型
	peer->peerConnection[type].peerPtr = peer;			// 指定peer指针
	peer->devicePtr = pDevice;

	if (EASYRTC_PEER_PUBLISHER == type)
	{
		// 配置视频轨道
		if ((pChannel->videoCodecID > 0) && (pChannel->mediaType & EASYRTC_MDIA_TYPE_VIDEO))
		{
			memset(&peer->peerConnection[type].video_track_, 0x00, sizeof(EasyRTC_MediaStreamTrack));
			peer->peerConnection[type].video_track_.kind = EasyRTC_MEDIA_STREAM_TRACK_KIND_VIDEO;
			peer->peerConnection[type].video_track_.codec = pChannel->videoCodecID;
			strcpy(peer->peerConnection[type].video_track_.streamId, "0");
			strcpy(peer->peerConnection[type].video_track_.trackId, "0");

			EasyRTC_RtpTransceiverInit transceiverInit;
			memset(&transceiverInit, 0x00, sizeof(EasyRTC_RtpTransceiverInit));
			transceiverInit.direction = pChannel->direction;

			__Print__(NULL, __FUNCTION__, __LINE__, false, "############## ptr:%llu\n", &peer->peerConnection[type]);

			retStatus = EasyRTC_AddTransceiver(&peer->peerConnection[type].videoTransceiver_, 
				peer->peerConnection[type].peer_, 
				&peer->peerConnection[type].video_track_, &transceiverInit, 
				__EasyRTC_Device_Transceiver_Callback, &peer->peerConnection[type]);
			if (retStatus != 0) {
				__Print__(NULL, __FUNCTION__, __LINE__, false, "Failed to add transceiver with status: %d\n", retStatus);
				return retStatus;
			}

			// 回调: 通知上层开始发送视频
			CallbackData(pDevice, peer->uuid, EASYRTC_CALLBACK_TYPE_START_VIDEO, pChannel->videoCodecID, true, NULL, 0, 0, 0);
		}

		if ((pChannel->audioCodecID > 0) && (pChannel->mediaType & EASYRTC_MDIA_TYPE_AUDIO))
		{
			memset(&peer->peerConnection[type].audio_track_, 0x00, sizeof(EasyRTC_MediaStreamTrack));
			peer->peerConnection[type].audio_track_.kind = EasyRTC_MEDIA_STREAM_TRACK_KIND_AUDIO;
			peer->peerConnection[type].audio_track_.codec = pChannel->audioCodecID;
			strcpy(peer->peerConnection[type].audio_track_.streamId, "0");
			strcpy(peer->peerConnection[type].audio_track_.trackId, "1");

			EasyRTC_RtpTransceiverInit transceiverInit;
			memset(&transceiverInit, 0x00, sizeof(EasyRTC_RtpTransceiverInit));
			transceiverInit.direction = pChannel->direction;

			retStatus = EasyRTC_AddTransceiver(&peer->peerConnection[type].audioTransceiver_,
				peer->peerConnection[type].peer_,
				&peer->peerConnection[type].audio_track_, &transceiverInit,
				__EasyRTC_Device_Transceiver_Callback, &peer->peerConnection[type]);
			if (retStatus != 0) {
				__Print__(NULL, __FUNCTION__, __LINE__, false, "Failed to add transceiver with status: %d\n", retStatus);
				return retStatus;
			}

			// 回调: 通知上层开始发送音频
			CallbackData(pDevice, peer->uuid, EASYRTC_CALLBACK_TYPE_START_AUDIO, pChannel->audioCodecID, true, NULL, 0, 0, 0);
		}

		EasyRTC_AddDataChannel(&peer->peerConnection[type].dc_, peer->peerConnection[type].peer_, "op", __EasyRTC_DataChannel_Callback, &peer->peerConnection[type]);
	}
	else if (EASYRTC_PEER_SUBSCRIBER == type && (NULL != offerSDP))
	{
		int ret = 0;
		int bIs = 0;
		if (strstr(offerSDP, "m=video ") != NULL)
		{
			memset(&peer->peerConnection[type].video_track_, 0x00, sizeof(EasyRTC_MediaStreamTrack));
			if (strstr(offerSDP, " H264/90000") != NULL)
			{
				bIs = 1;
				peer->peerConnection[type].video_track_.codec = EasyRTC_CODEC_H264;
			}
			else if (strstr(offerSDP, " H265/90000") != NULL)
			{
				bIs = 1;
				peer->peerConnection[type].video_track_.codec = EasyRTC_CODEC_H265;
			}
			else if (strstr(offerSDP, " VP8/90000") != NULL)
			{
				bIs = 1;
				peer->peerConnection[type].video_track_.codec = EasyRTC_CODEC_VP8;
			}

			if (bIs == 1)
			{
				peer->peerConnection[type].video_track_.kind = EasyRTC_MEDIA_STREAM_TRACK_KIND_VIDEO;
				strcpy(peer->peerConnection[type].video_track_.streamId, "0");
				strcpy(peer->peerConnection[type].video_track_.trackId, "0");

				EasyRTC_RtpTransceiverInit rtcRtpTransceiverInit_v_;
				rtcRtpTransceiverInit_v_.direction = EasyRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;

				retStatus = EasyRTC_AddTransceiver(&peer->peerConnection[type].videoTransceiver_,
					peer->peerConnection[type].peer_,
					&peer->peerConnection[type].video_track_, &rtcRtpTransceiverInit_v_,
					__EasyRTC_Caller_Transceiver_Callback, &peer->peerConnection[type]);
				if (retStatus != 0)
				{
					__Print__(NULL, __FUNCTION__, __LINE__, false, "Failed to add transceiver with status: %d\n", retStatus);
					return retStatus;
				}
			}
			else
			{
				__Print__(NULL, __FUNCTION__, __LINE__, false, "视频编码格式不被支持\n");
			}
		}

		bIs = 0;
		if (strstr(offerSDP, "m=audio ") != NULL)
		{
			memset(&peer->peerConnection[type].audio_track_, 0x00, sizeof(EasyRTC_MediaStreamTrack));
			if (strstr(offerSDP, " PCMA/8000") != NULL)
			{
				bIs = 1;
				peer->peerConnection[type].audio_track_.codec = EasyRTC_CODEC_ALAW;
			}
			else if (strstr(offerSDP, " PCMU/8000") != NULL)
			{
				bIs = 1;
				peer->peerConnection[type].audio_track_.codec = EasyRTC_CODEC_MULAW;
			}
			else if (strstr(offerSDP, " opus/48000") != NULL)
			{
				bIs = 1;
				peer->peerConnection[type].audio_track_.codec = EasyRTC_CODEC_OPUS;
			}

			if (bIs == 1)
			{
				peer->peerConnection[type].audio_track_.kind = EasyRTC_MEDIA_STREAM_TRACK_KIND_AUDIO;
				strcpy(peer->peerConnection[type].audio_track_.streamId, "0");
				strcpy(peer->peerConnection[type].audio_track_.trackId, "1");

				EasyRTC_RtpTransceiverInit rtcRtpTransceiverInit_a_;
				rtcRtpTransceiverInit_a_.direction = EasyRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;

				retStatus = EasyRTC_AddTransceiver(&peer->peerConnection[type].audioTransceiver_,
					peer->peerConnection[type].peer_,
					&peer->peerConnection[type].audio_track_, &rtcRtpTransceiverInit_a_,
					__EasyRTC_Caller_Transceiver_Callback, &peer->peerConnection[type]);
				if (retStatus != 0) {

					__Print__(NULL, __FUNCTION__, __LINE__, false, "Failed to add transceiver with status: %d\n", retStatus);
					return retStatus;
				}
			}
			else
			{
				__Print__(NULL, __FUNCTION__, __LINE__, false, "音频编码格式不被支持\n");
			}
		}

		if (strstr(offerSDP, " webrtc-datachannel") != NULL)
		{
			ret = EasyRTC_AddDataChannel(&peer->peerConnection[type].dc_, peer->peerConnection[type].peer_, NULL, __EasyRTC_DataChannel_Callback, &peer->peerConnection[type]);

			__Print__(NULL, __FUNCTION__, __LINE__, false, "RTC_peerConnectionOnDataChannel: %u\n", ret);
		}
	}

	return 0;
}

int EasyRTC_ReleasePeer(EASYRTC_PEER_T* peer)
{
	if (NULL == peer)		return 0;

	LockPeer(peer, __FUNCTION__, __LINE__);

	for (int i = 0; i < EASYRTC_PEER_MAX; i++)
	{
		if (NULL != peer->peerConnection[i].videoTransceiver_)
		{
			EasyRTC_FreeTransceiver(&peer->peerConnection[i].videoTransceiver_);
		}
		if (NULL != peer->peerConnection[i].audioTransceiver_)
		{
			EasyRTC_FreeTransceiver(&peer->peerConnection[i].audioTransceiver_);
		}

		if (NULL != peer->peerConnection[i].dc_)
		{
			EasyRTC_FreeDataChannel(&peer->peerConnection[i].dc_);
		}

		if (NULL != peer->peerConnection[i].peer_)
		{
			EasyRTC_ReleasePeerConnection(&peer->peerConnection[i].peer_);
		}

		BUFF_FREE(&peer->peerConnection[i].fullFrameData);
	}

	UnlockPeer(peer, __FUNCTION__, __LINE__);

	return 0;
}

int  CallbackData(EASYRTC_DEVICE_T* pDevice, const char* peerUUID, EASYRTC_DATA_TYPE_ENUM_E dataType, int codecID, int isBinary, char* data, int size, int keyframe, unsigned long long pts)
{
	if (NULL == pDevice)		return 0;
	if (NULL == pDevice->dataCallback)	return 0;

	time_t tStart = time(NULL);

	int ret = pDevice->dataCallback(pDevice->dataUserptr, peerUUID, dataType, codecID, isBinary, data, size, keyframe, pts);

	time_t tEnd = time(NULL);

	time_t tInterval = tEnd - tStart;
	if (tInterval > 0)
	{
		__Print__(NULL, __FUNCTION__, __LINE__, false, "dataType:%d  time: %llu   ret:%d\n", dataType, tInterval, ret);
	}

	return ret;
}


int GetOnlineDevices(EASYRTC_DEVICE_T* pDevice, EasyRTC_Data_Callback callback, void* userptr)
{
	if (NULL == pDevice)		return 0;
	if (NULL == pDevice->dataCallback)	return 0;

	REQ_GETONLINEDEVICES_INFO	reqInfo;
	reqInfo.length = sizeof(REQ_GETONLINEDEVICES_INFO);
	reqInfo.msgtype = HP_REQGETONLINEDEVICES_INFO;

	int ret = websocketSendData(pDevice->websocket, WS_OPCODE_BIN, (char*)&reqInfo, reqInfo.length);

	return ret;
}