// CDlgVideoRender.cpp: 实现文件
//

#include "pch.h"
#include "EasyRTC_Device.h"
#include "CDlgVideoRender.h"
#include "afxdialogex.h"


// CDlgVideoRender 对话框

IMPLEMENT_DYNAMIC(CDlgVideoRender, CDialogEx)

CDlgVideoRender::CDlgVideoRender(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_VIDEO_RENDER, pParent)
{

}

CDlgVideoRender::~CDlgVideoRender()
{
}

void CDlgVideoRender::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CDlgVideoRender, CDialogEx)
END_MESSAGE_MAP()


// CDlgVideoRender 消息处理程序
