#include "EasyRtcDevice.h"
#include <stdarg.h>
#ifndef _WIN32
#include <unistd.h>
#endif

EasyRtcDevice::EasyRtcDevice()
{
    InitMutex(&mutex);

    EasyRTC_initWebRtc();		// ��ʼ��webRTC
}



EasyRtcDevice::~EasyRtcDevice()
{
	Stop();
	EasyRTC_deinitWebRtc();

    DeinitMutex(&mutex);
}

#ifdef _WIN32
bool __MByteToWChar(LPCSTR lpcszStr, LPWSTR lpwszStr, DWORD dwSize)
{
	// Get the required size of the buffer that receives the Unicode
	// string.
	DWORD dwMinSize;
	dwMinSize = MultiByteToWideChar(CP_ACP, 0, lpcszStr, -1, NULL, 0);

	if (dwSize < dwMinSize)
	{
		return false;
	}

	// Convert headers from ASCII to Unicode.
	MultiByteToWideChar(CP_ACP, 0, lpcszStr, -1, lpwszStr, dwMinSize);
	return true;
}
#endif

int		EasyRtcDevice::__Print__(void* userptr, const char* functionName, const int lineNum, bool dataChannelCallback, const char* szFormat, ...)
{
	char szBuff[8192] = { 0 };

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


#ifdef _WIN32
	wchar_t wszLog[8192] = { 0 };
	__MByteToWChar(szBuff, wszLog, sizeof(wszLog) / sizeof(wszLog[0]));
	OutputDebugString(wszLog);
	printf("%s", szBuff);

	if (userptr)
	{
		EasyRtcDevice* pThis = (EasyRtcDevice*)userptr;

		pThis->CallbackLog(szBuff, (int)strlen(szBuff));
	}

#else
	printf("%s", szBuff);
#endif

	return 0;
}

void	EasyRtcDevice::CallbackLog(const char* pbuf, int bufsize)
{
	if (webrtcDevice.deviceCallback)
	{
		webrtcDevice.deviceCallback(webrtcDevice.deviceUserptr, pbuf, bufsize);
	}
}

void	EasyRtcDevice::LockDevice(const char* functionName, const int lineNum)
{
	LockMutex(&mutex);
}
void	EasyRtcDevice::UnlockDevice(const char* functionName, const int lineNum)
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
	EasyRtcDevice* pThis = (EasyRtcDevice*)pThread->userPtr;

	pThread->flag = THREAD_STATUS_RUNNING;

	EasyRtcDevice::__Print__(NULL, __FUNCTION__, __LINE__, false, "check thread startup. [%d]\n", pThread->customId);

	while (1)
	{
		if (pThread->flag == THREAD_STATUS_EXIT)			break;

		pThis->CheckWebRtcPeerConnection();

#ifdef _WIN32
		Sleep(1);
#else
		usleep(1000000);
#endif
	}

	pThread->flag = THREAD_STATUS_INIT;

	EasyRtcDevice::__Print__(NULL, __FUNCTION__, __LINE__, false, "check thread shutdown. [%d]\n", pThread->customId);

	return 0;
}


void func_RtcOnOpen(UINT64 customData, PRtcDataChannel dc)
{
	EasyRtcDevice::WEBRTC_PEER_T* webrtcpeer = (EasyRtcDevice::WEBRTC_PEER_T*)customData;
	char szHello[] = "Hello,EasyRTC";
	EasyRTC_dataChannelSend(dc, FALSE, (PBYTE)szHello, (UINT32)strlen(szHello) + 1);
	EasyRtcDevice::WEBRTC_DEVICE_T* pDevice = (EasyRtcDevice::WEBRTC_DEVICE_T*)webrtcpeer->userptr;
	EasyRtcDevice::__Print__(pDevice->pThis, __FUNCTION__, __LINE__, true, "func_RtcOnOpen........1111111111....\n");
}

void func_RtcOnPictureLoss(UINT64 customData)
{
	EasyRtcDevice::WEBRTC_PEER_T* webrtcpeer = (EasyRtcDevice::WEBRTC_PEER_T*)customData;
	EasyRtcDevice::WEBRTC_DEVICE_T* pDevice = (EasyRtcDevice::WEBRTC_DEVICE_T*)webrtcpeer->userptr;

	//������Ӧ��ִ��ǿ�ƹؼ�֡�Ĳ���,��Ϊ����������ζ�Ŷ�֡��,ֻ���Ǿ��췢�͹ؼ�֡
	//��rtspתwebrtc���������,��Ϊֻ���Ǳ�������,���޷�ǿ�ƹؼ�֡,����������û�������,
	//����ֻҪ�а취��ǿ�ƹؼ�֡,��Ӧ��ȥ������߼�
	EasyRtcDevice::__Print__(pDevice->pThis, __FUNCTION__, __LINE__, true, "func_RtcOnPictureLoss......\n");
}

void func_RtcOnMessage(UINT64 customData, PRtcDataChannel dc, BOOL isBinary, PBYTE msgData, UINT32 msgLen)
{
	EasyRtcDevice::WEBRTC_PEER_T* webrtcpeer = (EasyRtcDevice::WEBRTC_PEER_T*)customData;
	if (NULL == webrtcpeer)			return;
	EasyRtcDevice::WEBRTC_DEVICE_T* pDevice = (EasyRtcDevice::WEBRTC_DEVICE_T*)webrtcpeer->userptr;
	if (NULL == pDevice)			return;


	if (NULL != pDevice->dataChannelCallback)
	{
		EasyRTC_DataChannel_Callback pCB = (EasyRTC_DataChannel_Callback)pDevice->dataChannelCallback;
		pCB(pDevice->dataChannelUserptr, webrtcpeer, isBinary, msgData, msgLen);
	}
	else
	{
		EasyRtcDevice::__Print__(pDevice->pThis, __FUNCTION__, __LINE__, true, "func_RtcOnMessage dc_label=%s isBinary=%d msgLen=%d\n", dc->name, isBinary, msgLen);
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
	EasyRtcDevice::WEBRTC_PEER_T* webrtcpeer = (EasyRtcDevice::WEBRTC_PEER_T*)customData;
	EasyRtcDevice::WEBRTC_DEVICE_T* pDevice = (EasyRtcDevice::WEBRTC_DEVICE_T*)webrtcpeer->userptr;
	if (state == RTC_PEER_CONNECTION_STATE_CONNECTED)
	{
		webrtcpeer->connected = 1;
		//�������������ǿ��I֡�Ľӿں�������I֡�������
		EasyRtcDevice::__Print__(pDevice->pThis, __FUNCTION__, __LINE__, true, "RTC_PEER_CONNECTION_STATE_CONNECTED............\n");

		RtcIceCandidatePairStats RtcIceCandidatePairStats = { 0 };
		STATUS status = EasyRTC_getIceCandidatePairStats(webrtcpeer->peer_, &RtcIceCandidatePairStats);
		if (status != STATUS_SUCCESS)
			return;

		EasyRtcDevice::__Print__(pDevice->pThis, __FUNCTION__, __LINE__, true, "local ice type: %d\n", RtcIceCandidatePairStats.local_iceCandidateType);
		EasyRtcDevice::__Print__(pDevice->pThis, __FUNCTION__, __LINE__, true, "remote ice type: %d\n", RtcIceCandidatePairStats.remote_iceCandidateType);

		EasyRtcDevice::__Print__(pDevice->pThis, __FUNCTION__, __LINE__, true, "local ice port: %d\n", my_ntohs(RtcIceCandidatePairStats.local_ipAddress.port));
		EasyRtcDevice::__Print__(pDevice->pThis, __FUNCTION__, __LINE__, true, "remote ice port: %d\n", my_ntohs(RtcIceCandidatePairStats.remote_ipAddress.port));

		EasyRtcDevice::__Print__(pDevice->pThis, __FUNCTION__, __LINE__, true, "local ice address: %s\n", RtcIceCandidatePairStats.local_ipAddress.strAddress);
		EasyRtcDevice::__Print__(pDevice->pThis, __FUNCTION__, __LINE__, true, "remote ice address: %s\n", RtcIceCandidatePairStats.remote_ipAddress.strAddress);

		if ((RtcIceCandidatePairStats.local_iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED) || (RtcIceCandidatePairStats.remote_iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED))
		{
			//������ת�ķ�ʽ��������������
			EasyRtcDevice::__Print__(pDevice->pThis, __FUNCTION__, __LINE__, true, "This is use turn server exchange data\n");
		}
		else
		{
			//����ֱ���ķ�ʽ��������������
			EasyRtcDevice::__Print__(pDevice->pThis, __FUNCTION__, __LINE__, true, "This is use p2p exchange data\n");
		}
	}
	else if (state == RTC_PEER_CONNECTION_STATE_DISCONNECTED)
	{
		webrtcpeer->disconnected = 1;
		EasyRtcDevice::__Print__(pDevice->pThis, __FUNCTION__, __LINE__, true, "RTC_PEER_CONNECTION_STATE_DISCONNECTED............\n");
	}
	else if (state == RTC_PEER_CONNECTION_STATE_FAILED)
	{
		webrtcpeer->disconnected = 1;
		EasyRtcDevice::__Print__(pDevice->pThis, __FUNCTION__, __LINE__, true, "RTC_PEER_CONNECTION_STATE_FAILED............\n");
	}
	else if (state == RTC_PEER_CONNECTION_STATE_CLOSED)
	{
		webrtcpeer->disconnected = 1;
		EasyRtcDevice::__Print__(pDevice->pThis, __FUNCTION__, __LINE__, true, "RTC_PEER_CONNECTION_STATE_CLOSED............\n");
	}

	if ((state == RTC_PEER_CONNECTION_STATE_DISCONNECTED) || (state == RTC_PEER_CONNECTION_STATE_FAILED) || (state == RTC_PEER_CONNECTION_STATE_CLOSED))
	{
		webrtcpeer->createTime = 0;
	}
}


int		EasyRtcDevice::SendVideoFrame(char* framedata, const int framesize, bool keyframe, uint64_t pts)
{
	if (NULL == framedata)		return -1;
	if (framesize < 1)			return -1;

	LockDevice(__FUNCTION__, __LINE__);

	Frame frame = { 0 };
	frame.frameData = (PBYTE)framedata;
	frame.size = framesize;
	frame.presentationTs = pts * 10000;

	WEBRTC_PEER_MAP::iterator it = webrtcDevice.webrtcPeerMap.begin();
	while (it != webrtcDevice.webrtcPeerMap.end())
	{
		EasyRtcDevice::WEBRTC_PEER_T* peer = it->second;

		if ((peer->connected == 0) || (peer->disconnected == 1))
		{
			it++;
			continue;
		}
		if ((peer->noneedsI == 0) && (!keyframe)) //��ҪIDR֡,���ǵ�ǰ֡�ֲ���IDR֡
		{
			it++;
			continue;
		}

		peer->noneedsI = 1; //����Ҫ�������IDR֡��
		if (keyframe)	frame.flags = FRAME_FLAG_KEY_FRAME;
		EasyRTC_writeFrame(peer->videoTransceiver_, &frame);

		it++;
	}

	UnlockDevice(__FUNCTION__, __LINE__);

	return 0;
}

int		EasyRtcDevice::SendAudioFrame(char* framedata, const int framesize, uint64_t pts)
{
	if (NULL == framedata)		return -1;
	if (framesize < 1)			return -1;

	LockDevice(__FUNCTION__, __LINE__);

	Frame frame = { 0 };
	frame.frameData = (PBYTE)framedata;
	frame.size = framesize;
	frame.presentationTs = pts * 10000;

	WEBRTC_PEER_MAP::iterator it = webrtcDevice.webrtcPeerMap.begin();
	while (it != webrtcDevice.webrtcPeerMap.end())
	{
		EasyRtcDevice::WEBRTC_PEER_T* peer = it->second;

		if ((peer->connected == 0) || (peer->disconnected == 1) || (peer->noneedsI == 0)) //��Ƶ��û���͵�һ֡IDR֡,������Ƶ������
		{
			it++;
			continue;
		}
		EasyRTC_writeFrame(peer->audioTransceiver_, &frame);

		it++;
	}

	UnlockDevice(__FUNCTION__, __LINE__);

	return 0;
}

int		EasyRtcDevice::SendData(WEBRTC_PEER_T* webrtcPeer, BOOL isBinary, PBYTE msgData, UINT32 msgLen)
{
	//if (NULL == webrtcPeer)			return -1;
	//if (NULL == webrtcPeer->dc_)	return -1;
	if (NULL == msgData)			return -1;
	if (msgLen < 1)					return -1;

	if (NULL != webrtcPeer)
	{
		EasyRTC_dataChannelSend(webrtcPeer->dc_, isBinary, msgData, msgLen);
	}
	else
	{
		LockDevice(__FUNCTION__, __LINE__);

		WEBRTC_PEER_MAP::iterator it = webrtcDevice.webrtcPeerMap.begin();
		while (it != webrtcDevice.webrtcPeerMap.end())
		{
			EasyRtcDevice::WEBRTC_PEER_T* peer = it->second;

			if ((peer->connected == 0) || (peer->disconnected == 1))
			{
				it++;
				continue;
			}

			EasyRTC_dataChannelSend(peer->dc_, isBinary, msgData, msgLen);

			it++;
		}

		UnlockDevice(__FUNCTION__, __LINE__);
	}

	return 0;
}

void	EasyRtcDevice::CheckWebRtcPeerQueue()		// ���webrtcPeerMap�Ƿ񳬹����ֵ
{
	LockDevice(__FUNCTION__, __LINE__);

	if (webrtcDevice.webrtcPeerMap.size() + 1 > MAX_WEBRTC_PEERS_COUNT)
	{
		WEBRTC_PEER_MAP::iterator it = webrtcDevice.webrtcPeerMap.begin();
		if (it != webrtcDevice.webrtcPeerMap.end())
		{
			WEBRTC_PEER_T* webrtcPeer = it->second;
			EasyRTC_closePeerConnection(webrtcPeer->peer_);
			free(webrtcPeer);

			webrtcDevice.webrtcPeerMap.erase(it);
		}
	}

	EasyRtcDevice::__Print__(this, __FUNCTION__, __LINE__, true, "=================%s line[%d] webrtcPeer.size:%u\n", __FUNCTION__, __LINE__, webrtcDevice.webrtcPeerMap.size());

	UnlockDevice(__FUNCTION__, __LINE__);
}

void	EasyRtcDevice::AddWebRtcPeer2Map(WEBRTC_PEER_T* peer)		// ���WEBRTC_PEER_T���б�
{
	LockDevice(__FUNCTION__, __LINE__);

	peer->createTime = time(NULL);
	peer->userptr = (void*)&webrtcDevice;
	webrtcDevice.webrtcPeerMap.insert(WEBRTC_PEER_MAP::value_type((uint64_t)peer, peer));

	EasyRtcDevice::__Print__(this, __FUNCTION__, __LINE__, true, "=================%s line[%d] webrtcPeer.size:%u\n", __FUNCTION__, __LINE__, webrtcDevice.webrtcPeerMap.size());

	UnlockDevice(__FUNCTION__, __LINE__);
}

void	EasyRtcDevice::CheckWebRtcPeerConnection()					// �������״̬
{
	LockDevice(__FUNCTION__, __LINE__);

	WEBRTC_PEER_MAP::iterator it = webrtcDevice.webrtcPeerMap.begin();
	while (it != webrtcDevice.webrtcPeerMap.end())
	{
		WEBRTC_PEER_T* webrtcPeer = it->second;

		if (time(NULL) - webrtcPeer->createTime > 60)
		{
			if (webrtcPeer->connected == 0 || webrtcPeer->disconnected == 1)
			{
				EasyRtcDevice::__Print__(this, __FUNCTION__, __LINE__, true, "=================%s line[%d] %s: remove %p\n", __FUNCTION__, __LINE__, webrtcPeer->connected == 0 ? "connect fail" : "disconnected", webrtcPeer);

				EasyRTC_closePeerConnection(webrtcPeer->peer_);
				free(webrtcPeer);

				webrtcDevice.webrtcPeerMap.erase(it);
				it = webrtcDevice.webrtcPeerMap.begin();
				continue;
			}
		}

		it++;
	}

	UnlockDevice(__FUNCTION__, __LINE__);
}

int SignalingClientMessageReceivedCallback(unsigned char* message, int length, uint64_t customData)
{
	EasyRtcDevice* pRtcDevice = (EasyRtcDevice*)customData;

	BASE_MSG_INFO* pBaseInfo = (BASE_MSG_INFO*)message;
	switch (pBaseInfo->msgtype)
	{
	case SDK_ACKLOGINUSER_INFO:
	{
		SDK_ACK_LOGINUSER_INFO* pRecvInfo = (SDK_ACK_LOGINUSER_INFO*)message;
		EasyRtcDevice::__Print__(pRtcDevice, __FUNCTION__, __LINE__, true, "uuidΪ: %08X-%04X-%04X-%04X-%04X%08X �ĵ�¼�����:%d\n", pRecvInfo->myid[0], pRecvInfo->myid[1] >> 16, pRecvInfo->myid[1] & 0xFFFF, pRecvInfo->myid[2] >> 16, pRecvInfo->myid[2] & 0xFFFF, pRecvInfo->myid[3], pRecvInfo->status);
		// ����pRecvInfo->statusֵ��˵��: 0--->��¼�ɹ�; ���ֵΪʧ��
	}
	break;
	case SDK_REQGETWEBRTCOFFER_INFO:
	{
		SDK_REQ_GETWEBRTCOFFER_INFO* pRecvInfo = (SDK_REQ_GETWEBRTCOFFER_INFO*)message;
		EasyRtcDevice::__Print__(pRtcDevice, __FUNCTION__, __LINE__, true, "�յ���uuidΪ: %08X-%04X-%04X-%04X-%04X%08X ����������\n", pRecvInfo->hisid[0], pRecvInfo->hisid[1] >> 16, pRecvInfo->hisid[1] & 0xFFFF, pRecvInfo->hisid[2] >> 16, pRecvInfo->hisid[2] & 0xFFFF, pRecvInfo->hisid[3]);

		// ��ΪǶ��ʽ�豸ֻ���ȶ���֧�ֻ���ֻ׼��֧��ͬʱ��������,������������ﵽ���ֵ,��ֱ��ǿ�ƹر��ʼ���Ǹ�����
		pRtcDevice->CheckWebRtcPeerQueue();

		EasyRtcDevice::WEBRTC_PEER_T* webrtcpeer = (EasyRtcDevice::WEBRTC_PEER_T*)malloc(sizeof(EasyRtcDevice::WEBRTC_PEER_T));
		if (!webrtcpeer)
			return 0;

		memset(webrtcpeer, 0, sizeof(EasyRtcDevice::WEBRTC_PEER_T));
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
				//EasyRtcDevice::__Print__(__FUNCTION__, __LINE__, "%d:%s\n", iceServersCount, conf_.iceServers[iceServersCount].urls);
				iPos += (int)(strlen(pStrDatas + iPos) + 1);
				iceServersCount++;
			}
			else if (pRecvInfo->sturntypes[i] == 0x01)
			{
				// ���Ƿ�juice
				sprintf(conf_.iceServers[iceServersCount].urls, "turn:%s", pStrDatas + iPos);
				//EasyRtcDevice::__Print__(__FUNCTION__, __LINE__, "%d:%s\n", iceServersCount, conf_.iceServers[iceServersCount].urls);
				iPos += (int)(strlen(pStrDatas + iPos) + 1);
			}
			else if (pRecvInfo->sturntypes[i] == 0x02)
			{
				sprintf(conf_.iceServers[iceServersCount].username, "%s", pStrDatas + iPos);
				//EasyRtcDevice::__Print__(__FUNCTION__, __LINE__, "%d:%s\n", iceServersCount, conf_.iceServers[iceServersCount].username);
				iPos += (int)(strlen(pStrDatas + iPos) + 1);
			}
			else if (pRecvInfo->sturntypes[i] == 0x03)
			{
				sprintf(conf_.iceServers[iceServersCount].credential, "%s", pStrDatas + iPos);
				//EasyRtcDevice::__Print__(__FUNCTION__, __LINE__, "%d:%s\n", iceServersCount, conf_.iceServers[iceServersCount].credential);
				iPos += (int)(strlen(pStrDatas + iPos) + 1);
				iceServersCount++; //������һ�� turn, �����Ƿ����˺ź�����,�����봫���˺ź��������,����Ϊ��
				turncount++;
			}
			else if (pRecvInfo->sturntypes[i] == 0x04)
			{
				//����Ϊ�Լ�ʵ�ֵ�turn serverԤ��������
				sprintf(conf_.iceServers[iceServersCount].urls, "turn:%s", pStrDatas + iPos);
				//EasyRtcDevice::__Print__(__FUNCTION__, __LINE__, "%d:%s\n", iceServersCount, conf_.iceServers[iceServersCount].urls);
				iPos += (int)(strlen(pStrDatas + iPos) + 1);
			}
		}

		char* extradata0 = pRecvInfo->sturndatas + pRecvInfo->sturndataslen;
		int extradatalen0 = length - sizeof(SDK_REQ_GETWEBRTCOFFER_INFO) - pRecvInfo->sturndataslen;
		//EasyRtcDevice::__Print__(pRtcDevice, __FUNCTION__, __LINE__, true, "exdata:%s, len:%d\n", extradata0, extradatalen0);

		UINT32 ret = EasyRTC_createPeerConnection(&conf_, &webrtcpeer->peer_, pRecvInfo->hisid, pRecvInfo->sockid);
		EasyRtcDevice::__Print__(pRtcDevice, __FUNCTION__, __LINE__, true, "createPeerConnection: %u\n", ret);

		ret = EasyRTC_peerConnectionOnConnectionStateChange(webrtcpeer->peer_, (UINT64)webrtcpeer, func_RtcOnConnectionStateChange);
		EasyRtcDevice::__Print__(pRtcDevice, __FUNCTION__, __LINE__, true, "peerConnectionOnConnectionStateChange: %u\n", ret);

		RtcMediaStreamTrack video_track_;
		video_track_.codec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;	// Ĭ��ΪH264
		if (pRtcDevice->GetDevice()->videoCodecID == 0xAE)
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
		EasyRtcDevice::__Print__(pRtcDevice, __FUNCTION__, __LINE__, true, "video addTransceiver: %u\n", ret);
		ret = EasyRTC_transceiverOnPictureLoss(webrtcpeer->videoTransceiver_, (UINT64)webrtcpeer, func_RtcOnPictureLoss);
		EasyRtcDevice::__Print__(pRtcDevice, __FUNCTION__, __LINE__, true, "transceiverOnPictureLoss: %u\n", ret);

		int audioCodecID = pRtcDevice->GetDevice()->audioCodecID;

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
			EasyRtcDevice::__Print__(pRtcDevice, __FUNCTION__, __LINE__, true, "audio addTransceiver: %u\n", ret);
		}

		//һ·����Ƶ���� streamId ��ͬ,�� trackId ��ͬ
		//���Ҫ���Ͷ�·����Ƶ,��ô ʹ�ö�� streamId Ӧ�þͿ���

		//datachannel �������Ҫ,Ҳ�ǿ��Խ��������,���ҿ������ò�ͬ������(����һ��������,һ����������)

		RtcDataChannelInit dcInit = { 0 };
		dcInit.ordered = TRUE;
		ret = EasyRTC_createDataChannel(webrtcpeer->peer_, (PCHAR)"op", &dcInit, &webrtcpeer->dc_);
		EasyRtcDevice::__Print__(pRtcDevice, __FUNCTION__, __LINE__, true, "createDataChannel: %u\n", ret);
		ret = EasyRTC_dataChannelOnOpen(webrtcpeer->dc_, (UINT64)webrtcpeer, func_RtcOnOpen);
		EasyRtcDevice::__Print__(pRtcDevice, __FUNCTION__, __LINE__, true, "RTC_dataChannelOnOpen: %u\n", ret);
		ret = EasyRTC_dataChannelOnMessage(webrtcpeer->dc_, (UINT64)webrtcpeer, func_RtcOnMessage);
		EasyRtcDevice::__Print__(pRtcDevice, __FUNCTION__, __LINE__, true, "dataChannelOnMessage: %u\n", ret);

		ret = EasyRTC_createOfferAndSubmit(webrtcpeer->peer_);
		EasyRtcDevice::__Print__(pRtcDevice, __FUNCTION__, __LINE__, true, "RTC_createOfferAndSubmit: %u\n", ret);

		pRtcDevice->AddWebRtcPeer2Map(webrtcpeer);

		break;
	}

	case SDK_NTIWEBRTCANSWER_INFO:
	{
		SDK_NTI_WEBRTCANSWER_INFO* pRecvInfo = (SDK_NTI_WEBRTCANSWER_INFO*)message;
		EasyRtcDevice::__Print__(pRtcDevice, __FUNCTION__, __LINE__, true, "�յ���uuidΪ: %08X-%04X-%04X-%04X-%04X%08X ��answer sdp\n", pRecvInfo->hisid[0], pRecvInfo->hisid[1] >> 16, pRecvInfo->hisid[1] & 0xFFFF, pRecvInfo->hisid[2] >> 16, pRecvInfo->hisid[2] & 0xFFFF, pRecvInfo->hisid[3]);
		EasyRtcDevice::__Print__(pRtcDevice, __FUNCTION__, __LINE__, true, "answer sdp: %s\n", pRecvInfo->answersdp);
		break;
	}
	default:
		break;
	}

	return 0;
}

int SignalingClientStateChangedCallback(SIGNALING_CLIENT_STATE2 state, uint64_t customData)
{
	EasyRtcDevice* pRtcDevice = (EasyRtcDevice*)customData;

	switch (state)
	{
	case SIGNALING_CLIENT_STATE2_READY:
	{
		EasyRtcDevice::__Print__(pRtcDevice, __FUNCTION__, __LINE__, true, "����ͻ���׼�������׽���\n");
		break;
	}
	case SIGNALING_CLIENT_STATE2_CONNECTING:
	{
		EasyRtcDevice::__Print__(pRtcDevice, __FUNCTION__, __LINE__, true, "����ͻ���׼���������������\n");
		break;
	}
	case SIGNALING_CLIENT_STATE2_CONNECTED:
	{
		EasyRtcDevice::__Print__(pRtcDevice, __FUNCTION__, __LINE__, true, "����ͻ��������������������(TCPЭ������������)\n");
		break;
	}
	case SIGNALING_CLIENT_STATE2_WEBSOCKET:
	{
		EasyRtcDevice::__Print__(pRtcDevice, __FUNCTION__, __LINE__, true, "�ɹ�����Ϊ��websocketЭ���������������������\n");
		// RTC_loginUser ֻ���������׽��ִ�TCPЭ����������websocketЭ��֮��,���ܵ���
		char strmysn[64] = "92E22DB5-E74F-4EE2-BAB3-BD8999D15911"; //����У��g_strUUID������������˵ĺϷ���,Ŀǰ���û��У��,����Ūһ��uuid�ַ����Ϳ���
		char strmykey[32] = { 0 }; //�������Ϊ32���ֽڵ��ַ��������ʽ���ڵ�,Ҳ���ǲ�һ��Ҫ��C�ַ���,������ȷ����32���ֽ�,�û������Լ�����ʹ��ĳ�ּ����㷨
		char extradata0[] = "123\0"; //������ڿͻ������ӵ�ʱ��,������׶ξ��յ�����趨���ַ�����,Ҳ�����ڽ���webrtc����֮ǰ���ܱ��ͻ����յ�
		EasyRTC_loginUser(pRtcDevice->GetDevice()->strUUID, strmysn, strmykey, extradata0, 4, NULL, 0); //�����������������ڷ��͸����ַ����鵽�����������ȥ��,�����û���չ���������,Ŀǰ��Ϊû����������������û�����,����������
		break;
	}
	case SIGNALING_CLIENT_STATE2_DISCONNECTED:
	{
		EasyRtcDevice::__Print__(pRtcDevice, __FUNCTION__, __LINE__, true, "����ͻ��������������֮������ӶϿ���,���ص���,�������Զ�����\n");
		break;
	}
	default:
		break;
	}

	return 0;
}

int		EasyRtcDevice::Start(int videoCodecID, int audioCodecID, const char* uuid, bool enableLocalService, EasyRTC_DataChannel_Callback dataChannelCallback, void* dataChannelUserptr)
{
	if (NULL == uuid)					return -1;
	if (0 == strcmp(uuid, "\0"))		return -1;

	if (NULL == webrtcDevice.checkThread)
	{
		webrtcDevice.videoCodecID = videoCodecID;
		webrtcDevice.audioCodecID = audioCodecID;
		memset(webrtcDevice.strUUID, 0x00, sizeof(webrtcDevice.strUUID));
		strcpy(webrtcDevice.strUUID, uuid);
		webrtcDevice.enableLocalService = enableLocalService;
		webrtcDevice.dataChannelCallback = dataChannelCallback;
		webrtcDevice.dataChannelUserptr = dataChannelUserptr;

		webrtcDevice.pThis = this;

		EasyRTC_startSignalingClient(SignalingClientMessageReceivedCallback, SignalingClientStateChangedCallback, (uint64_t)this);

		if (enableLocalService)
		{
			char strmykey[32] = { 0 }; //�������Ϊ32���ֽڵ��ַ��������ʽ���ڵ�,Ҳ���ǲ�һ��Ҫ��C�ַ���,�����ǹ̶���32���ֽ�,�û������Լ�����ʹ��ĳ�ּ����㷨
			char extradata0[] = "123\0"; //������ڿͻ������ӵ�ʱ��,������׶ξ��յ�����趨���ַ�����,Ҳ�����ڽ���webrtc����֮ǰ���ܱ��ͻ����յ�
			EasyRTC_startPrivSignalingServer(6688, webrtcDevice.strUUID, strmykey, extradata0, 4, SignalingClientMessageReceivedCallback, 0);
		}
		CreateOSThread(&webrtcDevice.checkThread, __CheckWebRtcThread, (void*)this, 0);
	}


	return 0;
}

int		EasyRtcDevice::Stop()
{
	DeleteOSThread(&webrtcDevice.checkThread);

	// ɾ������webrtcPeer
	LockDevice(__FUNCTION__, __LINE__);

	WEBRTC_PEER_MAP::iterator it = webrtcDevice.webrtcPeerMap.begin();
	while (it != webrtcDevice.webrtcPeerMap.end())
	{
		WEBRTC_PEER_T* webrtcPeer = it->second;

		EasyRTC_closePeerConnection(webrtcPeer->peer_);
		free(webrtcPeer);

		webrtcDevice.webrtcPeerMap.erase(it);
		it = webrtcDevice.webrtcPeerMap.begin();
	}
	webrtcDevice.webrtcPeerMap.clear();

	UnlockDevice(__FUNCTION__, __LINE__);

	// �Ͽ��ͷ�����������
	EasyRTC_stopSignalingClient();
	if (webrtcDevice.enableLocalService)
	{
		EasyRTC_stopPrivSignalingServer();
	}

	return 0;
}


int		EasyRtcDevice::SetCallback(EasyRTC_Device_Callback callback, void* userptr)
{
	webrtcDevice.deviceCallback = callback;
	webrtcDevice.deviceUserptr = userptr;

	return 0;
}