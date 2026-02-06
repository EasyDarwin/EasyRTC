#include "EasyRTCDeviceAPI.h"
#include "RTCDevice.h"
#include <stdbool.h>
#include "EasyRTC_websocket.h"

/****************************************************
 * 函数: getBuildTime
 * 功能: 获取软件编译时间和日期
 ***************************************************/
void EasyRTCDevice_getBuildTime(int* _year, int* _month, int* _day, int* _hour, int* _minute, int* _second)
{
	char monthStr[12][5] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	char buf[16] = { 0 };
	char monthBuf[4] = { 0 };
	unsigned int i = 0;

	int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;

	strcpy(buf, __DATE__);
	memset(monthBuf, 0, 4);

	strncpy(monthBuf, buf, 3);
	day = buf[5] - '0';
	if (buf[4] != ' ')
	{
		day += (buf[4] - '0') * 10;
	}
	year = (buf[7] - '0') * 1000 + (buf[8] - '0') * 100 + (buf[9] - '0') * 10 + buf[10] - '0';
	strcpy(buf, __TIME__);
	hour = (buf[0] - '0') * 10 + buf[1] - '0';
	minute = (buf[3] - '0') * 10 + buf[4] - '0';
	second = (buf[6] - '0') * 10 + buf[7] - '0';
	for (i = 0; i < (sizeof(monthStr) / sizeof(monthStr[0])); i++)
	{
		if (0 == strcmp(monthStr[i], monthBuf))
		{
			month = i + 1;
			break;
		}
	}
	if (i >= 12)
	{
		month = 1;
	}

	if (_year)	*_year = year;
	if (_month)	*_month = month;
	if (_day)	*_day = day;
	if (_hour)	*_hour = hour;
	if (_minute)	*_minute = minute;
	if (_second)	*_second = second;
}

int	EASYRTC_DEVICE_API	EasyRTC_Device_GetVersion(char* version)
{
	int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
	EasyRTCDevice_getBuildTime(&year, &month, &day, &hour, &minute, &second);

	if (NULL == version)	return -1;

	sprintf(version, "EasyRTCDevice v1.0.%d.%02d%02d", year - 2000, month, day);

	return 0;
}

void exit_cleanly(int flag)
{
	//printf("======Receive exit signal, program exit...=======\n");

}

static int gEasyRTCDeviceInitFlag = 0;
int	EASYRTC_DEVICE_API	EasyRTC_Device_Init()
{
	if (gEasyRTCDeviceInitFlag == 0)
	{
#ifdef _WIN32
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
		signal(SIGPIPE, SIG_IGN);
		signal(SIGTERM, exit_cleanly);
		signal(SIGINT, exit_cleanly);
		signal(SIGQUIT, exit_cleanly);
#endif

		int ret = EasyRTC_Init();
		if (ret != 0)	return -1;

		gEasyRTCDeviceInitFlag = 1;
	}

	// 打印两个库的版本号

	char EasyRTCLibVersion[32] = { 0 };
	websocketGetVersion(EasyRTCLibVersion);

	char EasyRTCDeviceLibVersion[32] = { 0 };
	EasyRTC_Device_GetVersion(EasyRTCDeviceLibVersion);

	printf("%s\n%s\n", EasyRTCLibVersion, EasyRTCDeviceLibVersion);

	return gEasyRTCDeviceInitFlag == 1 ? 0 : -2;
}

int	EASYRTC_DEVICE_API	EasyRTC_Device_Create(EASYRTC_HANDLE* handle, const char* serverAddr, const int serverPort, const int isSecure, const char* local_id, EasyRTC_Data_Callback callback, void* userptr)
{
	if (gEasyRTCDeviceInitFlag == 0)	return EASYRTCDevice_Uninitialized;

	EASYRTC_DEVICE_T* pDevice = RTC_Device_Create(serverAddr, serverPort, isSecure);
	if (NULL == pDevice)	return -1;

	RTC_Device_Start(pDevice, local_id, callback, userptr);

	*handle = pDevice;

	return 0;
}


int	EASYRTC_DEVICE_API	EasyRTC_Device_SetChannelInfo(EASYRTC_HANDLE handle, EASYRTC_CODEC videoCodecID, EASYRTC_CODEC audioCodecID)
{
	if (gEasyRTCDeviceInitFlag == 0)	return EASYRTCDevice_Uninitialized;

	EASYRTC_DEVICE_T* pDevice = (EASYRTC_DEVICE_T*)handle;
	if (NULL == pDevice)		return -1;

	int ret = RTC_Device_SetChannelInfo(pDevice, videoCodecID, audioCodecID);

	return ret;
}

int EasyRTC_Device_PassiveCallResponse(EASYRTC_HANDLE handle, const char* peerUUID, const int decline)
{
	if (gEasyRTCDeviceInitFlag == 0)	return EASYRTCDevice_Uninitialized;

	EASYRTC_DEVICE_T* pDevice = (EASYRTC_DEVICE_T*)handle;
	if (NULL == pDevice)		return -1;

	int ret = RTC_Device_PassiveCallResponse(pDevice, peerUUID, decline);

	return ret;
}

int	EASYRTC_DEVICE_API	EasyRTC_Stop(EASYRTC_HANDLE handle)
{
	if (gEasyRTCDeviceInitFlag == 0)	return EASYRTCDevice_Uninitialized;

	EASYRTC_DEVICE_T* pDevice = (EASYRTC_DEVICE_T*)handle;
	if (NULL == pDevice)		return -1;

	int ret = RTC_Device_Stop(pDevice);

	return ret;
}


// 发送视频帧
int	EASYRTC_DEVICE_API	EasyRTC_Device_SendVideoFrame(EASYRTC_HANDLE handle, char* framedata, const int framesize, int keyframe, unsigned long long pts)
{
	if (gEasyRTCDeviceInitFlag == 0)	return EASYRTCDevice_Uninitialized;

	EASYRTC_DEVICE_T* pDevice = (EASYRTC_DEVICE_T*)handle;
	if (NULL == pDevice)		return -1;

	int ret = RTC_Device_SendVideoFrame(pDevice, framedata, framesize, keyframe, pts);

	return ret;
}

// 发送音频帧
int	EASYRTC_DEVICE_API	EasyRTC_Device_SendAudioFrame(EASYRTC_HANDLE handle, char* framedata, const int framesize, unsigned long long pts/*时间戳单位是:毫秒*/)
{
	if (gEasyRTCDeviceInitFlag == 0)	return EASYRTCDevice_Uninitialized;

	EASYRTC_DEVICE_T* pDevice = (EASYRTC_DEVICE_T*)handle;
	if (NULL == pDevice)		return -1;

	int ret = RTC_Device_SendAudioFrame(pDevice, framedata, framesize, pts);

	return ret;
}

int	EASYRTC_DEVICE_API	EasyRTC_Device_aecm_process(EASYRTC_HANDLE handle, short* nearendNoisy, short* outPcmData, int pcmSize)
{
	if (gEasyRTCDeviceInitFlag == 0)	return EASYRTCDevice_Uninitialized;

	EASYRTC_DEVICE_T* pDevice = (EASYRTC_DEVICE_T*)handle;
	if (NULL == pDevice)		return -1;

	int ret = RTC_Device_aecm_process(pDevice, nearendNoisy, outPcmData, pcmSize);

	return ret;
}


// 发送自定义数据到指定对端或所有对端
int	EASYRTC_DEVICE_API	EasyRTC_Device_SendCustomData(EASYRTC_HANDLE handle, const char* peerUUID, const int isBinary, const char* data, const int size)
{
	if (gEasyRTCDeviceInitFlag == 0)	return EASYRTCDevice_Uninitialized;

	EASYRTC_DEVICE_T* pDevice = (EASYRTC_DEVICE_T*)handle;
	if (NULL == pDevice)		return -1;

	int ret = RTC_Device_SendCustomData(pDevice, peerUUID, isBinary, data, size);

	return ret;
}

int	EASYRTC_DEVICE_API	EasyRTC_Device_Hangup(EASYRTC_HANDLE handle, const char* peerUUID)
{
	if (gEasyRTCDeviceInitFlag == 0)	return EASYRTCDevice_Uninitialized;

	EASYRTC_DEVICE_T* pDevice = (EASYRTC_DEVICE_T*)handle;
	if (NULL == pDevice)		return -1;

	int ret = RTC_Device_Hangup(pDevice, peerUUID);

	return ret;
}


	// 获取在线设备列表
int	EASYRTC_DEVICE_API	EasyRTC_GetOnlineDevices(EASYRTC_HANDLE handle, EasyRTC_Data_Callback callback, void* userptr)
{
	if (gEasyRTCDeviceInitFlag == 0)	return EASYRTCDevice_Uninitialized;

	EASYRTC_DEVICE_T* pDevice = (EASYRTC_DEVICE_T*)handle;
	if (NULL == pDevice)		return -1;

	int ret = GetOnlineDevices(pDevice, callback, userptr);

	return ret;
}


	// 连接指定设备
int	EASYRTC_DEVICE_API	EasyRTC_Caller_Connect(EASYRTC_HANDLE handle, const char* peerUUID)
{
	if (gEasyRTCDeviceInitFlag == 0)	return EASYRTCDevice_Uninitialized;

	EASYRTC_DEVICE_T* pDevice = (EASYRTC_DEVICE_T*)handle;
	if (NULL == pDevice)		return -1;

	int ret = RTC_Caller_Connect(pDevice, peerUUID);

	return ret;
}



int	EASYRTC_DEVICE_API	EasyRTC_Device_Release(EASYRTC_HANDLE* handle)
{
	if (NULL == handle)		return -1;
	if (NULL == *handle)		return -1;

	if (gEasyRTCDeviceInitFlag == 0)	return EASYRTCDevice_Uninitialized;

	RTC_Device_Stop(*handle);
	RTC_Device_Release((EASYRTC_DEVICE_T**)handle);

	return 0;
}


// 反初始化环境
int	EASYRTC_DEVICE_API	EasyRTC_Device_Deinit()
{
	if (gEasyRTCDeviceInitFlag == 1)
	{
		EasyRTC_Deinit();

#ifdef _WIN32
		WSACleanup();
#endif

		gEasyRTCDeviceInitFlag = 0;
	}
	return 0;
}