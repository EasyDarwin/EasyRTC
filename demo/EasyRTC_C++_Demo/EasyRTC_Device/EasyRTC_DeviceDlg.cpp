
// EasyRTC_DeviceDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "EasyRTC_Device.h"
#include "EasyRTC_DeviceDlg.h"
#include "afxdialogex.h"
#include "CharacterTranscoding.h"
#include "AudioPlayer/libAudioPlayerAPI.h"
#include "g711.h"
#pragma comment(lib, "AudioPlayer/AudioPlayer.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define WM_EASY_RTC_CALLBACK		WM_USER+2001
#define WM_EASY_RTC_VIDEO		WM_USER+2002

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CEasyRTCDeviceDlg 对话框



CEasyRTCDeviceDlg::CEasyRTCDeviceDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_EASYRTC_DEVICE_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	InitialComponents();
}

void CEasyRTCDeviceDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CEasyRTCDeviceDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_SIZING()
	ON_BN_CLICKED(IDC_BUTTON_START, &CEasyRTCDeviceDlg::OnBnClickedButtonStart)
	ON_BN_CLICKED(IDC_BUTTON_STOP, &CEasyRTCDeviceDlg::OnBnClickedButtonStop)
	ON_BN_CLICKED(IDC_BUTTON_SEND, &CEasyRTCDeviceDlg::OnBnClickedButtonSend)
	ON_MESSAGE(WM_EASY_RTC_CALLBACK, OnLog)
	ON_MESSAGE(WM_EASY_RTC_VIDEO, OnPeerVideoFrame)
END_MESSAGE_MAP()


// CEasyRTCDeviceDlg 消息处理程序

BOOL CEasyRTCDeviceDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	CreateComponents();

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CEasyRTCDeviceDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CEasyRTCDeviceDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		UpdateComponents();

		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CEasyRTCDeviceDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void	CEasyRTCDeviceDlg::InitialComponents()
{
	pDlgVideoRender = NULL;

	pStaticLogAndMsg = NULL;
	pRichEditCtrlLog = NULL;
	pStaticDataChannel = NULL;
	pRichEditCtrlSendMsg = NULL;
	pBtnSendMsg = NULL;
	pComboxVideoDeviceList = NULL;
	pComboxAudioDeviceList = NULL;
	pBtnStart = NULL;
	pBtnStop = NULL;

	transCodeHandle = NULL;
	d3dHandle = NULL;
	renderHwnd = NULL;

	InitMutex(&mutexLog);
	InitMutex(&mutexFrame);
}
void	CEasyRTCDeviceDlg::CreateComponents()
{
	if (NULL == pDlgVideoRender)
	{
		pDlgVideoRender = new CDlgVideoRender();
		if (NULL != pDlgVideoRender)
		{
			pDlgVideoRender->Create(IDD_DIALOG_VIDEO_RENDER, this);
			//pDlgVideoRender->ShowWindow(SW_SHOW);

			renderHwnd = pDlgVideoRender->GetSafeHwnd();
		}
	}

	__CREATE_WINDOW(pStaticLogAndMsg, CStatic, IDC_STATIC_LOG);
	__CREATE_WINDOW(pRichEditCtrlLog, CRichEditCtrl, IDC_RICHEDIT2_LOG);
	__CREATE_WINDOW(pStaticDataChannel, CStatic, IDC_STATIC_DATA_CHANNEL);
	__CREATE_WINDOW(pRichEditCtrlSendMsg, CRichEditCtrl, IDC_RICHEDIT2_SEND_MSG);
	__CREATE_WINDOW(pBtnSendMsg, CButton, IDC_BUTTON_SEND);
	

	__CREATE_WINDOW(pComboxVideoDeviceList, CComboBox, IDC_COMBO_VIDEO_DEVICE_LIST);
	__CREATE_WINDOW(pComboxAudioDeviceList, CComboBox, IDC_COMBO_AUDIO_DEVICE_LIST);
	
	__CREATE_WINDOW(pBtnStart, CButton, IDC_BUTTON_START);
	__CREATE_WINDOW(pBtnStop, CButton, IDC_BUTTON_STOP);

	wchar_t wszName[64] = { 0 };
	LOCAL_DEVICE_MAP::iterator itVideoDevice = LocalVideoDeviceMap.begin();
	while (itVideoDevice != LocalVideoDeviceMap.end())
	{
		memset(wszName, 0x00, sizeof(wszName));

		wchar_t wszCamera[64] = { 0 };
		MByteToWChar(itVideoDevice->first.data(), wszCamera, sizeof(wszCamera) / sizeof(wszCamera[0]));

		pComboxVideoDeviceList->AddString(wszCamera);
		itVideoDevice++;
	}
	if (pComboxVideoDeviceList->GetCount() > 0)	pComboxVideoDeviceList->SetCurSel(0);

	libAudioCapturer_Init();
	AUDIO_CAPTURER_DEVICE_MAP	audioCapturerDeviceMap;
	libAudioCapturer_GetAudioCaptureDeviceList(&audioCapturerDeviceMap);
	AUDIO_CAPTURER_DEVICE_MAP::iterator itAudioDevice = audioCapturerDeviceMap.begin();
	while (itAudioDevice != audioCapturerDeviceMap.end())
	{
		memset(wszName, 0x00, sizeof(wszName));
		MByteToWChar(itAudioDevice->second.description, wszName, sizeof(wszName) / sizeof(wszName[0]));
		pComboxAudioDeviceList->AddString(wszName);
		itAudioDevice++;
	}
	if (pComboxAudioDeviceList->GetCount() > 0)	pComboxAudioDeviceList->SetCurSel(0);
}
void	CEasyRTCDeviceDlg::UpdateComponents()
{
	CRect	rcClient;
	GetClientRect(&rcClient);
	if (rcClient.IsRectEmpty())		return;

	
	int logListHeight = (int)((float)rcClient.Height() * 0.8f);			// 日志列表高度
	int msgBoxHeight = (int)((float)rcClient.Height() * 0.1f);			// 发送消息框高度

	CRect	rcLog;
	rcLog.SetRect(rcClient.left, rcClient.top, rcClient.right, rcClient.top + 20);
	__MOVE_WINDOW(pStaticLogAndMsg, rcLog);
	rcLog.SetRect(rcLog.left, rcLog.bottom, rcLog.right, rcLog.bottom + logListHeight);
	__MOVE_WINDOW(pRichEditCtrlLog, rcLog);
	CRect	rcMsgBox;
	rcMsgBox.SetRect(rcLog.left, rcLog.bottom + 2, rcLog.right, rcLog.bottom + 2 + 20);
	__MOVE_WINDOW(pStaticDataChannel, rcMsgBox);
	
	rcMsgBox.SetRect(rcMsgBox.left, rcMsgBox.bottom + 2, rcLog.right - 140, rcMsgBox.bottom + msgBoxHeight);
	__MOVE_WINDOW(pRichEditCtrlSendMsg, rcMsgBox);

	CRect	rcBtnSend;
	rcBtnSend.SetRect(rcMsgBox.right + 5, rcMsgBox.top + 5, rcLog.right - 5, rcMsgBox.bottom - 5);
	__MOVE_WINDOW(pBtnSendMsg, rcBtnSend);
	
	int deviceListWidth = (int)((float)rcClient.Width() * 0.3f);

	CRect	rcLocalVideoDeviceList;
	rcLocalVideoDeviceList.SetRect(rcMsgBox.left + 8, rcMsgBox.bottom + 2, rcMsgBox.left + 8 + deviceListWidth, rcClient.bottom - 2);
	if (pComboxVideoDeviceList && pComboxVideoDeviceList->IsWindowVisible())
	{
		__MOVE_WINDOW(pComboxVideoDeviceList, rcLocalVideoDeviceList);
	}

	CRect	rcLocalAudioDeviceList;
	rcLocalAudioDeviceList.SetRect(rcLocalVideoDeviceList.right + 10, rcLocalVideoDeviceList.top, rcLocalVideoDeviceList.right + 10 + rcLocalVideoDeviceList.Width(), rcLocalVideoDeviceList.bottom);
	if (pComboxAudioDeviceList && pComboxAudioDeviceList->IsWindowVisible())
	{
		__MOVE_WINDOW(pComboxAudioDeviceList, rcLocalAudioDeviceList);
	}


	int buttonWidth = 130;
	CRect	rcBtnStart;
	rcBtnStart.SetRect(rcLocalAudioDeviceList.right + 30, rcLocalAudioDeviceList.top, rcLocalAudioDeviceList.right + 30 + buttonWidth, rcLocalAudioDeviceList.bottom);
	__MOVE_WINDOW(pBtnStart, rcBtnStart);

	CRect	rcBtnStop;
	rcBtnStop.SetRect(rcBtnStart.right + 10, rcBtnStart.top, rcBtnStart.right + 10 + rcBtnStart.Width(), rcBtnStart.bottom);
	__MOVE_WINDOW(pBtnStop, rcBtnStop);
}
void	CEasyRTCDeviceDlg::DeleteComponents()
{
	OnBnClickedButtonStop();

	if (NULL != pDlgVideoRender)
	{
		pDlgVideoRender->DestroyWindow();
		delete pDlgVideoRender;
		pDlgVideoRender = NULL;
	}

	libAudioCapturer_Deinit();

	DeinitMutex(&mutexLog);
	DeinitMutex(&mutexFrame);
}
void	CEasyRTCDeviceDlg::UpdateVideoPosition(LPRECT lpRect)
{

}

void CEasyRTCDeviceDlg::OnDestroy()
{
	DeleteComponents();

	CDialogEx::OnDestroy();
}


void CEasyRTCDeviceDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	UpdateComponents();
}


void CEasyRTCDeviceDlg::OnSizing(UINT fwSide, LPRECT pRect)
{
	CDialogEx::OnSizing(fwSide, pRect);

	// TODO: 在此处添加消息处理程序代码
	UpdateComponents();
}


int		videoCodecID = 0;
int		audioCodecID = 0;
int Easy_APICALL __EasyStreamClientCallBack(void* _channelPtr, int _frameType, void* pBuf, EASY_FRAME_INFO* _frameInfo)
{
	LOCAL_RTC_DEVICE_T* pLocalDevice = (LOCAL_RTC_DEVICE_T*)_channelPtr;
	EasyRtcDevice* pRtcDevice = (EasyRtcDevice*)&pLocalDevice->easyRtcDevice;

	if (_frameType == EASY_SDK_VIDEO_FRAME_FLAG)
	{
		uint64_t pts = _frameInfo->timestamp_sec * 1000 + _frameInfo->timestamp_usec / 1000;

		if (pLocalDevice->videoLastPts > 0)
		{
			uint64_t videoFrameInterval = pts - pLocalDevice->videoLastPts;
			if (videoFrameInterval > 0 && videoFrameInterval < 1000)
			{
				pLocalDevice->videoPTS += videoFrameInterval;
			}
			//printf("video interval: %d\n", videoFrameInterval);
		}
		else
		{
			pLocalDevice->videoPTS = 33;
		}
		pLocalDevice->videoLastPts = pts;

		if (videoCodecID < 1)	videoCodecID = _frameInfo->codec;

		//EasyRtcDevice::__Print__(__FUNCTION__, __LINE__, "videoPTS:%llu\n", pLocalDevice->videoPTS);

		pRtcDevice->SendVideoFrame((char*)pBuf, _frameInfo->length, _frameInfo->type, pLocalDevice->videoPTS);
	}
	else if (_frameType == EASY_SDK_AUDIO_FRAME_FLAG)
	{
		uint64_t pts = _frameInfo->timestamp_sec * 1000 + _frameInfo->timestamp_usec / 1000;

		//if (audioCodecID < 1)	audioCodecID = _frameInfo->codec;
	}

	return 0;
}

int CALLBACK __AudioDataCallBack(void* userptr, char* pbuf, const int bufsize)
{
	printf("%s line[%d] packet size:%d\n", __FUNCTION__, __LINE__, bufsize);

	LOCAL_RTC_DEVICE_T* pLocalDevice = (LOCAL_RTC_DEVICE_T*)userptr;
	EasyRtcDevice* pRtcDevice = (EasyRtcDevice*)&pLocalDevice->easyRtcDevice;

	if (pLocalDevice->videoPTS < 1)		return 0;

	// 时间戳计算
	pLocalDevice->audioDTS += pLocalDevice->audioDTSstep;
	pRtcDevice->SendAudioFrame(pbuf, bufsize, pLocalDevice->audioDTS);

	//EasyRtcDevice::__Print__(__FUNCTION__, __LINE__, "\tAudioDTS:%llu\n", pLocalDevice->audioDTS);

	return 0;
}

// 对端音视频及DataChannel 数据回调
bool bInitAudioPlayer = false;

int __EasyRTC_Data_Callback(void* userptr, void* webrtcPeer, EASYRTC_DATA_TYPE_ENUM_E dataType, int codecID, BOOL isBinary, PBYTE msgData, UINT32 msgLen)
{
	CEasyRTCDeviceDlg* pThis = (CEasyRTCDeviceDlg*)userptr;

	if (EASYRTC_DATA_TYPE_VIDEO == dataType)
	{
		pThis->OnRecvVideoData(codecID, (char*)msgData, (int)msgLen);
	}
	else if (EASYRTC_DATA_TYPE_AUDIO == dataType)
	{
		int idx = 0;
		char pcmBuf[1024] = { 0 };

		int samplerate = 8000;
		int bitPerSamples = 16;
		int channels = 1;

		if (codecID == 0x10007)
		{
			for (int i = 0; i < msgLen; i++)
			{
				short s = alaw2linear(msgData[i]);

				pcmBuf[idx++] = s & 0xFF;
				pcmBuf[idx++] = (s >> 8) & 0xFF;
				//short s = ((uc2 << 8) & 0xFF00) | (uc1 & 0xFF);
			}

			samplerate = 8000;
			bitPerSamples = 16;
			channels = 1;
		}

		if (!bInitAudioPlayer)
		{
			int ret = libAudioPlayer_Open(samplerate, bitPerSamples, channels);
			if (ret == 0)	bInitAudioPlayer = true;

			libAudioPlayer_Play();
		}

		if (idx > 0)
		{
#ifdef _DEBUG
			static FILE* f = fopen("1.pcm", "wb");
			if (f)
			{
				fwrite(pcmBuf, 1, idx, f);
			}

#endif


			libAudioPlayer_Write(pcmBuf, idx);
		}
	}
	if (EASYRTC_DATA_TYPE_METADATA == dataType)
	{
		pThis->AddDataChannel_Data((const char*)msgData, msgLen);
		pThis->PostMessageW(WM_EASY_RTC_CALLBACK, 1);
	}
	return 0;
}

// 日志回调
int __EasyRTC_Device_Callback(void* userptr, const char* pbuf, const int bufsize)
{
	CEasyRTCDeviceDlg* pThis = (CEasyRTCDeviceDlg*)userptr;

	pThis->PostMessageW(WM_EASY_RTC_CALLBACK, 0, (LPARAM)pbuf);

	return 0;
}

void	CEasyRTCDeviceDlg::AddDataChannel_Data(const char* pbuf, const int bufsize)
{
	LockLog();

	char* msg = new char[bufsize + 1];
	memset(msg, 0x00, bufsize + 1);
	memcpy(msg, pbuf, bufsize);
	msg[bufsize] = '\n';
	logVector.push_back(msg);

	UnlockLog();
}

LRESULT CEasyRTCDeviceDlg::OnLog(WPARAM wParam, LPARAM lParam)
{
	int type = (int)wParam;
	char* src = (char*)lParam;

	char szMsg[8192] = { 0 };
	if (type == 0)
	{
		strcpy(szMsg, src);			// 日志

		wchar_t wszMsg[1024] = { 0 };
		MByteToWChar(szMsg, wszMsg, sizeof(wszMsg) / sizeof(wszMsg[0]));
		pRichEditCtrlLog->SetSel(-1, -1);
		pRichEditCtrlLog->ReplaceSel(wszMsg);
	}
	else
	{
		// DataChannel传过来的数据
		LockLog();

		LOG_VECTOR::iterator it = logVector.begin();
		while (it != logVector.end())
		{
			char* src = (char*)*it;
			memset(szMsg, 0x00, sizeof(szMsg));
			libCharacterTranscoding_UTF8ToGB2312(szMsg, sizeof(szMsg), src, (int)strlen(src));

			wchar_t wszMsg[8192] = { 0 };
			MByteToWChar(szMsg, wszMsg, sizeof(wszMsg) / sizeof(wszMsg[0]));
			pRichEditCtrlLog->SetSel(-1, -1);
			pRichEditCtrlLog->ReplaceSel(wszMsg);

			logVector.erase(it);
			delete[]src;

			it = logVector.begin();
		}

		UnlockLog();
	}

	return 0;
}

void CEasyRTCDeviceDlg::OnBnClickedButtonStart()
{
	if (NULL == mLocalRTCDevice.easyStreamHandle)
	{
		char uuid[64] = { 0 };

		int streamNum = 0;

		mLocalRTCDevice.videoPTS = 0;
		mLocalRTCDevice.audioDTS = 0;
		mLocalRTCDevice.videoLastPts = 0;

		if (pComboxVideoDeviceList->GetCount() > 0)
		{
			char selStr[128] = { 0 };
			wchar_t wszSelStr[128] = { 0 };
			pComboxVideoDeviceList->GetWindowText(wszSelStr, sizeof(wszSelStr) / sizeof(wchar_t));
			WCharToMByte(wszSelStr, selStr, sizeof(selStr) / sizeof(selStr[0]));

			char url[MAX_PATH] = { 0 };
			sprintf(url, "video=%s", selStr);

			EasyStreamClient_Init(&mLocalRTCDevice.easyStreamHandle, 0);
			if (NULL != mLocalRTCDevice.easyStreamHandle)
			{
				EasyStreamClient_SetCallback(mLocalRTCDevice.easyStreamHandle, __EasyStreamClientCallBack);

				EasyStreamClient_OpenStream(mLocalRTCDevice.easyStreamHandle, url, EASY_RTP_OVER_TCP, (void*)&mLocalRTCDevice, 1000, 20, 1);

				unsigned long long ullAddr = (unsigned long long)&mLocalRTCDevice;
				unsigned long long ull1 = (ullAddr >> 24) & 0xFF;
				unsigned int ui1 = ullAddr & 0xFFFFFFFF;
				ullAddr = (ull1 << 32) | ui1;

				sprintf(uuid, "65617379-7274-6364-bac3-%012llu", ullAddr);

				streamNum++;
			}
		}

		if (pComboxVideoDeviceList->GetCount() > 0)
		{
			int audioDeviceIndex = pComboxVideoDeviceList->GetCurSel();
			int openDeviceRet = libAudioCapturer_OpenAudioCaptureDevice(audioDeviceIndex);
			if (openDeviceRet == 0)
			{
				//audioCodecID = 0x10006;	// mulaw
				audioCodecID = 0x10007;		// alaw

				int samplerate = 8000;			// 采样率
				int bitPerSamples = 16;			// 采样精度
				int channels = 1;				// 通道数
				int pcm_buf_size_per_sec = samplerate * bitPerSamples * channels / 8;			// 每秒数据量		比如8000*16*1/8=16000
				int pcm_buf_size_per_ms = pcm_buf_size_per_sec / 1000;							// 每毫秒数据量		16000/1000=16
				int interval_ms = 20;															// 间隔20毫秒
				int bytes_per_20ms = pcm_buf_size_per_ms * interval_ms;							// 每20毫秒数据量

				unsigned int audioCodec = AUDIO_CODEC_ID_ALAW;
				if (audioCodecID == 0x10006)	audioCodec = AUDIO_CODEC_ID_MULAW;

				mLocalRTCDevice.audioDTSstep = interval_ms;

				libAudioCapturer_StartAudioCapture(audioCodec, bytes_per_20ms,
					samplerate, bitPerSamples, channels, __AudioDataCallBack, (void*)&mLocalRTCDevice);

				streamNum++;
			}
		}

		if (streamNum == 2)
		{
			// 等待获取音视频编码格式
			for (int i = 0; i < 10; i++)
			{
				if (videoCodecID > 0 && audioCodecID > 0)		break;

				Sleep(1000);
			}

			mLocalRTCDevice.easyRtcDevice.Start(videoCodecID, audioCodecID, uuid, false, __EasyRTC_Data_Callback, this);
			mLocalRTCDevice.easyRtcDevice.SetCallback(__EasyRTC_Device_Callback, this);
		}
		else
		{
			OnBnClickedButtonStop();

			MessageBox(TEXT("没有找到视频或音频采集设备"));
		}
	}
}


void CEasyRTCDeviceDlg::OnBnClickedButtonStop()
{
	EasyStreamClient_Deinit(mLocalRTCDevice.easyStreamHandle);
	mLocalRTCDevice.easyStreamHandle = NULL;

	mLocalRTCDevice.easyRtcDevice.Stop();

	libAudioPlayer_Stop();
	libAudioPlayer_Close();
	bInitAudioPlayer = false;
	TransCode_Release(&transCodeHandle);
	D3D_Release(&d3dHandle);

	OnPeerVideoFrame(0, 1);

	if (NULL != pDlgVideoRender)
	{
		pDlgVideoRender->ShowWindow(SW_HIDE);
	}

	libAudioCapturer_StopAudioCapture();
	libAudioCapturer_CloseAudioCaptureDevice();
}


void CEasyRTCDeviceDlg::OnBnClickedButtonSend()
{
	wchar_t wszMsg[1024] = { 0 };
	pRichEditCtrlSendMsg->GetWindowText(wszMsg, sizeof(wszMsg) / sizeof(wchar_t));

	char szMsg[1024] = { 0 };
	WCharToMByte(wszMsg, szMsg, sizeof(szMsg) / sizeof(szMsg[0]));

	char utf8Str[1024] = { 0 };
	int msgLen = libCharacterTranscoding_GB2312ToUTF8(utf8Str, sizeof(utf8Str), szMsg, (int)strlen(szMsg));
	
	mLocalRTCDevice.easyRtcDevice.SendData(NULL, FALSE, (PBYTE)utf8Str, msgLen);
}


LRESULT CEasyRTCDeviceDlg::OnPeerVideoFrame(WPARAM, LPARAM lParam)
{
	LockFrameVector();

	FRAME_INFO_VECTOR::iterator it = frameInfoVector.begin();
	while (it != frameInfoVector.end())
	{
		if ((int)lParam == 0)
		{
			OnProcVideoData(0, it->pbuf, it->bufsize);
		}

		it++;
	}
	frameInfoVector.clear();

	UnlockFrameVector();

	return 0;
}

void	CEasyRTCDeviceDlg::OnRecvVideoData(int codecID, char* framedata, int framesize)
{
	if (framesize < 4)		return;

	LockFrameVector();
	FRAME_INFO_T	frameInfo;
	frameInfo.bufsize = framesize;
	frameInfo.pbuf = new char[frameInfo.bufsize];
	memcpy(frameInfo.pbuf, framedata, framesize);

	frameInfoVector.push_back(frameInfo);

	UnlockFrameVector();

	PostMessage(WM_EASY_RTC_VIDEO);
}

void	CEasyRTCDeviceDlg::OnProcVideoData(int codecID, char* framedata, int framesize)
{
	if (NULL == transCodeHandle)
	{
		TransCode_Create(&transCodeHandle);

		int pixelFormat = 0;		//YUV420
		int transcodeCodecId = 0;// TRANSCODE_VIDEO_CODEC_ID_H264;	//TRANSCODE_VIDEO_CODEC_ID_MJPEG

		int HWDecoderId = -1;// 7;
		TransCode_Init(transCodeHandle, 0, HWDecoderId, 0, 0, pixelFormat, transcodeCodecId, -1, 25, 25, 1024 * 1024);
	}

	TRANSCODE_DECODE_CALLBACK decodeCB = NULL;
	TRANSCODE_ENCODE_CALLBACK encodeCB = NULL;
	int width = 0, height = 0;
	char* cbFrameData = NULL;
	int outFrameSize = 0;
	int keyFrame = 0;
	int ret = TransCode_TransCodeVideo(transCodeHandle, framedata, framesize, &width, &height, &cbFrameData, &outFrameSize, &keyFrame,
										decodeCB, NULL,
										encodeCB, NULL, 0, true);

	if (ret != 0)							return;
	if (width < 1 || height < 1)			return;
	if (outFrameSize < width * height)		return;

	if (NULL == d3dHandle)
	{
		if (NULL != pDlgVideoRender)
		{
			pDlgVideoRender->ShowWindow(SW_SHOW);
			pDlgVideoRender->MoveWindow(0, 0, width, height);
			pDlgVideoRender->SetForegroundWindow();
		}

		if (NULL == renderHwnd)	
			renderHwnd = m_hWnd;		// 如果没有创建子窗口,则使用当前窗口句柄

		D3D_Initial(&d3dHandle, renderHwnd, width, height, 0, 1, D3D_FORMAT_YV12);
	}

	RECT rcDst;
	GetClientRect(&rcDst);
	if (NULL != pDlgVideoRender)
	{
		pDlgVideoRender->GetClientRect(&rcDst);
	}

	RECT rcSrc;
	SetRect(&rcSrc, 0, 0, width, height);

	D3D_UpdateData(d3dHandle, 0, (unsigned char*)cbFrameData,
		width, height, &rcSrc, NULL);

	ret = D3D_Render(d3dHandle, renderHwnd, 0, &rcDst);
}