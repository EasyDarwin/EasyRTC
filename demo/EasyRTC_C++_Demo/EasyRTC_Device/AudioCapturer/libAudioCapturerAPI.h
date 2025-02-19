#ifndef __LIB_AUDIO_CAPTURER_API_H__
#define __LIB_AUDIO_CAPTURER_API_H__

#ifdef _DEBUG
//#include <vld.h>
#endif

#include <winsock2.h>
#include <mmsystem.h>
#include <map>
#include <string>
using namespace std;

#ifdef _WIN32
#define LIB_AUDIO_CAPTURER_API  __declspec(dllexport)
#ifndef LIB_APICALL
#define LIB_APICALL  __stdcall
#endif
#define WIN32_LEAN_AND_MEAN
#else
#define LIB_AUDIO_CAPTURER_API
#define LIB_APICALL 
#endif


#define AUDIO_CODEC_ID_PCM		1
#define AUDIO_CODEC_ID_ALAW		2
#define AUDIO_CODEC_ID_MULAW	3


//�����豸��Ϣ
#define		MAX_MIXER_DEVICE_NUM		16
typedef struct __MIXER_DEVICE_INFO_T
{
	int		id;
	char	name[128];
	char	version[16];
}MIXER_DEVICE_INFO_T;

//��Ƶ�ɼ��豸��Ϣ
typedef struct __AUDIO_CAPTURE_DEVICE_INFO
{
	int			id;
	//LPGUID		lpGuid;
	//LPTSTR		lpstrDescription;
	//LPTSTR		lpstrModule;

	char		description[128];
	char		module[128];
}AUDIO_CAPTURE_DEVICE_INFO;

typedef map<int, AUDIO_CAPTURE_DEVICE_INFO>		AUDIO_CAPTURER_DEVICE_MAP;

//��Ƶ�ɼ���ʽ
//typedef struct __AUDIO_WAVE_FORMAT_INFO
//{
//	WORD        wFormatTag;         /* format type */
//	WORD        nChannels;          /* number of channels (i.e. mono, stereo...) */
//	DWORD       nSamplesPerSec;     /* sample rate */
//	DWORD       nAvgBytesPerSec;    /* for buffer estimation */
//	WORD        nBlockAlign;        /* block size of data */
//	WORD        wBitsPerSample;     /* number of bits per sample of mono data */
//	WORD        cbSize;             /* the count in bytes of the size of */
//									/* extra information (after cbSize) */
//}AUDIO_WAVE_FORMAT_INFO;
typedef map<int, WAVEFORMATEX>		AUDIO_WAVE_FORMAT_INFO_MAP;

typedef int (CALLBACK *AudioDataCallBack)(void *userptr, char *pbuf, const int bufsize);


#ifdef __cplusplus
extern "C"
{
#endif

	int LIB_AUDIO_CAPTURER_API	LIB_APICALL libAudioCapturer_Init();

	// ��ȡ��Ƶ�豸�б�
	int LIB_AUDIO_CAPTURER_API	LIB_APICALL libAudioCapturer_GetAudioCaptureDeviceList(AUDIO_CAPTURER_DEVICE_MAP* pMap);

	// ����Ƶ�ɼ��豸
	int LIB_AUDIO_CAPTURER_API	LIB_APICALL libAudioCapturer_OpenAudioCaptureDevice(int captureDeviceIndex);

	// ��ȡ֧�ֵĸ�ʽ
	int LIB_AUDIO_CAPTURER_API	LIB_APICALL libAudioCapturer_GetSupportWaveFormatList(AUDIO_WAVE_FORMAT_INFO_MAP* pMap);

	int LIB_AUDIO_CAPTURER_API	LIB_APICALL libAudioCapturer_StartAudioCapture(unsigned int codec, int packetSize, 
														int samplerate/*������*/, int bitsPerSample/*��������*/, int channels/*ͨ��*/,
														AudioDataCallBack callback, void* userptr);


	int LIB_AUDIO_CAPTURER_API	LIB_APICALL libAudioCapturer_StopAudioCapture();

	// �ر���Ƶ�ɼ��豸
	int LIB_AUDIO_CAPTURER_API	LIB_APICALL libAudioCapturer_CloseAudioCaptureDevice();

	int LIB_AUDIO_CAPTURER_API	LIB_APICALL libAudioCapturer_Deinit();

#ifdef __cplusplus
}
#endif



#endif
