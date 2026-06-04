// CDlgNewVersion.cpp: 实现文件
//

#include "pch.h"
#include "EasyRTC-P2P.h"
#include "CDlgNewVersion.h"
#include "afxdialogex.h"


// CDlgNewVersion 对话框

IMPLEMENT_DYNAMIC(CDlgNewVersion, CDialogEx)

CDlgNewVersion::CDlgNewVersion(const char* versionInfo, CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_NEW_VERSION, pParent)
{
	__SetNull(pRedtNewVersionInfo);

	mLatestVersionInfo = (char*)versionInfo;
}

CDlgNewVersion::~CDlgNewVersion()
{
}

void CDlgNewVersion::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CDlgNewVersion, CDialogEx)
	ON_WM_SHOWWINDOW()
	ON_WM_CLOSE()
END_MESSAGE_MAP()


// CDlgNewVersion 消息处理程序


void CDlgNewVersion::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialogEx::OnShowWindow(bShow, nStatus);

	// TODO: 在此处添加消息处理程序代码

	SetWindowText(L"有新版本");

	pRedtNewVersionInfo = (CRichEditCtrl*)GetDlgItem(IDC_RICHEDIT2_NEW_VERSION_INFO);

	wchar_t wszMsg[1024 * 2] = { 0 };
	MByteToWChar(mLatestVersionInfo, wszMsg, sizeof(wszMsg) / sizeof(wszMsg[0]));
	pRedtNewVersionInfo->SetSel(-1, -1);
	pRedtNewVersionInfo->ReplaceSel(wszMsg);
}


void CDlgNewVersion::OnClose()
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值

	::PostMessage(GetParent()->GetSafeHwnd(), WM_CLOSE, 0, 0);

	CDialogEx::OnClose();
}
