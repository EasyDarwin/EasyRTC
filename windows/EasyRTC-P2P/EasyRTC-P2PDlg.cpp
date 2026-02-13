
// EasyRTCDevice_WinDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "EasyRTC-P2P.h"
#include "EasyRTC-P2PDlg.h"
#include "afxdialogex.h"

#include "CreateDump.h"
#include "CharacterTranscoding.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


#define WM_EASY_RTC_CALLBACK_LOG		WM_USER+2001
#define WM_EASY_RTC_CALLBACK_STATUS	WM_USER+2002
#define WM_EASY_RTC_VIDEO		WM_USER+2003
#define WM_EASY_RTC_CALLBACK_ONLINE_DEVICE	WM_USER+2004
#define WM_EASY_RTC_UPDATE_WINDOW	WM_USER+2005
#define WM_EASY_RTC_REFRESH_DEVICE_LIST	WM_USER+2006
#define WM_EASY_RTC_SDP		WM_USER+2007
#define WM_EASY_RTC_CALL_END	WM_USER+2008

#define REFRESH_DEVICE_TIMER_ID	10000

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


// CEasyRTCDeviceWinDlg 对话框

/****************************************************
 * 函数: getBuildTime
 * 功能: 获取软件编译时间和日期
 ***************************************************/
void EasyRTCDevice_getBuildTime(int* _year, int* _month, int* _day, int* _hour, int* _minute, int* _second)
{
	char monthStr[12][5] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	char buf[16] = { 0 };
	char monthBuf[4] = { 0 };
	unsigned int i = 0;

	int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;

	strcpy(buf, __DATE__);
	memset(monthBuf, 0, 4);

	strncpy(monthBuf, buf, 3);
	day = buf[5] - '0';
	if (buf[4] != ' ')
	{
		day += (buf[4] - '0') * 10;
	}
	year = (buf[7] - '0') * 1000 + (buf[8] - '0') * 100 + (buf[9] - '0') * 10 + buf[10] - '0';
	strcpy(buf, __TIME__);
	hour = (buf[0] - '0') * 10 + buf[1] - '0';
	minute = (buf[3] - '0') * 10 + buf[4] - '0';
	second = (buf[6] - '0') * 10 + buf[7] - '0';
	for (i = 0; i < (sizeof(monthStr) / sizeof(monthStr[0])); i++)
	{
		if (0 == strcmp(monthStr[i], monthBuf))
		{
			month = i + 1;
			break;
		}
	}
	if (i >= 12)
	{
		month = 1;
	}

	if (_year)	*_year = year;
	if (_month)	*_month = month;
	if (_day)	*_day = day;
	if (_hour)	*_hour = hour;
	if (_minute)	*_minute = minute;
	if (_second)	*_second = second;
}



CEasyRTCDeviceWinDlg::CEasyRTCDeviceWinDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_EASYRTCDEVICE_WIN_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	InitComponents();
}

void CEasyRTCDeviceWinDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_DEVICE_LIST, mDeviceListCtrl);
}

BEGIN_MESSAGE_MAP(CEasyRTCDeviceWinDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_DESTROY()
	ON_CBN_SELCHANGE(IDC_COMBO_SOURCE_TYPE, &CEasyRTCDeviceWinDlg::OnCbnSelchangeComboSourceType)
	ON_BN_CLICKED(IDC_BUTTON_START_STOP, &CEasyRTCDeviceWinDlg::OnBnClickedButtonStartStop)
	ON_MESSAGE(WM_EASY_RTC_CALLBACK_LOG, OnLog)
	ON_MESSAGE(WM_EASY_RTC_CALLBACK_STATUS, OnStatus)
	ON_MESSAGE(WM_EASY_RTC_CALLBACK_ONLINE_DEVICE, OnOnlineDevice)
	ON_MESSAGE(WM_CLICK_BUTTON, OnClickCallButton)
	ON_MESSAGE(WM_EASY_RTC_UPDATE_WINDOW, OnUpdateVideoWindow)
	ON_MESSAGE(WM_EASY_RTC_REFRESH_DEVICE_LIST, OnRefreshDeviceList)
	ON_MESSAGE(WM_EASY_RTC_CALL_END, OnCallEnd)
	
	ON_BN_CLICKED(IDC_BUTTON_PREVIEW, &CEasyRTCDeviceWinDlg::OnBnClickedButtonPreview)
	ON_BN_CLICKED(IDC_BUTTON_REFRESH_DEVICELIST, &CEasyRTCDeviceWinDlg::OnBnClickedButtonRefreshDevicelist)
	ON_BN_CLICKED(IDC_BUTTON_SEND_MESSAGE, &CEasyRTCDeviceWinDlg::OnBnClickedButtonSendMessage)
	ON_BN_CLICKED(IDC_BUTTON_CALL_END, &CEasyRTCDeviceWinDlg::OnBnClickedButtonCallEnd)
	ON_WM_TIMER()
END_MESSAGE_MAP()


// CEasyRTCDeviceWinDlg 消息处理程序

#ifdef _WIN32
LONG CrashHandler_EasyRTCDevice_Win(EXCEPTION_POINTERS* pException)
{
	SYSTEMTIME	systemTime;
	GetLocalTime(&systemTime);

	wchar_t szFile[MAX_PATH] = { 0 };
	wsprintf(szFile, L"%s %04d%02d%02d %02d%02d%02d.dmp", _T("EasyRTC-P2P"),
		systemTime.wYear, systemTime.wMonth, systemTime.wDay, systemTime.wHour, systemTime.wMinute, systemTime.wSecond);
	CreateDumpFile(szFile, pException);

	return EXCEPTION_EXECUTE_HANDLER;		//返回值EXCEPTION_EXECUTE_HANDLER	EXCEPTION_CONTINUE_SEARCH	EXCEPTION_CONTINUE_EXECUTION
}
#endif

BOOL CEasyRTCDeviceWinDlg::OnInitDialog()
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

	SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)CrashHandler_EasyRTCDevice_Win);		//异常捕获

	CreateComponents();

	char version[128] = { 0 };
	//EasyRTC_Device_GetVersion(version);

	int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
	EasyRTCDevice_getBuildTime(&year, &month, &day, &hour, &minute, &second);

	sprintf(version, "EasyRTC-P2P v1.0.%d.%02d%02d", year - 2000, month, day);

	wchar_t wszTitle[128] = { 0 };
	MByteToWChar(version, wszTitle, sizeof(wszTitle) / sizeof(wszTitle[0]));

	SetWindowText(wszTitle);
	//MoveWindow(0, 0, 1280, 720);
	MoveWindow(0, 0, 1366, 720);
#ifndef _DEBUG
	ShowWindow(SW_MAXIMIZE);
#endif

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CEasyRTCDeviceWinDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

void CEasyRTCDeviceWinDlg::OnPaint()
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
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CEasyRTCDeviceWinDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CEasyRTCDeviceWinDlg::OnDestroy()
{
	DeleteComponents();

	CDialogEx::OnDestroy();
}

LRESULT CEasyRTCDeviceWinDlg::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (WM_PAINT == message || WM_SIZE == message)
	{
		UpdateComponents();
	}

	return CDialogEx::WindowProc(message, wParam, lParam);
}

LRESULT CEasyRTCDeviceWinDlg::OnLog(WPARAM wParam, LPARAM lParam)
{
	int type = (int)wParam;
	char* src = (char*)lParam;

	
	if (type == 0)
	{
		char szMsg[32] = { 0 };
		time_t tt = time(NULL);
		struct tm* _timetmp = NULL;
		_timetmp = localtime(&tt);
		if (NULL != _timetmp)   strftime(szMsg, 32, "[%Y-%m-%d %H:%M:%S] ", _timetmp);

		wchar_t wszMsg[1024] = { 0 };
		MByteToWChar(szMsg, wszMsg, sizeof(wszMsg) / sizeof(wszMsg[0]));
		pRedtLog->SetSel(-1, -1);
		pRedtLog->ReplaceSel(wszMsg);

		memset(wszMsg, 0x00, sizeof(wszMsg));
		MByteToWChar(src, wszMsg, sizeof(wszMsg) / sizeof(wszMsg[0]));
		pRedtLog->SetSel(-1, -1);
		pRedtLog->ReplaceSel(wszMsg);
	}
	else
	{
		char szMsg[8192] = { 0 };
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
			pRedtLog->SetSel(-1, -1);
			pRedtLog->ReplaceSel(wszMsg);

			logVector.erase(it);
			delete[]src;

			it = logVector.begin();
		}

		UnlockLog();
	}

	return 0;
}

LRESULT CEasyRTCDeviceWinDlg::OnStatus(WPARAM, LPARAM lParam)
{
	char* src = (char*)lParam;

	char szMsg[128] = { 0 };
	sprintf(szMsg, "当前接入状态: %s", src);
	wchar_t wszStatus[128] = { 0 };
	MByteToWChar(szMsg, wszStatus, sizeof(wszStatus) / sizeof(wszStatus[0]));
	pStaticStatus->SetWindowTextW(wszStatus);

	return 0;
}

void	CEasyRTCDeviceWinDlg::InitComponents()
{
	//pBtnSearch = NULL;
	//pBtnIPConfig = NULL;
	//pListDevice = NULL;

	__SetNull(pGrpRemote);
	__SetNull(pGrpLocal);
	__SetNull(pGrpLogMessage);

	__SetNull(pDlgVideoRemote);
	__SetNull(pDlgVideoLocal);
	__SetNull(pStaticStatus);
	__SetNull(pStaticServerIP);
	__SetNull(pStaticServerPort);
	__SetNull(pChkSSL);
	__SetNull(pStaticDeviceID);
	__SetNull(pGrpDeviceList);
	
	__SetNull(pEdtServerIP);
	__SetNull(pEdtServerPort);
	__SetNull(pEdtDeviceID);
	__SetNull(pBtnStartStop);
	__SetNull(pListCtrlDevice);
	__SetNull(pBtnRefresh);


	__SetNull(pEdtSendText);
	__SetNull(pBtnSendMessage);
	__SetNull(pBtnCallEnd);
	__SetNull(pComboxSourceType);
	__SetNull(pComboxLocalCameraList);
	__SetNull(pComboxLocalAudioList);
	__SetNull(pBtnPreview);
	__SetNull(pEdtStreamURL);
	__SetNull(pRedtLog);


	localFrameInfoVector.clear();
	remoteFrameInfoVector.clear();


	InitMutex(&mutexLog);
	InitMutex(&mutexLocalFrame);
	InitMutex(&mutexDecodeAndRender);

	InitMutex(&mutexRemoteFrame);
	InitMutex(&mutexRemoteDecodeAndRender);
	InitMutex(&mutexAudioFrameQueue);

	memset(mPeerUUID, 0x00, sizeof(mPeerUUID));
	memset(peerCustomMessage, 0x00, sizeof(peerCustomMessage));
}
void	CEasyRTCDeviceWinDlg::CreateComponents()
{
	//__CREATE_WINDOW(pBtnSearch, CButton, IDC_BUTTON_SEARCH);
	//__CREATE_WINDOW(pBtnIPConfig, CButton, IDC_BUTTON_IP_CONFIG);

	//__CREATE_WINDOW(pListDevice, CListCtrl, IDC_LIST_DEVICE);

	__CREATE_WINDOW(pGrpRemote, CStatic, IDC_STATIC_REMOTE);
	__CREATE_WINDOW(pGrpLocal, CStatic, IDC_STATIC_LOCAL);
	__CREATE_WINDOW(pGrpLogMessage, CStatic, IDC_STATIC_LOG_MESSAGE);

	if (pGrpRemote)	pGrpRemote->SetWindowTextW(_T("对端"));
	if (pGrpLocal)	pGrpLocal->SetWindowTextW(_T("本地"));
	if (pGrpLogMessage)	pGrpLogMessage->SetWindowTextW(_T("日志&&消息"));


	if (NULL == pDlgVideoLocal)
	{
		pDlgVideoLocal = new CDlgVideo();
		pDlgVideoLocal->Create(IDD_DIALOG_VIDEO, this);
		pDlgVideoLocal->ShowWindow(SW_SHOW);
	}
	if (NULL == pDlgVideoRemote)
	{
		pDlgVideoRemote = new CDlgVideo();
		pDlgVideoRemote->Create(IDD_DIALOG_VIDEO, this);
		pDlgVideoRemote->ShowWindow(SW_SHOW);
	}



	__CREATE_WINDOW(pStaticStatus, CStatic, IDC_STATIC_STATUS);
	__CREATE_WINDOW(pStaticServerIP, CStatic, IDC_STATIC_SERVER_IP);
	__CREATE_WINDOW(pStaticServerPort, CStatic, IDC_STATIC_SERVER_PORT);
	__CREATE_WINDOW(pStaticDeviceID, CStatic, IDC_STATIC_DEVICE_ID);
	__CREATE_WINDOW(pGrpDeviceList, CStatic, IDC_STATIC_DEVICE_LIST);

	__CREATE_WINDOW(pEdtServerIP, CEdit, IDC_EDIT_SERVER_IP);
	__CREATE_WINDOW(pEdtServerPort, CEdit, IDC_EDIT_SERVER_PORT);
	__CREATE_WINDOW(pChkSSL, CButton, IDC_CHECK_IS_SECURT);
	
	__CREATE_WINDOW(pEdtDeviceID, CEdit, IDC_EDIT_DEVICE_ID);
	__CREATE_WINDOW(pBtnStartStop, CButton, IDC_BUTTON_START_STOP);
	__CREATE_WINDOW(pListCtrlDevice, CButtonListCtrl, IDC_LIST_DEVICE_LIST);
	__CREATE_WINDOW(pBtnRefresh, CButton, IDC_BUTTON_REFRESH_DEVICELIST);


	__CREATE_WINDOW(pEdtSendText, CEdit, IDC_EDIT_SEND_TEXT);
	__CREATE_WINDOW(pBtnSendMessage, CButton, IDC_BUTTON_SEND_MESSAGE);
	__CREATE_WINDOW(pBtnCallEnd, CButton, IDC_BUTTON_CALL_END);

	__CREATE_WINDOW(pComboxSourceType, CComboBox, IDC_COMBO_SOURCE_TYPE);
	__CREATE_WINDOW(pComboxLocalCameraList, CComboBox, IDC_COMBO_LOCAL_CAMERA_LIST);
	__CREATE_WINDOW(pComboxLocalAudioList, CComboBox, IDC_COMBO_LOCAL_AUDIO_LIST);
	__CREATE_WINDOW(pEdtStreamURL, CEdit, IDC_EDIT_STREAM_URL);
	__CREATE_WINDOW(pBtnPreview, CButton, IDC_BUTTON_PREVIEW);
	__CREATE_WINDOW(pRedtLog, CRichEditCtrl, IDC_RICHEDIT2_LOG);

	PostMessageW(WM_EASY_RTC_CALLBACK_STATUS, 0, (LPARAM)"未接入");

	if (pBtnSendMessage)	pBtnSendMessage->SetWindowTextW(_T("发送"));
	if (pBtnCallEnd)	pBtnCallEnd->SetWindowTextW(_T("挂断"));
	if (pChkSSL)
	{
		pChkSSL->SetWindowTextW(_T("SSL"));
		pChkSSL->ShowWindow(SW_HIDE);			// 暂未实现wss, 所以此处隐藏
	}



#ifdef _DEBUG
	SetEditText(pEdtServerIP, "rts.easyrtc.cn");
	SetEditText(pEdtServerPort, "19000");
	SetEditText(pEdtDeviceID, "92092eea-be8d-4ec4-8ac5-987654321001");
	if (pEdtStreamURL)	pEdtStreamURL->SetWindowTextW(TEXT("rtsp://admin:admin12345@192.168.6.19"));
#else
	XmlConfig	xmlConfig;
	XML_CONFIG_T	config;
	memset(&config, 0x00, sizeof(XML_CONFIG_T));
	xmlConfig.LoadConfig(XML_CONFIG_FILENAME, &config);
	SetEditText(pEdtServerIP, config.serverIP);
	SetEditText(pEdtServerPort, config.serverPort);
	SetEditText(pEdtDeviceID, config.deviceID);
	SetEditText(pEdtStreamURL, config.sourceURL);
#endif
	pComboxSourceType->AddString(TEXT("本机设备"));
	pComboxSourceType->AddString(TEXT("网络串流"));
	pComboxSourceType->SetCurSel(0);
	ChangeSourceType();


	if (pStaticStatus)	pStaticStatus->ShowWindow(SW_HIDE);


	if (pBtnStartStop)	pBtnStartStop->SetWindowTextW(TEXT("接入"));
	if (pBtnPreview)	pBtnPreview->SetWindowTextW(TEXT("开启本地预览"));

	if (pGrpDeviceList)	pGrpDeviceList->SetWindowTextW(_T("设备列表"));
	if (pListCtrlDevice)
	{
		CRect rect;
		SetRect(&rect, 0, 0, 100, 100);

		mDeviceListCtrl.InsertColumn(0, _T("设备ID"), LVCFMT_CENTER, 100);
		mDeviceListCtrl.InsertColumn(1, _T("操作"), LVCFMT_CENTER, 120);

		mDeviceListCtrl.ModifyStyle(0, LVS_OWNERDRAWFIXED | LVS_REPORT | WS_TABSTOP);
		mDeviceListCtrl.SetRowHeight(80);
		mDeviceListCtrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
	}
	if (pBtnRefresh)	pBtnRefresh->SetWindowTextW(_T("刷新设备列表"));


	wchar_t wszName[64] = { 0 };
	LOCAL_DEVICE_MAP::iterator itVideoDevice = LocalVideoDeviceMap.begin();
	while (itVideoDevice != LocalVideoDeviceMap.end())
	{
		memset(wszName, 0x00, sizeof(wszName));

		wchar_t wszCamera[64] = { 0 };
		MByteToWChar(itVideoDevice->first.data(), wszCamera, sizeof(wszCamera) / sizeof(wszCamera[0]));

		pComboxLocalCameraList->AddString(wszCamera);
		itVideoDevice++;
	}
	if (pComboxLocalCameraList->GetCount() == 1)	pComboxLocalCameraList->SetCurSel(0);
	//if (pComboxLocalCameraList->GetCount() > 0)	pComboxLocalCameraList->SetCurSel(0);

	memset(mLastDeviceID, 0x00, sizeof(mLastDeviceID));
	mLocalRTCDevice.pThis = this;


	EnableTextMessage(FALSE);
	EnableCallEnd(FALSE);

	#ifndef _DEBUG
//#if 1
	libAudioCapturer_Init();
	AUDIO_CAPTURER_DEVICE_MAP	audioCapturerDeviceMap;
	libAudioCapturer_GetAudioCaptureDeviceList(&audioCapturerDeviceMap);
	AUDIO_CAPTURER_DEVICE_MAP::iterator itAudioDevice = audioCapturerDeviceMap.begin();
	while (itAudioDevice != audioCapturerDeviceMap.end())
	{
		memset(wszName, 0x00, sizeof(wszName));
		MByteToWChar(itAudioDevice->second.description, wszName, sizeof(wszName) / sizeof(wszName[0]));
		pComboxLocalAudioList->AddString(wszName);
		itAudioDevice++;
	}
	if (pComboxLocalAudioList->GetCount() == 1)	pComboxLocalAudioList->SetCurSel(0);
	//if (pComboxLocalAudioList->GetCount() > 0)	pComboxLocalAudioList->SetCurSel(0);
#endif
}


void	CEasyRTCDeviceWinDlg::UpdateComponents()
{
	CRect	rcClient;
	GetClientRect(&rcClient);
	if (rcClient.IsRectEmpty())		return;

	int title_height = 20;
	int button_width = 100;
	int button_height = 50;
	int padding = 2;


	int label_width = 80;
	int label_height = 30;
	CRect	rcLabel;
	rcLabel.SetRect(rcClient.left + padding, rcClient.top + padding * 5, rcClient.left + padding + label_width, rcClient.top + padding * 5 + label_height);
	__MOVE_WINDOW(pStaticServerIP, rcLabel);

	CRect rcEdt;
	rcEdt.SetRect(rcLabel.right + padding, rcLabel.top, rcLabel.right + padding + 300, rcLabel.bottom);
	__MOVE_WINDOW(pEdtServerIP, rcEdt);

	CRect rcServerPort;
	rcServerPort.SetRect(rcEdt.right + padding, rcLabel.top, rcEdt.right + padding + label_width, rcLabel.bottom);
	__MOVE_WINDOW(pStaticServerPort, rcServerPort);
	rcEdt.SetRect(rcServerPort.right + padding, rcEdt.top, rcServerPort.right + padding + 80, rcEdt.bottom);
	__MOVE_WINDOW(pEdtServerPort, rcEdt);

	CRect rcSSL;
	rcSSL.SetRect(rcEdt.right + padding * 2, rcEdt.top, rcEdt.right + padding * 2 + 100, rcEdt.bottom);
	__MOVE_WINDOW(pChkSSL, rcSSL);

	CRect rcBtnStart;
	int BUTTON_WIDTH = 130;
	rcBtnStart.SetRect(rcClient.right- BUTTON_WIDTH*2 - padding, rcLabel.top, rcClient.right-padding, rcLabel.bottom+50);
	
	int device_list_width = (int)((float)rcClient.Width() * 0.3f);

	//rcLabel.SetRect(rcEdt.right + padding, rcLabel.top, rcEdt.right + padding+rcLabel.Width(), rcLabel.bottom);
	rcLabel.SetRect(rcLabel.left, rcLabel.bottom+padding*2, rcLabel.right, rcLabel.bottom+2+padding*2+ rcLabel.Height());
	__MOVE_WINDOW(pStaticDeviceID, rcLabel);
	rcEdt.SetRect(rcLabel.right + padding, rcLabel.top, rcBtnStart.right - padding * 5 - device_list_width, rcLabel.bottom);
	__MOVE_WINDOW(pEdtDeviceID, rcEdt);

	rcBtnStart.SetRect(rcEdt.right + padding * 5, rcBtnStart.top, rcClient.right - padding * 2, rcEdt.bottom);
	__MOVE_WINDOW(pBtnStartStop, rcBtnStart);

	

	int videoWindowWidth = (rcClient.Width() - padding * 2 - device_list_width) / 2 - padding;
	int videoWindowHeight = (int)((float)rcClient.Height() * 0.6f);

	int nTop = rcLabel.bottom + padding * 4+10;

	// 对端视频窗口
	CRect rcGrpRemote;
	rcGrpRemote.SetRect(rcClient.left + padding, nTop, rcClient.left + videoWindowWidth - padding * 2, nTop + videoWindowHeight);
	__MOVE_WINDOW(pGrpRemote, rcGrpRemote);

	//CStatic* pGrpRemote;			// IDC_STATIC_REMOTE
	//CStatic* pGrpLocal;				// IDC_STATIC_LOCAL
	//CStatic* pGrpLogMessage;		// IDC_STATIC_LOG_MESSAGE

	CRect	rcRemoteVideo;
	rcRemoteVideo.SetRect(rcClient.left + padding, rcLabel.bottom + padding * 4, rcClient.left + videoWindowWidth, rcLabel.bottom + padding * 4 + videoWindowHeight);
	rcRemoteVideo.SetRect(rcGrpRemote.left + padding * 2, rcGrpRemote.top + padding * 10, rcGrpRemote.right - padding * 2, rcGrpRemote.bottom - padding * 5 - 30);
	__MOVE_WINDOW(pDlgVideoRemote, rcRemoteVideo);


	CRect	rcCallEnd;
	rcCallEnd.SetRect(rcRemoteVideo.right - BUTTON_WIDTH - padding, rcRemoteVideo.bottom + padding * 3, rcRemoteVideo.right - padding, rcGrpRemote.bottom - padding * 2);
	__MOVE_WINDOW(pBtnCallEnd, rcCallEnd);

	CRect rcSend;
	rcSend.SetRect(rcCallEnd.left-padding-BUTTON_WIDTH, rcCallEnd.top, rcCallEnd.left-padding, rcCallEnd.bottom);
	__MOVE_WINDOW(pBtnSendMessage, rcSend);

	CRect	rcEdtText;

	rcEdtText.SetRect(rcRemoteVideo.left+padding, rcSend.top, rcSend.left-padding, rcSend.bottom);
	__MOVE_WINDOW(pEdtSendText, rcEdtText);


	// 本地视频窗口
	CRect	rcGrpLocal;
	rcGrpLocal.SetRect(rcGrpRemote.right + padding * 2, rcGrpRemote.top, rcGrpRemote.right + padding + rcGrpRemote.Width(), rcGrpRemote.bottom);
	__MOVE_WINDOW(pGrpLocal, rcGrpLocal);

	CRect	rcLocalVideo;
	rcLocalVideo.SetRect(rcRemoteVideo.right+padding, rcRemoteVideo.top, rcRemoteVideo.right+padding+rcRemoteVideo.Width(), rcRemoteVideo.bottom);
	rcLocalVideo.SetRect(rcGrpLocal.left + padding * 2, rcGrpLocal.top + padding * 10, rcGrpLocal.right - padding * 2, rcGrpLocal.bottom - padding * 5 - 30);
	__MOVE_WINDOW(pDlgVideoLocal, rcLocalVideo);


	// 设备列表
	// ==================
	CRect rcDeviceList;
	rcDeviceList.SetRect(rcGrpLocal.right + padding * 3, rcGrpRemote.top, rcClient.right - padding, rcClient.bottom - padding);
	__MOVE_WINDOW(pGrpDeviceList, rcDeviceList);
	
	rcDeviceList.SetRect(rcDeviceList.left + padding, rcDeviceList.top + 20 + padding * 2, rcDeviceList.right - padding, rcDeviceList.bottom - 60 - padding);
	__MOVE_WINDOW(pListCtrlDevice, rcDeviceList);

	if (pListCtrlDevice)
	{
		int col2Width = pListCtrlDevice->GetColumnWidth(1);
		pListCtrlDevice->SetColumnWidth(0, rcDeviceList.Width() - padding * 2 - col2Width);
		pListCtrlDevice->UpdateButtonPosition();
	}

	CRect rcRefresh;
	rcRefresh.SetRect(rcDeviceList.left + padding, rcDeviceList.bottom + padding, rcDeviceList.right - padding, rcClient.bottom - padding);
	__MOVE_WINDOW(pBtnRefresh, rcRefresh);

	// =================
	
	CRect rcSourceType;
	rcSourceType.SetRect(rcLocalVideo.left + padding*3, rcLocalVideo.bottom + padding * 2, rcLocalVideo.left + padding * 3 + 110, rcLocalVideo.bottom + padding * 2 + 100);
	__MOVE_WINDOW(pComboxSourceType, rcSourceType);

	int local_camera_audio_width = (rcClient.Width() - device_list_width - rcSourceType.right - BUTTON_WIDTH - 20) / 2;
	CRect rcLocalCamera;
	rcLocalCamera.SetRect(rcSourceType.right + padding * 2, rcSourceType.top, rcSourceType.right + padding * 2 + local_camera_audio_width, rcSourceType.bottom);
	__MOVE_WINDOW(pComboxLocalCameraList, rcLocalCamera);
	CRect rcLocalAudio;
	rcLocalAudio.SetRect(rcLocalCamera.right + padding * 2, rcLocalCamera.top, rcClient.Width() - device_list_width - padding - BUTTON_WIDTH - 20, rcLocalCamera.bottom);
	__MOVE_WINDOW(pComboxLocalAudioList, rcLocalAudio);
	CRect rcStreamURL;
	rcStreamURL.SetRect(rcLocalCamera.left, rcLocalCamera.top, rcLocalAudio.right, rcLocalCamera.top + 30);
	__MOVE_WINDOW(pEdtStreamURL, rcStreamURL);
	CRect rcBtnPreview;
	rcBtnPreview.SetRect(rcStreamURL.right + padding * 2, rcStreamURL.top, rcLocalVideo.right - padding, rcStreamURL.bottom);
	__MOVE_WINDOW(pBtnPreview, rcBtnPreview);

	// 底部日志
	CRect rcGrpLog;
	rcGrpLog.SetRect(rcGrpRemote.left, rcGrpRemote.bottom + padding * 3, rcBtnPreview.right, rcClient.bottom - padding * 2);
	__MOVE_WINDOW(pGrpLogMessage, rcGrpLog);

	CRect rcLog;
	rcLog.SetRect(rcGrpLog.left + padding * 3, rcGrpLog.top + padding * 12, rcGrpLog.right - padding * 3, rcGrpLog.bottom - padding);
	__MOVE_WINDOW(pRedtLog, rcLog);

	//// 右上
	//int label_height = 30;
	//CRect	rcStatus;
	//rcStatus.SetRect(rcLocalVideo.right + padding, rcLocalVideo.top + 10, rcClient.right - padding, rcLocalVideo.top + 10 + label_height);
	//__MOVE_WINDOW(pStaticStatus, rcStatus);


	//int label_width = (int)((float)(rcClient.Width() - localVideoWidth) * 0.3f);
	//CRect	rcLabel;
	//rcLabel.SetRect(rcLocalVideo.right + padding, rcStatus.bottom + padding * 5, rcLocalVideo.right + padding + label_width, rcStatus.bottom + padding * 5 + label_height);
	//__MOVE_WINDOW(pStaticServerIP, rcLabel);

	//CRect rcEdt;
	//rcEdt.SetRect(rcLabel.right + padding, rcLabel.top, rcClient.right - padding * 2, rcLabel.bottom);
	//__MOVE_WINDOW(pEdtServerIP, rcEdt);

	//rcLabel.SetRect(rcLabel.left, rcLabel.bottom + padding * 2, rcLabel.right, rcLabel.bottom + padding * 2 + rcLabel.Height());
	//__MOVE_WINDOW(pStaticServerPort, rcLabel);
	//rcEdt.SetRect(rcEdt.left, rcLabel.top, rcEdt.right, rcLabel.bottom);
	//__MOVE_WINDOW(pEdtServerPort, rcEdt);

	//rcLabel.SetRect(rcLabel.left, rcLabel.bottom + padding * 2, rcLabel.right, rcLabel.bottom + padding * 2 + rcLabel.Height());
	//__MOVE_WINDOW(pStaticDeviceID, rcLabel);
	//rcEdt.SetRect(rcEdt.left, rcLabel.top, rcEdt.right, rcLabel.bottom);
	//__MOVE_WINDOW(pEdtDeviceID, rcEdt);

	//rcLabel.SetRect(rcLabel.left, rcLabel.bottom + padding * 2, rcLabel.right, rcLabel.bottom + padding * 2 + rcLabel.Height());
	//__MOVE_WINDOW(pStaticAccessPwd, rcLabel);
	//rcEdt.SetRect(rcEdt.left, rcLabel.top, rcEdt.right, rcLabel.bottom);
	//__MOVE_WINDOW(pEdtAccessPwd, rcEdt);

	//CRect rcBtnStartStop;
	//rcBtnStartStop.SetRect(rcLabel.left, rcLabel.bottom + padding * 3, rcEdt.right, rcLabel.bottom + padding * 3 + 40);
	//__MOVE_WINDOW(pBtnStartStop, rcBtnStartStop);



}
void	CEasyRTCDeviceWinDlg::DeleteComponents()
{
	CloseAll();

	if (NULL != pDlgVideoLocal)
	{
		pDlgVideoLocal->DestroyWindow();
		delete pDlgVideoLocal;
		pDlgVideoLocal = NULL;
	}

	if (NULL != pDlgVideoRemote)
	{
		pDlgVideoRemote->DestroyWindow();
		delete pDlgVideoRemote;
		pDlgVideoRemote = NULL;
	}

	libAudioCapturer_Deinit();
	libAudioPlayer_Stop();
	libAudioPlayer_Close();

	DeinitMutex(&mutexLog);
	DeinitMutex(&mutexLocalFrame);
	DeinitMutex(&mutexDecodeAndRender);

	DeinitMutex(&mutexRemoteFrame);
	DeinitMutex(&mutexRemoteDecodeAndRender);
	DeinitMutex(&mutexAudioFrameQueue);
}


void	CEasyRTCDeviceWinDlg::ChangeSourceType()
{
	bool isLocalVideo = false;

	if (pComboxSourceType)	isLocalVideo = pComboxSourceType->GetCurSel() == 0 ? true : false;

	if (pComboxLocalCameraList)	pComboxLocalCameraList->ShowWindow(isLocalVideo ? SW_SHOW : SW_HIDE);
	if (pComboxLocalAudioList)	pComboxLocalAudioList->ShowWindow(isLocalVideo ? SW_SHOW : SW_HIDE);

	if (NULL != pEdtStreamURL)	pEdtStreamURL->ShowWindow(isLocalVideo ? SW_HIDE : SW_SHOW);

}

void CEasyRTCDeviceWinDlg::OnCbnSelchangeComboSourceType()
{
	ChangeSourceType();
}

void	CEasyRTCDeviceWinDlg::ProcessRemoteVideoToQueue(void* pBuf, EASY_FRAME_INFO* _frameInfo, int rawVideo)
{
	LockRemoteFrameVector();

	if (remoteFrameInfoVector.size() >= 30)
	{
		FRAME_INFO_VECTOR::iterator it = remoteFrameInfoVector.begin();
		while (it != remoteFrameInfoVector.end())
		{
			if (NULL != it->pbuf)
			{
				delete[]it->pbuf;
			}

			remoteFrameInfoVector.erase(it);

			if (remoteFrameInfoVector.size() < 30)	break;

			it = remoteFrameInfoVector.begin();
			//it++;
		}
	}

	FRAME_INFO_T	remoteFrameInfo;
	memset(&remoteFrameInfo, 0x00, sizeof(FRAME_INFO_T));
	memcpy(&remoteFrameInfo.info, _frameInfo, sizeof(EASY_FRAME_INFO));

	remoteFrameInfo.rawVideo = rawVideo;

	remoteFrameInfo.pbuf = new char[_frameInfo->length];
	if (NULL != remoteFrameInfo.pbuf)
	{
		memcpy(remoteFrameInfo.pbuf, pBuf, _frameInfo->length);
		remoteFrameInfoVector.push_back(remoteFrameInfo);
	}

	UnlockRemoteFrameVector();
}


void	CEasyRTCDeviceWinDlg::ProcessRemoteAudioToQueue(void* pBuf, EASY_FRAME_INFO* _frameInfo)
{
	LockAudioFrameQueue();

	if (remoteAudioFrameInfoVector.size() >= 30)
	{
		FRAME_INFO_VECTOR::iterator it = remoteAudioFrameInfoVector.begin();
		while (it != remoteAudioFrameInfoVector.end())
		{
			if (NULL != it->pbuf)
			{
				delete[]it->pbuf;
			}

			remoteAudioFrameInfoVector.erase(it);

			if (remoteAudioFrameInfoVector.size() < 30)	break;

			it = remoteAudioFrameInfoVector.begin();
			//it++;
		}
	}

	FRAME_INFO_T	remoteFrameInfo;
	memset(&remoteFrameInfo, 0x00, sizeof(FRAME_INFO_T));
	memcpy(&remoteFrameInfo.info, _frameInfo, sizeof(EASY_FRAME_INFO));

	remoteFrameInfo.pbuf = new char[_frameInfo->length];
	if (NULL != remoteFrameInfo.pbuf)
	{
		memcpy(remoteFrameInfo.pbuf, pBuf, _frameInfo->length);
		remoteAudioFrameInfoVector.push_back(remoteFrameInfo);
	}

	UnlockAudioFrameQueue();
}
int	    CEasyRTCDeviceWinDlg::ProcessRemoteAudioFromQueue(int clear)
{
	int isRawVideo = 0;
	char* pBuf = NULL;
	EASY_FRAME_INFO remoteFrameInfo;
	memset(&remoteFrameInfo, 0x00, sizeof(EASY_FRAME_INFO));

	LockAudioFrameQueue();

	if (clear == 0)		// 逐帧处理
	{
		FRAME_INFO_VECTOR::iterator it = remoteAudioFrameInfoVector.begin();
		if (it != remoteAudioFrameInfoVector.end())
		{
			pBuf = it->pbuf;
			memcpy(&remoteFrameInfo, &it->info, sizeof(EASY_FRAME_INFO));
			isRawVideo = it->rawVideo;

			remoteAudioFrameInfoVector.erase(it);
		}
	}
	else
	{
		// 删除所有帧
		FRAME_INFO_VECTOR::iterator it = remoteAudioFrameInfoVector.begin();
		while (it != remoteAudioFrameInfoVector.end())
		{
			if (NULL != it->pbuf)
			{
				delete[]it->pbuf;
			}
			remoteAudioFrameInfoVector.erase(it);
			it = remoteAudioFrameInfoVector.begin();
		}

		remoteAudioFrameInfoVector.clear();
	}

	UnlockAudioFrameQueue();

	if (clear == 1)		return 0;

	int ret = -1;
	if (NULL != pBuf)
	{
		ret = 0;

		libAudioPlayer_Write(pBuf, remoteFrameInfo.length);

		//RenderVideo(&mLocalRTCDevice.remoteVideoRender, pBuf, &remoteFrameInfo, isRawVideo);

		delete[]pBuf;
	}

	return ret;
}

int	   CEasyRTCDeviceWinDlg::ProcessRemoteVideoFromQueue(int clear)
{
	int isRawVideo = 0;
	char* pBuf = NULL;
	EASY_FRAME_INFO remoteFrameInfo;
	memset(&remoteFrameInfo, 0x00, sizeof(EASY_FRAME_INFO));

	LockRemoteFrameVector();

	if (clear == 0)		// 逐帧处理
	{
		FRAME_INFO_VECTOR::iterator it = remoteFrameInfoVector.begin();
		if (it != remoteFrameInfoVector.end())
		{
			pBuf = it->pbuf;
			memcpy(&remoteFrameInfo, &it->info, sizeof(EASY_FRAME_INFO));
			isRawVideo = it->rawVideo;

			remoteFrameInfoVector.erase(it);
		}
	}
	else
	{
		// 删除所有帧
		FRAME_INFO_VECTOR::iterator it = remoteFrameInfoVector.begin();
		while (it != remoteFrameInfoVector.end())
		{
			if (NULL != it->pbuf)
			{
				delete[]it->pbuf;
			}
			remoteFrameInfoVector.erase(it);
			it = remoteFrameInfoVector.begin();
		}

		remoteFrameInfoVector.clear();
	}

	UnlockRemoteFrameVector();

	if (clear == 1)		return 0;

	int ret = -1;
	if (NULL != pBuf)
	{
		ret = 0;

		RenderVideo(&mLocalRTCDevice.remoteVideoRender, pBuf, &remoteFrameInfo, isRawVideo);

		delete[]pBuf;
	}

	return ret;
}


void	CEasyRTCDeviceWinDlg::ProcessLocalVideoToQueue(void* pBuf, EASY_FRAME_INFO* _frameInfo, int rawVideo)
{
	LockLocalFrameVector();

	if (localFrameInfoVector.size() >= 30)
	{
		FRAME_INFO_VECTOR::iterator it = localFrameInfoVector.begin();
		while (it != localFrameInfoVector.end())
		{
			if (NULL != it->pbuf)
			{
				delete[]it->pbuf;
			}

			localFrameInfoVector.erase(it);

			if (localFrameInfoVector.size() < 30)	break;

			it = localFrameInfoVector.begin();
			//it++;
		}
	}

	FRAME_INFO_T	localFrameInfo;
	memset(&localFrameInfo, 0x00, sizeof(FRAME_INFO_T));
	memcpy(&localFrameInfo.info, _frameInfo, sizeof(EASY_FRAME_INFO));

	localFrameInfo.rawVideo = rawVideo;
	
	localFrameInfo.pbuf = new char[_frameInfo->length];
	if (NULL != localFrameInfo.pbuf)
	{
		memcpy(localFrameInfo.pbuf, pBuf, _frameInfo->length);
		localFrameInfoVector.push_back(localFrameInfo);
	}

	UnlockLocalFrameVector();
}

int		CEasyRTCDeviceWinDlg::ProcessLocalVideoFromQueue(int clear)
{
	int isRawVideo = 0;
	char* pBuf = NULL;
	EASY_FRAME_INFO localFrameInfo;
	memset(&localFrameInfo, 0x00, sizeof(EASY_FRAME_INFO));

	LockLocalFrameVector();

	if (clear == 0)		// 逐帧处理
	{
		FRAME_INFO_VECTOR::iterator it = localFrameInfoVector.begin();
		if (it != localFrameInfoVector.end())
		{
			pBuf = it->pbuf;
			memcpy(&localFrameInfo, &it->info, sizeof(EASY_FRAME_INFO));
			isRawVideo = it->rawVideo;

			localFrameInfoVector.erase(it);
		}
	}
	else
	{
		// 删除所有帧
		FRAME_INFO_VECTOR::iterator it = localFrameInfoVector.begin();
		while (it != localFrameInfoVector.end())
		{
			if (NULL != it->pbuf)
			{
				delete[]it->pbuf;
			}
			localFrameInfoVector.erase(it);
			it = localFrameInfoVector.begin();
		}

		localFrameInfoVector.clear();
	}

	UnlockLocalFrameVector();

	if (clear == 1)		return 0;

	int ret = -1;
	if (NULL != pBuf)
	{
		ret = 0;

		if (mLocalRTCDevice.localPreview)
		{
			if ((!mLocalRTCDevice.findLocalKeyframe) ||
				(mLocalRTCDevice.findLocalKeyframe && localFrameInfo.type == 0x01))
			{
				RenderVideo(&mLocalRTCDevice.localVideoRender, pBuf, &localFrameInfo, isRawVideo);
				mLocalRTCDevice.findLocalKeyframe = false;
			}
		}
		else
		{
			mLocalRTCDevice.findLocalKeyframe = true;
		}
		delete[]pBuf;
	}

	return ret;
}

#ifdef _WIN32
DWORD WINAPI __LocalVideoRenderThread(void* lpParam)
#else
void* __LocalVideoRenderThread(void* lpParam)
#endif
{
	OSTHREAD_OBJ_T* pThread = (OSTHREAD_OBJ_T*)lpParam;
	CEasyRTCDeviceWinDlg * pThis = (CEasyRTCDeviceWinDlg*)pThread->userPtr;

	pThread->flag = THREAD_STATUS_RUNNING;

	pThis->GetRTCDevicePtr()->localVideoRender.startTime = 0;

	while (1)
	{
		if (pThread->flag == THREAD_STATUS_EXIT)		break;

		int ret = pThis->ProcessLocalVideoFromQueue(0);
		if (ret < 0)
		{
			Sleep(1);
		}
	}

	pThis->ProcessLocalVideoFromQueue(1);

	pThread->flag = THREAD_STATUS_INIT;



	return 0;
}



#ifdef _WIN32
DWORD WINAPI __RemoteAudioPlayThread(void* lpParam)
#else
void* __RemoteAudioPlayThread(void* lpParam)
#endif
{
	OSTHREAD_OBJ_T* pThread = (OSTHREAD_OBJ_T*)lpParam;
	CEasyRTCDeviceWinDlg* pThis = (CEasyRTCDeviceWinDlg*)pThread->userPtr;

	pThread->flag = THREAD_STATUS_RUNNING;

	while (1)
	{
		if (pThread->flag == THREAD_STATUS_EXIT)		break;

		int ret = pThis->ProcessRemoteAudioFromQueue(0);
		if (ret < 0)
		{
			timeBeginPeriod(1);
			//Sleep(1);
			SleepEx(1, FALSE);
			timeEndPeriod(1);
		}
	}

	pThis->ProcessRemoteAudioFromQueue(1);

	pThread->flag = THREAD_STATUS_INIT;

	return 0;
}


#ifdef _WIN32
DWORD WINAPI __RemoteVideoRenderThread(void* lpParam)
#else
void* __RemoteVideoRenderThread(void* lpParam)
#endif
{
	OSTHREAD_OBJ_T* pThread = (OSTHREAD_OBJ_T*)lpParam;
	CEasyRTCDeviceWinDlg* pThis = (CEasyRTCDeviceWinDlg*)pThread->userPtr;

	pThread->flag = THREAD_STATUS_RUNNING;

	pThis->GetRTCDevicePtr()->remoteVideoRender.startTime = 0;

	while (1)
	{
		if (pThread->flag == THREAD_STATUS_EXIT)		break;

		int ret = pThis->ProcessRemoteVideoFromQueue(0);
		if (ret < 0)
		{
			Sleep(1);
		}
	}

	pThis->ProcessRemoteVideoFromQueue(1);

	pThread->flag = THREAD_STATUS_INIT;



	return 0;
}



void	CEasyRTCDeviceWinDlg::RenderVideo(VIDEO_RENDER_T* pRender, void* pBuf, EASY_FRAME_INFO* _frameInfo, int rawVideo)
{
	if (NULL == pRender->transcodeHandle)
	{
		if (rawVideo == 0x01)
		{
			int dstPixelFormat = 1;	//YUY2
			TransCode_CreateSws(&pRender->transcodeHandle, _frameInfo->width, _frameInfo->height, 0, _frameInfo->width, _frameInfo->height, dstPixelFormat);
		}
		else
		{
			// 编码帧, 需解码
			TransCode_Create(&pRender->transcodeHandle);

			int pixelFormat = 1;		//D3D_FORMAT_YUY2
			int transcodeCodecId = 0;
			//int transcodeCodecId = TRANSCODE_VIDEO_CODEC_ID_H264;	//TRANSCODE_VIDEO_CODEC_ID_MJPEG
			//if (_frameInfo->codec == EASY_SDK_VIDEO_CODEC_H265)	transcodeCodecId = TRANSCODE_VIDEO_CODEC_ID_H265;

			int HWDecoderId = -1;// 7;
			TransCode_Init(pRender->transcodeHandle, 0, HWDecoderId, 0, 0, pixelFormat, transcodeCodecId, -1, 25, 25, 1024 * 1024);

		}
	}

	if (NULL != pRender->transcodeHandle)
	{
		unsigned char* rgbData = NULL;
		int rgbSize = 0;
		if (rawVideo == 0x01)
		{
			TransCode_SwsScale(pRender->transcodeHandle, (unsigned char*)pBuf, &rgbData, &rgbSize);
		}
		else
		{
			TRANSCODE_DECODE_CALLBACK decodeCB = NULL;
			TRANSCODE_ENCODE_CALLBACK encodeCB = NULL;
			int width = 0, height = 0;
			int keyFrame = 0;
			int ret = TransCode_TransCodeVideo(pRender->transcodeHandle, (char*)pBuf, _frameInfo->length,
				&width, &height, (char**)&rgbData, (int*)&rgbSize, &keyFrame,
				decodeCB, NULL,
				encodeCB, NULL, 0, true);

			if (ret != 0)							return;
			if (width < 1 || height < 1)			return;
			if (rgbSize < width * height)			return;

			if (_frameInfo->width < 1)	_frameInfo->width = width;
			if (_frameInfo->height < 1)	_frameInfo->height = height;
		}

		if (NULL == pRender->d3dHandle)
		{
			char fontName[32] = { 0 };
			strcpy(fontName, "Arial");
			D3D_FONT    d3dFont;
			memset(&d3dFont, 0x00, sizeof(D3D_FONT));
			MByteToWChar(fontName, d3dFont.name, sizeof(d3dFont.name) / sizeof(d3dFont.name[0]));
			d3dFont.size = 36;
			d3dFont.width = 36;
			D3D_Initial(&pRender->d3dHandle, pRender->renderHwnd, _frameInfo->width, _frameInfo->height, 0, 1, D3D_FORMAT_YUY2, &d3dFont);

			if (pRender->startTime < 1)
			{
				pRender->startTime = time(NULL);
				pRender->renderFrameNum = 0;
			}
		}

		if (pRender->d3dHandle && rgbData)// && rgbSize == width * height * 3)
		{
			RECT rcSrc, rcDst;
			SetRect(&rcSrc, 0, 0, _frameInfo->width, _frameInfo->height);
			::GetClientRect(pRender->renderHwnd, &rcDst);
			if (!EqualRect(&rcDst, &pRender->renderRect))
			{
				if (pRender->d3dHandle)
				{
					D3D_Release(&pRender->d3dHandle);
				}
				CopyRect(&pRender->renderRect, &rcDst);

				return;
			}

			time_t duration = time(NULL) - pRender->startTime;

			// 计算播放时长并显示
			int iHour = (int)((duration) / 3600);
			int iMinute = ((duration) / 60) % 60;
			int iSecond = (duration) % 60;

			char osdStr[64] = { 0 };
			//sprintf(osdStr, "[%d x %d]  FPS[%d][%d][%d] Total[%lld]", width, height, pCameraInfo->channel[streamId].sourceFPS, pCameraInfo->channel[streamId].recvFps.value, pCameraInfo->channel[streamId].renderFps.value, pCameraInfo->channel[streamId].renderFps.valueTotal);
			sprintf(osdStr, "[%d x %d]  %04d:%02d:%02d  %d", _frameInfo->width, _frameInfo->height, iHour, iMinute, iSecond, pRender->renderFrameNum);

			int showStatistics = 1;
			D3D_OSD     osd;
			//if (pCameraInfo->channel[streamId].showStatistic == 0x01)
			{

				memset(&osd, 0x00, sizeof(D3D_OSD));
				MByteToWChar(osdStr, osd.string, sizeof(osd.string) / sizeof(osd.string[0]));
				SetRect(&osd.rect, 2, 2, (int)wcslen(osd.string) * 18, (int)(float)((_frameInfo->height) * 0.04f));//40);
				if (osd.rect.bottom - osd.rect.top < 30) osd.rect.bottom = osd.rect.top + 30;
				osd.color = RGB(0xff, 0xff, 0x00);
				osd.shadowcolor = RGB(0x15, 0x15, 0x15);
				osd.alpha = 180;
			}

			int ret = D3D_UpdateData(pRender->d3dHandle, 0, rgbData, _frameInfo->width, _frameInfo->height, &rcSrc, NULL, showStatistics == 0x01 ? 1 : 0, &osd);
			if (ret != 0)
			{
				ret = D3D_UpdateData(pRender->d3dHandle, 0, rgbData, _frameInfo->width, _frameInfo->height, &rcSrc, NULL, showStatistics == 0x01 ? 1 : 0, &osd);
			}

			int ShownToScale = 1;
			D3D_Render(pRender->d3dHandle, pRender->renderHwnd, ShownToScale, &rcDst, 0, NULL);// 1, & osd);

			pRender->renderFrameNum++;
		}
	}
}

int Easy_APICALL __EasyStreamClientCallBack(void* _channelPtr, int _frameType, void* pBuf, EASY_FRAME_INFO* _frameInfo)
{
	LOCAL_RTC_DEVICE_T* pLocalDevice = (LOCAL_RTC_DEVICE_T*)_channelPtr;

	CEasyRTCDeviceWinDlg* pThis = (CEasyRTCDeviceWinDlg*)pLocalDevice->pThis;

	if (_frameType == EASY_SDK_VIDEO_FRAME_FLAG)
	{
		if (_frameInfo->codec == EASY_SDK_VIDEO_CODEC_RAWVIDEO)
		{
			_frameInfo->length = _frameInfo->width * _frameInfo->height * 3 / 2;
			pThis->ProcessLocalVideoToQueue(pBuf, _frameInfo, 1);		// 将视频帧送入队列
		}
		else
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

			if (pLocalDevice->videoCodecID < 1)
			{
				if (EASY_SDK_VIDEO_CODEC_H264 == _frameInfo->codec)	pLocalDevice->videoCodecID = EASYRTC_CODEC_H264;
				else if (EASY_SDK_VIDEO_CODEC_H265 == _frameInfo->codec)	pLocalDevice->videoCodecID = EASYRTC_CODEC_H265;
			}

			//EasyRTCDevice::__Print__(__FUNCTION__, __LINE__, "videoPTS:%llu\n", pLocalDevice->videoPTS);

			pLocalDevice->Lock();

			EasyRTC_Device_SendVideoFrame(pLocalDevice->easyRTCDeviceHandle, (char*)pBuf, _frameInfo->length, _frameInfo->type, pLocalDevice->videoPTS);
			EasyRTC_Device_SendVideoFrame(pLocalDevice->easyRTCCallerHandle, (char*)pBuf, _frameInfo->length, _frameInfo->type, pLocalDevice->videoPTS);
			
			pLocalDevice->Unlock();

			if (pLocalDevice->sourceType == 0x01)		// 本地拉流, 需在本地显示
			{
				pThis->ProcessLocalVideoToQueue(pBuf, _frameInfo, 0);		// 将视频帧送入队列
			}
		}
	}
	else if (_frameType == EASY_SDK_AUDIO_FRAME_FLAG)
	{
		uint64_t pts = _frameInfo->timestamp_sec * 1000 + _frameInfo->timestamp_usec / 1000;
		
		if (pLocalDevice->audioCodecID == EASYRTC_CODEC_AAC)
		{
			pLocalDevice->audioDTS += 1024;
		}
		else
		{
			// 时间戳计算
			if (pLocalDevice->audioDTSstep < 1)
			{
				if (pLocalDevice->audioBitPerSamples < 1)	pLocalDevice->audioBitPerSamples = 16;

				int data_size_sec = pLocalDevice->audioSamplerate * pLocalDevice->audioBitPerSamples * pLocalDevice->audioChannels / 8;	// 每秒数据量
				int data_size_msec = data_size_sec / 1000;	// 每毫秒数据量

				int interval_ms = _frameInfo->length / data_size_msec;	// 毫秒

				pLocalDevice->audioDTSstep = interval_ms;
			}
			
			pLocalDevice->audioDTS += pLocalDevice->audioDTSstep;
		}

		pLocalDevice->Lock();

		EasyRTC_Device_SendAudioFrame(pLocalDevice->easyRTCDeviceHandle, (char*)pBuf, _frameInfo->length, pLocalDevice->audioDTS);
		EasyRTC_Device_SendAudioFrame(pLocalDevice->easyRTCCallerHandle, (char*)pBuf, _frameInfo->length, pLocalDevice->audioDTS);

		pLocalDevice->Unlock();
	}
	else if (_frameType == EASY_SDK_MEDIA_INFO_FLAG)
	{
		EASY_MEDIA_INFO_T* mediainfo = (EASY_MEDIA_INFO_T*)pBuf;

		if (mediainfo->u32VideoCodec > 0)
		{
			if (EASY_SDK_VIDEO_CODEC_H264 == mediainfo->u32VideoCodec)	pLocalDevice->videoCodecID = EASYRTC_CODEC_H264;
			else if (EASY_SDK_VIDEO_CODEC_H265 == mediainfo->u32VideoCodec)	pLocalDevice->videoCodecID = EASYRTC_CODEC_H265;
		}
		if (mediainfo->u32AudioCodec > 0)
		{
			if (EASY_SDK_AUDIO_CODEC_AAC == mediainfo->u32AudioCodec)	pLocalDevice->audioCodecID = EASYRTC_CODEC_AAC;
			else if (EASY_SDK_AUDIO_CODEC_G711U == mediainfo->u32AudioCodec)	pLocalDevice->audioCodecID = EASYRTC_CODEC_MULAW;
			else if (EASY_SDK_AUDIO_CODEC_G711A == mediainfo->u32AudioCodec)	pLocalDevice->audioCodecID = EASYRTC_CODEC_ALAW;
		}

		pLocalDevice->audioSamplerate = mediainfo->u32AudioSamplerate;
		pLocalDevice->audioBitPerSamples = mediainfo->u32AudioBitsPerSample;
		pLocalDevice->audioChannels = mediainfo->u32AudioChannel;
	}

	
	return 0;
}


int CALLBACK __AudioDataCallBack(void* userptr, char* pbuf, const int bufsize)
{
	printf("%s line[%d] packet size:%d\n", __FUNCTION__, __LINE__, bufsize);

	LOCAL_RTC_DEVICE_T* pLocalDevice = (LOCAL_RTC_DEVICE_T*)userptr;

	if (pLocalDevice->videoPTS < 1)		return 0;

	// 时间戳计算
	pLocalDevice->audioDTS += pLocalDevice->audioDTSstep;

	pLocalDevice->Lock();

	EasyRTC_Device_SendAudioFrame(pLocalDevice->easyRTCDeviceHandle, pbuf, bufsize, pLocalDevice->audioDTS);
	EasyRTC_Device_SendAudioFrame(pLocalDevice->easyRTCCallerHandle, pbuf, bufsize, pLocalDevice->audioDTS);

	pLocalDevice->Unlock();

	//EasyRTCDevice::__Print__(__FUNCTION__, __LINE__, "\tAudioDTS:%llu\n", pLocalDevice->audioDTS);

	return 0;
}

int __EasyRTC_Data_Callback(void* userptr, const char* peerUUID, EASYRTC_DATA_TYPE_ENUM_E dataType, int codecID, int isBinary, char* data, int size, int keyframe, unsigned long long pts)
{
	CEasyRTCDeviceWinDlg* pThis = (CEasyRTCDeviceWinDlg*)userptr;

	if (EASYRTC_CALLBACK_TYPE_DNS_FAIL == dataType)
	{
		pThis->PostMessageW(WM_EASY_RTC_CALLBACK_LOG, 0, (LPARAM)"DNS解析失败...\n");
		pThis->PostMessageW(WM_EASY_RTC_CALLBACK_STATUS, 0, (LPARAM)"DNS解析失败");
	}
	else if (EASYRTC_CALLBACK_TYPE_CONNECTING == dataType)
	{
		pThis->PostMessageW(WM_EASY_RTC_CALLBACK_LOG, 0, (LPARAM)"正在连接平台...\n");
		pThis->PostMessageW(WM_EASY_RTC_CALLBACK_STATUS, 0, (LPARAM)"正在连接平台...");
	}
	else if (EASYRTC_CALLBACK_TYPE_CONNECTED == dataType)
	{
		pThis->PostMessageW(WM_EASY_RTC_CALLBACK_LOG, 0, (LPARAM)"连接平台成功.\n");
		pThis->PostMessageW(WM_EASY_RTC_CALLBACK_STATUS, 0, (LPARAM)"接入平台成功.");

		pThis->PostMessage(WM_EASY_RTC_REFRESH_DEVICE_LIST);	// 刷新列表
	}
	else if (EASYRTC_CALLBACK_TYPE_CONNECT_FAIL == dataType)
	{
		pThis->PostMessageW(WM_EASY_RTC_CALLBACK_LOG, 0, (LPARAM)"连接平台失败...\n");
		pThis->PostMessageW(WM_EASY_RTC_CALLBACK_STATUS, 0, (LPARAM)"连接平台失败...");
	}
	else if (EASYRTC_CALLBACK_TYPE_DISCONNECT == dataType)
	{
		pThis->PostMessageW(WM_EASY_RTC_CALLBACK_LOG, 0, (LPARAM)"平台链接已断开...\n");
		pThis->PostMessageW(WM_EASY_RTC_CALLBACK_STATUS, 0, (LPARAM)"平台链接已断开...");
	}

	else if (EASYRTC_CALLBACK_TYPE_PASSIVE_CALL == dataType)
	{
		//printf("Passive call..  peerUUID[%s]\n", peerUUID);

		//pThis->PostMessageW(WM_EASY_RTC_CALLBACK, 0, (LPARAM)"对端呼叫, 已响应...\n");

		char szLog[128] = { 0 };

		LOCAL_RTC_DEVICE_T* pRTCDevice = pThis->GetRTCDevicePtr();
		if (NULL != pRTCDevice->easyRTCCallerHandle && pRTCDevice->pRemoteRenderThread)		// 如果当前已经呼叫其他设备, 则不能被别人呼叫
		{
			sprintf(szLog, "对端[%s]呼叫, 当前已呼叫其他设备, 此处不允许被呼叫.\n", peerUUID);
			pThis->SendMessageW(WM_EASY_RTC_CALLBACK_LOG, 0, (LPARAM)szLog);
			return 0;
		}
		else
		{
			// 没有呼叫其他设备, 则可以被别人呼叫
			sprintf(szLog, "对端[%s]呼叫, 已成功响应.\n", peerUUID);
			pThis->SendMessageW(WM_EASY_RTC_CALLBACK_LOG, 0, (LPARAM)szLog);
			return 1;		// 返回1表示自动接受	如果返回0,则需要调用EasyRTC_Device_PassiveCallResponse来处理该请求: 接受或拒绝
		}
	}

	else if (EASYRTC_CALLBACK_TYPE_START_VIDEO == dataType)
	{
		printf("Start Video..  peerUUID[%s]\n", peerUUID);

		// 此时有用户请求发送视频
		pThis->SetPeerUUID(peerUUID);

		pThis->PostMessageW(WM_EASY_RTC_CALLBACK_LOG, 0, (LPARAM)"对端请求播放视频...\n");

		LOCAL_RTC_DEVICE_T* pRTCDevice = pThis->GetRTCDevicePtr();
		pRTCDevice->sendVideoFlag = true;
	}
	else if (EASYRTC_CALLBACK_TYPE_START_AUDIO == dataType)
	{
		printf("Start Audio.. peerUUID[%s]\n", peerUUID);

		pThis->PostMessageW(WM_EASY_RTC_CALLBACK_LOG, 0, (LPARAM)"对端请求播放音频...\n");
	}
	else if (EASYRTC_CALLBACK_TYPE_STOP_VIDEO == dataType)
	{
		// 此时用户已关闭视频，停止发送

		LOCAL_RTC_DEVICE_T* pRTCDevice = pThis->GetRTCDevicePtr();
		pRTCDevice->sendVideoFlag = false;

		pThis->PostMessageW(WM_EASY_RTC_CALLBACK_LOG, 0, (LPARAM)"对端已停止播放视频...\n");
	}
	else if (EASYRTC_CALLBACK_TYPE_STOP_AUDIO == dataType)
	{
		pThis->PostMessageW(WM_EASY_RTC_CALLBACK_LOG, 0, (LPARAM)"对端已停止播放音频...\n");
	}
	else if (EASYRTC_CALLBACK_TYPE_PEER_VIDEO == dataType)
	{
#ifndef _DEBUG
		printf("OnPeerVideo..\n");
#endif

		EASY_FRAME_INFO	frameinfo;
		memset(&frameinfo, 0x00, sizeof(EASY_FRAME_INFO));
		frameinfo.length = size;
		frameinfo.type = keyframe;
		//frameinfo.codec = codecID;
		frameinfo.pts = pts;

		//int codecID, int isBinary, char* data, int size, int keyframe, unsigned long long pts

		pThis->ProcessRemoteVideoToQueue(data, &frameinfo, 0);		// 将视频帧送入待解码显示队列
	}
	else if (EASYRTC_CALLBACK_TYPE_PEER_AUDIO == dataType)
	{
#ifndef _DEBUG
		printf("OnPeerAudio..\n");
#endif
		EASY_FRAME_INFO	frameinfo;
		memset(&frameinfo, 0x00, sizeof(EASY_FRAME_INFO));
		frameinfo.length = size;

		pThis->ProcessRemoteAudioToQueue(data, &frameinfo);		// 将音频帧送入待播放队列
	}
	else if (EASYRTC_CALLBACK_TYPE_LOCAL_AUDIO == dataType)
	{
		printf("Local audio..\n");
	}
	else if (EASYRTC_CALLBACK_TYPE_PEER_CONNECTED == dataType)
	{
		pThis->InitPlayResource();			// 连接已成功建立, 初始化播放资源

		char msg[128] = { 0 };
		sprintf(msg, "对端[%s]连接成功[%s]...\n", peerUUID, codecID == 1 ? "P2P" : "Relay");

		// 此时有用户请求发送视频
		pThis->SetPeerUUID(peerUUID);

		pThis->SendMessageW(WM_EASY_RTC_CALLBACK_LOG, 0, (LPARAM)msg);

		pThis->EnableCallEnd(TRUE);
	}
	else if (EASYRTC_CALLBACK_TYPE_PEER_CONNECT_FAIL == dataType)
	{
		pThis->PostMessageW(WM_EASY_RTC_CALLBACK_LOG, 0, (LPARAM)"对端连接失败...\n");
		pThis->EnableCallEnd(FALSE);
		pThis->EnableTextMessage(FALSE);
	}
	else if (EASYRTC_CALLBACK_TYPE_PEER_DISCONNECT == dataType)
	{
		pThis->PostMessageW(WM_EASY_RTC_CALLBACK_LOG, 0, (LPARAM)"对端连接已断开...\n");
		pThis->EnableCallEnd(FALSE);
		pThis->EnableTextMessage(FALSE);
	}
	else if (EASYRTC_CALLBACK_TYPE_PEER_CLOSED == dataType)
	{
		pThis->EnableCallEnd(FALSE);
		pThis->EnableTextMessage(FALSE);
		pThis->SetPeerUUID(NULL);
		pThis->UpdateCallState(true);

		//pThis->PostMessage(WM_CLICK_BUTTON, 9999, 0);
		pThis->PostMessage(WM_EASY_RTC_CALL_END);

		pThis->PostMessageW(WM_EASY_RTC_CALLBACK_LOG, 0, (LPARAM)"对端已关闭.\n");
		pThis->PostMessageW(WM_EASY_RTC_UPDATE_WINDOW, 0, 0);
	}
	else if (EASYRTC_CALLBACK_TYPE_PEER_ENABLED_CUSTOM_DATA == dataType)
	{
		pThis->EnableTextMessage(TRUE);
	}
	else if (EASYRTC_CALLBACK_TYPE_PEER_CUSTOM_DATA == dataType)
	{
		pThis->OnPeerMessage(data, size);
	}

	else if (EASYRTC_CALLBACK_TYPE_ONLINE_DEVICE == dataType)		// 在线设备列表
	{
		char* peerID = new char[128];
		if (peerID)
		{
			memset(peerID, 0x00, 128);
			strcpy(peerID, peerUUID);

			pThis->PostMessageW(WM_EASY_RTC_CALLBACK_ONLINE_DEVICE, 0, (LPARAM)peerID);
		}
	}

	return 0;
}

void	CEasyRTCDeviceWinDlg::SetPeerUUID(const char* peerUUID)
{
	memset(mPeerUUID, 0x00, sizeof(mPeerUUID));
	if (NULL != peerUUID)
	{
		strcpy(mPeerUUID, peerUUID);

		memset(mLastDeviceID, 0x0, sizeof(mLastDeviceID));
		strcpy(mLastDeviceID, peerUUID);
	}
}

void	CEasyRTCDeviceWinDlg::OnPeerMessage(char* msg, int size)
{
	memset(peerCustomMessage, 0x00, sizeof(peerCustomMessage));
	if (size < sizeof(peerCustomMessage) - 1)
	{
		strcpy(peerCustomMessage, "[接收]: ");
		int len = (int)strlen(peerCustomMessage);
		memcpy(peerCustomMessage+len, msg, size);
		strcat(peerCustomMessage, "\n");
	}

	PostMessageW(WM_EASY_RTC_CALLBACK_LOG, 0, (LPARAM)peerCustomMessage);
}

void	CEasyRTCDeviceWinDlg::CloseAll()
{
	KillTimer(REFRESH_DEVICE_TIMER_ID);

	EasyStreamClient_Deinit(mLocalRTCDevice.easyStreamHandle);
	mLocalRTCDevice.easyStreamHandle = NULL;

#if 1
	libAudioCapturer_StopAudioCapture();
	libAudioCapturer_CloseAudioCaptureDevice();
#endif

	EasyRTC_Device_Release(&mLocalRTCDevice.easyRTCDeviceHandle);
	EasyRTC_Device_Release(&mLocalRTCDevice.easyRTCCallerHandle);
	

	if (mLocalRTCDevice.pLocalRenderThread)
	{
		DeleteOSThread(&mLocalRTCDevice.pLocalRenderThread);
	}
	if (mLocalRTCDevice.pRemoteRenderThread)
	{
		DeleteOSThread(&mLocalRTCDevice.pRemoteRenderThread);
	}
	if (mLocalRTCDevice.pRemoteAudioPlayThread)
	{
		DeleteOSThread(&mLocalRTCDevice.pRemoteAudioPlayThread);
	}



	//libAudioPlayer_Stop();
	//libAudioPlayer_Close();
	//bInitAudioPlayer = false;
	TransCode_Release(&mLocalRTCDevice.localVideoRender.transcodeHandle);
	D3D_Release(&mLocalRTCDevice.localVideoRender.d3dHandle);

	TransCode_Release(&mLocalRTCDevice.remoteVideoRender.transcodeHandle);
	D3D_Release(&mLocalRTCDevice.remoteVideoRender.d3dHandle);


	PostMessageW(WM_EASY_RTC_CALLBACK_STATUS, 0, (LPARAM)"未连接");
	PostMessageW(WM_EASY_RTC_CALLBACK_LOG, 0, (LPARAM)"已断开与平台的链接.\n");

	if (pBtnStartStop)	pBtnStartStop->SetWindowTextW(TEXT("接入"));
	pBtnPreview->SetWindowTextW(TEXT("开启本地预览"));

	SetPeerUUID(NULL);
	mLocalRTCDevice.sourceType = -1;
	pDlgVideoLocal->Invalidate();
	pDlgVideoRemote->Invalidate();
}

void CEasyRTCDeviceWinDlg::OnBnClickedButtonStartStop()
{
	if (NULL == mLocalRTCDevice.easyRTCDeviceHandle)
	{
		char serverIP[64] = { 0 };
		char serverPort[32] = { 0 };
		int isSecure = 0;
		char deviceID[64] = { 0 };

		GetEditText(serverIP, sizeof(serverIP) / sizeof(serverIP[0]), pEdtServerIP);
		GetEditText(serverPort, sizeof(serverPort) / sizeof(serverPort[0]), pEdtServerPort);
		GetEditText(deviceID, sizeof(deviceID) / sizeof(deviceID[0]), pEdtDeviceID);
		isSecure = pChkSSL->GetCheck();

		if ((0 == strcmp(serverIP, "\0")) || (0 == strcmp(serverPort, "\0")) ||
			(strlen(deviceID) != 36))
		{
			MessageBox(TEXT("请输入有效的平台和设备信息"));
			return;
		}

		int streamNum = 0;

		mLocalRTCDevice.videoPTS = 0;
		mLocalRTCDevice.audioDTS = 0;
		mLocalRTCDevice.videoLastPts = 0;
		mLocalRTCDevice.sendVideoFlag = false;
		
		mLocalRTCDevice.remoteVideoRender.renderHwnd = pDlgVideoRemote->GetSafeHwnd();

		if (NULL == mLocalRTCDevice.easyStreamHandle)		// 如果没打开本地源
		{
			if (!OpenLocalSource())			// 打开本地源
			{
				//MessageBox(TEXT("打开本地源失败"));
				return;
			}
		}


		{
			// 等待获取音视频编码格式
			for (int i = 0; i < 10; i++)
			{
				if (pComboxLocalAudioList->GetCount() > 0)
				{
					if (mLocalRTCDevice.videoCodecID > 0 && mLocalRTCDevice.audioCodecID > 0)		break;
				}
				else
				{
					if (mLocalRTCDevice.videoCodecID > 0)		break;
				}

				Sleep(1000);
			}

			if (NULL == mLocalRTCDevice.easyRTCDeviceHandle)
			{
				EasyRTC_Device_Create(&mLocalRTCDevice.easyRTCDeviceHandle, serverIP, atoi(serverPort), isSecure, deviceID, __EasyRTC_Data_Callback, this);
			}


			//char hostname[64] = { 0 };
			//strcpy(hostname, "EasyRTCDevice(");
			//int len = (int)strlen(hostname);
			//gethostname(hostname + len, sizeof(hostname) - len);
			//strcat(hostname, ")");
			//EasyRTC_Start(mLocalRTCDevice.easyRTCDeviceHandle, videoCodecID, audioCodecID, uuid, hostname, false, 6688, __EasyRTC_Data_Callback, this);
			//mLocalRTCDevice.easyRtcDevice.SetCallback(__EasyRTC_Device_Callback, this);

			EasyRTC_Device_SetChannelInfo(mLocalRTCDevice.easyRTCDeviceHandle, 
				(EASYRTC_CODEC)mLocalRTCDevice.videoCodecID, EASYRTC_CODEC_MULAW);// (EASYRTC_CODEC)mLocalRTCDevice.audioCodecID);

			if (pBtnStartStop)	pBtnStartStop->SetWindowTextW(TEXT("断开"));

			pBtnPreview->SetWindowTextW(mLocalRTCDevice.localPreview ? TEXT("关闭本地预览") : TEXT("开启本地预览"));

			SetTimer(REFRESH_DEVICE_TIMER_ID, 5000, NULL);
		}
	}
	else
	{
		CloseAll();



		//OnPeerVideoFrame(0, 1);

		//if (pDlgVideoRender && pDlgVideoRender->m_hWnd)
		//{
		//	pDlgVideoRender->ShowWindow(SW_HIDE);
		//}
	}
}

void	CEasyRTCDeviceWinDlg::InitPlayResource()
{
	if (NULL == mLocalRTCDevice.pRemoteRenderThread)
	{
		CreateOSThread(&mLocalRTCDevice.pRemoteRenderThread, __RemoteVideoRenderThread, this, 0);
	}
	if (NULL == mLocalRTCDevice.pRemoteAudioPlayThread)
	{
		CreateOSThread(&mLocalRTCDevice.pRemoteAudioPlayThread, __RemoteAudioPlayThread, this, 0);
	}
}

bool	CEasyRTCDeviceWinDlg::OpenLocalSource()
{
	mLocalRTCDevice.localVideoRender.renderHwnd = pDlgVideoLocal->GetSafeHwnd();

	mLocalRTCDevice.sourceType = pComboxSourceType->GetCurSel();

	int streamNum = 0;

	char szURL[1024] = { 0 };
	if (mLocalRTCDevice.sourceType == 0)
	{
		if (pComboxLocalCameraList->GetCount() < 1 || pComboxLocalAudioList->GetCount() < 1)
		{
			MessageBox(TEXT("没有找到视频或音频采集设备"));
			mLocalRTCDevice.sourceType = -1;
			return false;
		}

		if (pComboxLocalCameraList->GetCurSel() < 0 || pComboxLocalAudioList->GetCurSel() < 0)
		{
			MessageBox(TEXT("请选择视频和音频采集设备"));
			mLocalRTCDevice.sourceType = -1;
			return false;
		}


		// video=
		wchar_t wszURL[1024] = { 0 };
		pComboxLocalCameraList->GetWindowText(wszURL, sizeof(wszURL) / sizeof(wchar_t));
		WCharToMByte(wszURL, szURL, sizeof(szURL) / sizeof(szURL[0]));

		if (0 != strcmp(szURL, "\0"))
		{
			memset(szURL, 0x00, sizeof(szURL));
			strcpy(szURL, "video=");
			WCharToMByte(wszURL, szURL + 6, (sizeof(szURL) - 6) / sizeof(szURL[0]));
		}


		if (0 != strcmp(szURL, "\0"))
		{
			int audioDeviceIndex = pComboxLocalAudioList->GetCurSel();
			int openDeviceRet = libAudioCapturer_OpenAudioCaptureDevice(audioDeviceIndex);
			if (openDeviceRet == 0)
			{
				mLocalRTCDevice.audioCodecID = EASYRTC_CODEC_MULAW;// EASYRTC_CODEC_ALAW;		// alaw

				int samplerate = 8000;			// 采样率
				int bitPerSamples = 16;			// 采样精度
				int channels = 1;				// 通道数
				int pcm_buf_size_per_sec = samplerate * bitPerSamples * channels / 8;			// 每秒数据量		比如8000*16*1/8=16000
				int pcm_buf_size_per_ms = pcm_buf_size_per_sec / 1000;							// 每毫秒数据量		16000/1000=16
				int interval_ms = 20;															// 间隔20毫秒
				int bytes_per_20ms = pcm_buf_size_per_ms * interval_ms;							// 每20毫秒数据量

				unsigned int audioCodec = AUDIO_CODEC_ID_ALAW;
				if (mLocalRTCDevice.audioCodecID == EASYRTC_CODEC_MULAW)	audioCodec = AUDIO_CODEC_ID_MULAW;

				mLocalRTCDevice.audioDTSstep = interval_ms;

				libAudioCapturer_StartAudioCapture(audioCodec, bytes_per_20ms,
					samplerate, bitPerSamples, channels, __AudioDataCallBack, (void*)&mLocalRTCDevice);

				streamNum++;
			}
		}
	}
	else
	{
		GetEditText(szURL, sizeof(szURL) / sizeof(szURL[0]), pEdtStreamURL);

		if (0 == strcmp(szURL, "\0") || (int)strlen(szURL) < 16)
		{
			MessageBox(TEXT("请输入有效的流地址"));
			pEdtStreamURL->SetFocus();
			mLocalRTCDevice.sourceType = -1;
			return false;
		}
	}


	if (NULL == mLocalRTCDevice.easyStreamHandle)
	{
		EasyStreamClient_Init(&mLocalRTCDevice.easyStreamHandle, 0);
		if (NULL != mLocalRTCDevice.easyStreamHandle)
		{
			EasyStreamClient_SetAudioEnable(mLocalRTCDevice.easyStreamHandle, 1);
			if (mLocalRTCDevice.sourceType == 1)
			{
				EasyStreamClient_SetAudioOutFormat(mLocalRTCDevice.easyStreamHandle, EASY_SDK_AUDIO_CODEC_G711U, 8000, 1);
			}

			EasyStreamClient_SetCallback(mLocalRTCDevice.easyStreamHandle, __EasyStreamClientCallBack);
			EasyStreamClient_OpenStream(mLocalRTCDevice.easyStreamHandle, szURL, EASY_RTP_OVER_TCP, (void*)&mLocalRTCDevice, 1000, 20, 1);

			streamNum++;
		}
	}

	if (NULL == mLocalRTCDevice.pLocalRenderThread)
	{
		CreateOSThread(&mLocalRTCDevice.pLocalRenderThread, __LocalVideoRenderThread, this, 0);
	}

	if (!mLocalRTCDevice.initAudioPlayer)
	{
		libAudioPlayer_Open(8000, 16, 1);
		libAudioPlayer_Play();

		mLocalRTCDevice.initAudioPlayer = true;
	}


	return true;
}

bool	CEasyRTCDeviceWinDlg::CloseLocalSource()
{
	//mLocalRTCDevice.localPreview = false;


	if (NULL == mLocalRTCDevice.easyRTCDeviceHandle)		// 如果没有开启EasyRTC, 则关闭本地源
	{
		EasyStreamClient_Deinit(mLocalRTCDevice.easyStreamHandle);
		mLocalRTCDevice.easyStreamHandle = NULL;

#if 1
		libAudioCapturer_StopAudioCapture();
		libAudioCapturer_CloseAudioCaptureDevice();
#endif

		if (mLocalRTCDevice.pLocalRenderThread)
		{
			DeleteOSThread(&mLocalRTCDevice.pLocalRenderThread);
		}

		mLocalRTCDevice.sourceType = -1;
	}

	pDlgVideoLocal->Invalidate();

	return true;
}


void CEasyRTCDeviceWinDlg::OnBnClickedButtonPreview()
{
	if (mLocalRTCDevice.sourceType < 0)
	{
		mLocalRTCDevice.localPreview = OpenLocalSource();
	}
	else
	{
		if (NULL == mLocalRTCDevice.easyRTCDeviceHandle)
		{
			CloseLocalSource();
			
		}

		mLocalRTCDevice.localPreview = !mLocalRTCDevice.localPreview;
	}

	pBtnPreview->SetWindowTextW(mLocalRTCDevice.localPreview ? TEXT("关闭本地预览") : TEXT("开启本地预览"));
	if (mLocalRTCDevice.localPreview)
	{
		pDlgVideoLocal->Invalidate();
	}
}


void CEasyRTCDeviceWinDlg::OnBnClickedButtonRefreshDevicelist()
{
	RefreshDeviceList();
}

LRESULT CEasyRTCDeviceWinDlg::OnRefreshDeviceList(WPARAM, LPARAM)
{
	RefreshDeviceList();

	return 0;
}

void	CEasyRTCDeviceWinDlg::RefreshDeviceList()
{
	mDeviceListCtrl.DeleteAllItems();
	mDeviceListCtrl.RemoveAllButtons();

	if (NULL != mLocalRTCDevice.easyRTCDeviceHandle)
	{
		EasyRTC_GetOnlineDevices(mLocalRTCDevice.easyRTCDeviceHandle, __EasyRTC_Data_Callback, this);
	}
	else if (NULL != mLocalRTCDevice.easyRTCCallerHandle)
	{
		EasyRTC_GetOnlineDevices(mLocalRTCDevice.easyRTCCallerHandle, __EasyRTC_Data_Callback, this);
	}

}

LRESULT CEasyRTCDeviceWinDlg::OnOnlineDevice(WPARAM, LPARAM lParam)
{
	char* peerID = (char*)lParam;

	wchar_t wszID[128] = { 0 };
	MByteToWChar(peerID, wszID, sizeof(wszID) / sizeof(wszID[0]));

	mDeviceListCtrl.AddItem(wszID, (0 == strcmp(mLastDeviceID, peerID)) ? _T("挂断") : _T("呼叫"));
	mDeviceListCtrl.UpdateButtonPosition();

	delete[]peerID;

	return 0;
}

LRESULT CEasyRTCDeviceWinDlg::OnClickCallButton(WPARAM wParam, LPARAM lParam)
{
	bool resetLastDeviceId = false;

	if (NULL != mLocalRTCDevice.easyRTCCallerHandle)
	{
		mLocalRTCDevice.Lock();
		EasyRTC_Device_Release(&mLocalRTCDevice.easyRTCCallerHandle);
		mLocalRTCDevice.Unlock();

		SetPeerUUID(NULL);
		EnableCallEnd(FALSE);
		EnableTextMessage(FALSE);

		resetLastDeviceId = true;

		PostMessageW(WM_EASY_RTC_CALLBACK_LOG, 0, (LPARAM)"已关闭对端音视频.\n");
	}

	if (0 != strcmp(mPeerUUID, "\0"))
	{
		EasyRTC_Device_Hangup(mLocalRTCDevice.easyRTCDeviceHandle, mPeerUUID);
	}

	if (mLocalRTCDevice.pRemoteRenderThread)
	{
		DeleteOSThread(&mLocalRTCDevice.pRemoteRenderThread);
	}
	if (mLocalRTCDevice.pRemoteAudioPlayThread)
	{
		DeleteOSThread(&mLocalRTCDevice.pRemoteAudioPlayThread);
	}

	TransCode_Release(&mLocalRTCDevice.remoteVideoRender.transcodeHandle);
	D3D_Release(&mLocalRTCDevice.remoteVideoRender.d3dHandle);

	pDlgVideoRemote->Invalidate();


	int nSel = (int)wParam;
	if (nSel >= 0 && nSel < mDeviceListCtrl.GetItemCount())
	{
		CString strID = mDeviceListCtrl.GetItemText(nSel, 0);
		DWORD dwValue = mDeviceListCtrl.GetItemData(nSel);

		if (0 == strcmp(mLastDeviceID, "\0"))
		{
			char peerUUID[128] = { 0 };
			WCharToMByte(strID.GetBuffer(strID.GetLength()), peerUUID, sizeof(peerUUID) / sizeof(peerUUID[0]));

			//if (NULL == mLocalRTCDevice.pRemoteRenderThread)
			//{
			//	CreateOSThread(&mLocalRTCDevice.pRemoteRenderThread, __RemoteVideoRenderThread, this, 0);
			//}

			char serverIP[64] = { 0 };
			char serverPort[32] = { 0 };
			int isSecure = 0;
			char deviceID[64] = { 0 };

			GetEditText(serverIP, sizeof(serverIP) / sizeof(serverIP[0]), pEdtServerIP);
			GetEditText(serverPort, sizeof(serverPort) / sizeof(serverPort[0]), pEdtServerPort);
			isSecure = pChkSSL->GetCheck();

			EasyRTC_Device_Create(&mLocalRTCDevice.easyRTCCallerHandle, serverIP, atoi(serverPort), isSecure, NULL, __EasyRTC_Data_Callback, this);
			EasyRTC_Device_SetChannelInfo(mLocalRTCDevice.easyRTCCallerHandle,
				(EASYRTC_CODEC)mLocalRTCDevice.videoCodecID, EASYRTC_CODEC_MULAW);// (EASYRTC_CODEC)mLocalRTCDevice.audioCodecID);

			EasyRTC_Caller_Connect(mLocalRTCDevice.easyRTCCallerHandle, peerUUID);

			//mDeviceListCtrl.SetButtonText(nSel, _T("挂断"));
			//mDeviceListCtrl.SetItemData(nSel, 1);
			memset(mLastDeviceID, 0x0, sizeof(mLastDeviceID));

			strcpy(mLastDeviceID, peerUUID);

			char szLog[128] = { 0 };
			sprintf(szLog, "呼叫设备[%s]...\n", peerUUID);
			OnLog(0, (LPARAM)szLog);
		}
		else
		{
			mDeviceListCtrl.SetButtonText(nSel, _T("呼叫"));
			mDeviceListCtrl.SetItemData(nSel, 0);

			resetLastDeviceId = true;
		}
	}

	UpdateCallState(resetLastDeviceId);

	return 0;
}

void	CEasyRTCDeviceWinDlg::UpdateCallState(bool reset)			// 在设备列表中更新呼叫状态
{
	if (reset)
	{
		memset(mLastDeviceID, 0x00, sizeof(mLastDeviceID));
	}

	int nCount = mDeviceListCtrl.GetItemCount();
	for (int i = 0; i < nCount; i++)
	{
		CString strID = mDeviceListCtrl.GetItemText(i, 0);

		char peerUUID[128] = { 0 };
		WCharToMByte(strID.GetBuffer(strID.GetLength()), peerUUID, sizeof(peerUUID) / sizeof(peerUUID[0]));

		if (0 == strcmp(mLastDeviceID, peerUUID))
		{
			mDeviceListCtrl.SetButtonText(i, _T("挂断"));
		}
		else
		{
			mDeviceListCtrl.SetButtonText(i, _T("呼叫"));
		}
	}
}

LRESULT CEasyRTCDeviceWinDlg::OnUpdateVideoWindow(WPARAM, LPARAM)
{
	if (pDlgVideoRemote)	pDlgVideoRemote->Invalidate();

	EnableTextMessage(FALSE);

	return 0;
}

void CEasyRTCDeviceWinDlg::OnBnClickedButtonSendMessage()
{
	char message[1024] = { 0 };
	GetEditText(message, sizeof(message) - 1, pEdtSendText);

	int msgLen = (int)strlen(message);

	if (msgLen < 1)	return;

	char log[2048] = { 0 };
	strcpy(log, "[发送]: ");
	int len = (int)strlen(log);
	memcpy(log + len, message, msgLen);
	strcat(log, "\n");
	OnLog(0, (LPARAM)log);		// 加入到本地消息框中

	if (NULL != mLocalRTCDevice.easyRTCCallerHandle)
	{
		EasyRTC_Device_SendCustomData(mLocalRTCDevice.easyRTCCallerHandle, NULL, 0, message, msgLen);
	}
	else
	{
		EasyRTC_Device_SendCustomData(mLocalRTCDevice.easyRTCDeviceHandle, NULL, 0, message, msgLen);
	}

	pEdtSendText->SetWindowTextW(_T(""));
}


LRESULT CEasyRTCDeviceWinDlg::OnCallEnd(WPARAM, LPARAM)
{
	OnBnClickedButtonCallEnd();
	return 0;
}

void CEasyRTCDeviceWinDlg::OnBnClickedButtonCallEnd()
{
	if (0 == strcmp(mPeerUUID, "\0"))
	{
		OnClickCallButton(99999, 0);
		return;
	}

	int ret = EasyRTC_Device_Hangup(mLocalRTCDevice.easyRTCDeviceHandle, mPeerUUID);
	if (ret != 0)
	{
		ret = EasyRTC_Device_Hangup(mLocalRTCDevice.easyRTCCallerHandle, mPeerUUID);
	}

	if (ret == 0)
	{
		EnableCallEnd(FALSE);
		EnableTextMessage(FALSE);
		OnUpdateVideoWindow(0, 0);
		
		PostMessageW(WM_EASY_RTC_CALLBACK_LOG, 0, (LPARAM)"已挂断.\n");
	}
}

void	CEasyRTCDeviceWinDlg::EnableTextMessage(BOOL bEnable)
{
	if (pEdtSendText)	pEdtSendText->EnableWindow(bEnable);
	if (pBtnSendMessage)	pBtnSendMessage->EnableWindow(bEnable);
}
void	CEasyRTCDeviceWinDlg::EnableCallEnd(BOOL bEnable)
{
	if (pBtnCallEnd)	pBtnCallEnd->EnableWindow(bEnable);
}


void CEasyRTCDeviceWinDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值

	if (nIDEvent == REFRESH_DEVICE_TIMER_ID)
	{
		PostMessage(WM_EASY_RTC_REFRESH_DEVICE_LIST);	// 刷新列表
	}

	CDialogEx::OnTimer(nIDEvent);
}
