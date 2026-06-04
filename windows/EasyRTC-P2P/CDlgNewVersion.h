#pragma once


// CDlgNewVersion 对话框

class CDlgNewVersion : public CDialogEx
{
	DECLARE_DYNAMIC(CDlgNewVersion)

public:
	CDlgNewVersion(const char *versionInfo, CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CDlgNewVersion();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_NEW_VERSION };
#endif

public:
	CRichEditCtrl* pRedtNewVersionInfo;	// IDC_RICHEDIT2_NEW_VERSION_INFO


	char	*mLatestVersionInfo;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnClose();
};
