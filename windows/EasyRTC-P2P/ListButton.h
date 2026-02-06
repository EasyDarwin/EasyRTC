// ListButton.h
#pragma once
#include <afxwin.h>

class CListButton : public CButton
{
    DECLARE_DYNAMIC(CListButton)
public:
    CListButton();
    virtual ~CListButton();
    
    virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
    void SetText(CString strText) { m_strText = strText; }
    
protected:
    DECLARE_MESSAGE_MAP()
    
private:
    CString m_strText;
    BOOL m_bHover;
    BOOL m_bPressed;
    CFont   mFont;
    
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
};