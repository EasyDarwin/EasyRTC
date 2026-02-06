
// EasyRTCDevice_WinDlg.h: 头文件
//

#pragma once



#include "EasyRTCDeviceAPI.h"
#ifdef _DEBUG
#pragma comment(lib, "../../windows/x64/Debug/EasyRTC_Open.lib")
#else
#pragma comment(lib, "../../windows/x64/Release/EasyRTC_Open.lib")
#endif

#include "EasyStreamClient/EasyStreamClientAPI.h"
#pragma comment(lib, "EasyStreamClient/libEasyStreamClient.lib")
#include "AudioCapturer/libAudioCapturerAPI.h"
#pragma comment(lib, "AudioCapturer/AudioCapturer.lib")
#include "FFTranscode/TransCodeAPI.h"
#pragma comment(lib, "FFTranscode/FFTransCode.lib")
#include "D3DRender/D3DRenderAPI.h"
#pragma comment(lib, "D3DRender/D3DRender.lib")
#include "AudioPlayer/libAudioPlayerAPI.h"
#pragma comment(lib, "AudioPlayer/AudioPlayer.lib")
#pragma comment(lib, "winmm.lib")
#include "CDlgVideo.h"
#include "ButtonListCtrl.h"
#include <vector>
extern "C"
{
#include "osmutex.h"
#include "osthread.h"
}

#include "XmlConfig.h"

typedef struct __VIDEO_RENDER_T
{
	TRANSCODE_HANDLE	transcodeHandle;
	D3D_HANDLE		d3dHandle;
	D3D_SUPPORT_FORMAT	d3dRenderFormat;
	HWND		renderHwnd;
	RECT		renderRect;

	int			renderFrameNum;			// 已渲染帧数

	time_t		startTime;
}VIDEO_RENDER_T;

typedef struct __LOCAL_RTC_DEVICE_T
{
	int				sourceType;			// 0x00(USB Camera)		0x01(pull stream)
	Easy_Handle		easyStreamHandle;
	EASYRTC_HANDLE	easyRTCDeviceHandle;
	EASYRTC_HANDLE	easyRTCCallerHandle;
	bool			callStatus;
	char			peerUUID[128];

	bool			sendVideoFlag;		// 是否有播放端请求发送视频

	int		videoCodecID;
	int		audioCodecID;
	int		audioSamplerate;			// 采样率
	int		audioBitPerSamples;			// 采样精度
	int		audioChannels;				// 通道数

	uint64_t		videoPTS;
	uint64_t		audioDTS;

	uint64_t		videoLastPts;
	int				audioDTSstep;

	bool			localPreview;		// 本地预览
	bool			findLocalKeyframe;
	bool			initAudioPlayer;	// 初始化音频播放器
	
	VIDEO_RENDER_T	localVideoRender;
	OSTHREAD_OBJ_T* pLocalRenderThread;

	VIDEO_RENDER_T	remoteVideoRender;
	OSTHREAD_OBJ_T* pRemoteRenderThread;
	OSTHREAD_OBJ_T* pRemoteAudioPlayThread;

	OSMutex			mutex;
	void* pThis;

	__LOCAL_RTC_DEVICE_T()
	{
		sourceType = -1;
		easyStreamHandle = NULL;
		easyRTCDeviceHandle = NULL;
		easyRTCCallerHandle = NULL;
		callStatus = false;
		memset(peerUUID, 0x00, sizeof(peerUUID));

		videoCodecID = 0;
		audioCodecID = 0;

		videoPTS = 0;
		audioDTS = 0;
		videoLastPts = 0;
		audioDTSstep = 0;

		sendVideoFlag = false;

		InitMutex(&mutex);

		pThis = NULL;
		pLocalRenderThread = NULL;
		pRemoteRenderThread = NULL;
		pRemoteAudioPlayThread = NULL;
		localPreview = true;
		initAudioPlayer = false;

		memset(&localVideoRender, 0x00, sizeof(VIDEO_RENDER_T));
		memset(&remoteVideoRender, 0x00, sizeof(VIDEO_RENDER_T));
	};

	~__LOCAL_RTC_DEVICE_T()
	{
		DeinitMutex(&mutex);
	}

	void	Lock()	{ LockMutex(&mutex); }
	void	Unlock() { UnlockMutex(&mutex); }

}LOCAL_RTC_DEVICE_T;

typedef struct __FRAME_INFO_T
{
	int		rawVideo;
	EASY_FRAME_INFO	info;
	char* pbuf;

}FRAME_INFO_T;

typedef vector<char*>	LOG_VECTOR;
typedef vector< FRAME_INFO_T>	FRAME_INFO_VECTOR;


// CEasyRTCDeviceWinDlg 对话框
class CEasyRTCDeviceWinDlg : public CDialogEx
{
// 构造
public:
	CEasyRTCDeviceWinDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_EASYRTCDEVICE_WIN_DIALOG };
#endif

public:
	void	InitComponents();
	void	CreateComponents();
	void	UpdateComponents();
	void	DeleteComponents();

	void	ProcessLocalVideoToQueue(void* pBuf, EASY_FRAME_INFO* _frameInfo, int rawVideo);
	int	    ProcessLocalVideoFromQueue(int clear);

	void	ProcessRemoteVideoToQueue(void* pBuf, EASY_FRAME_INFO* _frameInfo, int rawVideo);
	int	    ProcessRemoteVideoFromQueue(int clear);

	void	ProcessRemoteAudioToQueue(void* pBuf, EASY_FRAME_INFO* _frameInfo);
	int	    ProcessRemoteAudioFromQueue(int clear);

	void	InitPlayResource();

	void	OnPeerMessage(char* msg, int size);
	void	SetPeerUUID(const char* peerUUID);

	void	EnableTextMessage(BOOL bEnable);
	void	EnableCallEnd(BOOL bEnable);

	void	RefreshDeviceList();
	void	UpdateCallState(bool reset);			// 在设备列表中更新呼叫状态

	LOCAL_RTC_DEVICE_T* GetRTCDevicePtr() { return&mLocalRTCDevice; }

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持

private:
	CStatic* pGrpRemote;			// IDC_STATIC_REMOTE
	CStatic* pGrpLocal;				// IDC_STATIC_LOCAL
	CStatic* pGrpLogMessage;		// IDC_STATIC_LOG_MESSAGE
	CDlgVideo* pDlgVideoRemote;		// CDlgVideo
	CDlgVideo* pDlgVideoLocal;		// CDlgVideo

	CStatic* pStaticStatus;			// IDC_STATIC_STATUS
	CStatic* pStaticServerIP;		// IDC_STATIC_SERVER_IP
	CStatic* pStaticServerPort;		// IDC_STATIC_SERVER_PORT
	CButton* pChkSSL;				// IDC_CHECK_IS_SECURT
	CStatic* pStaticDeviceID;		// IDC_STATIC_DEVICE_ID
	CEdit* pEdtServerIP;			// IDC_EDIT_SERVER_IP
	CEdit* pEdtServerPort;			// IDC_EDIT_SERVER_PORT
	CEdit* pEdtDeviceID;			// IDC_EDIT_DEVICE_ID
	CButton* pBtnStartStop;			// IDC_BUTTON_START_STOP
	CStatic* pGrpDeviceList;		// IDC_STATIC_DEVICE_LIST
	CButtonListCtrl* pListCtrlDevice;		// IDC_LIST_DEVICE_LIST
	CButton* pBtnRefresh;			// IDC_BUTTON_REFRESH_DEVICELIST


	CEdit* pEdtSendText;			// IDC_EDIT_SEND_TEXT
	CButton* pBtnSendMessage;		// IDC_BUTTON_SEND_MESSAGE
	CButton* pBtnCallEnd;			// IDC_BUTTON_CALL_END
	CComboBox* pComboxSourceType;	// IDC_COMBO_SOURCE_TYPE
	CComboBox* pComboxLocalCameraList;	// IDC_COMBO_LOCAL_CAMERA_LIST
	CComboBox* pComboxLocalAudioList;	// IDC_COMBO_LOCAL_AUDIO_LIST
	CEdit* pEdtStreamURL;				// IDC_EDIT_STREAM_URL
	CButton* pBtnPreview;				// IDC_BUTTON_PREVIEW
	CRichEditCtrl* pRedtLog;			// IDC_RICHEDIT2_LOG

	char	mLastDeviceID[128];
	

	void	ChangeSourceType();

	LOCAL_RTC_DEVICE_T	mLocalRTCDevice;
	OSMutex				mutexLog;
	LOG_VECTOR			logVector;

	OSMutex				mutexLocalFrame;
	FRAME_INFO_VECTOR	localFrameInfoVector;
	OSMutex				mutexDecodeAndRender;

	OSMutex				mutexRemoteFrame;
	FRAME_INFO_VECTOR	remoteFrameInfoVector;
	OSMutex				mutexRemoteDecodeAndRender;

	OSMutex				mutexAudioFrameQueue;
	FRAME_INFO_VECTOR	remoteAudioFrameInfoVector;

	char	mPeerUUID[128];
	char	peerCustomMessage[1024];		// 暂存对方发送的消息

	bool	OpenLocalSource();
	void	RenderLocalVideo(void* pBuf, EASY_FRAME_INFO* _frameInfo, int rawVideo);
	void	RenderVideo(VIDEO_RENDER_T *pRender, void* pBuf, EASY_FRAME_INFO* _frameInfo, int rawVideo);
	//void	RenderRemoteVideo(void* pBuf, EASY_FRAME_INFO* _frameInfo);
	bool	CloseLocalSource();
	void	CloseAll();

	void	LockLog() { LockMutex(&mutexLog); };
	void	UnlockLog() { UnlockMutex(&mutexLog); };

	void	LockLocalFrameVector() { LockMutex(&mutexLocalFrame); };
	void	UnlockLocalFrameVector() { UnlockMutex(&mutexLocalFrame); };

	void	LockRemoteFrameVector() { LockMutex(&mutexRemoteFrame); };
	void	UnlockRemoteFrameVector() { UnlockMutex(&mutexRemoteFrame); };

	void	LockDecodeAndRender() { LockMutex(&mutexDecodeAndRender); }
	void	UnlockDecodeAndRender() { UnlockMutex(&mutexDecodeAndRender); }


	void	LockAudioFrameQueue() { LockMutex(&mutexAudioFrameQueue); }
	void	UnlockAudioFrameQueue() { UnlockMutex(&mutexAudioFrameQueue); }

	


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
	afx_msg LRESULT OnStatus(WPARAM, LPARAM);
	afx_msg LRESULT OnOnlineDevice(WPARAM, LPARAM);
	afx_msg LRESULT OnClickCallButton(WPARAM, LPARAM);
	afx_msg LRESULT OnUpdateVideoWindow(WPARAM, LPARAM);
	afx_msg LRESULT OnRefreshDeviceList(WPARAM, LPARAM);
public:
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	afx_msg void OnDestroy();
	afx_msg void OnCbnSelchangeComboSourceType();
	afx_msg void OnBnClickedButtonStartStop();
	afx_msg void OnBnClickedButtonPreview();
	afx_msg void OnBnClickedButtonRefreshDevicelist();
	CButtonListCtrl mDeviceListCtrl;
	afx_msg void OnBnClickedButtonSendMessage();
	afx_msg void OnBnClickedButtonCallEnd();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
};
