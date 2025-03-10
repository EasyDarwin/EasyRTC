#ifndef __D3D_RENDER_H__
#define __D3D_RENDER_H__

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <MMSystem.h>
#include "VADef.h"
#define D3DRENDER_API  __declspec(dllexport)


//Ŀǰ֧�ֵĸ�ʽ
typedef enum
{
	D3D_FORMAT_YUY2		=	MAKEFOURCC('Y','U','Y','2'),	//844715353			-->decode output format:	PIX_FMT_YUYV422		1
	D3D_FORMAT_YV12		=	MAKEFOURCC('Y','V','1','2'),	//842094169
	D3D_FORMAT_UYVY		=	MAKEFOURCC('U', 'Y', 'V', 'Y'),	//1498831189		-->decode output format:	PIX_FMT_UYVY422		17
	D3D_FORMAT_A8R8G8B8	=	21,								//					-->decode output format:	PIX_FMT_BGRA		30
	D3D_FORMAT_X8R8G8B8	=	22,								//					-->decode output format:	PIX_FMT_BGRA		30
	D3D_FORMAT_RGB565	=	23,								//					-->decode output format:	PIX_FMT_RGB565LE	44
	D3D_FORMAT_RGB555	=	25,								//					-->decode output format:	PIX_FMT_RGB555LE	46

	GDI_FORMAT_RGB24,
	GDI_FORMAT_RGB32
}D3D_SUPPORT_FORMAT;

//�Կ�
#ifndef D3DADAPTER_DEFAULT
#define D3DADAPTER_DEFAULT	0
#endif

#define	D3D_ADAPTER_NUM		3		//���3���Կ�
#define	D3D_FORMAT_NUM		6		//���6�ָ�ʽ
typedef struct __D3D_ADAPTER_T
{
	int		num;
	D3D_SUPPORT_FORMAT	format[D3D_ADAPTER_NUM][D3D_FORMAT_NUM];
}D3D_ADAPTER_T;


typedef struct D3D_FONT
{
	wchar_t	name[36];
	int		size;
	int		width;
	int		bold;
	int		italic;
}D3D_FONT;



typedef struct D3D_OSD
{
	wchar_t	string[128];
	DWORD	alpha;		//0-255
	DWORD	color;		//RGB(0xf9,0xf9,0xf9)
	DWORD	shadowcolor;		//RGB(0x4d,0x4d,0x4d)
	RECT	rect;		//��������
}D3D_OSD;


#define	D3D_SHOW_NO					0x00000000
#define	D3D_SHOW_CENTER_LINE		0x00000001
#define	D3D_SHOW_SEL_BOX			0x00000002
#define	D3D_SHOW_NONALARM_TITLE_BOX	0x00000004
#define D3D_SHOW_ZONE				0x00000008

#define D3D9_MAX_NODE_NUM				(30)		//���ڵ���
#define D3D9_MAX_NAME_LENGTH			32

typedef struct __D3D9_NODE
{
	int		x;
	int		y;
}D3D9_NODE;	//�ڵ�
typedef struct __D3D9_ZONE
{
	unsigned short		usZoneId;
	char				strZoneName[D3D9_MAX_NAME_LENGTH];
	DWORD				dwColor;
	int					alpha;
	unsigned int		uiTotalNodes;
	D3D9_NODE			pNodes[D3D9_MAX_NODE_NUM];
}D3D9_ZONE;	//����
typedef struct __D3D9_LINE
{
	unsigned short		usLineId;
	char				strLineName[D3D9_MAX_NAME_LENGTH];
	DWORD				dwColor;
	unsigned int		uiTotalNodes;
	D3D9_NODE			pNodes[D3D9_MAX_NODE_NUM];
}D3D9_LINE;	//��
typedef struct  __D3D9_COUNTER
{
	unsigned short		usCounterId;
	char				strCounterName[D3D9_MAX_NAME_LENGTH];
	DWORD				dwColor;
	RECT				rcRegion;
	int					iCounterValue;
	unsigned int		uiStatus;
}D3D9_COUNTER;	//������



//�������
typedef enum
{
	D3D_NO_ERROR				=	0x00,
	D3D_IN_PARAM_ERROR,							//�����������
	D3D_NOT_ENABLED,							//D3Dû������
	D3D_GET_FORMAT_FAIL,						//��ȡ�Կ�֧�ֵĸ�ʽ��Ϣʧ��
	D3D_FORMAT_NOT_SUPPORT,						//��֧��ָ���ĸ�ʽת��
	D3D_VERTEX_HAL_NOT_SUPPORT,					//��֧��Ӳ��������Ⱦ
	D3D_DEVICE_CREATE_FAIL,						//D3DDevice����ʧ��
	D3D_GETBACKBUFFER_FAIL,						//��ȡBackBufferʧ��
	D3D_CREATESURFACE_FAIL,						//����Surfaceʧ��
	D3D_LOCKSURFACE_FAIL,						//����Surfaceʧ��
	D3D_UNLOCKSURFACE_FAIL,						//����Surfaceʧ��
	D3D_UPDATESURFACE_FAIL,						//����Surfaceʧ��
	D3D_DEVICE_LOST,							//�豸��ʧ
		

	D3D_ERR_UNKNOWN
}D3D_ERR_CODE;

//D3DRender Handle
typedef void *D3D_HANDLE;


extern "C"
{
	//��ȡ�������
	int	 D3DRENDER_API D3D_GetD3DErrCode(D3D_HANDLE handle);

	//GDI��ʾ
	int	D3DRENDER_API  D3D_RenderRGB24ByGDI(HWND hWnd, char *pBuff, int width, int height, int ShownToScale, int OSDNum=0, D3D_OSD *_osd = NULL);

	//GDI��ʾ
	int	D3DRENDER_API  GDI_InitDraw(D3D_HANDLE *handle);
	int	D3DRENDER_API  GDI_DrawData(D3D_HANDLE handle, HWND hWnd, char *pBuff, int width, int height, int bitCount, LPRECT lpRectSrc, 
										int ShownToScale, COLORREF bkColor, int flip=0, int OSDNum=0, D3D_OSD *_osd = NULL);
	void D3DRENDER_API  GDI_SetDragStartPoint(D3D_HANDLE handle, float fX, float fY, unsigned char showBox);
	void D3DRENDER_API  GDI_SetDragEndPoint(D3D_HANDLE handle, float fX, float fY);
	void D3DRENDER_API  GDI_SetZoomIn(D3D_HANDLE handle, int zoomIn);
	void D3DRENDER_API  GDI_ResetDragPoint(D3D_HANDLE handle);



	
	//���ܷ�������
	void D3DRENDER_API  GDI_SetRenderMode(D3D_HANDLE handle, RENDER_MODE_ENUM _mode);				//������Ⱦģʽ
	RENDER_MODE_ENUM D3DRENDER_API  GDI_GetRenderMode(D3D_HANDLE handle);							//��ȡ��Ⱦģʽ

	void D3DRENDER_API  GDI_SetWarningAreaType(D3D_HANDLE handle, VA_WARN_TYPE_ENUM type);	//���þ�������������  1Ϊ������ 0Ϊ������
	void D3DRENDER_API  GDI_SetWarningGrade(D3D_HANDLE handle, VA_WARN_GRADE_ENUM grade);		//���þ������ȼ�
	void D3DRENDER_API  GDI_SetFullWarning(D3D_HANDLE handle);					//����ȫ���澯��
	void D3DRENDER_API  GDI_ShowAllWarningArea(D3D_HANDLE handle, int show);	//��ʾ���о�������
	void D3DRENDER_API  GDI_SetPenColor(D3D_HANDLE handle, COLORREF color);		//���û�����ɫ
	void D3DRENDER_API  GDI_SetWarningLineDirection(D3D_HANDLE handle, VA_DIRECTION_ENUM _direction);	//���þ����߷���
	void D3DRENDER_API  GDI_SetWarningLineDirectionById(D3D_HANDLE handle, int lineId, VA_DIRECTION_ENUM _direction);					//���ݾ�����ID�޸��䷽��
	void D3DRENDER_API  GDI_AddWarningAreaNode(D3D_HANDLE handle, VA_DETECT_POINT_T *p);	//��Ӿ�������ڵ�
	void D3DRENDER_API  GDI_UpdateWarningAreaNode(D3D_HANDLE handle, VA_DETECT_POINT_T *p);	//���¾�������ڵ�
	void D3DRENDER_API  GDI_EndWarningAreaNode(D3D_HANDLE handle);	//����������������ڵ�
	void D3DRENDER_API  GDI_DeleteWarningAreaAndLine(D3D_HANDLE handle, int _type, int del_all, int areaOrLineId);		//ɾ���������
	void D3DRENDER_API  GDI_UpdateWarningAreaPosition(D3D_HANDLE handle, LPRECT lpRect);		//��������ʱ�İٷֱ�, ���յ�ǰ��rect,����λ�õ�
	void D3DRENDER_API  GDI_GetWarningAreaList(D3D_HANDLE handle, VA_DETECT_ZONE_LIST_T **list);//��ȡ���������б�
	void D3DRENDER_API  GDI_SetWarningAreaList(D3D_HANDLE handle, VA_DETECT_ZONE_LIST_T *list, int num);	//���þ��������б�

	void D3DRENDER_API  GDI_GetWarningLineList(D3D_HANDLE handle, VA_DETECT_LINE_LIST_T **list);//��ȡ�������б�
	void D3DRENDER_API  GDI_SetWarningLineList(D3D_HANDLE handle, VA_DETECT_LINE_LIST_T *list, int num);	//���þ������б�


	//�Զ�������
	void D3DRENDER_API  GDI_AddCustomZonePoint(D3D_HANDLE handle, int id, char *name, VA_DETECT_POINT_T *p, 
											unsigned char hasArrow, unsigned char fill, unsigned char move/*�����ƶ�*/, 
											unsigned char showPoint/*��ʾ��*/,
											unsigned char showZoneSize/*��ʾ�����С*/,
											int minPointNum,int maxPointNum, unsigned char checkOverlap, 
											COLORREF zoneColor, COLORREF borderColor, COLORREF textColor, int maxZoneNum,
											int alphaNormal, int alphaSelected);
	void D3DRENDER_API  GDI_MoveCustomZonePoint(D3D_HANDLE handle, VA_DETECT_POINT_T *p, unsigned char keyRelease/*LButtonUp�¼�ʱ��ֵΪ1*/, unsigned char checkOverlap);
	void D3DRENDER_API  GDI_EndCustomZonePoint(D3D_HANDLE handle, unsigned char checkOverlap);	//�������Ƶ�ǰ����
	void D3DRENDER_API  GDI_DeleteCustomZone(D3D_HANDLE handle, unsigned char deleteAll, char *name, int zoneId, unsigned char updateId);	//ɾ������
	void D3DRENDER_API  GDI_ShowCustomZoneById(D3D_HANDLE handle, int id);				//��ʾȫ��  or ��ʾָ��ID������
	void D3DRENDER_API  GDI_UpdateCustomZonePosition(D3D_HANDLE handle, LPRECT lpRect);//��������ʱ�İٷֱ�, ���յ�ǰ��rect,����λ�õ�
	int  D3DRENDER_API  GDI_GetCustomZone(D3D_HANDLE handle, VA_DETECT_ZONE_LIST_T **list, int *num);
	int  D3DRENDER_API  GDI_SetCustomZone(D3D_HANDLE handle, VA_DETECT_ZONE_LIST_T *list, int num);


	//2019.01.23
	int  D3DRENDER_API  GDI_AddRectangleZone(D3D_HANDLE handle, VA_DETECT_POINT_T *p, int id, char *name, unsigned char hasArrow, 
												unsigned char fill, 
												unsigned char move, 
												unsigned char showPoint, 
												unsigned char showZoneSize,
												unsigned char checkOverlap/*�Ƿ����ص�*/, COLORREF zoneColor, COLORREF borderColor, COLORREF textColor, int maxZoneNum,
												int alphaNormal, int alphaSelected);			//��Ӿ�������
	int  D3DRENDER_API  GDI_MoveRectangleZone(D3D_HANDLE handle, VA_DETECT_POINT_T *p, unsigned char keyRelease, unsigned char checkOverlap/*�Ƿ����ص�*/);
	int  D3DRENDER_API  GDI_EndRectangleZone(D3D_HANDLE handle, unsigned char checkOverlap/*�Ƿ����ص�*/);


	//�Զ�����
	int	 D3DRENDER_API  GDI_AddCustomLinePoint(D3D_HANDLE handle, VA_DETECT_POINT_T *p, int id, char *name, unsigned char hasArrow, VA_DIRECTION_ENUM arrowDirection,
								COLORREF lineColor, COLORREF textColor, int maxLineNum);
	int	 D3DRENDER_API  GDI_MoveCustomLinePoint(D3D_HANDLE handle, VA_DETECT_POINT_T *p, unsigned char keyRelease/*LButtonUp�¼�ʱ��ֵΪ1*/);
	int	 D3DRENDER_API  GDI_EndCustomLinePoint(D3D_HANDLE handle);	//�������Ƶ�ǰ����
	void D3DRENDER_API  GDI_DeleteCustomLine(D3D_HANDLE handle, unsigned char deleteAll, char *name, int lineId, unsigned char updateId);	//ɾ������
	void D3DRENDER_API  GDI_ShowCustomLineById(D3D_HANDLE handle, int id);				//��ʾȫ��  or ��ʾָ��ID������
	void D3DRENDER_API  GDI_UpdateCustomLinePosition(D3D_HANDLE handle, LPRECT lpRect);//��������ʱ�İٷֱ�, ���յ�ǰ��rect,����λ�õ�
	int  D3DRENDER_API  GDI_GetCustomLine(D3D_HANDLE handle, VA_DETECT_LINE_LIST_T **list, int *num);
	int  D3DRENDER_API  GDI_SetCustomLine(D3D_HANDLE handle, VA_DETECT_LINE_LIST_T *list, int num);

	//Goto ��
	int  D3DRENDER_API  GDI_SetGotoLine(D3D_HANDLE handle, VA_DETECT_POINT_T *pStart, VA_DETECT_POINT_T *pEnd, unsigned char hasArrow, COLORREF lineColor, unsigned char keyRelease);
	int  D3DRENDER_API  GDI_SetCustomZonePtr(D3D_HANDLE handle, VA_DETECT_ZONE_T *pZonePtr);	//�ⲿָ����������



	void D3DRENDER_API  GDI_DrawBoundBox(D3D_HANDLE handle, int drawProperty, char *pBox, int boxNum, char *pEvents, int evtNum, 
							char *pCounters, int counterNum, char *pStats, int statsNum, LPPOINT trkPoint, int *trkObjId);


	int D3DRENDER_API  GDI_DeinitDraw(D3D_HANDLE *handle);

	//=====================================================
	//��ʼ��,������Դ
	//D3D_FONT:  OSD�����ڴ�ָ��
	int D3DRENDER_API D3D_Initial(D3D_HANDLE *handle, HWND hWnd, unsigned int width, unsigned int height, unsigned int nAdapterNo = D3DADAPTER_DEFAULT,
									int maxch=1, D3D_SUPPORT_FORMAT format=D3D_FORMAT_YUY2,D3D_FONT *font=NULL);	//D3DFMT_YUY2
	//�ͷ�������Դ
	int D3DRENDER_API D3D_Release(D3D_HANDLE *handle);

	//��ȡ�Կ�֧�ֵĸ�ʽ(���������ʾ��ʽ: YV12 YUY2 RGB565 A8R8G8B8)
	D3D_ERR_CODE D3DRENDER_API D3D_GetSupportFormat(D3D_ADAPTER_T *adapterinfo);

	//����
	bool D3DRENDER_API D3D_Clear( D3D_HANDLE handle, COLORREF _color);

	//ѡ�и�ͨ��,�ı�߿���ɫ
	bool D3DRENDER_API D3D_SelectCH( D3D_HANDLE handle, int ch);


	//����Surface�ϵ�����,�������(������ffmpeg�е�sws_scaleֱ��ת�����),֮���ٵ���D3D_UpdateData������ʾ(D3D_UpdateData�е�pBuff����ΪNULL)
	int D3DRENDER_API D3D_LockSurfaceData(D3D_HANDLE handle, int ch, void **pBuff, int *pitch, int width, int height);
	int D3DRENDER_API D3D_UnlockSurfaceData(D3D_HANDLE handle, int ch, unsigned int timestamp);
	int D3DRENDER_API D3D_GetDisplaySurface(D3D_HANDLE handle, int _chLow, int _chHigh, int *surfaceId, int *frameNum, unsigned int *_timestamp);

	//������Ƶ����	(ͨ��(���ͨ����D3D_Initialʱmaxch����), ��������Ƶ����(YUV), width��, height��, ͼ���С, ��������, OSD����, OSD����)
	//LPRECT lpRectSrc: ��������ʾ����Ϊ0,0,width,height
	//                  �����Ŵ�Ч��,��: 100,100,300,300   ��ʾ��Left100,Top100,Right300,Bottom300��ʼ��ʾͼ��lpRectDst
	int D3DRENDER_API D3D_UpdateData(D3D_HANDLE handle, int ch, unsigned char *pBuff, int width, int height, LPRECT lpRectSrc, LPRECT lpRectDst, 
										int OSDNum=0, D3D_OSD *_osd = NULL);


	int D3DRENDER_API D3D_DrawTrackingBox(D3D_HANDLE handle, int ch, int width, int height, int drawProperty, 
										char *pBox, int boxNum, char *pEvents, int evtNum, char *pCounters, int cntNum, 
										char *pStats, int statsNum, LPPOINT trkPoint, int *trkObjId);
	int D3DRENDER_API D3D_DrawEventBox(D3D_HANDLE handle, int ch, int width, int height, char *pBox, int boxNum);


	//Render (���Ƶ�����)
	int D3DRENDER_API D3D_Render(D3D_HANDLE handle, HWND hWnd, int ShownToScale, LPRECT lpRect=NULL, int osdNum=0, D3D_OSD *d3dOsd=NULL);


	//��ȡ�Զ����ٵ�����
	int D3DRENDER_API D3D_GetTrackingRect(D3D_HANDLE handle, LPRECT lpRect);

	//������ʾѡ��
	int D3DRENDER_API D3D_SetDisplayFlag(D3D_HANDLE handle, unsigned int flag);
	int D3DRENDER_API D3D_GetDisplayFlag(D3D_HANDLE handle, unsigned int *flag);

	//������ק��ʼ��
	void D3DRENDER_API D3D_SetStartPoint(D3D_HANDLE handle, float fX, float fY);
	void D3DRENDER_API D3D_SetEndPoint(D3D_HANDLE handle, float fX, float fY);
	void D3DRENDER_API D3D_ResetSelZone(D3D_HANDLE handle);

	//=====================================================
	//�������
	int D3DRENDER_API D3D_AddZone(D3D_HANDLE handle, D3D9_ZONE *zone);
	int D3DRENDER_API D3D_DeleteAllZones(D3D_HANDLE handle);
	int D3DRENDER_API D3D_AddLine(D3D_HANDLE handle, D3D9_LINE *line);
	int D3DRENDER_API D3D_DeleteAllLines(D3D_HANDLE handle);

	//���ƶ��������
	int D3DRENDER_API D3D_EnableMotionGraph(D3D_HANDLE handle, unsigned char _row, unsigned char _col, unsigned char *_mdConfig, unsigned char _enable);
};



#endif




/*
����:
D3DFMT_A8R8G8B8             = 21,
D3DFMT_R5G6B5               = 23,
D3DFMT_A2R10G10B10          = 35,
*/

