#ifdef _WIN32
#ifdef _DEBUG
//#include <vld.h>
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <string.h>
#include <windows.h>
#include <process.h>
#pragma comment(lib, "winmm.lib")
#else
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <time.h>

#define Sleep(x)	usleep(x*1000)
#endif
#include "osthread.h"
#include "gettimeofdayEx.h"

#include "EasyRtcDevice.h"
#include "ESFileParser.h"
#include "g711.h"
#include "FileParser/buff.h"


void SleepEx(int ms)
{
#ifdef _WIN32
	timeBeginPeriod(1);
#endif

	Sleep(ms);

#ifdef _WIN32
	timeEndPeriod(1);
#endif
}

int		videoCodecID = 0;
int		audioCodecID = 0;

uint64_t u64_max_value = 0xFFFFFFFFFFFFFFFF;		// uint64_t最大值
uint64_t u64_max_dts = u64_max_value / 10000;		// 因为库中还要再乘10000,所以此处使用的dts为uint64_t最大值除以10000
uint64_t u64_init_dts = 0;							// dts 初始值
uint64_t videoPTS = u64_init_dts;					// 视频时间戳
uint64_t audioDTS = u64_init_dts;					// 音频时间戳
#ifdef _WIN32
DWORD WINAPI __ReadVideoFileThread(void* lpParam)
#else
void* __ReadVideoFileThread(void* lpParam)
#endif
{
	OSTHREAD_OBJ_T* pThread = (OSTHREAD_OBJ_T*)lpParam;
	EasyRtcDevice* pThis = (EasyRtcDevice*)pThread->userPtr;

	pThread->flag = THREAD_STATUS_RUNNING;

	printf("read video file thread startup. [%d]\n", pThread->customId);

	// 读文件
	ESFileParser	esFileParse;
	if (0 == esFileParse.OpenEsFile("1M.h264", true))
	{
		videoCodecID = esFileParse.GetVideoCodec();

		int interval_ms = 40;

		struct timeval tvStartTime = { 0,0 };
		while (1)
		{
			if (pThread->flag == THREAD_STATUS_EXIT)			break;

			if (audioDTS < 1)		// 如果还未读到音频,则等待
			{
				SleepEx(1);
				continue;
			}

			char* frameData = NULL;
			int frameSize = 0;
			int frameType = 0;
			if (esFileParse.ReadFrame(&frameData, &frameSize, &frameType) == 0)
			{
				videoPTS += interval_ms;
				if (videoPTS > u64_max_dts)
				{
					videoPTS = interval_ms;
				}

				pThis->SendVideoFrame((char*)frameData, frameSize, frameType, videoPTS);
			}

			int delay = interval_ms;
			if (tvStartTime.tv_sec > 0)
			{
				struct timeval tvEndTime;
				gettimeofdayEx(&tvEndTime, NULL);
				uint64_t u64Interval = 0;
				if (tvEndTime.tv_sec == tvStartTime.tv_sec)
				{
					u64Interval = (tvEndTime.tv_usec - tvStartTime.tv_usec) / 1000;
				}
				else
				{
					u64Interval = ((uint64_t)(tvEndTime.tv_sec - tvStartTime.tv_sec) - 1) * 1000;
					u64Interval += (1000000 - tvStartTime.tv_usec + tvEndTime.tv_usec) / 1000;
				}

				delay -= (int)u64Interval;
			}

			if (delay > 0 && delay <= interval_ms)
			{
				SleepEx(delay);
			}
			else
			{
				printf("video delay: %d\n", delay);
			}
			gettimeofdayEx(&tvStartTime, NULL);
		}
	}

	pThread->flag = THREAD_STATUS_INIT;


	printf("read video file thread shutdown. [%d]\n", pThread->customId);

	return 0;
}

#ifdef _WIN32
DWORD WINAPI __ReadAudioFileThread(void* lpParam)
#else
void* __ReadAudioFileThread(void* lpParam)
#endif
{
	OSTHREAD_OBJ_T* pThread = (OSTHREAD_OBJ_T*)lpParam;
	EasyRtcDevice* pThis = (EasyRtcDevice*)pThread->userPtr;

	pThread->flag = THREAD_STATUS_RUNNING;

	printf("read audio file thread startup. [%d]\n", pThread->customId);

	int samplerate = 8000;
	int channels = 1;

	int pcm_buf_size_per_sec = samplerate * 16 * channels / 8;			// 每秒数据量		比如8000*16*1/8=16000
	int pcm_buf_size_per_ms = pcm_buf_size_per_sec / 1000;				// 每毫秒数据量		16000/1000=16

	int interval_ms = 20;												// 间隔20毫秒
	int bytes_per_20ms = pcm_buf_size_per_ms * interval_ms;				// 每20毫秒数据量

	
	BUFF_T	buff;
	memset(&buff, 0x00, sizeof(BUFF_T));
	BUFF_MALLOC(&buff, bytes_per_20ms + 1);

	BUFF_T	bufG711;
	memset(&bufG711, 0x00, sizeof(BUFF_T));
	BUFF_MALLOC(&bufG711, bytes_per_20ms + 1);

	// 读PCM文件
	FILE* fAudio = fopen("music.pcm", "rb");		// 8K,16bit,1ch
	if (NULL != fAudio)
	{
		//audioCodecID = 0x10006;	// mulaw
		audioCodecID = 0x10007;		// alaw

		struct timeval tvStartTime = { 0,0 };
		while (1)
		{
			if (pThread->flag == THREAD_STATUS_EXIT)			break;

			buff.bufpos = fread(buff.pbuf, 1, bytes_per_20ms, fAudio);
			if (buff.bufpos < bytes_per_20ms)
			{
				fseek(fAudio, 0, SEEK_SET);
				buff.bufpos = fread(buff.pbuf, 1, bytes_per_20ms, fAudio);
			}
			if (buff.bufpos == bytes_per_20ms)
			{
				// 转码为G711ALAW
				int idx = 0;
				for (int i = 0; i < buff.bufpos; i+=2) {

					unsigned char uc1 = buff.pbuf[i];
					unsigned char uc2 = buff.pbuf[i + 1];

					short s = ((uc2 << 8) & 0xFF00) | (uc1 & 0xFF);
					bufG711.pbuf[idx++] = linear2alaw(s);
				}

				bufG711.bufpos = idx;

				// 时间戳递增
				audioDTS += interval_ms;
				if (audioDTS > u64_max_dts)
				{
					audioDTS = interval_ms;
				}

				pThis->SendAudioFrame((char*)bufG711.pbuf, bufG711.bufpos, audioDTS);
			}
			int delay = interval_ms;
			if (tvStartTime.tv_sec > 0)
			{
				struct timeval tvEndTime;
				gettimeofdayEx(&tvEndTime, NULL);
				uint64_t u64Interval = 0;

				if (tvEndTime.tv_sec == tvStartTime.tv_sec)
				{
					u64Interval = (tvEndTime.tv_usec - tvStartTime.tv_usec) / 1000;
				}
				else
				{
					u64Interval = (tvEndTime.tv_sec - tvStartTime.tv_sec - 1) * 1000;
					u64Interval += (1000000 - tvStartTime.tv_usec + tvEndTime.tv_usec) / 1000;
				}

				delay -= (int)u64Interval;
			}


			if (delay > 0 && delay <= interval_ms)
			{
				SleepEx(delay);
			}
			else
			{
				printf("audio delay: %d\n", delay);
			}

			gettimeofdayEx(&tvStartTime, NULL);
		}
	}
	BUFF_FREE(&buff);
	BUFF_FREE(&bufG711);
	pThread->flag = THREAD_STATUS_INIT;


	printf("read audio file thread shutdown. [%d]\n", pThread->customId);

	return 0;
}





int __EasyRTC_Data_Callback(void* userptr, void* webrtcPeer, EASYRTC_DATA_TYPE_ENUM_E dataType, int codecID, BOOL isBinary, PBYTE pbuf, UINT32 bufsize)
{
	if (EASYRTC_DATA_TYPE_VIDEO == dataType)								// 对端的视频帧
	{
		printf("Receive video frame from peer: %u\n", bufsize);
	}
	else if (EASYRTC_DATA_TYPE_AUDIO == dataType)							// 对端的音频帧
	{
		printf("Receive audio frame from peer: %u\n", bufsize);
	}
	else if (EASYRTC_DATA_TYPE_METADATA == dataType)						// 对端DataChannel传输的数据
	{
		if (!isBinary) // 文本消息
		{
			printf("Receive message from peer: %u   %s\n", bufsize, pbuf);
		}
		else			// 非文本
		{
			printf("Receive binary data from peer: %u\n", bufsize);
		}
	}
	return 0;
}
int main(int argc, char* argv[])
{
	char uuid[128] = { 0 };
	EasyRtcDevice	mEasyRtcDevice;					// rtc设备

	if (argc < 2)
	{
		// 根据mEasyRtcDevice的内存地址生成uuid

		unsigned long long ullAddr = (unsigned long long)&mEasyRtcDevice;
		unsigned long long ull1 = (ullAddr >> 24) & 0xFF;
		unsigned int ui1 = ullAddr & 0xFFFFFFFF;
		ullAddr = (ull1 << 32) | ui1;

#ifdef _DEBUG
		sprintf(uuid, "92092eea-be8d-4ec4-8ac5-123456789012");			// 为方便调试,Debug模式下写一个固定的值
#else
		sprintf(uuid, "92092eea-be8d-4ec4-8ac5-%012llu", ullAddr);
#endif
	}
	else
	{
		strcpy(uuid, argv[1]);
	}

	// 创建读视频文件线程
	OSTHREAD_OBJ_T* readVideoFileThread = NULL;
	CreateOSThread(&readVideoFileThread, __ReadVideoFileThread, (void*)&mEasyRtcDevice, 0);

	// 创建读音频文件线程
	OSTHREAD_OBJ_T* readAudioFileThread = NULL;
	CreateOSThread(&readAudioFileThread, __ReadAudioFileThread, (void*)&mEasyRtcDevice, 0);

	// 等待获取音视频编码格式
	for (int i = 0; i < 10; i++)
	{
		if (videoCodecID > 0 && audioCodecID > 0)		break;

		Sleep(1000);
	}

	if (videoCodecID > 0 && audioCodecID > 0)
	{
		mEasyRtcDevice.Start(videoCodecID, audioCodecID, uuid, false, __EasyRTC_Data_Callback, NULL);

		while (getchar() != 'Q');
	}
	else
	{
		printf("########## Failed to obtain media information.##########\n");
	}


	DeleteOSThread(&readVideoFileThread);				// 关闭读视频文件线程
	DeleteOSThread(&readAudioFileThread);				// 关闭读音频文件线程

	mEasyRtcDevice.Stop();

    return 0;
}