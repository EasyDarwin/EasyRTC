#ifndef __TRANS_CODE_API_H__
#define __TRANS_CODE_API_H__



#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
//#include <windows.h>
#include <winsock2.h>

#define TRANSCODE_API  __declspec(dllexport)
#define TRANSCODE_APICALL  __stdcall
#define WIN32_LEAN_AND_MEAN
#else
#define TRANSCODE_API  __attribute__ ((visibility("default")))
#define TRANSCODE_APICALL 
#define CALLBACK
#endif



#define TRANSCODE_VIDEO_CODEC_ID_H264		27
#define TRANSCODE_VIDEO_CODEC_ID_H265		173
#define TRANSCODE_VIDEO_CODEC_ID_MPEG4		12
#define TRANSCODE_VIDEO_CODEC_ID_MJPEG		7


typedef struct __TRANSCODE_HW_CODEC_T
{
	int			id;
	char		name[16];
}TRANSCODE_HW_CODEC_T;



//typedef int (CALLBACK* NVSourceCallBack)(int channelId, void* userPtr, int mediaType, char* pBuf, NVS_FRAME_INFO* frameInfo);


typedef void (CALLBACK* TRANSCODE_DECODE_CALLBACK)(void* userPtr, int width, int height, unsigned char* decode_data, int datasize, int keyframe);
typedef void (CALLBACK* TRANSCODE_ENCODE_CALLBACK)(void *userPtr, int keyFrame, int width, int height, unsigned char* transcode_framedata, int transcode_framesize, unsigned int pts);
#define TRANSCODE_HANDLE void*

#ifdef __cplusplus
extern "C"
{
#endif

	TRANSCODE_API int TRANSCODE_APICALL TransCode_Create(TRANSCODE_HANDLE* handle);

	// ����decoderId, ��ȡ��Ӧ��Ӳ��������
	TRANSCODE_API int TRANSCODE_APICALL TransCode_GetSupportHardwareCodecs(TRANSCODE_HANDLE handle, int codecType/*0(Decoder)  1(Encoder)*/, int codecId, TRANSCODE_HW_CODEC_T** ppCodecs, int* num);

	// ===============================================
	// ===============================================
	// ===============================================
	// ===============================================
	// ���÷�ʽ: 1. ��ɽ���ͱ���   2. ������  3. ������


	// ��ʼ��
	// decoderId:	��Ƶ������Id, ��: TRANSCODE_VIDEO_CODEC_ID_H264 TRANSCODE_VIDEO_CODEC_ID_H265
	//				�粻���������,����0x7FFFFFFF
	// hwDecoderId: ����0���ʾʹ��Ӳ��, ��ֵ��TransCode_GetSupportHardwareDecoder�л��, �����ʼ��Ӳ��ʧ��,���Զ��л�Ϊ���
	// width:  Դ�ֱ���-��
	// height: Դ�ֱ���-��
	// pixelFormat: 0(YUV420P)  3(BGR24)   23(NV12)   ����ɲο�ffmpeg4.4.1(AVPixelFormat����)
	// encoderID: ��Ϊ0���ʾ������, ����0ʱ: ��hwDecoderId����0(ʹ��Ӳ����),��ʹ����Ӧ�ı���ģ��, ���������,��ʹ�������
	// encFps:  ����֡��
	// encGop:  ����GOP
	// encBitrate: ��������

	TRANSCODE_API int TRANSCODE_APICALL TransCode_Init(TRANSCODE_HANDLE handle, int decoderId, int hwDecoderId, int width, int height, 
														int pixelFormat/* 0(YUV420P)  3(BGR24)   23(NV12) */ ,
														int encoderId/*Ϊ0���ʾ������*/, int hwEncoderId, int encFps, int encGop, int encBitrate);

	// ����������
	TRANSCODE_API int TRANSCODE_APICALL TransCode_RebootEncoder(TRANSCODE_HANDLE handle, 
														int encoderId/*Ϊ0���ʾ������*/, int hwEncoderId, int encFps, int encGop, int encBitrate);



	// TransCode_TransCodeVideo
	// outFrameData: 
	//				�粻ΪNULL, ���ʾͬ��ִ��, ����������ֱ�ӷ���
	//				��ΪNULL, ��Ϊ�첽���, �ᴴ���������߳̽��лص�(TRANSCODE_DECODE_CALLBACK)���, ���ò���ڸûص��н��з����Ȳ���
	//				



	TRANSCODE_API int TRANSCODE_APICALL TransCode_TransCodeVideo(TRANSCODE_HANDLE handle, const char* framedata, const int framesize, 
																		int *dstWidth, int* dstHeight,
																		char** outFrameData, int* outFrameSize, int* keyFrame, 
																		TRANSCODE_DECODE_CALLBACK decodeCallback, void *decodeUserPtr,
																		TRANSCODE_ENCODE_CALLBACK encodeCallback, void *encodeUserPtr,
																		unsigned int pts, bool autoParseResolution);

	// ============================================
	TRANSCODE_API int TRANSCODE_APICALL TransCode_GetSwsSize(int pixelFormat, int width, int height);
	TRANSCODE_API int TRANSCODE_APICALL TransCode_CreateSws(TRANSCODE_HANDLE *handle, int srcWidth, int srcHeight, int srcPixelFormat, int dstWidth, int dstHeight, int dstPixelFormat);
	TRANSCODE_API int TRANSCODE_APICALL TransCode_SwsScale(TRANSCODE_HANDLE handle, const unsigned char* inData, unsigned char** outData, int* outSize);
	TRANSCODE_API int TRANSCODE_APICALL TransCode_ReleaseSws(TRANSCODE_HANDLE* handle);
	// ============================================

	TRANSCODE_API int TRANSCODE_APICALL TransCode_Release(TRANSCODE_HANDLE* handle);


#ifdef __cplusplus
};
#endif


#endif
