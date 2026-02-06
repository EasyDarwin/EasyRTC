#ifndef __CHARACTER_TRANCODING_H__
#define __CHARACTER_TRANCODING_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define LIB_CHARACTER_TRANSCODING_API  __declspec(dllexport)
#ifndef LIB_APICALL
#define LIB_APICALL  __stdcall
#endif
#define WIN32_LEAN_AND_MEAN
#else
#define LIB_CHARACTER_TRANSCODING_API	__attribute__ ((visibility("default"))) 
#define LIB_APICALL 
#endif




#ifdef __cplusplus
extern "C"
{
#endif

	bool	LIB_CHARACTER_TRANSCODING_API LIB_APICALL	libCharacterTranscoding_IsUTF8(const char *str);
	bool	LIB_CHARACTER_TRANSCODING_API LIB_APICALL	libCharacterTranscoding_IsGBK(const char* str);

	int		LIB_CHARACTER_TRANSCODING_API LIB_APICALL	libCharacterTranscoding_UTF8ToGB2312(char *sOut, int iMaxOutLen, const char *sIn, int iInLen);
	int		LIB_CHARACTER_TRANSCODING_API LIB_APICALL	libCharacterTranscoding_GB2312ToUTF8(char* sOut, int iMaxOutLen, const char* sIn, int iInLen);

#ifdef __cplusplus
}
#endif

#endif
