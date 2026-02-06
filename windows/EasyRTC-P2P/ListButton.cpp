// ListButton.cpp
#include "pch.h"
#include "ListButton.h"

IMPLEMENT_DYNAMIC(CListButton, CButton)

CListButton::CListButton() 
    : m_bHover(FALSE)
    , m_bPressed(FALSE)
{
    mFont.CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Arial"));
}

CListButton::~CListButton()
{
    mFont.DeleteObject();
}

BEGIN_MESSAGE_MAP(CListButton, CButton)
    ON_WM_MOUSEMOVE()
    ON_WM_SETCURSOR()
END_MESSAGE_MAP()

void CListButton::OnMouseMove(UINT nFlags, CPoint point)
{
    CRect rect;
    GetClientRect(&rect);
    
    if (rect.PtInRect(point))
    {
        if (!m_bHover)
        {
            m_bHover = TRUE;
            Invalidate();
        }
    }
    else
    {
        if (m_bHover)
        {
            m_bHover = FALSE;
            Invalidate();
        }
    }
    
    CButton::OnMouseMove(nFlags, point);
}

BOOL CListButton::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    ::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_HAND));
    return TRUE;
}

void CListButton::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);
    CRect rect = lpDrawItemStruct->rcItem;
    
    // 保存DC状态
    int nSavedDC = pDC->SaveDC();
    
    // 设置背景透明
    pDC->SetBkMode(TRANSPARENT);
    
    // 绘制按钮背景
    if (lpDrawItemStruct->itemState & ODS_SELECTED || m_bPressed)
    {
        // 按下状态
        pDC->FillSolidRect(rect, RGB(0, 120, 215));
        pDC->Draw3dRect(rect, RGB(0, 90, 180), RGB(0, 150, 255));
    }
    else if (m_bHover)
    {
        // 悬停状态
        pDC->FillSolidRect(rect, RGB(229, 241, 251));
        pDC->Draw3dRect(rect, RGB(0, 120, 215), RGB(0, 120, 215));
    }
    else
    {
        // 正常状态
        pDC->FillSolidRect(rect, RGB(240, 240, 240));
        pDC->Draw3dRect(rect, RGB(200, 200, 200), RGB(200, 200, 200));
    }
    
    // 绘制文本
    if (!m_strText.IsEmpty())
    {
        CFont* pOldFont = pDC->SelectObject(&mFont);
        
        COLORREF clrText = RGB(0, 0, 0);
        if (lpDrawItemStruct->itemState & ODS_SELECTED || m_bPressed)
        {
            clrText = RGB(255, 255, 255);
        }
        
        pDC->SetTextColor(clrText);
        
        // 居中绘制文本
        CSize textSize = pDC->GetTextExtent(m_strText);
        int x = rect.left + (rect.Width() - textSize.cx) / 2;
        int y = rect.top + (rect.Height() - textSize.cy) / 2;
        
        pDC->TextOut(x, y, m_strText);
        pDC->SelectObject(pOldFont);
    }
    
    // 恢复DC状态
    pDC->RestoreDC(nSavedDC);
}