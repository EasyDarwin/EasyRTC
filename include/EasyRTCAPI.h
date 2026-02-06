#ifndef __EASYRTC_API_H__
#define __EASYRTC_API_H__

#ifdef _WIN32
//#ifdef _DEBUG
//#include <vld.h>
//#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#define EASYRTC_API  __declspec(dllexport)
#else
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define EASYRTC_API __attribute__ ((visibility("default")))
#define	CALLBACK
#endif

#ifdef ANDROID_BUILD
typedef int                 BOOL, *PBOOL;
typedef signed char         INT8, * PINT8;
typedef signed short        INT16, * PINT16;
typedef signed int          INT32, * PINT32;
//typedef signed long long	INT64, * PINT64;
typedef unsigned char       UINT8, * PUINT8;
typedef unsigned short      UINT16, * PUINT16;
typedef unsigned int        UINT32, * PUINT32;
typedef unsigned long	    UINT64;// , * PUINT64;
typedef unsigned char       BYTE, * PBYTE;
typedef unsigned short      WORD;
typedef float               FLOAT;
typedef FLOAT* PFLOAT;

#ifndef FALSE
#define FALSE               0
#endif

#ifndef TRUE
#define TRUE                1
#endif

#endif

#define MAX_ICE_CONFIG_URI_COUNT 4
#define MAX_ICE_CONFIG_COUNT 5
#define MAX_ICE_SERVERS_COUNT (MAX_ICE_CONFIG_COUNT * MAX_ICE_CONFIG_URI_COUNT + 1)
#define MAX_ICE_CONFIG_URI_LEN 127
#define MAX_ICE_CONFIG_USER_NAME_LEN 256
#define MAX_ICE_CONFIG_CREDENTIAL_LEN 256
#define MAX_MEDIA_STREAM_ID_LEN 255
#define MAX_CANDIDATE_ID_LENGTH 9U

typedef enum __EASYRTC_ERROR_CODE_E
{
    EASYRTC_ERROR_NO    =   0,
    EASYRTC_ERROR_INVALID_ARG =   -1,
    EASYRTC_ERROR_OPERATION_FAILED = -2,
	EASYRTC_ERROR_NOT_ENOUGH_MEMORY=-3,
}EASYRTC_ERROR_CODE_E;

typedef enum {
	EasyRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV = 1, // 发送&接收
	EasyRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY = 2, // 仅发送
	EasyRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY = 3, // 仅接收
	EasyRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE = 4, // 对端无法发送或接收数据
} EasyRTC_RTP_TRANSCEIVER_DIRECTION;

typedef struct {
	EasyRTC_RTP_TRANSCEIVER_DIRECTION direction; // Transceiver方向 - 仅发送、接收、发送
} EasyRTC_RtpTransceiverInit;

typedef enum {
	EasyRTC_ICE_TRANSPORT_POLICY_RELAY = 1, // ICE代理只使用媒体中继
	EasyRTC_ICE_TRANSPORT_POLICY_ALL = 2,   // ICE代理使用任何类型的候选
} EasyRTC_ICE_TRANSPORT_POLICY;

typedef struct {
	char urls[MAX_ICE_CONFIG_URI_LEN + 1];              // STUN/TURN服务器的URL
	char username[MAX_ICE_CONFIG_USER_NAME_LEN + 1];    // TURN服务器使用的用户名
	char credential[MAX_ICE_CONFIG_CREDENTIAL_LEN + 1]; // TURN服务器使用的密码
} EasyRTC_IceServer;

typedef struct {
	EasyRTC_ICE_TRANSPORT_POLICY iceTransportPolicy;        // 指示允许ICE代理使用哪些候选类型
	EasyRTC_IceServer iceServers[MAX_ICE_SERVERS_COUNT];    // 可供ICE使用的服务器,如STUN和TURN服务器
} EasyRTC_Configuration;

typedef enum {
	EasyRTC_MEDIA_STREAM_TRACK_KIND_AUDIO = 1, // 音频轨
	EasyRTC_MEDIA_STREAM_TRACK_KIND_VIDEO = 2, // 视频轨
} EasyRTC_MEDIA_STREAM_TRACK_KIND;

typedef enum {
	EasyRTC_CODEC_H264 = 1,						// H264
	EasyRTC_CODEC_OPUS = 2,						// OPUS
	EasyRTC_CODEC_VP8 = 3,						// VP8
	EasyRTC_CODEC_MULAW = 4,					// MULAW
	EasyRTC_CODEC_ALAW = 5,						// ALAW
	EasyRTC_CODEC_H265 = 6,                     // H265
	EasyRTC_CODEC_AAC = 7,                      // AAC
	EasyRTC_CODEC_UNKNOWN = 8,
} EasyRTC_CODEC;

typedef struct {
	EasyRTC_CODEC codec;                            // 音视频编码ID
	char trackId[MAX_MEDIA_STREAM_ID_LEN + 1];		// 单个轨道ID
	char streamId[MAX_MEDIA_STREAM_ID_LEN + 1];		// 流ID
	EasyRTC_MEDIA_STREAM_TRACK_KIND kind;           // 音视频类型
} EasyRTC_MediaStreamTrack;

typedef struct {
	BOOL isNull;  // 若此标志被设置,则忽略value字段的值
	UINT32 value; // 仅当isNull未设置时该值有效,可设置为无符号32位整数值
} EasyRTC_NullableUint32;

typedef enum {
	EASYRTC_FRAME_FLAG_NONE = 0,						// 未设置任何标志
	EASYRTC_FRAME_FLAG_KEY_FRAME = (1 << 0),			// 关键帧标志 - I帧或IDR帧
	EASYRTC_FRAME_FLAG_DISCARDABLE_FRAME = (1 << 1),	// 可丢弃帧标志 - 无其他帧依赖于该帧
	EASYRTC_FRAME_FLAG_INVISIBLE_FRAME = (1 << 2),		// 不可见帧标志 - 渲染时不显示
	EASYRTC_FRAME_FLAG_END_OF_FRAGMENT = (1 << 3),		// 分片结束的显式标记
} EasyRTC_FRAME_FLAGS;

typedef struct __EasyRTC_Frame {
	UINT32 version;             // 版本号
	UINT32 index;               // 帧标识索引
	EasyRTC_FRAME_FLAGS flags;  // 帧关联标志(如关键帧标志等)
	UINT64 decodingTs;         	// 解码时间戳(100纳秒精度)
	UINT64 presentationTs;     	// 呈现时间戳(100纳秒精度)
	UINT64 duration;           	// 帧持续时间(100纳秒精度,可为0)
	UINT32 size;              	// 帧数据大小(字节数)
	PBYTE frameData;          	// 帧数据比特流
	UINT64 trackId;           	// 该帧所属的音视频轨道标识
} EasyRTC_Frame;

typedef enum {
	EasyRTC_ICE_CANDIDATE_PAIR_STATE_FROZEN = 0,       // 候选对状态：冻结
	EasyRTC_ICE_CANDIDATE_PAIR_STATE_WAITING = 1,      // 候选对状态：等待
	EasyRTC_ICE_CANDIDATE_PAIR_STATE_IN_PROGRESS = 2,  // 候选对状态：进行中
	EasyRTC_ICE_CANDIDATE_PAIR_STATE_SUCCEEDED = 3,    // 候选对状态：成功
	EasyRTC_ICE_CANDIDATE_PAIR_STATE_FAILED = 4,       // 候选对状态：失败
} EasyRTC_ICE_CANDIDATE_PAIR_STATE;

typedef enum {
	EasyRTC_IP_FAMILY_TYPE_IPV4 = (UINT16)0x0001,  // IP地址族类型：IPv4
	EasyRTC_IP_FAMILY_TYPE_IPV6 = (UINT16)0x0002,  // IP地址族类型：IPv6
} EasyRTC_IP_FAMILY_TYPE;

#define IPV6_ADDRESS_LENGTH (UINT16) 16    // IPv6地址字节长度
#define IPV4_ADDRESS_LENGTH (UINT16) 4     // IPv4地址字节长度
#define IPV6_ADDRSTR_LENGTH (UINT16) 46    // IPv6地址字符串最大长度

typedef struct {
	UINT16 family;                          // 地址族类型
	UINT16 port;                           	// 端口号(网络字节序存储)
	BYTE address[IPV6_ADDRESS_LENGTH];     	// IP地址(网络字节序存储)
	char strAddress[IPV6_ADDRSTR_LENGTH];  	// IP地址字符串形式
} EasyRTC_IpAddress;

typedef enum {
	EasyRTC_ICE_CANDIDATE_TYPE_HOST = 0,             // 主机候选者类型
	EasyRTC_ICE_CANDIDATE_TYPE_PEER_REFLEXIVE = 1,   // 对端反射候选者类型
	EasyRTC_ICE_CANDIDATE_TYPE_SERVER_REFLEXIVE = 2, // 服务器反射候选者类型
	EasyRTC_ICE_CANDIDATE_TYPE_RELAYED = 3,          // 中继候选者类型
} EasyRTC_ICE_CANDIDATE_TYPE;// ICE候选者类型枚举

typedef struct {
	char localCandidateId[MAX_CANDIDATE_ID_LENGTH + 1];	// 在RTCIceCandidateStats中检查的本地候选者标识
	char remoteCandidateId[MAX_CANDIDATE_ID_LENGTH + 1];// 在RTCIceCandidateStats中检查的远端候选者标识
	EasyRTC_ICE_CANDIDATE_PAIR_STATE state;				// 本地-远端候选者对的状态检查列表状态
	BOOL nominated;										// 若代理为控制代理则标记为TRUE,否则为FALSE.代理角色基于STUN_ATTRIBUTE_TYPE_USE_CANDIDATE标志确定
	EasyRTC_NullableUint32 circuitBreakerTriggerCount;	// 媒体传输期间触发熔断机制的次数统计,若用户代理未启用此功能则值未定义
	UINT32 packetsDiscardedOnSend; 		// 因套接字错误在此候选对丢弃的数据包总数
	UINT64 packetsSent;					// 此候选对上发送的数据包总数
	UINT64 packetsReceived;				// 此候选对上接收的数据包总数
	UINT64 bytesSent;                   // 此候选对上发送的字节总数(不含包头和填充数据)
	UINT64 bytesReceived;				// 此候选对上接收的字节总数(不含包头和填充数据)
	UINT64 lastPacketSentTimestamp;		// 此特定候选对上最后发送数据包的时间戳(STUN数据包除外)
	UINT64 lastPacketReceivedTimestamp;	// 此特定候选对上最后接收数据包的时间戳(STUN数据包除外)
	UINT64 firstRequestTimestamp;		// 此特定候选对上首次发送STUN请求的时间戳
	UINT64 lastRequestTimestamp;		// 此特定候选对上最后发送STUN请求的时间戳,可由此计算连续两次连通性检查的平均间隔
										// 例如:(lastRequestTimestamp-firstRequestTimestamp)/requestsSent
	UINT64 lastResponseTimestamp;    	// 此特定候选对上最后接收STUN响应的时间戳
	double totalRoundTripTime;       	// 会话开始至今所有往返时间总和(秒),基于STUN连通性检查响应(responsesReceived),包含用于验证许可的请求响应.
										// 可通过将totalRoundTripTime除以responsesReceived计算平均往返时间
	double currentRoundTripTime;     	// 最新往返时间(秒)
	double availableOutgoingBitrate; 	// 此候选对上所有出站RTP流的可用总比特率,由底层拥塞控制算法计算
	double availableIncomingBitrate; 	// 此候选对上所有入站RTP流的可用总比特率,由底层拥塞控制算法计算
	UINT64 requestsReceived;         	// 接收的连通性检查请求总数(含重传)
	UINT64 requestsSent;             	// 发送的连通性检查请求总数(不含重传)
	UINT64 responsesReceived;        	// 接收的连通性检查响应总数
	UINT64 responsesSent;            	// 发送的连通性检查响应总数
	UINT64 bytesDiscardedOnSend;     	// 因套接字错误在此候选对丢弃的字节总数
	EasyRTC_ICE_CANDIDATE_TYPE local_iceCandidateType;
	EasyRTC_ICE_CANDIDATE_TYPE remote_iceCandidateType;
	EasyRTC_IpAddress local_ipAddress;
	EasyRTC_IpAddress remote_ipAddress;
} EasyRTC_IceCandidatePairStats;

typedef enum {
	EASYRTC_PEER_CONNECTION_STATE_NONE = 0,         // 初始状态
	EASYRTC_PEER_CONNECTION_STATE_NEW = 1,          // 当ICE代理等待远程凭证时设置此状态
	EASYRTC_PEER_CONNECTION_STATE_CONNECTING = 2,   // 当ICE代理检查连接时设置此状态
	EASYRTC_PEER_CONNECTION_STATE_CONNECTED = 3,    // 当ICE代理就绪时设置此状态
	EASYRTC_PEER_CONNECTION_STATE_DISCONNECTED = 4, // 当ICE代理断开连接时设置此状态
	EASYRTC_PEER_CONNECTION_STATE_FAILED = 5,       // 当ICE代理转换为失败状态时设置此状态
	EASYRTC_PEER_CONNECTION_STATE_CLOSED = 6,       // 此状态导致流会话终止
	EASYRTC_PEER_CONNECTION_TOTAL_STATE_COUNT = 7,  // 此状态表示对等连接状态的最大数量
} EASYRTC_PEER_CONNECTION_STATE;

typedef enum __EASYRTC_TRANSCEIVER_CALLBACK_TYPE_E
{
	EASYRTC_TRANSCEIVER_CALLBACK_VIDEO_FRAME = 0x01,// 回调视频帧数据
	EASYRTC_TRANSCEIVER_CALLBACK_AUDIO_FRAME,		// 回调音频帧数据
	EASYRTC_TRANSCEIVER_CALLBACK_BANDWIDTH,			// 回调带宽
	EASYRTC_TRANSCEIVER_CALLBACK_KEY_FRAME_REQ,		// 请求关键帧
}EASYRTC_TRANSCEIVER_CALLBACK_TYPE_E;

typedef enum __EASYRTC_DATACHANNEL_CALLBACK_TYPE_E
{
	EASYRTC_DATACHANNEL_CALLBACK_OPENED = 0x01,		// DataChannel 已打开
	EASYRTC_DATACHANNEL_CALLBACK_DATA,				// DataChannel 数据
}EASYRTC_DATACHANNEL_CALLBACK_TYPE_E;


typedef void* EasyRTC_PeerConnection;				// PeerConnection 句柄
typedef void* EasyRTC_Transceiver;					// Transceiver 句柄
typedef void* EasyRTC_DataChannel;					// DataChannel 句柄


typedef int (*EasyRTC_ConnectionStateChange_Callback)(void* userPtr, EASYRTC_PEER_CONNECTION_STATE state);
typedef int (*EasyRTC_Transceiver_Callback)(void* userPtr, EASYRTC_TRANSCEIVER_CALLBACK_TYPE_E type, EasyRTC_CODEC codecID, EasyRTC_Frame* frame, double bandwidthEstimation);
typedef int (*EasyRTC_DataChannel_Callback)(void* userPtr, EASYRTC_DATACHANNEL_CALLBACK_TYPE_E type, BOOL isBinary, const char* msgData, int msgLen);
typedef int (*EasyRTC_IceCandidate_Callback)(void* userPtr, const int isOffer, const char* sdp);

#ifdef __cplusplus
extern "C"
{
#endif

	// 初始化EasyRTC
    int	EASYRTC_API	EasyRTC_Init();
	// 反初始化EasyRTC
    int	EASYRTC_API	EasyRTC_Deinit();

	// 创建PeerConnection
	// 输入:
	//		pRtcConfiguration	配置参数
	//		callback:	状态回调函数
	//		userptr:	用户自定义指针
	// 
	// 输出: ppPeerConnection
    int	EASYRTC_API	EasyRTC_CreatePeerConnection(EasyRTC_PeerConnection* ppPeerConnection, EasyRTC_Configuration* pRtcConfiguration,
													EasyRTC_ConnectionStateChange_Callback callback, void* userptr);
	// 释放PeerConnection
	// 输入:	ppPeerConnection
	int	EASYRTC_API	EasyRTC_ReleasePeerConnection(EasyRTC_PeerConnection* ppPeerConnection);
	
	// 添加Transceiver
	int	EASYRTC_API	EasyRTC_AddTransceiver(EasyRTC_Transceiver* ppTransceiver,
													EasyRTC_PeerConnection pPeerConnection, 
													EasyRTC_MediaStreamTrack* pRtcMediaStreamTrack, 
													EasyRTC_RtpTransceiverInit* pRtcRtpTransceiverInit,
													EasyRTC_Transceiver_Callback callback, void* userptr);
	// 发送音视频帧
	int	EASYRTC_API	EasyRTC_SendFrame(EasyRTC_Transceiver pTransceiver, EasyRTC_Frame* pFrame);
	// 释放Transceiver
	int	EASYRTC_API	EasyRTC_FreeTransceiver(EasyRTC_Transceiver* ppTransceiver);

	// 创建DataChannel,创建后由PeerConnection内部接管,无需外部释放
	int	EASYRTC_API	EasyRTC_AddDataChannel(EasyRTC_DataChannel* ppRtcDataChannel,
													EasyRTC_PeerConnection pPeerConnection, 
													const char* pDataChannelName, 
													EasyRTC_DataChannel_Callback callback, void* userptr);
	// 通过DataChannel发送数据
	int	EASYRTC_API	EasyRTC_DataChannelSend(EasyRTC_DataChannel pRtcDataChannel, BOOL isBinary, const char* pMessage, int messageLen);

	// 释放DataChannel
	int	EASYRTC_API	EasyRTC_FreeDataChannel(EasyRTC_DataChannel* ppRtcDataChannel);


	// 创建Offer,sdp通过回调函数给出
	int	EASYRTC_API	EasyRTC_CreateOffer(EasyRTC_PeerConnection pPeerConnection, EasyRTC_IceCandidate_Callback callback, void* userptr);
	// 设置远端SDP:当本地为设备端时,本地创建了offer发给对端,待对端回复answer后调用EasyRTC_SetRemoteDescription
	int	EASYRTC_API	EasyRTC_SetRemoteDescription(EasyRTC_PeerConnection pPeerConnection, const char* sdp);
	// 创建Answer:根据对端offer的sdp创建answer,sdp通过回调函数给出
	int	EASYRTC_API	EasyRTC_CreateAnswer(EasyRTC_PeerConnection pPeerConnection, const char* offersdp, EasyRTC_IceCandidate_Callback callback, void* userptr);

	// 获取IceCandidate配对统计信息:当EasyRTC_ConnectionStateChange_Callback的回调状态为EASYRTC_PEER_CONNECTION_STATE_CONNECTED时调用,用于区分是P2P还是Relay
	int	EASYRTC_API	EasyRTC_GetIceCandidatePairStats(EasyRTC_PeerConnection pPeerConnection, EasyRTC_IceCandidatePairStats* pRtcIceCandidatePairStats);
#ifdef __cplusplus
};
#endif
#endif
