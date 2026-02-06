// CDlgVideo.cpp: 实现文件
//

#include "pch.h"
#include "EasyRTC-P2P.h"
#include "CDlgVideo.h"
#include "afxdialogex.h"


// CDlgVideo 对话框

IMPLEMENT_DYNAMIC(CDlgVideo, CDialogEx)

CDlgVideo::CDlgVideo(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_VIDEO, pParent)
{

}

CDlgVideo::~CDlgVideo()
{
}

void CDlgVideo::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CDlgVideo, CDialogEx)
END_MESSAGE_MAP()


// CDlgVideo 消息处理程序
