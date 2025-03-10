#ifndef __LIB_AUDIO_PLAYER_API_H__
#define __LIB_AUDIO_PLAYER_API_H__

#ifdef _DEBUG
//#include <vld.h>
#endif

#include <winsock2.h>
#include <mmsystem.h>
#include <map>
#include <string>
using namespace std;

#ifdef _WIN32
#define LIB_AUDIO_PLAYER_API  __declspec(dllexport)
#ifndef LIB_APICALL
#define LIB_APICALL  __stdcall
#endif
#define WIN32_LEAN_AND_MEAN
#else
#define LIB_AUDIO_PLAYER_API
#define LIB_APICALL 
#endif


#ifdef __cplusplus
extern "C"
{
#endif

	// ����Ƶ�����豸
	int LIB_AUDIO_PLAYER_API	LIB_APICALL libAudioPlayer_Open(const int samplerate, const int bitPerSamples, const int channelNum);

	// ��ʼ����
	int LIB_AUDIO_PLAYER_API	LIB_APICALL libAudioPlayer_Play();

	// д����Ƶ����
	int LIB_AUDIO_PLAYER_API	LIB_APICALL libAudioPlayer_Write(char* pbuf, int bufsize);

	// �ر���Ƶ�����豸
	int LIB_AUDIO_PLAYER_API	LIB_APICALL libAudioPlayer_Stop();

	// �ر���Ƶ�����豸
	int LIB_AUDIO_PLAYER_API	LIB_APICALL libAudioPlayer_Close();

#ifdef __cplusplus
}
#endif



#endif
