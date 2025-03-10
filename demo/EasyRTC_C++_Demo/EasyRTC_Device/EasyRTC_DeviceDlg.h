
// EasyRTC_DeviceDlg.h: 头文件
//

#pragma once

#include "EasyRtcDevice.h"
#include "EasyStreamClient/EasyStreamClientAPI.h"
#pragma comment(lib, "EasyStreamClient/libEasyStreamClient.lib")
#include "AudioCapturer/libAudioCapturerAPI.h"
#pragma comment(lib, "AudioCapturer/AudioCapturer.lib")
#include "FFTranscode/TransCodeAPI.h"
#pragma comment(lib, "FFTranscode/FFTransCode.lib")
#include "D3DRender/D3DRenderAPI.h"
#pragma comment(lib, "D3DRender/D3DRender.lib")
#include "CDlgVideoRender.h"

typedef struct __LOCAL_RTC_DEVICE_T
{
	Easy_Handle		easyStreamHandle;
	EasyRtcDevice	easyRtcDevice;

	uint64_t		videoPTS;
	uint64_t		audioDTS;

	uint64_t		videoLastPts;
	int				audioDTSstep;


	__LOCAL_RTC_DEVICE_T()
	{
		easyStreamHandle = NULL;
		videoPTS = 0;
		audioDTS = 0;
	};

}LOCAL_RTC_DEVICE_T;

typedef struct __FRAME_INFO_T
{
	int bufsize;
	char* pbuf;

}FRAME_INFO_T;

typedef vector<char*>	LOG_VECTOR;
typedef vector< FRAME_INFO_T>	FRAME_INFO_VECTOR;
// CEasyRTCDeviceDlg 对话框
class CEasyRTCDeviceDlg : public CDialogEx
{
// 构造
public:
	CEasyRTCDeviceDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_EASYRTC_DEVICE_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持

public:
	void	AddDataChannel_Data(const char *pbuf, const int bufsize);

	void	OnRecvVideoData(int codecID, char* framedata, int framesize);
	void	OnProcVideoData(int codecID, char* framedata, int framesize);

private:
	CDlgVideoRender* pDlgVideoRender;
	CStatic* pStaticLogAndMsg;			//IDC_STATIC_LOG
	CRichEditCtrl* pRichEditCtrlLog;	//IDC_RICHEDIT2_LOG
	CStatic* pStaticDataChannel;		//IDC_STATIC_DATA_CHANNEL
	CRichEditCtrl* pRichEditCtrlSendMsg;	//IDC_RICHEDIT2_SEND_MSG
	CButton* pBtnSendMsg;				//IDC_BUTTON_SEND
	CComboBox* pComboxVideoDeviceList;	//IDC_COMBO_VIDEO_DEVICE_LIST
	CComboBox* pComboxAudioDeviceList;	//IDC_COMBO_AUDIO_DEVICE_LIST
	CButton* pBtnStart;					//IDC_BUTTON_START
	CButton* pBtnStop;					//IDC_BUTTON_STOP


	LOCAL_RTC_DEVICE_T	mLocalRTCDevice;
	OSMutex				mutexLog;
	LOG_VECTOR			logVector;

	OSMutex				mutexFrame;
	FRAME_INFO_VECTOR	frameInfoVector;

	TRANSCODE_HANDLE	transCodeHandle;
	D3D_HANDLE d3dHandle;
	HWND		renderHwnd;


	void	LockLog()	{ LockMutex(&mutexLog); };
	void	UnlockLog()	{ UnlockMutex(&mutexLog); };

	void	LockFrameVector() { LockMutex(&mutexFrame); };
	void	UnlockFrameVector() { UnlockMutex(&mutexFrame); };


	void	InitialComponents();
	void	CreateComponents();
	void	UpdateComponents();
	void	DeleteComponents();
	void	UpdateVideoPosition(LPRECT lpRect);

// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
	afx_msg LRESULT OnLog(WPARAM, LPARAM);
	afx_msg LRESULT OnPeerVideoFrame(WPARAM, LPARAM);
public:
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSizing(UINT fwSide, LPRECT pRect);
	afx_msg void OnBnClickedButtonStart();
	afx_msg void OnBnClickedButtonStop();
	afx_msg void OnBnClickedButtonSend();
};
