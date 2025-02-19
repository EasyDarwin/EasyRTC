#include "EasyRtcDevice_C.h"
#include <stdarg.h>
#ifndef _WIN32
#include <unistd.h>
#endif

// DataChannel 数据回调
int __EasyRTC_DataChannel_Callback(void* userptr, WEBRTC_PEER_T* webrtcPeer, BOOL isBinary, PBYTE msgData, UINT32 msgLen)
{
    // isBinary: 是否为bin类型数据, FALSE表示为文本

    if (!isBinary)
    {
        printf("recv msg[%u]: %s\n", msgLen, msgData);
    }

    return 0;
}

int main()
{
    // 初始化
    EasyRTC_Device_Init();


    unsigned int videoCodecID = 0x1C;           // H264(0x1C)       H265(0xAE)
    unsigned int audioCodecID = 0x10007;        // ALAW(0x10007)    MULAW(0x10006)      OPUS(3)

    const char* uuid = "1d1c0297-5cf3-4142-a9e5-255707b83b83";      // 设备自身唯一标识，每个设备都需不同

    // WEBRTC_DEVICE_T 结构体
    WEBRTC_DEVICE_T webRTCDevice;
    memset(&webRTCDevice, 0x00, sizeof(WEBRTC_DEVICE_T));

    BOOL enableLocalService = FALSE;        // 不启用本地服务
    // 连接服务器 && 上报设备信息
    EasyRTC_Device_Start(&webRTCDevice, videoCodecID, audioCodecID, uuid, enableLocalService, __EasyRTC_DataChannel_Callback, NULL);

    // 仅作示例:
    // 创建视频发送线程, 在线程中调用EasyRTC_Device_SendVideoFrame发送视频帧
    // 创建音频发送线程, 在线程中调用EasyRTC_Device_SendAudioFrame发送音频帧


    // 可调用EasyRTC_Device_SendData发送自定义数据给对端, 如自定义文本信息,如下
    // 发送文本消息给所有对端
    int num = webRTCDevice.webRTCPeerNum;
    for (int i = 0; i < num; i++)
    {
        if (NULL == webRTCDevice.webrtcPeer[i])     continue;

        WEBRTC_PEER_T* peer = webRTCDevice.webrtcPeer[i];
        if ((peer->connected == 0) || (peer->disconnected == 1))    continue;       // 如果不在线则跳过

        const char* msg = "Hello";
        EasyRTC_Device_SendData(peer, FALSE, (PBYTE)msg, (UINT32)strlen(msg));      // 发送文本消息
    }

    // 按回车键退出
    getchar();

    // 断开与服务器的连接
    EasyRTC_Device_Stop(&webRTCDevice);

    // 反初始化
    EasyRTC_Device_Deinit();


    return 0;
}