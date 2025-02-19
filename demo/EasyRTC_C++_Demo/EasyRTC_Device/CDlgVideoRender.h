#pragma once


// CDlgVideoRender 对话框

class CDlgVideoRender : public CDialogEx
{
	DECLARE_DYNAMIC(CDlgVideoRender)

public:
	CDlgVideoRender(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CDlgVideoRender();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_VIDEO_RENDER };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
};
