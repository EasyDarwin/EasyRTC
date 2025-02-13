#ifndef __EASYRTC_H__
#define __EASYRTC_H__

#include <stdint.h>

#if !defined(_WIN32)
typedef char CHAR;
typedef short WCHAR;
typedef uint8_t UINT8;
typedef int8_t INT8;
typedef uint16_t UINT16;
typedef int16_t INT16;
typedef uint32_t UINT32;
typedef int32_t INT32;
typedef uint64_t UINT64;
typedef int64_t INT64;
typedef double DOUBLE;
typedef long double LDOUBLE;
typedef float FLOAT;
typedef bool BOOL;
typedef UINT8 BYTE;
typedef BYTE *PBYTE;
typedef CHAR *PCHAR;

#ifndef VOID
#define VOID void
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif
#endif

#if defined(_MSC_VER) // Microsoft compiler
#define __attribute__(x)
#elif defined(__GNUC__) // GNU compiler
#else
#define __attribute__(x)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define RTC_C_EXPORT

#define MAX_ICE_CONFIG_URI_COUNT 4
#define MAX_ICE_CONFIG_COUNT 5
#define MAX_ICE_SERVERS_COUNT (MAX_ICE_CONFIG_COUNT * MAX_ICE_CONFIG_URI_COUNT + 1)
#define MAX_RTCCONFIGURATION_CERTIFICATES 3

#define MAX_ICE_CONFIG_URI_LEN 127
#define MAX_ICE_CONFIG_USER_NAME_LEN 256
#define MAX_ICE_CONFIG_CREDENTIAL_LEN 256

#define MAX_MEDIA_STREAM_ID_LEN 255

#define MAX_DATA_CHANNEL_PROTOCOL_LEN 255
#define MAX_DATA_CHANNEL_NAME_LEN 255

#define MAX_SESSION_DESCRIPTION_INIT_SDP_LEN 25000

#define MAX_CANDIDATE_ID_LENGTH 9U
#define MAX_STATS_STRING_LENGTH 255U
#define IP_ADDR_STR_LENGTH 45U
#define MAX_TLS_VERSION_LENGTH 8U
#define MAX_DTLS_CIPHER_LENGTH 64U
#define MAX_SRTP_CIPHER_LENGTH 64U
#define MAX_TLS_GROUP_LENGHTH 32U
#define MAX_PROTOCOL_LENGTH 7U
#define MAX_CANDIDATE_TYPE_LENGTH 7U

//这两个是设备端收到的数据包类型
#define SDK_ACKLOGINUSER_INFO 0x20001
#define SDK_REQGETWEBRTCOFFER_INFO 0x20003
#define SDK_NTIWEBRTCANSWER_INFO 0x20004

//这两个是客户端(网页端)收到的数据包类型
#define SDK_ACKCONNECTUSER_INFO 0x20002
#define SDK_NTIWEBRTCOFFER_INFO 0x20005

//这里是错误码
#define STATUS UINT32
#define STATUS_SUCCESS ((STATUS)0x00000000)

#define STATUS_FAILED(x) (((STATUS)(x)) != STATUS_SUCCESS)
#define STATUS_SUCCEEDED(x) (!STATUS_FAILED(x))

#define STATUS_BASE                              0x00000000
#define STATUS_NULL_ARG                          STATUS_BASE + 0x00000001
#define STATUS_INVALID_ARG                       STATUS_BASE + 0x00000002
#define STATUS_INVALID_ARG_LEN                   STATUS_BASE + 0x00000003
#define STATUS_NOT_ENOUGH_MEMORY                 STATUS_BASE + 0x00000004
#define STATUS_BUFFER_TOO_SMALL                  STATUS_BASE + 0x00000005
#define STATUS_UNEXPECTED_EOF                    STATUS_BASE + 0x00000006
#define STATUS_FORMAT_ERROR                      STATUS_BASE + 0x00000007
#define STATUS_INVALID_HANDLE_ERROR              STATUS_BASE + 0x00000008
#define STATUS_OPEN_FILE_FAILED                  STATUS_BASE + 0x00000009
#define STATUS_READ_FILE_FAILED                  STATUS_BASE + 0x0000000a
#define STATUS_WRITE_TO_FILE_FAILED              STATUS_BASE + 0x0000000b
#define STATUS_INTERNAL_ERROR                    STATUS_BASE + 0x0000000c
#define STATUS_INVALID_OPERATION                 STATUS_BASE + 0x0000000d
#define STATUS_NOT_IMPLEMENTED                   STATUS_BASE + 0x0000000e
#define STATUS_OPERATION_TIMED_OUT               STATUS_BASE + 0x0000000f
#define STATUS_NOT_FOUND                         STATUS_BASE + 0x00000010
#define STATUS_CREATE_THREAD_FAILED              STATUS_BASE + 0x00000011
#define STATUS_THREAD_NOT_ENOUGH_RESOURCES       STATUS_BASE + 0x00000012
#define STATUS_THREAD_INVALID_ARG                STATUS_BASE + 0x00000013
#define STATUS_THREAD_PERMISSIONS                STATUS_BASE + 0x00000014
#define STATUS_THREAD_DEADLOCKED                 STATUS_BASE + 0x00000015
#define STATUS_THREAD_DOES_NOT_EXIST             STATUS_BASE + 0x00000016
#define STATUS_JOIN_THREAD_FAILED                STATUS_BASE + 0x00000017
#define STATUS_WAIT_FAILED                       STATUS_BASE + 0x00000018
#define STATUS_CANCEL_THREAD_FAILED              STATUS_BASE + 0x00000019
#define STATUS_THREAD_IS_NOT_JOINABLE            STATUS_BASE + 0x0000001a
#define STATUS_DETACH_THREAD_FAILED              STATUS_BASE + 0x0000001b
#define STATUS_THREAD_ATTR_INIT_FAILED           STATUS_BASE + 0x0000001c
#define STATUS_THREAD_ATTR_SET_STACK_SIZE_FAILED STATUS_BASE + 0x0000001d
#define STATUS_MEMORY_NOT_FREED                  STATUS_BASE + 0x0000001e
#define STATUS_INVALID_THREAD_PARAMS_VERSION     STATUS_BASE + 0x0000001f

#define STATUS_WEBRTC_BASE 0x55000000

#define STATUS_SESSION_DESCRIPTION_INIT_NOT_OBJECT                 STATUS_WEBRTC_BASE + 0x00000001
#define STATUS_SESSION_DESCRIPTION_INIT_MISSING_SDP_OR_TYPE_MEMBER STATUS_WEBRTC_BASE + 0x00000002
#define STATUS_SESSION_DESCRIPTION_INIT_INVALID_TYPE               STATUS_WEBRTC_BASE + 0x00000003
#define STATUS_SESSION_DESCRIPTION_INIT_MISSING_SDP                STATUS_WEBRTC_BASE + 0x00000004
#define STATUS_SESSION_DESCRIPTION_INIT_MISSING_TYPE               STATUS_WEBRTC_BASE + 0x00000005
#define STATUS_SESSION_DESCRIPTION_INIT_MAX_SDP_LEN_EXCEEDED       STATUS_WEBRTC_BASE + 0x00000006
#define STATUS_SESSION_DESCRIPTION_INVALID_SESSION_DESCRIPTION     STATUS_WEBRTC_BASE + 0x00000007
#define STATUS_SESSION_DESCRIPTION_MISSING_ICE_VALUES              STATUS_WEBRTC_BASE + 0x00000008
#define STATUS_SESSION_DESCRIPTION_MISSING_CERTIFICATE_FINGERPRINT STATUS_WEBRTC_BASE + 0x00000009
#define STATUS_SESSION_DESCRIPTION_MAX_MEDIA_COUNT                 STATUS_WEBRTC_BASE + 0x0000000A

#define STATUS_SDP_BASE                         STATUS_WEBRTC_BASE + 0x01000000
#define STATUS_SDP_MISSING_ITEMS                STATUS_SDP_BASE + 0x00000001
#define STATUS_SDP_ATTRIBUTES_ERROR             STATUS_SDP_BASE + 0x00000002
#define STATUS_SDP_BANDWIDTH_ERROR              STATUS_SDP_BASE + 0x00000003
#define STATUS_SDP_CONNECTION_INFORMATION_ERROR STATUS_SDP_BASE + 0x00000004
#define STATUS_SDP_EMAIL_ADDRESS_ERROR          STATUS_SDP_BASE + 0x00000005
#define STATUS_SDP_ENCYRPTION_KEY_ERROR         STATUS_SDP_BASE + 0x00000006
#define STATUS_SDP_INFORMATION_ERROR            STATUS_SDP_BASE + 0x00000007
#define STATUS_SDP_MEDIA_NAME_ERROR             STATUS_SDP_BASE + 0x00000008
#define STATUS_SDP_ORIGIN_ERROR                 STATUS_SDP_BASE + 0x00000009
#define STATUS_SDP_PHONE_NUMBER_ERROR           STATUS_SDP_BASE + 0x0000000A
#define STATUS_SDP_TIME_DECRYPTION_ERROR        STATUS_SDP_BASE + 0x0000000B
#define STATUS_SDP_TIMEZONE_ERROR               STATUS_SDP_BASE + 0x0000000C
#define STATUS_SDP_URI_ERROR                    STATUS_SDP_BASE + 0x0000000D
#define STATUS_SDP_VERSION_ERROR                STATUS_SDP_BASE + 0x0000000E
#define STATUS_SDP_ATTRIBUTE_MAX_EXCEEDED       STATUS_SDP_BASE + 0x0000000F

#define STATUS_STUN_BASE                                           STATUS_SDP_BASE + 0x01000000
#define STATUS_STUN_MESSAGE_INTEGRITY_NOT_LAST                     STATUS_STUN_BASE + 0x00000001
#define STATUS_STUN_MESSAGE_INTEGRITY_SIZE_ALIGNMENT               STATUS_STUN_BASE + 0x00000002
#define STATUS_STUN_FINGERPRINT_NOT_LAST                           STATUS_STUN_BASE + 0x00000003
#define STATUS_STUN_MAGIC_COOKIE_MISMATCH                          STATUS_STUN_BASE + 0x00000004
#define STATUS_STUN_INVALID_ADDRESS_ATTRIBUTE_LENGTH               STATUS_STUN_BASE + 0x00000005
#define STATUS_STUN_INVALID_USERNAME_ATTRIBUTE_LENGTH              STATUS_STUN_BASE + 0x00000006
#define STATUS_STUN_INVALID_MESSAGE_INTEGRITY_ATTRIBUTE_LENGTH     STATUS_STUN_BASE + 0x00000007
#define STATUS_STUN_INVALID_FINGERPRINT_ATTRIBUTE_LENGTH           STATUS_STUN_BASE + 0x00000008
#define STATUS_STUN_MULTIPLE_MESSAGE_INTEGRITY_ATTRIBUTES          STATUS_STUN_BASE + 0x00000009
#define STATUS_STUN_MULTIPLE_FINGERPRINT_ATTRIBUTES                STATUS_STUN_BASE + 0x0000000A
#define STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY STATUS_STUN_BASE + 0x0000000B
#define STATUS_STUN_MESSAGE_INTEGRITY_AFTER_FINGERPRINT            STATUS_STUN_BASE + 0x0000000C
#define STATUS_STUN_MAX_ATTRIBUTE_COUNT                            STATUS_STUN_BASE + 0x0000000D
#define STATUS_STUN_MESSAGE_INTEGRITY_MISMATCH                     STATUS_STUN_BASE + 0x0000000E
#define STATUS_STUN_FINGERPRINT_MISMATCH                           STATUS_STUN_BASE + 0x0000000F
#define STATUS_STUN_INVALID_PRIORITY_ATTRIBUTE_LENGTH              STATUS_STUN_BASE + 0x00000010
#define STATUS_STUN_INVALID_USE_CANDIDATE_ATTRIBUTE_LENGTH         STATUS_STUN_BASE + 0x00000011
#define STATUS_STUN_INVALID_LIFETIME_ATTRIBUTE_LENGTH              STATUS_STUN_BASE + 0x00000012
#define STATUS_STUN_INVALID_REQUESTED_TRANSPORT_ATTRIBUTE_LENGTH   STATUS_STUN_BASE + 0x00000013
#define STATUS_STUN_INVALID_REALM_ATTRIBUTE_LENGTH                 STATUS_STUN_BASE + 0x00000014
#define STATUS_STUN_INVALID_NONCE_ATTRIBUTE_LENGTH                 STATUS_STUN_BASE + 0x00000015
#define STATUS_STUN_INVALID_ERROR_CODE_ATTRIBUTE_LENGTH            STATUS_STUN_BASE + 0x00000016
#define STATUS_STUN_INVALID_ICE_CONTROL_ATTRIBUTE_LENGTH           STATUS_STUN_BASE + 0x00000017
#define STATUS_STUN_INVALID_CHANNEL_NUMBER_ATTRIBUTE_LENGTH        STATUS_STUN_BASE + 0x00000018
#define STATUS_STUN_INVALID_CHANGE_REQUEST_ATTRIBUTE_LENGTH        STATUS_STUN_BASE + 0x00000019

#define STATUS_NETWORKING_BASE                     STATUS_STUN_BASE + 0x01000000
#define STATUS_GET_LOCAL_IP_ADDRESSES_FAILED       STATUS_NETWORKING_BASE + 0x00000016
#define STATUS_CREATE_UDP_SOCKET_FAILED            STATUS_NETWORKING_BASE + 0x00000017
#define STATUS_BINDING_SOCKET_FAILED               STATUS_NETWORKING_BASE + 0x00000018
#define STATUS_GET_PORT_NUMBER_FAILED              STATUS_NETWORKING_BASE + 0x00000019
#define STATUS_SEND_DATA_FAILED                    STATUS_NETWORKING_BASE + 0x0000001a
#define STATUS_RESOLVE_HOSTNAME_FAILED             STATUS_NETWORKING_BASE + 0x0000001b
#define STATUS_HOSTNAME_NOT_FOUND                  STATUS_NETWORKING_BASE + 0x0000001c
#define STATUS_SOCKET_CONNECT_FAILED               STATUS_NETWORKING_BASE + 0x0000001d
#define STATUS_CREATE_SSL_FAILED                   STATUS_NETWORKING_BASE + 0x0000001e
#define STATUS_SSL_CONNECTION_FAILED               STATUS_NETWORKING_BASE + 0x0000001f
#define STATUS_SECURE_SOCKET_READ_FAILED           STATUS_NETWORKING_BASE + 0x00000020
#define STATUS_SOCKET_CONNECTION_NOT_READY_TO_SEND STATUS_NETWORKING_BASE + 0x00000021
#define STATUS_SOCKET_CONNECTION_CLOSED_ALREADY    STATUS_NETWORKING_BASE + 0x00000022
#define STATUS_SOCKET_SET_SEND_BUFFER_SIZE_FAILED  STATUS_NETWORKING_BASE + 0x00000023
#define STATUS_GET_SOCKET_FLAG_FAILED              STATUS_NETWORKING_BASE + 0x00000024
#define STATUS_SET_SOCKET_FLAG_FAILED              STATUS_NETWORKING_BASE + 0x00000025
#define STATUS_CLOSE_SOCKET_FAILED                 STATUS_NETWORKING_BASE + 0x00000026
#define STATUS_CREATE_SOCKET_PAIR_FAILED           STATUS_NETWORKING_BASE + 0x00000027
#define STATUS_SOCKET_WRITE_FAILED                 STATUS_NETWORKING_BASE + 0X00000028
#define STATUS_INVALID_ADDRESS_LENGTH              STATUS_NETWORKING_BASE + 0X00000029

#define STATUS_DTLS_BASE                                  STATUS_NETWORKING_BASE + 0x01000000
#define STATUS_CERTIFICATE_GENERATION_FAILED              STATUS_DTLS_BASE + 0x00000001
#define STATUS_SSL_CTX_CREATION_FAILED                    STATUS_DTLS_BASE + 0x00000002
#define STATUS_SSL_REMOTE_CERTIFICATE_VERIFICATION_FAILED STATUS_DTLS_BASE + 0x00000003
#define STATUS_SSL_PACKET_BEFORE_DTLS_READY               STATUS_DTLS_BASE + 0x00000004
#define STATUS_SSL_UNKNOWN_SRTP_PROFILE                   STATUS_DTLS_BASE + 0x00000005
#define STATUS_SSL_INVALID_CERTIFICATE_BITS               STATUS_DTLS_BASE + 0x00000006
#define STATUS_DTLS_SESSION_ALREADY_FREED                 STATUS_DTLS_BASE + 0x00000007

#define STATUS_ICE_BASE                                                    STATUS_DTLS_BASE + 0x01000000
#define STATUS_ICE_AGENT_NO_SELECTED_CANDIDATE_AVAILABLE                   STATUS_ICE_BASE + 0x00000001
#define STATUS_ICE_CANDIDATE_STRING_MISSING_PORT                           STATUS_ICE_BASE + 0x00000002
#define STATUS_ICE_CANDIDATE_STRING_MISSING_IP                             STATUS_ICE_BASE + 0x00000003
#define STATUS_ICE_CANDIDATE_STRING_INVALID_IP                             STATUS_ICE_BASE + 0x00000004
#define STATUS_ICE_CANDIDATE_STRING_IS_TCP                                 STATUS_ICE_BASE + 0x00000005
#define STATUS_ICE_FAILED_TO_COMPUTE_MD5_FOR_LONG_TERM_CREDENTIAL          STATUS_ICE_BASE + 0x00000006
#define STATUS_ICE_URL_INVALID_PREFIX                                      STATUS_ICE_BASE + 0x00000007
#define STATUS_ICE_URL_TURN_MISSING_USERNAME                               STATUS_ICE_BASE + 0x00000008
#define STATUS_ICE_URL_TURN_MISSING_CREDENTIAL                             STATUS_ICE_BASE + 0x00000009
#define STATUS_ICE_AGENT_STATE_CHANGE_FAILED                               STATUS_ICE_BASE + 0x0000000a
#define STATUS_ICE_NO_LOCAL_CANDIDATE_AVAILABLE_AFTER_GATHERING_TIMEOUT    STATUS_ICE_BASE + 0x0000000b
#define STATUS_ICE_AGENT_TERMINATED_ALREADY                                STATUS_ICE_BASE + 0x0000000c
#define STATUS_ICE_NO_CONNECTED_CANDIDATE_PAIR                             STATUS_ICE_BASE + 0x0000000d
#define STATUS_ICE_CANDIDATE_PAIR_LIST_EMPTY                               STATUS_ICE_BASE + 0x0000000e
#define STATUS_ICE_NOMINATED_CANDIDATE_NOT_CONNECTED                       STATUS_ICE_BASE + 0x00000010
#define STATUS_ICE_CANDIDATE_INIT_MALFORMED                                STATUS_ICE_BASE + 0x00000011
#define STATUS_ICE_CANDIDATE_MISSING_CANDIDATE                             STATUS_ICE_BASE + 0x00000012
#define STATUS_ICE_FAILED_TO_NOMINATE_CANDIDATE_PAIR                       STATUS_ICE_BASE + 0x00000013
#define STATUS_ICE_MAX_REMOTE_CANDIDATE_COUNT_EXCEEDED                     STATUS_ICE_BASE + 0x00000014
#define STATUS_ICE_INVALID_STATE                                           STATUS_ICE_BASE + 0x0000001c
#define STATUS_ICE_NO_LOCAL_HOST_CANDIDATE_AVAILABLE                       STATUS_ICE_BASE + 0x0000001d
#define STATUS_ICE_NO_NOMINATED_VALID_CANDIDATE_PAIR_AVAILABLE             STATUS_ICE_BASE + 0x0000001e
#define STATUS_TURN_CONNECTION_NO_HOST_INTERFACE_FOUND                     STATUS_ICE_BASE + 0x0000001f
#define STATUS_TURN_CONNECTION_STATE_TRANSITION_TIMEOUT                    STATUS_ICE_BASE + 0x00000020
#define STATUS_TURN_CONNECTION_FAILED_TO_CREATE_PERMISSION                 STATUS_ICE_BASE + 0x00000021
#define STATUS_TURN_CONNECTION_FAILED_TO_BIND_CHANNEL                      STATUS_ICE_BASE + 0x00000022
#define STATUS_TURN_NEW_DATA_CHANNEL_MSG_HEADER_BEFORE_PREVIOUS_MSG_FINISH STATUS_ICE_BASE + 0x00000023
#define STATUS_TURN_MISSING_CHANNEL_DATA_HEADER                            STATUS_ICE_BASE + 0x00000024
#define STATUS_ICE_FAILED_TO_RECOVER_FROM_DISCONNECTION                    STATUS_ICE_BASE + 0x00000025
#define STATUS_ICE_NO_AVAILABLE_ICE_CANDIDATE_PAIR                         STATUS_ICE_BASE + 0x00000026
#define STATUS_TURN_CONNECTION_PEER_NOT_USABLE                             STATUS_ICE_BASE + 0x00000027
#define STATUS_ICE_SERVER_INDEX_INVALID                                    STATUS_ICE_BASE + 0x00000028
#define STATUS_ICE_CANDIDATE_STRING_MISSING_TYPE                           STATUS_ICE_BASE + 0x00000029
#define STATUS_TURN_CONNECTION_ALLOCATION_FAILED                           STATUS_ICE_BASE + 0x0000002a
#define STATUS_TURN_INVALID_STATE                                          STATUS_ICE_BASE + 0x0000002b
#define STATUS_TURN_CONNECTION_GET_CREDENTIALS_FAILED                      STATUS_ICE_BASE + 0x0000002c

#define STATUS_SRTP_BASE                             STATUS_ICE_BASE + 0x01000000
#define STATUS_SRTP_DECRYPT_FAILED                   STATUS_SRTP_BASE + 0x00000001
#define STATUS_SRTP_ENCRYPT_FAILED                   STATUS_SRTP_BASE + 0x00000002
#define STATUS_SRTP_TRANSMIT_SESSION_CREATION_FAILED STATUS_SRTP_BASE + 0x00000003
#define STATUS_SRTP_RECEIVE_SESSION_CREATION_FAILED  STATUS_SRTP_BASE + 0x00000004
#define STATUS_SRTP_INIT_FAILED                      STATUS_SRTP_BASE + 0x00000005
#define STATUS_SRTP_NOT_READY_YET                    STATUS_SRTP_BASE + 0x00000006

#define STATUS_RTP_BASE                   STATUS_SRTP_BASE + 0x01000000
#define STATUS_RTP_INPUT_PACKET_TOO_SMALL STATUS_RTP_BASE + 0x00000001
#define STATUS_RTP_INPUT_MTU_TOO_SMALL    STATUS_RTP_BASE + 0x00000002
#define STATUS_RTP_INVALID_NALU           STATUS_RTP_BASE + 0x00000003
#define STATUS_RTP_INVALID_EXTENSION_LEN  STATUS_RTP_BASE + 0x00000004

#define STATUS_SIGNALING_BASE STATUS_RTP_BASE + 0x01000000

#define STATUS_PEERCONNECTION_BASE                                     STATUS_SIGNALING_BASE + 0x01000000
#define STATUS_PEERCONNECTION_CREATE_ANSWER_WITHOUT_REMOTE_DESCRIPTION STATUS_PEERCONNECTION_BASE + 0x00000001
#define STATUS_PEERCONNECTION_CODEC_INVALID                            STATUS_PEERCONNECTION_BASE + 0x00000002
#define STATUS_PEERCONNECTION_CODEC_MAX_EXCEEDED                       STATUS_PEERCONNECTION_BASE + 0x00000003
#define STATUS_PEERCONNECTION_EARLY_DNS_RESOLUTION_FAILED              STATUS_PEERCONNECTION_BASE + 0x00000004

#define STATUS_SCTP_BASE                 STATUS_PEERCONNECTION_BASE + 0x01000000
#define STATUS_SCTP_SESSION_SETUP_FAILED STATUS_SCTP_BASE + 0x00000001
#define STATUS_SCTP_INVALID_DCEP_PACKET  STATUS_SCTP_BASE + 0x00000002

#define STATUS_RTCP_BASE                         STATUS_SCTP_BASE + 0x01000000
#define STATUS_RTCP_INPUT_PACKET_TOO_SMALL       STATUS_RTCP_BASE + 0x00000001
#define STATUS_RTCP_INPUT_PACKET_INVALID_VERSION STATUS_RTCP_BASE + 0x00000002
#define STATUS_RTCP_INPUT_PACKET_LEN_MISMATCH    STATUS_RTCP_BASE + 0x00000003
#define STATUS_RTCP_INPUT_NACK_LIST_INVALID      STATUS_RTCP_BASE + 0x00000004
#define STATUS_RTCP_INPUT_SSRC_INVALID           STATUS_RTCP_BASE + 0x00000005
#define STATUS_RTCP_INPUT_PARTIAL_PACKET         STATUS_RTCP_BASE + 0x00000006
#define STATUS_RTCP_INPUT_REMB_TOO_SMALL         STATUS_RTCP_BASE + 0x00000007
#define STATUS_RTCP_INPUT_REMB_INVALID           STATUS_RTCP_BASE + 0x00000008

#define STATUS_ROLLING_BUFFER_BASE         STATUS_RTCP_BASE + 0x01000000
#define STATUS_ROLLING_BUFFER_NOT_IN_RANGE STATUS_ROLLING_BUFFER_BASE + 0x00000001

typedef enum {
	ICE_TRANSPORT_POLICY_RELAY = 1, //!< The ICE Agent uses only media relay candidates such as candidates
									//!< passing through a TURN server

	ICE_TRANSPORT_POLICY_ALL = 2, //!< The ICE Agent can use any type of candidate when this value is specified.
} ICE_TRANSPORT_POLICY;

typedef enum {
	RTC_PEER_CONNECTION_STATE_NONE = 0,         //!< Starting state of peer connection
	RTC_PEER_CONNECTION_STATE_NEW = 1,          //!< This state is set when ICE Agent is waiting for remote credential
	RTC_PEER_CONNECTION_STATE_CONNECTING = 2,   //!< This state is set when ICE agent checks connection
	RTC_PEER_CONNECTION_STATE_CONNECTED = 3,    //!< This state is set when CIE Agent is ready
	RTC_PEER_CONNECTION_STATE_DISCONNECTED = 4, //!< This state is set when ICE Agent is disconnected
	RTC_PEER_CONNECTION_STATE_FAILED = 5,       //!< This state is set when ICE Agent transitions to fail state
	RTC_PEER_CONNECTION_STATE_CLOSED = 6,       //!< This state leads to termination of streaming session
	RTC_PEER_CONNECTION_TOTAL_STATE_COUNT = 7,  //!< This state indicates maximum number of Peer connection states
} RTC_PEER_CONNECTION_STATE;

typedef enum {
	RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE = 1, //!< H264 video codec
	RTC_CODEC_OPUS = 2,															  //!< OPUS audio codec
	RTC_CODEC_VP8 = 3,															  //!< VP8 video codec.
	RTC_CODEC_MULAW = 4,														  //!< MULAW audio codec
	RTC_CODEC_ALAW = 5,															  //!< ALAW audio codec
	RTC_CODEC_H265 = 6,                                                           //!< H265 video codec
	RTC_CODEC_AAC = 7,                                                            //!< AAC audio codec
	RTC_CODEC_UNKNOWN = 8,
} RTC_CODEC;

typedef enum {
	MEDIA_STREAM_TRACK_KIND_AUDIO = 1, //!< Audio track. Track information is set before add transceiver
	MEDIA_STREAM_TRACK_KIND_VIDEO = 2, //!< Video track. Track information is set before add transceiver
} MEDIA_STREAM_TRACK_KIND;

typedef enum {
	RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV = 1, //!< This indicates that peer can send and receive data
	RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY = 2, //!< This indicates that the peer can only send information
	RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY = 3, //!< This indicates that the peer can only receive information
	RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE = 4, //!< This indicates that the peer can not send or receive data
} RTC_RTP_TRANSCEIVER_DIRECTION;

typedef struct {
	RTC_RTP_TRANSCEIVER_DIRECTION direction; //!< Transceiver direction - SENDONLY, RECVONLY, SENDRECV
} RtcRtpTransceiverInit, * PRtcRtpTransceiverInit;

typedef enum {
	/**
	 * No flags are set
	 */
	FRAME_FLAG_NONE = 0,

	/**
	 * The frame is a key frame - I or IDR
	 */
	FRAME_FLAG_KEY_FRAME = (1 << 0),

	/**
	 * The frame is discardable - no other frames depend on it
	 */
	FRAME_FLAG_DISCARDABLE_FRAME = (1 << 1),

	/**
	 * The frame is invisible for rendering
	 */
	FRAME_FLAG_INVISIBLE_FRAME = (1 << 2),

	/**
	 * The frame is an explicit marker for the end-of-fragment
	 */
	FRAME_FLAG_END_OF_FRAGMENT = (1 << 3),
} FRAME_FLAGS;

typedef enum {
	SDP_TYPE_OFFER = 1,  //!< SessionDescription is type offer
	SDP_TYPE_ANSWER = 2, //!< SessionDescription is type answer
} SDP_TYPE;

typedef struct {
	SDP_TYPE type;                                      //!< Indicates an offer/answer SDP type
	BOOL useTrickleIce;                                 //!< Indicates if an offer should set trickle ice
	CHAR sdp[MAX_SESSION_DESCRIPTION_INIT_SDP_LEN + 1]; //!< SDP Data containing media capabilities, transport addresses
														//!< and related metadata in a transport agnostic manner
														//!<
} RtcSessionDescriptionInit, * PRtcSessionDescriptionInit;

typedef struct {
	CHAR urls[MAX_ICE_CONFIG_URI_LEN + 1];              //!< URL of STUN/TURN Server
	CHAR username[MAX_ICE_CONFIG_USER_NAME_LEN + 1];    //!< Username to be used with TURN server
	CHAR credential[MAX_ICE_CONFIG_CREDENTIAL_LEN + 1]; //!< Password to be used with TURN server
} RtcIceServer, * PRtcIceServer;

typedef BOOL(*IceSetInterfaceFilterFunc)(UINT64, PCHAR);

typedef struct {
	UINT16 maximumTransmissionUnit; //!< Controls the size of the largest packet the WebRTC SDK will send
									//!< Some networks may drop packets if they exceed a certain size, and is useful in those conditions.
									//!< A smaller MTU will incur higher bandwidth usage however since more packets will be generated with
									//!< smaller payloads. If unset DEFAULT_MTU_SIZE_BYTES will be used

	UINT32 iceLocalCandidateGatheringTimeout; //!< Maximum time ice will wait for gathering STUN and RELAY candidates. Once
											  //!< it's reached, ice will proceed with whatever candidate it current has. Use default value if 0.

	UINT32 iceConnectionCheckTimeout; //!< Maximum time allowed waiting for at least one ice candidate pair to receive
									  //!< binding response from the peer. Use default value if 0.

	UINT32 iceCandidateNominationTimeout; //!< If client is ice controlling, this is the timeout for receiving bind response of requests that has
										  //!< USE_CANDIDATE attribute. If client is ice controlled, this is the timeout for receiving binding request
										  //!< that has USE_CANDIDATE attribute after connection check is done. Use default value if 0.

	UINT32 iceConnectionCheckPollingInterval; //!< Ta in https://datatracker.ietf.org/doc/html/rfc8445#section-14.2
											  //!< rate at which binding request packets are sent during connection check. Use default interval if 0.

	INT32 generatedCertificateBits; //!< GeneratedCertificateBits controls the amount of bits the locally generated self-signed certificate uses
									//!< A smaller amount of bits may result in less CPU usage on startup, but will cause a weaker certificate to be
									//!< generated If set to 0 the default GENERATED_CERTIFICATE_BITS will be used

	BOOL generateRSACertificate; //!< GenerateRSACertificate controls if an ECDSA or RSA certificate is generated.
								 //!< By default we generate an ECDSA certificate but some platforms may not support them.

	UINT32 sendBufSize; //!< Socket send buffer length. Item larger then this size will get dropped. Use system default if 0.

	UINT32 recvBufSize; //!< Socket recv buffer length. Item larger then this size will get dropped. Use system default if 0.

	UINT64 filterCustomData; //!< Custom Data that can be populated by the developer while developing filter function

	IceSetInterfaceFilterFunc iceSetInterfaceFilterFunc; //!< Filter function callback to be set when the developer
														 //!< would like to whitelist/blacklist specific network interfaces

	BOOL disableSenderSideBandwidthEstimation; //!< Disable TWCC feedback based sender bandwidth estimation, enabled by default.
											   //!< You want to set this to TRUE if you are on a very stable connection and want to save 1.2MB of
											   //!< memory
} KvsRtcConfiguration, * PKvsRtcConfiguration;

typedef struct {
	// The certificate bits and the size
	PBYTE pCertificate;     //!< Certificate bits
	UINT32 certificateSize; //!< Size of certificate in bytes (optional)

	// The private key bits and the size in bytes
	PBYTE pPrivateKey;     //!< Private key bit
	UINT32 privateKeySize; //!< Size of private key in bytes (optional)
} RtcCertificate, * PRtcCertificate;

typedef struct {
	ICE_TRANSPORT_POLICY iceTransportPolicy;        //!< Indicates which candidates the ICE Agent is allowed to use.
	RtcIceServer iceServers[MAX_ICE_SERVERS_COUNT]; //!< Servers available to be used by ICE, such as STUN and TURN servers.
	KvsRtcConfiguration kvsRtcConfiguration;        //!< Non-standard configuration options

	RtcCertificate
		certificates[MAX_RTCCONFIGURATION_CERTIFICATES]; //!< Set of certificates that the RtcPeerConnection uses to authenticate.
														 //!< Although any given DTLS connection will use only one certificate, this
														 //!< attribute allows the caller to provide multiple certificates that support
														 //!< different algorithms.
														 //!<
														 //!< If this value is absent, then a default set of certificates is generated
														 //!< for each RtcPeerConnection.
														 //!<
														 //!< An absent value is determined by the certificate pointing to NULL
														 //!<
														 //!< Doc: https://www.w3.org/TR/webrtc/#dom-rtcconfiguration-certificates
														 //!<
														 //!< !!!!!!!!!! IMPORTANT !!!!!!!!!!
														 //!< It is recommended to rotate the certificates often - preferably for every peer
														 //!< connection to avoid a compromised client weakening the security of the new connections.
														 //!<
														 //!< NOTE: The certificates, if specified, can be freed after the peer connection create call
														 //!<
} RtcConfiguration, * PRtcConfiguration;

typedef struct {
	UINT32 version; //!< Version of peer connection structure
} RtcPeerConnection, * PRtcPeerConnection;

typedef struct {
	RTC_CODEC codec;                            //!< non-standard, codec that the track is using
	CHAR trackId[MAX_MEDIA_STREAM_ID_LEN + 1];  //!< non-standard, id of this individual track
	CHAR streamId[MAX_MEDIA_STREAM_ID_LEN + 1]; //!< non-standard, id of the MediaStream this track belongs too
	MEDIA_STREAM_TRACK_KIND kind;               //!< Kind of track - audio or video
} RtcMediaStreamTrack, * PRtcMediaStreamTrack;

typedef struct {
	RtcMediaStreamTrack track; //!< Track with details of codec, trackId, streamId and track kind
} RtcRtpReceiver, * PRtcRtpReceiver;

typedef struct {
	RTC_RTP_TRANSCEIVER_DIRECTION direction; //!< Transceiver direction
	RtcRtpReceiver receiver;                 //!< RtcRtpReceiver that has track specific information
} RtcRtpTransceiver, * PRtcRtpTransceiver;

typedef struct {
	BOOL isNull;  //!< If this value is set, the value field will be ignored
	UINT16 value; //!< This value is used only if isNull is not set. Can be set to a unsigned 16 bit value
} NullableUint16;

typedef struct {
	BOOL isNull;  //!< If this value is set, the value field will be ignored
	UINT32 value; //!< This value is used only if isNull is not set. Can be set to a unsigned 32 bit value
} NullableUint32;

typedef struct {
	BOOL ordered;                                     //!< Decides the order in which data is sent. If true, data is sent in order
	NullableUint16 maxPacketLifeTime;                 //!< Limits the time (in milliseconds) during which the channel will (re)transmit
													  //!< data if not acknowledged. This value may be clamped if it exceeds the maximum
													  //!< value supported by the user agent.
	NullableUint16 maxRetransmits;                    //!< Control number of times a channel retransmits data if not delivered successfully
	CHAR protocol[MAX_DATA_CHANNEL_PROTOCOL_LEN + 1]; //!< Sub protocol name for the channel
	BOOL negotiated;                                  //!< If set to true, it is up to the application to negotiate the channel and create an
													  //!< RTCDataChannel object with the same id as the other peer.
} RtcDataChannelInit, * PRtcDataChannelInit;

typedef struct __RtcDataChannel {
	CHAR name[MAX_DATA_CHANNEL_NAME_LEN + 1]; //!< Define name of data channel. Max length is 256 characters
	UINT32 id;                                //!< Read only field. Setting this in the application has no effect. This field is populated with the id
			   //!< set by the peer connection's createDataChannel() call or the channel id is set in createDataChannel()
			   //!< on embedded end.
} RtcDataChannel, * PRtcDataChannel;

typedef struct __Frame {
	UINT32 version;

	// Id of the frame
	UINT32 index;

	// Flags associated with the frame (ex. IFrame for frames)
	FRAME_FLAGS flags;

	// The decoding timestamp of the frame in 100ns precision
	UINT64 decodingTs;

	// The presentation timestamp of the frame in 100ns precision
	UINT64 presentationTs;

	// The duration of the frame in 100ns precision. Can be 0.
	UINT64 duration;

	// Size of the frame data in bytes
	UINT32 size;

	// The frame bits
	PBYTE frameData;

	// Id of the track this frame belong to
	UINT64 trackId;
} Frame, * PFrame;

typedef enum {
	SIGNALING_CLIENT_STATE2_READY,
	SIGNALING_CLIENT_STATE2_CONNECTING,
	SIGNALING_CLIENT_STATE2_CONNECTED,
	SIGNALING_CLIENT_STATE2_WEBSOCKET,
	SIGNALING_CLIENT_STATE2_DISCONNECTED,
} SIGNALING_CLIENT_STATE2;

/////
typedef enum {
	ICE_CANDIDATE_PAIR_STATE_FROZEN = 0,
	ICE_CANDIDATE_PAIR_STATE_WAITING = 1,
	ICE_CANDIDATE_PAIR_STATE_IN_PROGRESS = 2,
	ICE_CANDIDATE_PAIR_STATE_SUCCEEDED = 3,
	ICE_CANDIDATE_PAIR_STATE_FAILED = 4,
} ICE_CANDIDATE_PAIR_STATE;

typedef enum {
	KVS_IP_FAMILY_TYPE_IPV4 = (UINT16)0x0001,
	KVS_IP_FAMILY_TYPE_IPV6 = (UINT16)0x0002,
} KVS_IP_FAMILY_TYPE;

#define IPV6_ADDRESS_LENGTH (UINT16) 16
#define IPV4_ADDRESS_LENGTH (UINT16) 4
#define IPV6_ADDRSTR_LENGTH (UINT16) 46

typedef struct {
	UINT16 family;
	UINT16 port;                       // port is stored in network byte order
	BYTE address[IPV6_ADDRESS_LENGTH]; // address is stored in network byte order
	char strAddress[IPV6_ADDRSTR_LENGTH];
} KvsIpAddress, * PKvsIpAddress;

#define IS_IPV4_ADDR(pAddress) ((pAddress)->family == KVS_IP_FAMILY_TYPE_IPV4)

/**
 * Type of ICE Candidate
 */
typedef enum {
	ICE_CANDIDATE_TYPE_HOST = 0,             //!< ICE_CANDIDATE_TYPE_HOST
	ICE_CANDIDATE_TYPE_PEER_REFLEXIVE = 1,   //!< ICE_CANDIDATE_TYPE_PEER_REFLEXIVE
	ICE_CANDIDATE_TYPE_SERVER_REFLEXIVE = 2, //!< ICE_CANDIDATE_TYPE_SERVER_REFLEXIVE
	ICE_CANDIDATE_TYPE_RELAYED = 3,          //!< ICE_CANDIDATE_TYPE_RELAYED
} ICE_CANDIDATE_TYPE;

typedef struct {
	CHAR localCandidateId[MAX_CANDIDATE_ID_LENGTH + 1];  //!< Local candidate that is inspected in RTCIceCandidateStats
	CHAR remoteCandidateId[MAX_CANDIDATE_ID_LENGTH + 1]; //!< Remote candidate that is inspected in RTCIceCandidateStats
	ICE_CANDIDATE_PAIR_STATE state;                      //!< State of checklist for the local-remote candidate pair
	BOOL nominated; //!< Flag is TRUE if the agent is a controlling agent and FALSE otherwise. The agent role is based on the
					//!< STUN_ATTRIBUTE_TYPE_USE_CANDIDATE flag
	NullableUint32 circuitBreakerTriggerCount; //!< Represents number of times circuit breaker is triggered during media transmission
											   //!< It is undefined if the user agent does not use this
	UINT32 packetsDiscardedOnSend;             //!< Total number of packets discarded for candidate pair due to socket errors,
	UINT64 packetsSent;                        //!< Total number of packets sent on this candidate pair;
	UINT64 packetsReceived;                    //!< Total number of packets received on this candidate pair
	UINT64 bytesSent;                          //!< Total number of bytes (minus header and padding) sent on this candidate pair
	UINT64 bytesReceived;                      //!< Total number of bytes (minus header and padding) received on this candidate pair
	UINT64 lastPacketSentTimestamp;            //!< Represents the timestamp at which the last packet was sent on this particular
											   //!< candidate pair, excluding STUN packets.
	UINT64 lastPacketReceivedTimestamp;        //!< Represents the timestamp at which the last packet was sent on this particular
											   //!< candidate pair, excluding STUN packets.
	UINT64 firstRequestTimestamp;    //!< Represents the timestamp at which the first STUN request was sent on this particular candidate pair.
	UINT64 lastRequestTimestamp;     //!< Represents the timestamp at which the last STUN request was sent on this particular candidate pair.
									 //!< The average interval between two consecutive connectivity checks sent can be calculated:
									 //! (lastRequestTimestamp - firstRequestTimestamp) / requestsSent.
	UINT64 lastResponseTimestamp;    //!< Represents the timestamp at which the last STUN response was received on this particular candidate pair.
	DOUBLE totalRoundTripTime;       //!< The sum of all round trip time (seconds) since the beginning of the session, based
									 //!< on STUN connectivity check responses (responsesReceived), including those that reply to requests
									 //!< that are sent in order to verify consent. The average round trip time can be computed from
									 //!< totalRoundTripTime by dividing it by responsesReceived.
	DOUBLE currentRoundTripTime;     //!< Latest round trip time (seconds)
	DOUBLE availableOutgoingBitrate; //!< TODO: Total available bit rate for all the outgoing RTP streams on this candidate pair. Calculated by
									 //!< underlying congestion control
	DOUBLE availableIncomingBitrate; //!< TODO: Total available bit rate for all the outgoing RTP streams on this candidate pair. Calculated by
									 //!< underlying congestion control
	UINT64 requestsReceived;         //!< Total number of connectivity check requests received (including retransmission)
	UINT64 requestsSent;             //!< The total number of connectivity check requests sent (without retransmissions).
	UINT64 responsesReceived;        //!< The total number of connectivity check responses received.
	UINT64 responsesSent;            //!< The total number of connectivity check responses sent.
	UINT64 bytesDiscardedOnSend;     //!< Total number of bytes for this candidate pair discarded due to socket errors

	ICE_CANDIDATE_TYPE local_iceCandidateType;
	ICE_CANDIDATE_TYPE remote_iceCandidateType;

	KvsIpAddress local_ipAddress;
	KvsIpAddress remote_ipAddress;
} RtcIceCandidatePairStats, *PRtcIceCandidatePairStats;

typedef CHAR DOMString[MAX_STATS_STRING_LENGTH + 1];
typedef struct {
	DOMString url;                               //!< For local candidates this is the URL of the ICE server from which the candidate was obtained
	CHAR address[IP_ADDR_STR_LENGTH + 1];        //!< IPv4 or IPv6 address of the candidate
	CHAR protocol[MAX_PROTOCOL_LENGTH + 1];      //!< Valid values: UDP, TCP
	CHAR relayProtocol[MAX_PROTOCOL_LENGTH + 1]; //!< Protocol used by endpoint to communicate with TURN server. (Only for local candidate)
												 //!< Valid values: UDP, TCP, TLS
	INT32 priority;                              //!< Computed using the formula in https://tools.ietf.org/html/rfc5245#section-15.1
	INT32 port;                                  //!< Port number of the candidate
	CHAR candidateType[MAX_CANDIDATE_TYPE_LENGTH + 1]; //!< Type of local/remote ICE candidate
} RtcIceCandidateStats, * PRtcIceCandidateStats;

typedef struct {
	CHAR url[MAX_ICE_CONFIG_URI_LEN + 1];   //!< STUN/TURN server URL
	CHAR protocol[MAX_PROTOCOL_LENGTH + 1]; //!< Valid values: UDP, TCP
	UINT32 iceServerIndex;                  //!< Ice server index to get stats from. Not available in spec! Needs to be
											//!< populated by the application to get specific server stats
	INT32 port;                             //!< Port number used by client
	UINT64 totalRequestsSent;               //!< Total amount of requests that have been sent to the server
	UINT64 totalResponsesReceived;          //!< Total number of responses received from the server
	UINT64 totalRoundTripTime;              //!< Sum of RTTs of all the requests for which response has been received
} RtcIceServerStats, * PRtcIceServerStats;

typedef enum {
	RTC_ICE_ROLE_UNKNOWN,     //!< An agent whose role is undetermined. Initial state
	RTC_ICE_ROLE_CONTROLLING, //!< A controlling agent. The ICE agent that is responsible for selecting the final choice of candidate pairs
	RTC_ICE_ROLE_CONTROLLED   //!< A controlled agent. The iCE agent waits for the controlling agent to select the final choice of candidate pairs
} RTC_ICE_ROLE;

typedef enum {
	RTC_DTLS_TRANSPORT_STATE_STATS_NEW,        //!< DTLS has not started negotiating yet
	RTC_DTLS_TRANSPORT_STATE_STATS_CONNECTING, //!< DTLS is in the process of negotiating a secure connection
	RTC_DTLS_TRANSPORT_STATE_STATS_CONNECTED,  //!< DTLS has completed negotiation of a secure connection
	RTC_DTLS_TRANSPORT_STATE_STATS_CLOSED,     //!< The transport has been closed intentionally
	RTC_DTLS_TRANSPORT_STATE_STATS_FAILED      //!< The transport has failed as the result of an error
} RTC_DTLS_TRANSPORT_STATE_STATS;

typedef struct {
	DOMString rtcpTransportStatsId;              //!< ID of the transport that gives stats for the RTCP component
	DOMString selectedCandidatePairId;           //!< ID of the object inspected to produce RtcIceCandidatePairStats
	DOMString localCertificateId;                //!< For components where DTLS is negotiated, give local certificate
	DOMString remoteCertificateId;               //!< For components where DTLS is negotiated, give remote certificate
	CHAR tlsVersion[MAX_TLS_VERSION_LENGTH + 1]; //!< For components where DTLS is negotiated, the TLS version agreed
	CHAR dtlsCipher[MAX_DTLS_CIPHER_LENGTH +
		1]; //!< Descriptive name of the cipher suite used for the DTLS transport.
			//!< Acceptable values: https://www.iana.org/assignments/tls-parameters/tls-parameters.xhtml#tls-parameters-4
	CHAR srtpCipher[MAX_SRTP_CIPHER_LENGTH + 1]; //!< Descriptive name of the protection profile used for the SRTP transport
												 //!< Acceptable values: https://www.iana.org/assignments/srtp-protection/srtp-protection.xhtml
	CHAR tlsGroup[MAX_TLS_GROUP_LENGHTH +
		1];     //!< Descriptive name of the group used for the encryption
				//!< Acceptable values: https://www.iana.org/assignments/tls-parameters/tls-parameters.xhtml#tls-parameters-8
	RTC_ICE_ROLE iceRole; //!< Set to the current value of the "role" attribute of the underlying RTCDtlsTransport's "transport"
	RTC_DTLS_TRANSPORT_STATE_STATS dtlsState; //!< Set to the current value of the "state" attribute of the underlying RTCDtlsTransport
	UINT64 packetsSent;                       //!< Total number of packets sent over the transport
	UINT64 packetsReceived;                   //!< Total number of packets received over the transport
	UINT64 bytesSent;                         //!< The total number of payload bytes sent on this PeerConnection (excluding header and padding)
	UINT64 bytesReceived;                     //!< The total number of payload bytes received on this PeerConnection (excluding header and padding)
	UINT32 selectedCandidatePairChanges;      //!< The number of times that the selected candidate pair of this transport has changed
} RtcTransportStats, * PRtcTransportStats;

typedef struct {
	DOMString localId;                //!< Used to look up RTCOutboundRtpStreamStats for the SSRC
	UINT64 roundTripTime;             //!< Estimated round trip time (milliseconds) for this SSRC based on the RTCP timestamps
	UINT64 totalRoundTripTime;        //!< The cumulative sum of all round trip time measurements in seconds since the beginning of the session
	DOUBLE fractionLost;              //!< The fraction packet loss reported for this SSRC
	UINT64 reportsReceived;           //!< Total number of RTCP RR blocks received for this SSRC
	UINT64 roundTripTimeMeasurements; //!< Total number of RTCP RR blocks received for this SSRC that contain a valid round trip time
} RtcRemoteInboundRtpStreamStats, * PRtcRemoteInboundRtpStreamStats;

typedef struct {
	UINT32 ssrc; //!< The 32-bit unsigned integer value per [RFC3550] used to identify the source of the stream of RTP packets that this stats object
				 //!< concerns.
	DOMString kind; //!< Either "audio" or "video". This MUST match the media type part of the information in the corresponding codecType member of
					//!< RTCCodecStats, and MUST match the "kind" attribute of the related MediaStreamTrack.
	// TODO: transportId not yet populated
	DOMString transportId; //!< It is a unique identifier that is associated to the object that was inspected to produce the RTCTransportStats
						   //!< associated with this RTP stream.

	// TODO: codecId not yet populated
	DOMString codecId; //!< It is a unique identifier that is associated to the object that was inspected to produce the RTCCodecStats associated with
					   //!< this RTP stream.
} RTCRtpStreamStats, * PRTCRtpStreamStats;

typedef struct {
	RTCRtpStreamStats rtpStream;
	UINT64 packetsSent;
	UINT64 bytesSent; //!< Total number of bytes sent for this SSRC. Calculated as defined in [RFC3550]
					  //!< section 6.4.1.
					  //!< The total number of payload octets (i.e., not including header or padding)
					  //!< transmitted in RTP data packets by the sender since starting transmission
} RTCSentRtpStreamStats, * PRTCSentRtpStreamStats;

typedef enum {
	RTC_QUALITY_LIMITATION_REASON_NONE,      //!< Resolution as expected. Default value
	RTC_QUALITY_LIMITATION_REASON_BANDWIDTH, //!< Reason for limitation is congestion cues during bandwidth estimation
	RTC_QUALITY_LIMITATION_REASON_CPU,       //!< The resolution and/or framerate is primarily limited due to CPU load.
	RTC_QUALITY_LIMITATION_REASON_OTHER,     //!< Limitation due to reasons other than above
} RTC_QUALITY_LIMITATION_REASON;

typedef struct {
	UINT64 durationInSeconds;                              //!< Time (seconds) spent in each state
	RTC_QUALITY_LIMITATION_REASON qualityLimitationReason; //!< Quality limitation reason
} QualityLimitationDurationsRecord, * PQualityLimitationDurationsRecord;

typedef struct {
	DOMString dscp;                  //!< DSCP String
	UINT64 totalNumberOfPacketsSent; //!< Number of packets sent
} DscpPacketsSentRecord, * PDscpPacketsSentRecord;

typedef struct {
	RTCSentRtpStreamStats sent;      //!< Comprises of information such as packetsSent and bytesSent
	BOOL voiceActivityFlag;          //!< Only valid for audio. Whether the last RTP packet sent contained voice activity or not based on the presence
									 //!< of the V bit in the extension header
	DOMString trackId;               //!< ID representing current track attached to the sender of the stream
	DOMString mediaSourceId;         //!< TODO ID representing the current media source
	DOMString senderId;              //!< TODO The stats ID used to look up the RTCAudioSenderStats or RTCVideoSenderStats object sending this stream
	DOMString remoteId;              //!< TODO ID to look up the remote RTCRemoteInboundRtpStreamStats object for the same SSRC
	DOMString rid;                   //!< TODO Exposes the rid encoding parameter of this RTP stream if it has been set, otherwise it is undefined
	DOMString encoderImplementation; //!< Identifies the encoder implementation used.
	UINT32 packetsDiscardedOnSend;   //!< Total number of RTP packets for this SSRC that have been discarded due to socket errors
	UINT32 framesSent;               //!< Only valid for video. Represents the total number of frames sent on this RTP stream
	UINT32 hugeFramesSent;           //!< Only valid for video. Represents the total number of huge frames sent by this RTP stream
									 //!< Huge frames have an encoded size at least 2.5 times the average size of the frames
	UINT32 framesEncoded;         //!< Only valid for video. It represents the total number of frames successfully encoded for this RTP media stream
	UINT32 keyFramesEncoded;      //!< Only valid for video. It represents the total number of key frames encoded successfully in the RTP Stream
	UINT32 framesDiscardedOnSend; //!< Total number of video frames that have been discarded for this SSRC due to socket errors
	UINT32 frameWidth;            //!< Only valid for video. Represents the width of the last encoded frame
	UINT32 frameHeight;           //!< Only valid for video. Represents the height of the last encoded frame
	UINT32 frameBitDepth;         //!< Only valid for video. Represents the bit depth per pixel of the last encoded frame. Typical values: 24, 30, 36
	UINT32 nackCount;             //!< Count the total number of Negative ACKnowledgement (NACK) packets received by this sender.
	UINT32 firCount;              //!< Only valid for video. Count the total number of Full Intra Request (FIR) packets received by this sender
	UINT32 pliCount;              //!< Only valid for video. Count the total number of Picture Loss Indication (PLI) packets received by this sender
	UINT32 sliCount;              //!< Only valid for video. Count the total number of Slice Loss Indication (SLI) packets received by this sender
	UINT32 qualityLimitationResolutionChanges; //!< Only valid for video. The number of times that the resolution has changed because we are quality
											   //!< limited
	INT32 fecPacketsSent; //!< TODO Total number of RTP FEC packets sent for this SSRC. Can also be incremented while sending FEC packets in band
	UINT64 lastPacketSentTimestamp;  //!< The timestamp in milliseconds at which the last packet was sent for this SSRC
	UINT64 headerBytesSent;          //!< Total number of RTP header and padding bytes sent for this SSRC
	UINT64 bytesDiscardedOnSend;     //!< Total number of bytes for this SSRC that have been discarded due to socket errors
	UINT64 retransmittedPacketsSent; //!< The total number of packets that were retransmitted for this SSRC
	UINT64 retransmittedBytesSent;   //!< The total number of PAYLOAD bytes retransmitted for this SSRC
	UINT64 targetBitrate;            //!< Current target TIAS bitrate configured for this particular SSRC
	UINT64 totalEncodedBytesTarget;  //!< Increased by the target frame size in bytes every time a frame has been encoded
	DOUBLE framesPerSecond;          //!< Only valid for video. The number of encoded frames during the last second
	UINT64 qpSum;            //!< TODO Only valid for video. The sum of the QP values of frames encoded by this sender. QP value depends on the codec
	UINT64 totalSamplesSent; //!< TODO Only valid for audio. The total number of samples that have been sent over this RTP stream
	UINT64 samplesEncodedWithSilk; //!< TODO Only valid for audio and when the audio codec is Opus. Represnets only SILK portion of codec
	UINT64 samplesEncodedWithCelt; //!< TODO Only valid for audio and when the audio codec is Opus. Represnets only CELT portion of codec
	UINT64 totalEncodeTime;        //!< Total number of milliseconds that has been spent encoding the framesEncoded frames of the stream
	UINT64 totalPacketSendDelay;   //!< Total time (seconds) packets have spent buffered locally before being transmitted onto the network
	UINT64 averageRtcpInterval;    //!< The average RTCP interval between two consecutive compound RTCP packets
	QualityLimitationDurationsRecord qualityLimitationDurations; //!< Total time (seconds) spent in each reason state
	DscpPacketsSentRecord perDscpPacketsSent;                    //!< Total number of packets sent for this SSRC, per DSCP
	RTC_QUALITY_LIMITATION_REASON qualityLimitationReason;       //!< Only valid for video.
} RtcOutboundRtpStreamStats, * PRtcOutboundRtpStreamStats;

typedef struct {
	RTCRtpStreamStats rtpStream;
	UINT64 packetsReceived; //!< Total number of RTP packets received for this SSRC.
	INT64 packetsLost; //!< TODO Total number of RTP packets lost for this SSRC. Calculated as defined in [RFC3550] section 6.4.1. Note that because
					   //!< of how this is estimated, it can be negative if more packets are received than sent.
	DOUBLE jitter;     //!< Packet Jitter measured in seconds for this SSRC. Calculated as defined in section 6.4.1. of [RFC3550].
	UINT64 packetsDiscarded; //!< The cumulative number of RTP packets discarded by the jitter buffer due to late or early-arrival, i.e., these
							 //!< packets are not played out. RTP packets discarded due to packet duplication are not reported in this metric
							 //!< [XRBLOCK-STATS]. Calculated as defined in [RFC7002] section 3.2 and Appendix A.a.
	UINT64 packetsRepaired; //!< TODO The cumulative number of lost RTP packets repaired after applying an error-resilience mechanism [XRBLOCK-STATS].
	UINT64 burstPacketsLost;      //!< TODO The cumulative number of RTP packets lost during loss bursts, Appendix A (c) of [RFC6958].
	UINT64 burstPacketsDiscarded; //!< TODO The cumulative number of RTP packets discarded during discard bursts, Appendix A (b) of [RFC7003].
	UINT32 burstLossCount; //!< TODO The cumulative number of bursts of lost RTP packets, Appendix A (e) of [RFC6958].     [RFC3611] recommends a Gmin
						   //!< (threshold) value of 16 for classifying a sequence of packet losses or discards as a burst.
	UINT32 burstDiscardCount; //!< TODO The cumulative number of bursts of discarded RTP packets, Appendix A (e) of [RFC8015].
	DOUBLE burstLossRate;     //!< TODO The fraction of RTP packets lost during bursts to the total number of RTP packets expected in the bursts. As
							  //!< defined in Appendix A (a) of [RFC7004], however, the actual value is reported without multiplying by 32768.
	DOUBLE burstDiscardRate;  //!< TODO The fraction of RTP packets discarded during bursts to the total number of RTP packets expected in bursts. As
							  //!< defined in Appendix A (e) of [RFC7004], however, the actual value is reported without multiplying by 32768.
	DOUBLE gapLossRate; //!< TODO The fraction of RTP packets lost during the gap periods. Appendix A (b) of [RFC7004], however, the actual value is
						//!< reported without multiplying by 32768.
	DOUBLE gapDiscardRate;    //!< TODO The fraction of RTP packets discarded during the gap periods. Appendix A (f) of [RFC7004], however, the actual
							  //!< value is reported without multiplying by 32768.
	UINT32 framesDropped;     //!< Only valid for video. The total number of frames dropped prior to decode or dropped because the frame missed its
							  //!< display deadline for this receiver's track. The measurement begins when the receiver is created and is a cumulative
							  //!< metric as defined in Appendix A (g) of [RFC7004].
	UINT32 partialFramesLost; //!< TODO Only valid for video. The cumulative number of partial frames lost. The measurement begins when the receiver
							  //!< is created and is a cumulative metric as defined in Appendix A (j) of [RFC7004]. This metric is incremented when
							  //!< the frame is sent to the decoder. If the partial frame is received and recovered via retransmission or FEC before
							  //!< decoding, the framesReceived counter is incremented.
	UINT32 fullFramesLost; //!< Only valid for video. The cumulative number of full frames lost. The measurement begins when the receiver is created
						   //!< and is a cumulative metric as defined in Appendix A (i) of [RFC7004].
} RtcReceivedRtpStreamStats, * PRtcReceivedRtpStreamStats;

typedef UINT64 DOMHighResTimeStamp;
typedef struct {
	RtcReceivedRtpStreamStats received; // dictionary RTCInboundRtpStreamStats : RTCReceivedRtpStreamStats
	DOMString trackId;    //!< TODO The identifier of the stats object representing the receiving track, an RTCReceiverAudioTrackAttachmentStats or
						  //!< RTCReceiverVideoTrackAttachmentStats.
	DOMString receiverId; //!< TODO The stats ID used to look up the RTCAudioReceiverStats or RTCVideoReceiverStats object receiving this stream.
	DOMString remoteId;   //!< TODO The remoteId is used for looking up the remote RTCRemoteOutboundRtpStreamStats object for the same SSRC.
	UINT32 framesDecoded; //!< TODO Only valid for video. It represents the total number of frames correctly decoded for this RTP stream, i.e., frames
						  //!< that would be displayed if no frames are dropped.
	UINT32 keyFramesDecoded; //!< TODO Only valid for video. It represents the total number of key frames, such as key frames in VP8 [RFC6386] or
							 //!< IDR-frames in H.264 [RFC6184], successfully decoded for this RTP media stream. This is a subset of framesDecoded.
							 //!< framesDecoded - keyFramesDecoded gives you the number of delta frames decoded.
	UINT16 frameWidth;       //!< TODO Only valid for video. Represents the width of the last decoded frame. Before the first frame is decoded this
							 //!< attribute is missing.
	UINT16 frameHeight;      //!< TODO Only valid for video. Represents the height of the last decoded frame. Before the first frame is decoded this
							 //!< attribute is missing.
	UINT8 frameBitDepth; //!< TODO Only valid for video. Represents the bit depth per pixel of the last decoded frame. Typical values are 24, 30, or
						 //!< 36 bits. Before the first frame is decoded this attribute is missing.
	DOUBLE framesPerSecond; //!< TODO Only valid for video. The number of decoded frames in the last second.
	UINT64 qpSum;           //!< TODO Only valid for video. The sum of the QP values of frames decoded by this receiver. The count of frames is in
				  //!< framesDecoded. The definition of QP value depends on the codec; for VP8, the QP value is the value carried in the frame header
				  //!< as the syntax element "y_ac_qi", and defined in [RFC6386] section 19.2. Its range is 0..127. Note that the QP value is only an
				  //!< indication of quantizer values used; many formats have ways to vary the quantizer value within the frame.
	DOUBLE totalDecodeTime; //!< TODO Total number of seconds that have been spent decoding the framesDecoded frames of this stream. The average
							//!< decode time can be calculated by dividing this value with framesDecoded. The time it takes to decode one frame is the
							//!< time passed between feeding the decoder a frame and the decoder returning decoded data for that frame.
	DOUBLE totalInterFrameDelay; //!< TODO Sum of the interframe delays in seconds between consecutively decoded frames, recorded just after a frame
								 //!< has been decoded. The interframe delay variance be calculated from totalInterFrameDelay,
								 //!< totalSquaredInterFrameDelay, and framesDecoded according to the formula: (totalSquaredInterFrameDelay -
								 //!< totalInterFrameDelay^2/ framesDecoded)/framesDecoded.
	DOUBLE totalSquaredInterFrameDelay; //!< TODO Sum of the squared interframe delays in seconds between consecutively decoded frames, recorded just
										//!< after a frame has been decoded. See totalInterFrameDelay for details on how to calculate the interframe
										//!< delay variance.

	BOOL voiceActivityFlag; //!< TODO Only valid for audio. Whether the last RTP packet whose frame was delivered to the RTCRtpReceiver's
							//!< MediaStreamTrack for playout contained voice activity or not based on the presence of the V bit in the extension
							//!< header, as defined in [RFC6464]. This is the stats-equivalent of RTCRtpSynchronizationSource.voiceActivityFlag in
							//!< [[WEBRTC].
	DOMHighResTimeStamp
		lastPacketReceivedTimestamp; //!< Represents the timestamp at which the last packet was received for this SSRC. This differs from timestamp,
									 //!< which represents the time at which the statistics were generated by the local endpoint.
	DOUBLE averageRtcpInterval; //!< TODO The average RTCP interval between two consecutive compound RTCP packets. This is calculated by the sending
								//!< endpoint when sending compound RTCP reports. Compound packets must contain at least a RTCP RR or SR block and an
								//!< SDES packet with the CNAME item.
	UINT64 headerBytesReceived; //!< Total number of RTP header and padding bytes received for this SSRC. This does not include the size of transport
								//!< layer headers such as IP or UDP. headerBytesReceived + bytesReceived equals the number of bytes received as
								//!< payload over the transport.
	UINT64 fecPacketsReceived;  //!< TODO Total number of RTP FEC packets received for this SSRC. This counter can also be incremented when receiving
								//!< FEC packets in-band with media packets (e.g., with Opus).
	UINT64
		fecPacketsDiscarded;  //!< TODO Total number of RTP FEC packets received for this SSRC where the error correction payload was discarded by the
							  //!< application. This may happen 1. if all the source packets protected by the FEC packet were received or already
							  //!< recovered by a separate FEC packet, or 2. if the FEC packet arrived late, i.e., outside the recovery window, and
							  //!< the lost RTP packets have already been skipped during playout. This is a subset of fecPacketsReceived.
	UINT64 bytesReceived; //!< Total number of bytes received for this SSRC. Calculated as defined in [RFC3550] section 6.4.1.
	UINT64 packetsFailedDecryption; //!< The cumulative number of RTP packets that failed to be decrypted according to the procedures in [RFC3711].
									//!< These packets are not counted by packetsDiscarded.
	UINT64 packetsDuplicated; //!< TODO The cumulative number of packets discarded because they are duplicated. Duplicate packets are not counted in
							  //!< packetsDiscarded. Duplicated packets have the same RTP sequence number and content as a previously received
	//!< packet. If multiple duplicates of a packet are received, all of them are counted. An improved estimate of lost
	//!< packets can be calculated by adding packetsDuplicated to packetsLost; this will always result in a positive number,
	//!< but not the same number as RFC 3550 would calculate.

	UINT32 nackCount; //!< TODO Count the total number of Negative ACKnowledgement (NACK) packets sent by this receiver.
	UINT32 firCount;  //!< TODO Only valid for video. Count the total number of Full Intra Request (FIR) packets sent by this receiver.
	UINT32 pliCount;  //!< TODO Only valid for video. Count the total number of Picture Loss Indication (PLI) packets sent by this receiver.
	UINT32 sliCount;  //!< TODO Only valid for video. Count the total number of Slice Loss Indication (SLI) packets sent by this receiver.
	DOMHighResTimeStamp estimatedPlayoutTimestamp; //!< TODO This is the estimated playout time of this receiver's track.
	DOUBLE jitterBufferDelay; //!< TODO It is the sum of the time, in seconds, each audio sample or video frame takes from the time it is received and
							  //!< to the time it exits the jitter buffer.
	UINT64 jitterBufferEmittedCount; //!< TODO The total number of audio samples or video frames that have come out of the jitter buffer (increasing
									 //!< jitterBufferDelay).
	UINT64 totalSamplesReceived; //!< TODO Only valid for audio. The total number of samples that have been received on this RTP stream. This includes
								 //!< concealedSamples.
	UINT64 samplesDecodedWithSilk; //!< TODO Only valid for audio and when the audio codec is Opus. The total number of samples decoded by the SILK
								   //!< portion of the Opus codec.
	UINT64 samplesDecodedWithCelt; //!< TODO Only valid for audio and when the audio codec is Opus. The total number of samples decoded by the CELT
								   //!< portion of the Opus codec.
	UINT64 concealedSamples; //!< TODO Only valid for audio. The total number of samples that are concealed samples. A concealed sample is a sample
							 //!< that was replaced with synthesized samples generated locally before being played out.
	UINT64 silentConcealedSamples; //!< TODO Only valid for audio. The total number of concealed samples inserted that are "silent".
	UINT64 concealmentEvents; //!< TODO Only valid for audio. The number of concealment events. This counter increases every time a concealed sample
							  //!< is synthesized after a non-concealed sample.
	UINT64 insertedSamplesForDeceleration; //!< TODO Only valid for audio. When playout is slowed down, this counter is increased by the difference
										   //!< between the number of samples received and the number of samples played out.
	UINT64 removedSamplesForAcceleration; //!< TODO Only valid for audio. When playout is sped up, this counter is increased by the difference between
										  //!< the number of samples received and the number of samples played out.
	DOUBLE audioLevel; //!< TODO Only valid for audio. Represents the audio level of the receiving track. For audio levels of tracks attached locally,
					   //!< see RTCAudioSourceStats instead. The value is between 0..1 (linear), where 1.0 represents 0 dBov, 0 represents silence,
					   //!< and 0.5 represents approximately 6 dBSPL change in the sound pressure level from 0 dBov.The audioLevel is averaged over
					   //!< some small interval, using the algortihm described under totalAudioEnergy. The interval used is implementation dependent.
	DOUBLE totalAudioEnergy; //!< TODO Only valid for audio. Represents the audio energy of the receiving track. For audio energy of tracks attached
							 //!< locally, see RTCAudioSourceStats instead.
	DOUBLE totalSamplesDuration; //!< TODO Only valid for audio. Represents the audio duration of the receiving track. For audio durations of tracks
								 //!< attached locally, see RTCAudioSourceStats instead.
	UINT32 framesReceived;       //!< Only valid for video. Represents the total number of complete frames received on this RTP stream. This metric is
								 //!< incremented when the complete frame is received.
	DOMString decoderImplementation; //!<  TODO Identifies the decoder implementation used. This is useful for diagnosing interoperability issues.
} RtcInboundRtpStreamStats, * PRtcInboundRtpStreamStats;

typedef enum {
	RTC_DATA_CHANNEL_STATE_CONNECTING, //!< Set while creating data channel
	RTC_DATA_CHANNEL_STATE_OPEN,       //!< Set on opening data channel on embedded side or receiving onOpen event
	RTC_DATA_CHANNEL_STATE_CLOSING,    //!< TODO: Set the state to closed after adding onClosing handler to data channel
	RTC_DATA_CHANNEL_STATE_CLOSED      //!< TODO: Set the state to closed after adding onClose handler to data channel
} RTC_DATA_CHANNEL_STATE;

typedef struct {
	DOMString label;              //!< The "label" value of the RTCDataChannel object.
	DOMString protocol;           //!< The "protocol" value of the RTCDataChannel object.
	INT32 dataChannelIdentifier;  //!< The "id" attribute of the RTCDataChannel object.
	DOMString transportId;        //!< TODO: A stats object reference for the transport used to carry this datachannel.
	RTC_DATA_CHANNEL_STATE state; //!< The "readyState" value of the RTCDataChannel object.
	UINT32 messagesSent;          //!< Represents the total number of API "message" events sent.
	UINT64 bytesSent;        //!< Represents the total number of payload bytes sent on this RTCDatachannel, i.e., not including headers or padding.
	UINT32 messagesReceived; //!< Represents the total number of API "message" events received.
	UINT64 bytesReceived;    //!< Represents the total number of bytes received on this RTCDatachannel, i.e., not including headers or padding.
} RtcDataChannelStats, * PRtcDataChannelStats;

typedef enum {
	RTC_STATS_TYPE_CANDIDATE_PAIR, //!< ICE candidate pair statistics related to the RTCIceTransport objects
	RTC_STATS_TYPE_CERTIFICATE,    //!< Information about a certificate used by an RTCIceTransport.
	RTC_STATS_TYPE_CODEC, //!< Stats for a codec that is currently being used by RTP streams being sent or received by this RTCPeerConnection object.
	RTC_STATS_TYPE_ICE_SERVER,          //!< Information about the connection to an ICE server (e.g. STUN or TURN)
	RTC_STATS_TYPE_CSRC,                //!< Stats for a contributing source (CSRC) that contributed to an inbound RTP stream.
	RTC_STATS_TYPE_DATA_CHANNEL,        //!< Stats related to each RTCDataChannel id
	RTC_STATS_TYPE_INBOUND_RTP,         //!< Statistics for an inbound RTP stream that is currently received with this RTCPeerConnection object
	RTC_STATS_TYPE_LOCAL_CANDIDATE,     //!< ICE local candidate statistics related to the RTCIceTransport objects
	RTC_STATS_TYPE_OUTBOUND_RTP,        //!< Statistics for an outbound RTP stream that is currently received with this RTCPeerConnection object
	RTC_STATS_TYPE_PEER_CONNECTION,     //!< Statistics related to the RTCPeerConnection object.
	RTC_STATS_TYPE_RECEIVER,            //!< Statistics related to a specific receiver and the corresponding media-level metrics
	RTC_STATS_TYPE_REMOTE_CANDIDATE,    //!< ICE remote candidate statistics related to the RTCIceTransport objects
	RTC_STATS_TYPE_REMOTE_INBOUND_RTP,  //!< Statistics for the remote endpoint's inbound RTP stream corresponding to an outbound stream that
										//!< is currently sent with this RTCPeerConnection object
	RTC_STATS_TYPE_REMOTE_OUTBOUND_RTP, //!< Statistics for the remote endpoint's outbound RTP stream corresponding to an inbound stream that
										//!< is currently sent with this RTCPeerConnection object
	RTC_STATS_TYPE_SENDER,              //!< Statistics related to a specific RTCRtpSender and the corresponding media-level metrics
	RTC_STATS_TYPE_TRACK,               //!< Statistics related to a specific MediaStreamTrack's attachment to an RTCRtpSender
	RTC_STATS_TYPE_TRANSPORT,           //!< Transport statistics related to the RTCPeerConnection object.
	RTC_STATS_TYPE_SCTP_TRANSPORT,      //!< SCTP transport statistics related to an RTCSctpTransport object
	RTC_STATS_TYPE_TRANSCEIVER,         //!< Statistics related to a specific RTCRtpTransceiver
	RTC_STATS_TYPE_RTC_ALL              //!< Report all supported stats
} RTC_STATS_TYPE;

typedef struct {
	RtcIceCandidatePairStats iceCandidatePairStats;             //!< ICE Candidate Pair  stats object
	RtcIceCandidateStats localIceCandidateStats;                //!< local ICE Candidate stats object
	RtcIceCandidateStats remoteIceCandidateStats;               //!< remote ICE Candidate stats object
	RtcIceServerStats iceServerStats;                           //!< ICE Server Pair stats object
	RtcTransportStats transportStats;                           //!< Transport stats object
	RtcOutboundRtpStreamStats outboundRtpStreamStats;           //!< Outbound RTP Stream stats object
	RtcRemoteInboundRtpStreamStats remoteInboundRtpStreamStats; //!< Remote Inbound RTP Stream stats object
	RtcInboundRtpStreamStats inboundRtpStreamStats;             //!< Inbound RTP Stream stats object
	RtcDataChannelStats rtcDataChannelStats;
} RtcStatsObject, * PRtcStatsObject;

typedef struct {
	UINT64 timestamp;                    //!< Timestamp of request for stats
	RTC_STATS_TYPE requestedTypeOfStats; //!< Type of stats requested. Set to RTC_ALL to get all supported stats
	RtcStatsObject rtcStatsObject;       //!< Object that is populated by the SDK on request
} RtcStats, * PRtcStats;

#if defined(_MSC_VER) // Microsoft compiler
#pragma pack(1)
#endif

//webrtc
typedef struct
{
	uint32_t length;
	uint32_t msgtype;
} __attribute__((packed)) BASE_MSG_INFO; //与信令服务器之间的交互的数据包都必然满足这个定义

//下面是设备端收到的数据包格式
typedef struct
{
	uint32_t length;
	uint32_t msgtype;
	uint32_t myid[4];
	int32_t status;
} __attribute__((packed)) SDK_ACK_LOGINUSER_INFO;

typedef struct
{
	uint32_t length;
	uint32_t msgtype;
	uint32_t hisid[4];
	int32_t sockid;
	uint16_t extradatalen0;
	uint16_t sturndataslen;
	int8_t sturncount;
	int8_t sturntypes[8];
	char sturndatas[0];
	//在这之后是 extradata0
} __attribute__((packed)) SDK_REQ_GETWEBRTCOFFER_INFO;

typedef struct
{
	uint32_t length;
	uint32_t msgtype;
	uint32_t hisid[4];
	uint16_t sdplen;
	char answersdp[0];
} __attribute__((packed)) SDK_NTI_WEBRTCANSWER_INFO;

//下面是客户端(网页端)收到的数据包格式
typedef struct
{
	uint32_t length;
	uint32_t msgtype;
	uint32_t hisid[4];
	uint32_t myid[4];
	int32_t status;
	uint16_t extradatalen0;
	char extradata0[0]; //这是另一端发送过来的
} __attribute__((packed)) SDK_ACK_CONNECTUSER_INFO;

typedef struct
{
	uint32_t length;
	uint32_t msgtype;
	uint32_t hisid[4];
	int32_t sockid;
	uint16_t sdplen;
	uint16_t sturndataslen;
	int8_t sturncount;
	int8_t sturntypes[8];
	char sturndatas[0]; //在这之后是 offersdp
						//char     offersdp[0];
} __attribute__((packed)) SDK_NTI_WEBRTCOFFER_INFO;

#if defined(_MSC_VER)
#pragma pack()
#endif

typedef void *PRIVSIGNALINGCLIENT_HANDLE;
typedef VOID (*RtcOnFrame)(UINT64, PFrame);
typedef VOID (*RtcOnBandwidthEstimation)(UINT64, DOUBLE);
typedef VOID (*RtcOnSenderBandwidthEstimation)(UINT64, UINT32, UINT32, UINT32, UINT32, UINT64);
typedef VOID (*RtcOnPictureLoss)(UINT64);
typedef VOID (*RtcOnConnectionStateChange)(UINT64, RTC_PEER_CONNECTION_STATE);
typedef VOID (*RtcOnMessage)(UINT64, PRtcDataChannel, BOOL, PBYTE, UINT32);
typedef VOID (*RtcOnOpen)(UINT64, PRtcDataChannel);
typedef VOID (*RtcOnDataChannel)(UINT64, PRtcDataChannel);

typedef VOID (*RtcOnIceCandidate)(UINT64, PCHAR);

typedef int (*SIGNALINGCLIENT_MESSAGERECEIVED_CALLBACK)(unsigned char *message, int length, uint64_t customData);
typedef int (*SIGNALINGCLIENT_STATECHANGED_CALLBACK)(SIGNALING_CLIENT_STATE2 state, uint64_t customData);

typedef int (*PRIVSIGNALINGSERVER_MESSAGERECEIVED_CALLBACK)(unsigned char *message, int length, uint64_t customData);
typedef int (*PRIVSIGNALINGCLIENT_MESSAGERECEIVED_CALLBACK)(unsigned char *message, int length, uint64_t customData);

RTC_C_EXPORT STATUS EasyRTC_initWebRtc(VOID);
RTC_C_EXPORT STATUS EasyRTC_deinitWebRtc(VOID);

RTC_C_EXPORT STATUS EasyRTC_createRtcCertificate(PRtcCertificate *ppRtcCertificate);
RTC_C_EXPORT STATUS EasyRTC_freeRtcCertificate(PRtcCertificate pRtcCertificate);

RTC_C_EXPORT int EasyRTC_startSignalingClient(SIGNALINGCLIENT_MESSAGERECEIVED_CALLBACK SignalingClientMessageReceivedCallback, SIGNALINGCLIENT_STATECHANGED_CALLBACK SignalingClientStateChangedCallback, uint64_t customData);
RTC_C_EXPORT int EasyRTC_loginUser(char *myid, char *mysn, char *mykey, char *extradata0, int extradatalen0, char *extradata1, int extradatalen1);
RTC_C_EXPORT int EasyRTC_connectUser(char *hisid, char *hiskey, char *extradata0, int extradatalen0, char *extradata1, int extradatalen1);
RTC_C_EXPORT int EasyRTC_stopSignalingClient();

RTC_C_EXPORT int EasyRTC_startPrivSignalingServer(int myport, char *myid, char *mykey, char *extradata0, int extradatalen0, PRIVSIGNALINGSERVER_MESSAGERECEIVED_CALLBACK PrivSignalingServerMessageReceivedCallback, uint64_t customData);
RTC_C_EXPORT int EasyRTC_stopPrivSignalingServer();

RTC_C_EXPORT PRIVSIGNALINGCLIENT_HANDLE EasyRTC_connectPrivSignalingServer(char *hisip, int hisport, PRIVSIGNALINGCLIENT_MESSAGERECEIVED_CALLBACK SignalingClientMessageReceivedCallback, uint64_t customData);
RTC_C_EXPORT int EasyRTC_isConnectedPrivSignalingServer(PRIVSIGNALINGCLIENT_HANDLE privClientHandle);
RTC_C_EXPORT int EasyRTC_connectPrivUser(PRIVSIGNALINGCLIENT_HANDLE privClientHandle, char *hisid, char *hiskey, char *extradata0, int extradatalen0);
RTC_C_EXPORT int EasyRTC_disconnectPrivSignalingServer(PRIVSIGNALINGCLIENT_HANDLE privClientHandle);

RTC_C_EXPORT STATUS EasyRTC_createPeerConnection(PRtcConfiguration pRtcConfiguration, PRtcPeerConnection *ppRtcPeerConnection, uint32_t *uuids, int32_t sockfd);
RTC_C_EXPORT STATUS EasyRTC_peerConnectionOnSenderBandwidthEstimation(PRtcPeerConnection pRtcPeerConnection, UINT64 customData, RtcOnSenderBandwidthEstimation rtcOnSenderBandwidthEstimation);
RTC_C_EXPORT STATUS EasyRTC_peerConnectionOnDataChannel(PRtcPeerConnection pRtcPeerConnection, UINT64 customData, RtcOnDataChannel rtcOnDataChannel);
RTC_C_EXPORT STATUS EasyRTC_peerConnectionOnConnectionStateChange(PRtcPeerConnection pRtcPeerConnection, UINT64 customData, RtcOnConnectionStateChange rtcOnConnectionStateChange);
RTC_C_EXPORT STATUS EasyRTC_addTransceiver(PRtcPeerConnection pRtcPeerConnection, PRtcMediaStreamTrack pRtcMediaStreamTrack, PRtcRtpTransceiverInit pRtcRtpTransceiverInit, PRtcRtpTransceiver *ppRtcRtpTransceiver);
RTC_C_EXPORT STATUS EasyRTC_transceiverOnFrame(PRtcRtpTransceiver pRtcRtpTransceiver, UINT64 customData, RtcOnFrame rtcOnFrame);
RTC_C_EXPORT STATUS EasyRTC_transceiverOnBandwidthEstimation(PRtcRtpTransceiver pRtcRtpTransceiver, UINT64 customData, RtcOnBandwidthEstimation rtcOnBandwidthEstimation);
RTC_C_EXPORT STATUS EasyRTC_transceiverOnPictureLoss(PRtcRtpTransceiver pRtcRtpTransceiver, UINT64 customData, RtcOnPictureLoss rtcOnPictureLoss);
RTC_C_EXPORT STATUS EasyRTC_freeTransceiver(PRtcRtpTransceiver *pRtcRtpTransceiver);
RTC_C_EXPORT STATUS EasyRTC_createDataChannel(PRtcPeerConnection pRtcPeerConnection, PCHAR pDataChannelName, PRtcDataChannelInit pRtcDataChannelInit, PRtcDataChannel *ppRtcDataChannel);
RTC_C_EXPORT STATUS EasyRTC_dataChannelOnMessage(PRtcDataChannel pRtcDataChannel, UINT64 customData, RtcOnMessage rtcOnMessage);
RTC_C_EXPORT STATUS EasyRTC_dataChannelOnOpen(PRtcDataChannel pRtcDataChannel, UINT64 customData, RtcOnOpen rtcOnOpen);
RTC_C_EXPORT STATUS EasyRTC_dataChannelSend(PRtcDataChannel pRtcDataChannel, BOOL isBinary, PBYTE pMessage, UINT32 pMessageLen);
RTC_C_EXPORT STATUS EasyRTC_createOfferAndSubmit(PRtcPeerConnection pRtcPeerConnection);
RTC_C_EXPORT STATUS EasyRTC_createAnswerAndSubmit(PRtcPeerConnection pRtcPeerConnection, PBYTE offersdp);
RTC_C_EXPORT STATUS EasyRTC_writeFrame(PRtcRtpTransceiver pRtcRtpTransceiver, PFrame pFrame);

RTC_C_EXPORT STATUS EasyRTC_getIceCandidatePairStats(PRtcPeerConnection pRtcPeerConnection, PRtcIceCandidatePairStats pRtcIceCandidatePairStats);
RTC_C_EXPORT STATUS EasyRTC_getIceCandidateStats(PRtcPeerConnection pRtcPeerConnection, BOOL isRemote, PRtcIceCandidateStats pRtcIceCandidateStats);
RTC_C_EXPORT STATUS EasyRTC_getIceServerStats(PRtcPeerConnection pRtcPeerConnection, PRtcIceServerStats pRtcIceServerStats);
RTC_C_EXPORT STATUS EasyRTC_getTransportStats(PRtcPeerConnection pRtcPeerConnection, PRtcTransportStats pRtcTransportStats);
RTC_C_EXPORT STATUS EasyRTC_getRtpRemoteInboundStats(PRtcPeerConnection pRtcPeerConnection, PRtcRtpTransceiver pRtcRtpTransceiver, PRtcRemoteInboundRtpStreamStats pRtcRemoteInboundRtpStreamStats);
RTC_C_EXPORT STATUS EasyRTC_getRtpOutboundStats(PRtcPeerConnection pRtcPeerConnection, PRtcRtpTransceiver pRtcRtpTransceiver, PRtcOutboundRtpStreamStats pRtcOutboundRtpStreamStats);
RTC_C_EXPORT STATUS EasyRTC_getRtpInboundStats(PRtcPeerConnection pRtcPeerConnection, PRtcRtpTransceiver pRtcRtpTransceiver, PRtcInboundRtpStreamStats pRtcInboundRtpStreamStats);
RTC_C_EXPORT STATUS EasyRTC_getDataChannelStats(PRtcPeerConnection pRtcPeerConnection, PRtcDataChannelStats pRtcDataChannelStats);
RTC_C_EXPORT STATUS EasyRTC_rtcPeerConnectionGetMetrics(PRtcPeerConnection pRtcPeerConnection, PRtcRtpTransceiver pRtcRtpTransceiver, PRtcStats pRtcStats);

RTC_C_EXPORT STATUS EasyRTC_closePeerConnection(PRtcPeerConnection pRtcPeerConnection);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
