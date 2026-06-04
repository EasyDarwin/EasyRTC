#ifndef __LIB_CURL_WEB_CLIENT_API_H__
#define __LIB_CURL_WEB_CLIENT_API_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#ifdef _WIN32
#include <winsock2.h>
#define LIB_WEB_CLIENT_API  __declspec(dllexport)
#ifndef LIB_APICALL
#define LIB_APICALL  __stdcall
#endif
#define WIN32_LEAN_AND_MEAN
#else
#define LIB_WEB_CLIENT_API __attribute__ ((visibility("default"))) 
#define LIB_APICALL 
#endif



typedef enum __WEB_CLIENT_REQUEST_TYPE_E
{
	WEB_CLIENT_REQUEST_GET	=	0x00000001,
	WEB_CLIENT_REQUEST_POST,
	WEB_CLIENT_REQUEST_WEBSOCKET,
}WEB_CLIENT_REQUEST_TYPE_E;

typedef int (*CURL_RESPONSE_CALLBACK)(void* userptr, size_t responseSize, char* responseData);

typedef void* WEBCLIENT_HANDLE;
#ifdef __cplusplus
extern "C"
{
#endif

	//
	LIB_WEB_CLIENT_API int LIB_APICALL	libWebClient_curl_init();//curl_global_init(CURL_GLOBAL_ALL);

	LIB_WEB_CLIENT_API int LIB_APICALL	libWebClient_curl_Deinit(); //curl_global_cleanup();



	LIB_WEB_CLIENT_API int LIB_APICALL	libWebClient_GetData(WEB_CLIENT_REQUEST_TYPE_E requestType, const char* url, const char* contentType, const char *body, CURL_RESPONSE_CALLBACK callback, void* userptr, int timeoutSecs);

	LIB_WEB_CLIENT_API int LIB_APICALL	libWebClient_Get(const char* url, const char* contentType, CURL_RESPONSE_CALLBACK callback, void* userptr, int timeoutSecs);


	LIB_WEB_CLIENT_API int LIB_APICALL	libWebClient_Post(const char* url, const char* contentType, const char *body, CURL_RESPONSE_CALLBACK callback, void* userptr, int timeoutSecs);


	LIB_WEB_CLIENT_API int LIB_APICALL	libWebClient_http_client_post(const char* url,
		const char* content_type,
		const char* body,
		CURL_RESPONSE_CALLBACK callback,
		void* userptr,
		int timeout_secs);


#ifdef __cplusplus
}
#endif


#endif

