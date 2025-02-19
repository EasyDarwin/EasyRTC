#include "EasyRtcDevice_C.h"
#include <stdarg.h>
#ifndef _WIN32
#include <unistd.h>
#endif

// DataChannel ���ݻص�
int __EasyRTC_DataChannel_Callback(void* userptr, WEBRTC_PEER_T* webrtcPeer, BOOL isBinary, PBYTE msgData, UINT32 msgLen)
{
    // isBinary: �Ƿ�Ϊbin��������, FALSE��ʾΪ�ı�

    if (!isBinary)
    {
        printf("recv msg[%u]: %s\n", msgLen, msgData);
    }

    return 0;
}

int main()
{
    // ��ʼ��
    EasyRTC_Device_Init();


    unsigned int videoCodecID = 0x1C;           // H264(0x1C)       H265(0xAE)
    unsigned int audioCodecID = 0x10007;        // ALAW(0x10007)    MULAW(0x10006)      OPUS(3)

    const char* uuid = "1d1c0297-5cf3-4142-a9e5-255707b83b83";      // �豸����Ψһ��ʶ��ÿ���豸���費ͬ

    // WEBRTC_DEVICE_T �ṹ��
    WEBRTC_DEVICE_T webRTCDevice;
    memset(&webRTCDevice, 0x00, sizeof(WEBRTC_DEVICE_T));

    BOOL enableLocalService = FALSE;        // �����ñ��ط���
    // ���ӷ����� && �ϱ��豸��Ϣ
    EasyRTC_Device_Start(&webRTCDevice, videoCodecID, audioCodecID, uuid, enableLocalService, __EasyRTC_DataChannel_Callback, NULL);

    // ����ʾ��:
    // ������Ƶ�����߳�, ���߳��е���EasyRTC_Device_SendVideoFrame������Ƶ֡
    // ������Ƶ�����߳�, ���߳��е���EasyRTC_Device_SendAudioFrame������Ƶ֡


    // �ɵ���EasyRTC_Device_SendData�����Զ������ݸ��Զ�, ���Զ����ı���Ϣ,����
    // �����ı���Ϣ�����жԶ�
    int num = webRTCDevice.webRTCPeerNum;
    for (int i = 0; i < num; i++)
    {
        if (NULL == webRTCDevice.webrtcPeer[i])     continue;

        WEBRTC_PEER_T* peer = webRTCDevice.webrtcPeer[i];
        if ((peer->connected == 0) || (peer->disconnected == 1))    continue;       // ���������������

        const char* msg = "Hello";
        EasyRTC_Device_SendData(peer, FALSE, (PBYTE)msg, (UINT32)strlen(msg));      // �����ı���Ϣ
    }

    // ���س����˳�
    getchar();

    // �Ͽ��������������
    EasyRTC_Device_Stop(&webRTCDevice);

    // ����ʼ��
    EasyRTC_Device_Deinit();


    return 0;
}