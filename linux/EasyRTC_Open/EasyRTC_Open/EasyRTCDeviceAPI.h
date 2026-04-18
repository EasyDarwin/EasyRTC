#ifndef __GBS_DEVICE_API_H__
#define __GBS_DEVICE_API_H__

#ifdef _WIN32

#define EASYRTC_DEVICE_API  __declspec(dllexport)
#ifndef LIB_APICALL
#define LIB_APICALL  __stdcall
#endif
//#define WIN32_LEAN_AND_MEAN
#include <stdbool.h>
#else
#define EASYRTC_DEVICE_API  __attribute__ ((visibility("default")))
#define LIB_APICALL 
#define bool int
#endif


typedef enum {

    EASYRTCDevice_Uninitialized = -100

}EASYRTCDevice_ERROR_CODE;

typedef enum {
    EASYRTC_CODEC_NO    =   0,
    EASYRTC_CODEC_H264 = 1,                                                           //!< H264 video codec
    EASYRTC_CODEC_OPUS = 2,                                                           //!< OPUS audio codec
    EASYRTC_CODEC_VP8 = 3,                                                            //!< VP8 video codec.
    EASYRTC_CODEC_MULAW = 4,                                                          //!< MULAW audio codec
    EASYRTC_CODEC_ALAW = 5,                                                           //!< ALAW audio codec
    EASYRTC_CODEC_H265 = 6,                                                           //!< H265 video codec
    EASYRTC_CODEC_AAC = 7,                                                             //!< AAC audio codec
    EASYRTC_CODEC_PCM = 65536
} EASYRTC_CODEC;

typedef enum __EASYRTC_DATA_TYPE_ENUM_E
{
    // 以下5个连接状态必须和底层库中的CONNECT_STATUS_E定义一致

    EASYRTC_CALLBACK_TYPE_DNS_FAIL = 0x01,		    // DNS解析失败
    EASYRTC_CALLBACK_TYPE_CONNECTING,       		// 连接中
    EASYRTC_CALLBACK_TYPE_CONNECTED,				// 连接成功
    EASYRTC_CALLBACK_TYPE_CONNECT_FAIL,				// 连接失败
    EASYRTC_CALLBACK_TYPE_DISCONNECT,				// 连接断开

    EASYRTC_CALLBACK_TYPE_PASSIVE_CALL,             // 被动呼叫

    EASYRTC_CALLBACK_TYPE_START_VIDEO = 0x10,		// 对端请求视频数据
    EASYRTC_CALLBACK_TYPE_START_AUDIO,		        // 对端请求音频数据
    EASYRTC_CALLBACK_TYPE_STOP_VIDEO,	        	// 对端关闭视频数据
    EASYRTC_CALLBACK_TYPE_STOP_AUDIO,		        // 对端关闭音频数据

    EASYRTC_CALLBACK_TYPE_PEER_VIDEO = 0x20,		// 回调对端的视频数据
    EASYRTC_CALLBACK_TYPE_PEER_AUDIO,				// 回调对端的音频数据
    EASYRTC_CALLBACK_TYPE_PEER_ENABLED_CUSTOM_DATA,	// 回调对端已启用DataChannel
    EASYRTC_CALLBACK_TYPE_PEER_CUSTOM_DATA,			// 回调对端的DataChannel数据
    EASYRTC_CALLBACK_TYPE_LOCAL_AUDIO,				// 回调本地回音消除后的音频数据
    EASYRTC_CALLBACK_TYPE_PEER_CONNECTED,           // 连接对端成功
    EASYRTC_CALLBACK_TYPE_PEER_CONNECT_FAIL,        // 连接对端失败
    EASYRTC_CALLBACK_TYPE_PEER_DISCONNECT,          // 与对端连接断开
    EASYRTC_CALLBACK_TYPE_PEER_CLOSED,              // 与对端连接已关闭

    EASYRTC_CALLBACK_TYPE_OFFER,               // Offer SDP
    EASYRTC_CALLBACK_TYPE_ANSWER,              // Answer SDP

    EASYRTC_CALLBACK_TYPE_CALL_SUCCESS = 0x50,      // 被叫端响应成功

    //EASYRTC_CALLBACK_TYPE_PASSIVE_CALL = 0x20,		// 被动呼叫
    //EASYRTC_CALLBACK_TYPE_PROACTIVE_CALL_RESPONSE,  // 主动呼叫响应

    //EASYRTC_CALLBACK_TYPE_CALL_ACCEPT,              // 对方接受呼叫
    //EASYRTC_CALLBACK_TYPE_CALL_REFUSE,              // 对方拒绝呼叫
    //EASYRTC_CALLBACK_TYPE_HANG_UP,                  // 对方挂断

    EASYRTC_CALLBACK_TYPE_ONLINE_DEVICE = 0x80,   // 在线设备列表
}EASYRTC_DATA_TYPE_ENUM_E;

// 数据回调
typedef int (*EasyRTC_Data_Callback)(void* userptr, const char* peerUUID, EASYRTC_DATA_TYPE_ENUM_E dataType, int codecID, int isBinary, char* data, int size, int keyframe, unsigned long long pts);

typedef void* EASYRTC_HANDLE;

#ifdef __cplusplus
extern "C"
{
#endif

    int	EASYRTC_DEVICE_API	EasyRTC_Device_GetVersion(char* version);

    // 初始化环境
    int	EASYRTC_DEVICE_API	EasyRTC_Device_Init();

    // 创建句柄
    // serverAddr: 服务器地址, IP或域名
    // serverPort: 服务器端口
    // accessToken: 密码
    // local_id: 本地设备ID
    // callback: 回调函数
    // userptr: 用户指针, 在回调函数中输出
    int	EASYRTC_DEVICE_API	EasyRTC_Device_Create(EASYRTC_HANDLE* handle, const char* serverAddr, const int serverPort, const int isSecure, const char* local_id, EasyRTC_Data_Callback callback, void* userptr);

    
    // 设置通道信息
    // videoCodecID: 视频编码id
    // audioCodecID: 音频编码id
    int	EASYRTC_DEVICE_API	EasyRTC_Device_SetChannelInfo(EASYRTC_HANDLE handle, EASYRTC_CODEC videoCodecID, EASYRTC_CODEC audioCodecID);

    // 被动呼叫响应
    // peerUUID: 对端id, 可在回调函数中,通过回调类型为EASYRTC_CALLBACK_TYPE_PASSIVE_CALL时获取
    // decline: 1为拒绝, 0为接受
    int EasyRTC_Device_PassiveCallResponse(EASYRTC_HANDLE handle, const char* peerUUID, const int decline);

    // 发送视频帧
    // channelId: 通道id, 从0开始
    // framedata: 视频帧数据
    // framesize: 视频帧大小
    // keyframe: 是否为关键帧
    // pts: 时间戳，单位ms
    int	EASYRTC_DEVICE_API	EasyRTC_Device_SendVideoFrame(EASYRTC_HANDLE handle, char* framedata, const int framesize, int keyframe, unsigned long long pts/*时间戳单位是:毫秒*/);

    // 发送音频帧
    // channelId: 通道id, 从0开始
    // framedata: 音频帧数据
    // framesize: 音频帧大小
    // pts: 时间戳，单位ms
    int	EASYRTC_DEVICE_API	EasyRTC_Device_SendAudioFrame(EASYRTC_HANDLE handle, char* framedata, const int framesize, unsigned long long pts/*时间戳单位是:毫秒*/);

    // 回音消除(仅Android使用)
    int	EASYRTC_DEVICE_API	EasyRTC_Device_aecm_process(EASYRTC_HANDLE handle, short* nearendNoisy, short* outPcmData, int pcmSize);

    // 发送自定义数据到指定对端或所有对端(如果peerUUID为NULL或为""则发送到所有对端)
    // channelId: 通道id, 从0开始
    // peerUUID:  对端id, 可在回调函数中,通过回调类型为EASYRTC_CALLBACK_TYPE_START_VIDEO时获取
    // isBinary:  是否为binary
    // data:      数据
    // size:      数据大小
    int	EASYRTC_DEVICE_API	EasyRTC_Device_SendCustomData(EASYRTC_HANDLE handle, const char* peerUUID, const int isBinary, const char* data, const int size);

    // 连接指定设备
    int	EASYRTC_DEVICE_API	EasyRTC_Caller_Connect(EASYRTC_HANDLE handle, const char* peerUUID);

    // 获取在线设备列表
    int	EASYRTC_DEVICE_API	EasyRTC_GetOnlineDevices(EASYRTC_HANDLE handle, EasyRTC_Data_Callback callback, void* userptr);




    // 挂断
    // channelId: 通道id, 从0开始
    // peerUUID:  对端id, 可在回调函数中,通过回调类型为EASYRTC_CALLBACK_TYPE_START_VIDEO时获取
    int	EASYRTC_DEVICE_API	EasyRTC_Device_Hangup(EASYRTC_HANDLE handle, const char* peerUUID);

    // 释放句柄
    // handle: EasyRTC_Device_Create创建的句柄
    int	EASYRTC_DEVICE_API	EasyRTC_Device_Release(EASYRTC_HANDLE* handle);

    // 反初始化环境
    int	EASYRTC_DEVICE_API	EasyRTC_Device_Deinit();


    /*
    // 调用顺序:

    int __EasyRTC_Data_Callback(void* userptr, const char* peerUUID, EASYRTC_DATA_TYPE_ENUM_E dataType, int codecID, int isBinary, char* data, int size, int keyframe, unsigned long long pts)
    {
        if (EASYRTC_CALLBACK_TYPE_START_VIDEO == dataType)
        {
            // 请求视频
            // 置发送标志位为1, 在视频发送线程中调用EasyRTC_Device_SendVideoFrame发送视频帧
        }
        else if (EASYRTC_CALLBACK_TYPE_START_AUDIO == dataType)
        {
            // 请求音频
            // 置发送标志位为1, 在音频发送线程中调用EasyRTC_Device_SendAudioFrame发送音频帧
        }
        else if (EASYRTC_CALLBACK_TYPE_STOP_VIDEO == dataType)
        {
            // 对方已关闭， 置发送标志为0, 即视频发送线程中不调用EasyRTC_Device_SendVideoFrame
        }
        else if (EASYRTC_CALLBACK_TYPE_STOP_AUDIO == dataType)
        {
            // 对方已关闭， 置发送标志为0, 即音频发送线程中不调用EasyRTC_Device_SendAudioFrame
        }
        else if (EASYRTC_CALLBACK_TYPE_PEER_VIDEO == dataType)
        {
            // codecID: 对方的视频编码ID
            // data: 对方的视频帧数据
            // size: 对方的视频帧大小
        }
        else if (EASYRTC_CALLBACK_TYPE_PEER_AUDIO == dataType)
        {
            // codecID: 此处格式为解码后的PCM
            // data: 对方的音频帧数据
            // size: 对方的音频帧大小
        }
        else
        {
            // ... 请按需处理
        }

        return 0;
    }

    // 初始化环境
    EasyRTC_Device_Init();

    EASYRTC_HANDLE  rtcHandle = NULL;       // 设备句柄

    // 创建设备
    EasyRTC_Device_Create(&rtcHandle, serverIP, serverPort, accessToken, "device", deviceID, channelNum, __EasyRTC_Data_Callback, NULL);
    for (int i = 0; i < channelNum; i++)
    {
        // 设置通道信息
        char code[64] = { 0 };
        sprintf(code, "340200000013200000%02d", i+1);
        EasyRTC_Device_SetChannelInfo(rtcHandle, i, code, EASYRTC_CODEC_H264, EASYRTC_CODEC_ALAW);
    }

    // 在发送线程中调用
    // 根据发送标志位，决定是否发送音视频帧
    // EasyRTC_Device_SendVideoFrame
    // EasyRTC_Device_SendAudioFrame


    // 删除设备
    EasyRTC_Device_Release(&rtcHandle);

    // 反初始化
    EasyRTC_Device_Deinit();

    
    */


#ifdef __cplusplus
}
#endif


#endif