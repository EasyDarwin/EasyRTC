#include "EasyRtcDevice_C.h"
#include <stdarg.h>
#ifndef _WIN32
#include <unistd.h>
#endif

OSMutex		mutex;

int		EasyRTC_Device_Init()
{
	InitMutex(&mutex);

	EasyRTC_initWebRtc();		// ��ʼ��webRTC

	return 0;
}
void	EasyRTC_Device_Deinit()
{
	EasyRTC_deinitWebRtc();

	DeinitMutex(&mutex);
}



int		__Print__(const char* functionName, const int lineNum, const char* szFormat, ...)
{
	char szBuff[1024] = { 0 };

	//char szTime[32] = { 0 };
	time_t tt = time(NULL);
	struct tm* _timetmp = NULL;
	_timetmp = localtime(&tt);
	if (NULL != _timetmp)   strftime(szBuff, 32, "[%Y-%m-%d %H:%M:%S] ", _timetmp);

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


	printf("%s", szBuff);


	return 0;
}


void	LockDevice(const char* functionName, const int lineNum)
{
	LockMutex(&mutex);
}
void	UnlockDevice(const char* functionName, const int lineNum)
{
	UnlockMutex(&mutex);
}



#ifdef _WIN32
DWORD WINAPI __CheckWebRtcThread(void* lpParam)
#else
void* __CheckWebRtcThread(void* lpParam)
#endif
{
	OSTHREAD_OBJ_T* pThread = (OSTHREAD_OBJ_T*)lpParam;
	WEBRTC_DEVICE_T* pDevice = (WEBRTC_DEVICE_T*)pThread->userPtr;

	pThread->flag = THREAD_STATUS_RUNNING;

	__Print__(__FUNCTION__, __LINE__, "check thread startup. [%d]\n", pThread->customId);

	while (1)
	{
		if (pThread->flag == THREAD_STATUS_EXIT)			break;

		LockDevice(__FUNCTION__, __LINE__);

		for (int i = 0; i < pDevice->webRTCPeerNum; i++)
		{
			WEBRTC_PEER_T* webrtcPeer = pDevice->webrtcPeer[i];

			if (time(NULL) - webrtcPeer->createTime > 60)			// �������״̬
			{
				if (webrtcPeer->connected == 0 || webrtcPeer->disconnected == 1)
				{
					__Print__(__FUNCTION__, __LINE__, "=================%s line[%d] %s: remove %p\n", __FUNCTION__, __LINE__, webrtcPeer->connected == 0 ? "connect fail" : "disconnected", webrtcPeer);

					EasyRTC_closePeerConnection(webrtcPeer->peer_);
					free(webrtcPeer);

					for (int j = i; j < pDevice->webRTCPeerNum; j++)
					{
						pDevice->webrtcPeer[j] = pDevice->webrtcPeer[j + 1];
					}
					pDevice->webRTCPeerNum--;
				}
			}
		}


		UnlockDevice(__FUNCTION__, __LINE__);

#ifdef _WIN32
		Sleep(1);
#else
		usleep(1000000);
#endif
	}

	pThread->flag = THREAD_STATUS_INIT;

	__Print__(__FUNCTION__, __LINE__, "check thread shutdown. [%d]\n", pThread->customId);

	return 0;
}


void func_RtcOnOpen(UINT64 customData, PRtcDataChannel dc)
{
	WEBRTC_PEER_T* webrtcpeer = (WEBRTC_PEER_T*)customData;
	char szHello[] = "Hello,EasyRTC";
	EasyRTC_dataChannelSend(dc, FALSE, (PBYTE)szHello, (UINT32)strlen(szHello) + 1);
	__Print__(__FUNCTION__, __LINE__, "func_RtcOnOpen........1111111111....\n");
}

void func_RtcOnPictureLoss(UINT64 customData)
{
	WEBRTC_PEER_T* webrtcpeer = (WEBRTC_PEER_T*)customData;
	//������Ӧ��ִ��ǿ�ƹؼ�֡�Ĳ���,��Ϊ����������ζ�Ŷ�֡��,ֻ���Ǿ��췢�͹ؼ�֡
	//��rtspתwebrtc���������,��Ϊֻ���Ǳ�������,���޷�ǿ�ƹؼ�֡,����������û�������,
	//����ֻҪ�а취��ǿ�ƹؼ�֡,��Ӧ��ȥ������߼�
	__Print__(__FUNCTION__, __LINE__, "func_RtcOnPictureLoss......\n");
}

void func_RtcOnMessage(UINT64 customData, PRtcDataChannel dc, BOOL isBinary, PBYTE msgData, UINT32 msgLen)
{
	WEBRTC_PEER_T* webrtcpeer = (WEBRTC_PEER_T*)customData;
	if (NULL == webrtcpeer)			return;
	WEBRTC_DEVICE_T* pDevice = (WEBRTC_DEVICE_T*)webrtcpeer->userptr;
	if (NULL == pDevice)			return;

	if (NULL != pDevice->callback)
	{
		EasyRTC_DataChannel_Callback pCB = (EasyRTC_DataChannel_Callback)pDevice->callback;
		pCB(pDevice->userptr, webrtcpeer, isBinary, msgData, msgLen);
	}
	else
	{
		__Print__(__FUNCTION__, __LINE__, "func_RtcOnMessage dc_label=%s isBinary=%d msgLen=%d\n", dc->name, isBinary, msgLen);
		//if (!isBinary)
		//{
		//	char* newdata = (char*)malloc(msgLen + 6);
		//	newdata[0] = 'e';
		//	newdata[1] = 'c';
		//	newdata[2] = 'h';
		//	newdata[3] = 'o';
		//	newdata[4] = ':';
		//	newdata[msgLen + 5] = 0;
		//	memcpy(newdata + 5, msgData, msgLen);
		//	EasyRTC_dataChannelSend(dc, isBinary, (PBYTE)newdata, msgLen + 5);
		//	free(newdata);
		//	__Print__(__FUNCTION__, __LINE__, "\t%.*s\n", msgLen, msgData);
		//}
	}



}

//�������������,����ֱ��ʵ������
//�����������������������,��ô���黹��ʹ�� ntohs ����
uint16_t my_ntohs(uint16_t netshort)
{
	uint16_t byte1 = netshort & 0xFF;
	uint16_t byte2 = (netshort >> 8) & 0xFF;
	return (byte1 << 8) | byte2;
}

void func_RtcOnConnectionStateChange(UINT64 customData, RTC_PEER_CONNECTION_STATE state)
{
	WEBRTC_PEER_T* webrtcpeer = (WEBRTC_PEER_T*)customData;
	if (state == RTC_PEER_CONNECTION_STATE_CONNECTED)
	{
		webrtcpeer->connected = 1;
		//�������������ǿ��I֡�Ľӿں�������I֡�������
		__Print__(__FUNCTION__, __LINE__, "RTC_PEER_CONNECTION_STATE_CONNECTED............\n");

		RtcIceCandidatePairStats RtcIceCandidatePairStats = { 0 };
		STATUS status = EasyRTC_getIceCandidatePairStats(webrtcpeer->peer_, &RtcIceCandidatePairStats);
		if (status != STATUS_SUCCESS)
			return;

		__Print__(__FUNCTION__, __LINE__, "local ice type: %d\n", RtcIceCandidatePairStats.local_iceCandidateType);
		__Print__(__FUNCTION__, __LINE__, "remote ice type: %d\n", RtcIceCandidatePairStats.remote_iceCandidateType);

		__Print__(__FUNCTION__, __LINE__, "local ice port: %d\n", my_ntohs(RtcIceCandidatePairStats.local_ipAddress.port));
		__Print__(__FUNCTION__, __LINE__, "remote ice port: %d\n", my_ntohs(RtcIceCandidatePairStats.remote_ipAddress.port));

		__Print__(__FUNCTION__, __LINE__, "local ice address: %s\n", RtcIceCandidatePairStats.local_ipAddress.strAddress);
		__Print__(__FUNCTION__, __LINE__, "remote ice address: %s\n", RtcIceCandidatePairStats.remote_ipAddress.strAddress);

		if ((RtcIceCandidatePairStats.local_iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED) || (RtcIceCandidatePairStats.remote_iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED))
		{
			//������ת�ķ�ʽ��������������
			__Print__(__FUNCTION__, __LINE__, "This is use turn server exchange data\n");
		}
		else
		{
			//����ֱ���ķ�ʽ��������������
			__Print__(__FUNCTION__, __LINE__, "This is use p2p exchange data\n");
		}
	}
	else if (state == RTC_PEER_CONNECTION_STATE_DISCONNECTED)
	{
		webrtcpeer->disconnected = 1;
		__Print__(__FUNCTION__, __LINE__, "RTC_PEER_CONNECTION_STATE_DISCONNECTED............\n");
	}
	else if (state == RTC_PEER_CONNECTION_STATE_FAILED)
	{
		webrtcpeer->disconnected = 1;
		__Print__(__FUNCTION__, __LINE__, "RTC_PEER_CONNECTION_STATE_FAILED............\n");
	}
	else if (state == RTC_PEER_CONNECTION_STATE_CLOSED)
	{
		webrtcpeer->disconnected = 1;
		__Print__(__FUNCTION__, __LINE__, "RTC_PEER_CONNECTION_STATE_CLOSED............\n");
	}

	if ((state == RTC_PEER_CONNECTION_STATE_DISCONNECTED) || (state == RTC_PEER_CONNECTION_STATE_FAILED) || (state == RTC_PEER_CONNECTION_STATE_CLOSED))
	{
		webrtcpeer->createTime = 0;
	}
}


int		EasyRTC_Device_SendVideoFrame(WEBRTC_DEVICE_T* pDevice, char* framedata, const int framesize, int keyframe, uint64_t pts)
{
	if (NULL == framedata)		return -1;
	if (framesize < 1)			return -1;


	Frame frame = { 0 };
	frame.frameData = (PBYTE)framedata;
	frame.size = framesize;
	frame.presentationTs = pts * 10000;

	LockDevice(__FUNCTION__, __LINE__);

	for (int i = 0; i < pDevice->webRTCPeerNum; i++)
	{
		WEBRTC_PEER_T* peer = pDevice->webrtcPeer[i];

		if ((peer->connected == 0) || (peer->disconnected == 1))
		{
			continue;
		}
		if ((peer->noneedsI == 0) && (!keyframe)) //��ҪIDR֡,���ǵ�ǰ֡�ֲ���IDR֡
		{
			continue;
		}

		peer->noneedsI = 1; //����Ҫ�������IDR֡��
		if (keyframe)	frame.flags = FRAME_FLAG_KEY_FRAME;
		EasyRTC_writeFrame(peer->videoTransceiver_, &frame);
	}

	UnlockDevice(__FUNCTION__, __LINE__);

	return 0;
}

int		EasyRTC_Device_SendAudioFrame(WEBRTC_DEVICE_T* pDevice, char* framedata, const int framesize, uint64_t pts)
{
	if (NULL == framedata)		return -1;
	if (framesize < 1)			return -1;

	Frame frame = { 0 };
	frame.frameData = (PBYTE)framedata;
	frame.size = framesize;
	frame.presentationTs = pts * 10000;

	LockDevice(__FUNCTION__, __LINE__);

	for (int i = 0; i < pDevice->webRTCPeerNum; i++)
	{
		WEBRTC_PEER_T* peer = pDevice->webrtcPeer[i];

		if ((peer->connected == 0) || (peer->disconnected == 1) || (peer->noneedsI == 0)) //��Ƶ��û���͵�һ֡IDR֡,������Ƶ������
		{
			continue;
		}
		EasyRTC_writeFrame(peer->audioTransceiver_, &frame);
	}

	UnlockDevice(__FUNCTION__, __LINE__);

	return 0;
}

int		EasyRTC_Device_SendData(WEBRTC_PEER_T* webrtcPeer, BOOL isBinary, PBYTE msgData, UINT32 msgLen)
{
	if (NULL == webrtcPeer)			return -1;
	if (NULL == webrtcPeer->dc_)	return -1;
	if (NULL == msgData)			return -1;
	if (msgLen < 1)					return -1;

	EasyRTC_dataChannelSend(webrtcPeer->dc_, isBinary, msgData, msgLen);

	return 0;
}


void	CheckWebRtcPeerQueue(WEBRTC_DEVICE_T* pDevice)		// ���webrtcPeerMap�Ƿ񳬹����ֵ
{
	LockDevice(__FUNCTION__, __LINE__);

	WEBRTC_PEER_T* webrtcpeer = NULL;
	if (pDevice->webRTCPeerNum + 1 > MAX_WEBRTC_PEERS_COUNT)
	{
		webrtcpeer = pDevice->webrtcPeer[0];
		pDevice->webRTCPeerNum--;
		for (int j = 0; j < pDevice->webRTCPeerNum; j++)
		{
			pDevice->webrtcPeer[j] = pDevice->webrtcPeer[j + 1];
		}
		EasyRTC_closePeerConnection(webrtcpeer->peer_);
		free(webrtcpeer);
	}

	__Print__(__FUNCTION__, __LINE__, "=================%s line[%d] webRTCPeerNum:%u\n", __FUNCTION__, __LINE__, pDevice->webRTCPeerNum);

	UnlockDevice(__FUNCTION__, __LINE__);
}

void	AddWebRtcPeer2Map(WEBRTC_DEVICE_T* pDevice, WEBRTC_PEER_T* peer)		// ���WEBRTC_PEER_T���б�
{
	LockDevice(__FUNCTION__, __LINE__);

	peer->createTime = time(NULL);
	peer->userptr = pDevice;

	pDevice->webrtcPeer[pDevice->webRTCPeerNum++] = peer;

	__Print__(__FUNCTION__, __LINE__, "=================%s line[%d] webRTCPeerNum:%u\n", __FUNCTION__, __LINE__, pDevice->webRTCPeerNum);

	UnlockDevice(__FUNCTION__, __LINE__);
}


int SignalingClientMessageReceivedCallback(unsigned char* message, int length, uint64_t customData)
{
	WEBRTC_DEVICE_T* pRtcDevice = (WEBRTC_DEVICE_T*)customData;

	BASE_MSG_INFO* pBaseInfo = (BASE_MSG_INFO*)message;
	switch (pBaseInfo->msgtype)
	{
	case SDK_ACKLOGINUSER_INFO:
	{
		SDK_ACK_LOGINUSER_INFO* pRecvInfo = (SDK_ACK_LOGINUSER_INFO*)message;
		__Print__(__FUNCTION__, __LINE__, "uuidΪ: %08X-%04X-%04X-%04X-%04X%08X �ĵ�¼�����:%d\n", pRecvInfo->myid[0], pRecvInfo->myid[1] >> 16, pRecvInfo->myid[1] & 0xFFFF, pRecvInfo->myid[2] >> 16, pRecvInfo->myid[2] & 0xFFFF, pRecvInfo->myid[3], pRecvInfo->status);
		// ����pRecvInfo->statusֵ��˵��: 0--->��¼�ɹ�; ���ֵΪʧ��
	}
	break;
	case SDK_REQGETWEBRTCOFFER_INFO:
	{
		SDK_REQ_GETWEBRTCOFFER_INFO* pRecvInfo = (SDK_REQ_GETWEBRTCOFFER_INFO*)message;
		__Print__(__FUNCTION__, __LINE__, "�յ���uuidΪ: %08X-%04X-%04X-%04X-%04X%08X ����������\n", pRecvInfo->hisid[0], pRecvInfo->hisid[1] >> 16, pRecvInfo->hisid[1] & 0xFFFF, pRecvInfo->hisid[2] >> 16, pRecvInfo->hisid[2] & 0xFFFF, pRecvInfo->hisid[3]);

		// ��ΪǶ��ʽ�豸ֻ���ȶ���֧�ֻ���ֻ׼��֧��ͬʱ��������,������������ﵽ���ֵ,��ֱ��ǿ�ƹر��ʼ���Ǹ�����
		CheckWebRtcPeerQueue(pRtcDevice);

		WEBRTC_PEER_T* webrtcpeer = (WEBRTC_PEER_T*)malloc(sizeof(WEBRTC_PEER_T));
		if (!webrtcpeer)
			return 0;

		memset(webrtcpeer, 0, sizeof(WEBRTC_PEER_T));
		webrtcpeer->connected = 0;
		webrtcpeer->disconnected = 0;
		webrtcpeer->noneedsI = 0;

		RtcConfiguration conf_;
		memset(&conf_, 0, sizeof conf_);
		conf_.iceTransportPolicy = ICE_TRANSPORT_POLICY_ALL;
		conf_.kvsRtcConfiguration.generateRSACertificate = 0;
		conf_.kvsRtcConfiguration.sendBufSize = 1024 * 1024;

		int iPos = 0;
		int turncount = 0;
		int iceServersCount = 0;
		char* pStrDatas = pRecvInfo->sturndatas;

		for (int i = 0; i < pRecvInfo->sturncount; i++)
		{
			if (pRecvInfo->sturntypes[i] == 0x00)
			{
				sprintf(conf_.iceServers[iceServersCount].urls, "stun:%s", pStrDatas + iPos);
				//__Print__(__FUNCTION__, __LINE__, "%d:%s\n", iceServersCount, conf_.iceServers[iceServersCount].urls);
				iPos += (int)(strlen(pStrDatas + iPos) + 1);
				iceServersCount++;
			}
			else if (pRecvInfo->sturntypes[i] == 0x01)
			{
				// ���Ƿ�juice
				sprintf(conf_.iceServers[iceServersCount].urls, "turn:%s", pStrDatas + iPos);
				//__Print__(__FUNCTION__, __LINE__, "%d:%s\n", iceServersCount, conf_.iceServers[iceServersCount].urls);
				iPos += (int)(strlen(pStrDatas + iPos) + 1);
			}
			else if (pRecvInfo->sturntypes[i] == 0x02)
			{
				sprintf(conf_.iceServers[iceServersCount].username, "%s", pStrDatas + iPos);
				//__Print__(__FUNCTION__, __LINE__, "%d:%s\n", iceServersCount, conf_.iceServers[iceServersCount].username);
				iPos += (int)(strlen(pStrDatas + iPos) + 1);
			}
			else if (pRecvInfo->sturntypes[i] == 0x03)
			{
				sprintf(conf_.iceServers[iceServersCount].credential, "%s", pStrDatas + iPos);
				//__Print__(__FUNCTION__, __LINE__, "%d:%s\n", iceServersCount, conf_.iceServers[iceServersCount].credential);
				iPos += (int)(strlen(pStrDatas + iPos) + 1);
				iceServersCount++; //������һ�� turn, �����Ƿ����˺ź�����,�����봫���˺ź��������,����Ϊ��
				turncount++;
			}
			else if (pRecvInfo->sturntypes[i] == 0x04)
			{
				//����Ϊ�Լ�ʵ�ֵ�turn serverԤ��������
				sprintf(conf_.iceServers[iceServersCount].urls, "turn:%s", pStrDatas + iPos);
				//__Print__(__FUNCTION__, __LINE__, "%d:%s\n", iceServersCount, conf_.iceServers[iceServersCount].urls);
				iPos += (int)(strlen(pStrDatas + iPos) + 1);
			}
		}

		char* extradata0 = pRecvInfo->sturndatas + pRecvInfo->sturndataslen;
		int extradatalen0 = length - sizeof(SDK_REQ_GETWEBRTCOFFER_INFO) - pRecvInfo->sturndataslen;
		__Print__(__FUNCTION__, __LINE__, "exdata:%s, len:%d\n", extradata0, extradatalen0);

		UINT32 ret = EasyRTC_createPeerConnection(&conf_, &webrtcpeer->peer_, pRecvInfo->hisid, pRecvInfo->sockid);
		__Print__(__FUNCTION__, __LINE__, "createPeerConnection: %u\n", ret);

		ret = EasyRTC_peerConnectionOnConnectionStateChange(webrtcpeer->peer_, (UINT64)webrtcpeer, func_RtcOnConnectionStateChange);
		__Print__(__FUNCTION__, __LINE__, "peerConnectionOnConnectionStateChange: %u\n", ret);

		RtcMediaStreamTrack video_track_;
		video_track_.codec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;	// Ĭ��ΪH264
		if (pRtcDevice->videoCodecID == 0xAE)
		{
			video_track_.codec = RTC_CODEC_H265;
		}

		video_track_.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;
		strcpy(video_track_.streamId, "0");
		strcpy(video_track_.trackId, "0");
		RtcRtpTransceiverInit rtcRtpTransceiverInit_v_;
		//����Զ�ҲҪ������Ƶ����,��ô����ʹ�� RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV,��ʹ�� RTC_transceiverOnFrame ���õĻص�����
		rtcRtpTransceiverInit_v_.direction = RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;
		ret = EasyRTC_addTransceiver(webrtcpeer->peer_, &video_track_, &rtcRtpTransceiverInit_v_, &webrtcpeer->videoTransceiver_);
		__Print__(__FUNCTION__, __LINE__, "video addTransceiver: %u\n", ret);
		ret = EasyRTC_transceiverOnPictureLoss(webrtcpeer->videoTransceiver_, (UINT64)webrtcpeer, func_RtcOnPictureLoss);
		__Print__(__FUNCTION__, __LINE__, "transceiverOnPictureLoss: %u\n", ret);

		int audioCodecID = pRtcDevice->audioCodecID;

		if (audioCodecID > 0)
		{
			RtcMediaStreamTrack audio_track_;
			if (audioCodecID == 0x10007)
				audio_track_.codec = RTC_CODEC_ALAW;
			else if (audioCodecID == 0x10006)
				audio_track_.codec = RTC_CODEC_MULAW;
			else if (audioCodecID == 0x15002)
				audio_track_.codec = RTC_CODEC_AAC;
			else if (audioCodecID == 3)
				audio_track_.codec = RTC_CODEC_OPUS;
			audio_track_.kind = MEDIA_STREAM_TRACK_KIND_AUDIO;
			strcpy(audio_track_.streamId, "0");
			strcpy(audio_track_.trackId, "1");

			RtcRtpTransceiverInit rtcRtpTransceiverInit_a_;
			//����Զ�ҲҪ������Ƶ����,��ô����ʹ�� RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV,��ʹ�� RTC_transceiverOnFrame ���õĻص�����
			rtcRtpTransceiverInit_a_.direction = RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
			ret = EasyRTC_addTransceiver(webrtcpeer->peer_, &audio_track_, &rtcRtpTransceiverInit_a_, &webrtcpeer->audioTransceiver_);
			__Print__(__FUNCTION__, __LINE__, "audio addTransceiver: %u\n", ret);
		}

		//һ·����Ƶ���� streamId ��ͬ,�� trackId ��ͬ
		//���Ҫ���Ͷ�·����Ƶ,��ô ʹ�ö�� streamId Ӧ�þͿ���

		//datachannel �������Ҫ,Ҳ�ǿ��Խ��������,���ҿ������ò�ͬ������(����һ��������,һ����������)

		RtcDataChannelInit dcInit = { 0 };
		dcInit.ordered = TRUE;
		ret = EasyRTC_createDataChannel(webrtcpeer->peer_, (PCHAR)"op", &dcInit, &webrtcpeer->dc_);
		__Print__(__FUNCTION__, __LINE__, "createDataChannel: %u\n", ret);
		ret = EasyRTC_dataChannelOnOpen(webrtcpeer->dc_, (UINT64)webrtcpeer, func_RtcOnOpen);
		__Print__(__FUNCTION__, __LINE__, "RTC_dataChannelOnOpen: %u\n", ret);
		ret = EasyRTC_dataChannelOnMessage(webrtcpeer->dc_, (UINT64)webrtcpeer, func_RtcOnMessage);
		__Print__(__FUNCTION__, __LINE__, "dataChannelOnMessage: %u\n", ret);

		ret = EasyRTC_createOfferAndSubmit(webrtcpeer->peer_);
		__Print__(__FUNCTION__, __LINE__, "RTC_createOfferAndSubmit: %u\n", ret);

		AddWebRtcPeer2Map(pRtcDevice, webrtcpeer);

		break;
	}

	case SDK_NTIWEBRTCANSWER_INFO:
	{
		SDK_NTI_WEBRTCANSWER_INFO* pRecvInfo = (SDK_NTI_WEBRTCANSWER_INFO*)message;
		__Print__(__FUNCTION__, __LINE__, "�յ���uuidΪ: %08X-%04X-%04X-%04X-%04X%08X ��answer sdp\n", pRecvInfo->hisid[0], pRecvInfo->hisid[1] >> 16, pRecvInfo->hisid[1] & 0xFFFF, pRecvInfo->hisid[2] >> 16, pRecvInfo->hisid[2] & 0xFFFF, pRecvInfo->hisid[3]);
		__Print__(__FUNCTION__, __LINE__, "answer sdp: %s\n", pRecvInfo->answersdp);
		break;
	}
	default:
		break;
	}

	return 0;
}

int SignalingClientStateChangedCallback(SIGNALING_CLIENT_STATE2 state, uint64_t customData)
{
	WEBRTC_DEVICE_T* pRtcDevice = (WEBRTC_DEVICE_T*)customData;

	switch (state)
	{
	case SIGNALING_CLIENT_STATE2_READY:
	{
		__Print__(__FUNCTION__, __LINE__, "����ͻ���׼�������׽���\n");
		break;
	}
	case SIGNALING_CLIENT_STATE2_CONNECTING:
	{
		__Print__(__FUNCTION__, __LINE__, "����ͻ���׼���������������\n");
		break;
	}
	case SIGNALING_CLIENT_STATE2_CONNECTED:
	{
		__Print__(__FUNCTION__, __LINE__, "����ͻ��������������������(TCPЭ������������)\n");
		break;
	}
	case SIGNALING_CLIENT_STATE2_WEBSOCKET:
	{
		__Print__(__FUNCTION__, __LINE__, "�ɹ�����Ϊ��websocketЭ���������������������\n");
		// RTC_loginUser ֻ���������׽��ִ�TCPЭ����������websocketЭ��֮��,���ܵ���
		char strmysn[64] = "92E22DB5-E74F-4EE2-BAB3-BD8999D15911"; //����У��g_strUUID������������˵ĺϷ���,Ŀǰ���û��У��,����Ūһ��uuid�ַ����Ϳ���
		char strmykey[32] = { 0 }; //�������Ϊ32���ֽڵ��ַ��������ʽ���ڵ�,Ҳ���ǲ�һ��Ҫ��C�ַ���,������ȷ����32���ֽ�,�û������Լ�����ʹ��ĳ�ּ����㷨
		char extradata0[] = "123\0"; //������ڿͻ������ӵ�ʱ��,������׶ξ��յ�����趨���ַ�����,Ҳ�����ڽ���webrtc����֮ǰ���ܱ��ͻ����յ�
		EasyRTC_loginUser(pRtcDevice->strUUID, strmysn, strmykey, extradata0, 4, NULL, 0); //�����������������ڷ��͸����ַ����鵽�����������ȥ��,�����û���չ���������,Ŀǰ��Ϊû����������������û�����,����������
		break;
	}
	case SIGNALING_CLIENT_STATE2_DISCONNECTED:
	{
		__Print__(__FUNCTION__, __LINE__, "����ͻ��������������֮������ӶϿ���,���ص���,�������Զ�����\n");
		break;
	}
	default:
		break;
	}

	return 0;
}

int		EasyRTC_Device_Start(WEBRTC_DEVICE_T* pDevice, int videoCodecID, int audioCodecID, const char* uuid, BOOL enableLocalService, EasyRTC_DataChannel_Callback callback, void* userptr)
{
	if (NULL == uuid)					return -1;
	if (0 == strcmp(uuid, "\0"))		return -1;

	if (NULL == pDevice->checkThread)
	{
		pDevice->videoCodecID = videoCodecID;
		pDevice->audioCodecID = audioCodecID;
		memset(pDevice->strUUID, 0x00, sizeof(pDevice->strUUID));
		strcpy(pDevice->strUUID, uuid);
		pDevice->enableLocalService = enableLocalService;
		pDevice->callback = callback;
		pDevice->userptr = userptr;

		EasyRTC_startSignalingClient(SignalingClientMessageReceivedCallback, SignalingClientStateChangedCallback, (uint64_t)pDevice);

		if (enableLocalService)
		{
			char strmykey[32] = { 0 }; //�������Ϊ32���ֽڵ��ַ��������ʽ���ڵ�,Ҳ���ǲ�һ��Ҫ��C�ַ���,�����ǹ̶���32���ֽ�,�û������Լ�����ʹ��ĳ�ּ����㷨
			char extradata0[] = "123\0"; //������ڿͻ������ӵ�ʱ��,������׶ξ��յ�����趨���ַ�����,Ҳ�����ڽ���webrtc����֮ǰ���ܱ��ͻ����յ�
			EasyRTC_startPrivSignalingServer(6688, pDevice->strUUID, strmykey, extradata0, 4, SignalingClientMessageReceivedCallback, 0);
		}
		CreateOSThread(&pDevice->checkThread, __CheckWebRtcThread, (void*)pDevice, 0);
	}


	return 0;
}

int		EasyRTC_Device_Stop(WEBRTC_DEVICE_T* pDevice)
{
	DeleteOSThread(&pDevice->checkThread);

	// ɾ������webrtcPeer
	LockDevice(__FUNCTION__, __LINE__);

	WEBRTC_PEER_T* webrtcpeer = NULL;

	for (int i = 0; i < pDevice->webRTCPeerNum; i++)
	{
		webrtcpeer = pDevice->webrtcPeer[i];
		EasyRTC_closePeerConnection(webrtcpeer->peer_);
		free(webrtcpeer);
	}
	pDevice->webRTCPeerNum = 0;

	UnlockDevice(__FUNCTION__, __LINE__);

	// �Ͽ��ͷ�����������
	EasyRTC_stopSignalingClient();
	if (pDevice->enableLocalService)
	{
		EasyRTC_stopPrivSignalingServer();
	}

	return 0;
}