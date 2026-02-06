// ButtonListCtrl.cpp
#include "pch.h"
#include "ButtonListCtrl.h"
#include <afxdialogex.h>

IMPLEMENT_DYNAMIC(CButtonListCtrl, CListCtrl)

CButtonListCtrl::CButtonListCtrl() 
    : m_nButtonWidth(80)
    , m_nButtonHeight(25)
    , m_nButtonMargin(5)
{
}

CButtonListCtrl::~CButtonListCtrl()
{
}

BEGIN_MESSAGE_MAP(CButtonListCtrl, CListCtrl)
    ON_WM_DESTROY()
    ON_WM_SIZE()
    ON_WM_HSCROLL()
    ON_WM_VSCROLL()
    ON_WM_DRAWITEM()
    ON_WM_MEASUREITEM()
    ON_WM_MEASUREITEM_REFLECT()
    ON_MESSAGE(WM_USER + 100, &CButtonListCtrl::OnButtonClickHandler)
END_MESSAGE_MAP()

void CButtonListCtrl::PreSubclassWindow()
{
    CListCtrl::PreSubclassWindow();
    
    // 必须设置所有者绘制样式
    ModifyStyle(0, LVS_OWNERDRAWFIXED);

    // 设置扩展样式
    SetExtendedStyle(GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    
    // 创建按钮字体
    LOGFONT lf = {0};
    GetFont()->GetLogFont(&lf);
    lf.lfHeight = -12;
    lstrcpy(lf.lfFaceName, _T("Microsoft YaHei"));
    m_font.CreateFontIndirect(&lf);
    
    //// 插入两列
    //InsertColumn(0, _T("项目名称"), LVCFMT_LEFT, 200);
    //InsertColumn(1, _T("操作"), LVCFMT_CENTER, 120);
    
    // 设置行高
    //SetItemHeight(0, m_nButtonHeight + m_nButtonMargin * 2);
    SetRowHeight(80);
}

BOOL CButtonListCtrl::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_LBUTTONDOWN)
    {
        //CPoint point(pMsg->lParam);
        //ScreenToClient(&point);
        CPoint point;
        ::GetCursorPos(&point);
        ScreenToClient(&point);
        
        // 检查是否点击了按钮
        for (int i = 0; i < GetItemCount(); i++)
        {
            CRect rect = GetButtonRect(i);
            if (rect.PtInRect(point))
            {
                OnButtonClick(i);
                return TRUE;
            }
        }
    }
    
    return CListCtrl::PreTranslateMessage(pMsg);
}

int CButtonListCtrl::AddItem(LPCTSTR lpszText, LPCTSTR lpszButtonText, DWORD_PTR dwData)
{
    int nItem = InsertItem(GetItemCount(), lpszText);
    if (nItem >= 0)
    {
        SetItemData(nItem, dwData);
        
        // 设置按钮文本
        CString strButtonText = lpszButtonText ? lpszButtonText : _T("点击");
        SetItemText(nItem, 1, strButtonText);
        
        // 创建按钮
        CreateButton(nItem);
    }
    
    return nItem;
}

void CButtonListCtrl::SetButtonText(int nItem, LPCTSTR lpszButtonText)
{
    if (nItem >= 0 && nItem < GetItemCount())
    {
        SetItemText(nItem, 1, lpszButtonText);
        
        CListButton* pButton = NULL;
        if (m_mapButtons.Lookup(nItem, pButton) && pButton)
        {
            pButton->SetText(lpszButtonText);
            pButton->Invalidate();
        }
    }
}

CString CButtonListCtrl::GetButtonText(int nItem)
{
    if (nItem >= 0 && nItem < GetItemCount())
    {
        return GetItemText(nItem, 1);
    }
    return _T("");
}

void CButtonListCtrl::CreateButton(int nItem)
{
    CListButton* pButton = NULL;
    if (m_mapButtons.Lookup(nItem, pButton))
    {
        if (pButton && ::IsWindow(pButton->m_hWnd))
        {
            pButton->DestroyWindow();
            delete pButton;
        }
        m_mapButtons.RemoveKey(nItem);
    }
    

    // 创建新按钮
    pButton = new CListButton();
    CRect rect = GetButtonRect(nItem);
    
    CString strButtonText = GetItemText(nItem, 1);
    if (strButtonText.IsEmpty())
        strButtonText = _T("点击");
    
    pButton->Create(strButtonText, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, rect, this, 1000 + nItem);
    pButton->SetFont(&m_font);
    pButton->SetText(strButtonText);
    
    m_mapButtons.SetAt(nItem, pButton);
}

void CButtonListCtrl::UpdateButtonPos(int nItem)
{
    CListButton* pButton = NULL;
    if (m_mapButtons.Lookup(nItem, pButton) && pButton)
    {
        int w = GetColumnWidth(1);
        CRect rect = GetButtonRect(nItem);
        rect.right = rect.left + w - 8;
        rect.top += 2;
        rect.bottom -= 3;
        pButton->MoveWindow(&rect);
    }
}

void CButtonListCtrl::OnButtonClick(int nItem)
{
    // 发送自定义消息通知父窗口
    //GetParent()->SendMessage(WM_COMMAND, 
    //    MAKEWPARAM(GetDlgCtrlID(), BN_CLICKED), 
    //    (LPARAM)m_hWnd);
    
    // 发送自定义消息，包含项目索引
    GetParent()->SendMessage(WM_CLICK_BUTTON, (WPARAM)nItem, (LPARAM)m_hWnd);
}

CRect CButtonListCtrl::GetButtonRect(int nItem)
{
    CRect rect;
    if (!GetSubItemRect(nItem, 1, LVIR_BOUNDS, rect))
    {
        rect.SetRectEmpty();
        return rect;
    }
    
    // 计算按钮位置（在第二列中居中）
    int nLeft = rect.left + m_nButtonMargin;
    int nTop = rect.top + (rect.Height() - m_nButtonHeight) / 2;
    int nRight = nLeft + m_nButtonWidth;
    int nBottom = nTop + m_nButtonHeight;
    
    // 确保按钮不超出列边界
    if (nRight > rect.right - m_nButtonMargin)
        nRight = rect.right - m_nButtonMargin;
    
    return CRect(nLeft, nTop, nRight, nBottom);
}

void CButtonListCtrl::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    if (lpDrawItemStruct->itemID == -1)
        return;
    
    CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);
    CRect rcItem = lpDrawItemStruct->rcItem;
    int nItem = lpDrawItemStruct->itemID;
    
    // 保存DC状态
    int nSavedDC = pDC->SaveDC();
    
    // 设置字体
    pDC->SelectObject(GetFont());
    
    // 绘制背景
    if (lpDrawItemStruct->itemState & ODS_SELECTED)
    {
        pDC->FillSolidRect(rcItem, GetSysColor(COLOR_HIGHLIGHT));
        pDC->SetTextColor(GetSysColor(COLOR_HIGHLIGHTTEXT));
    }
    else
    {
        pDC->FillSolidRect(rcItem, GetSysColor(COLOR_WINDOW));
        pDC->SetTextColor(GetSysColor(COLOR_WINDOWTEXT));
    }
    
    // 绘制第一列文本
    CRect rcText = rcItem;
    rcText.right = rcText.left + GetColumnWidth(0);
    rcText.DeflateRect(2, 0);
    
    CString strText = GetItemText(nItem, 0);
    pDC->DrawText(strText, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    
    // 绘制网格线
    pDC->SelectStockObject(BLACK_PEN);
    pDC->SelectStockObject(NULL_BRUSH);
    pDC->Rectangle(&rcItem);
    
    // 恢复DC状态
    pDC->RestoreDC(nSavedDC);
}

void CButtonListCtrl::MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct)
{
    lpMeasureItemStruct->itemHeight = m_nButtonHeight + m_nButtonMargin * 2;
}

void CButtonListCtrl::OnDestroy()
{
    // 清理按钮
    RemoveAllButtons();
    
    CListCtrl::OnDestroy();
}

void    CButtonListCtrl::UpdateButtonPosition()
{
    // 更新所有按钮位置
    for (int i = 0; i < GetItemCount(); i++)
    {
        UpdateButtonPos(i);
    }
}

void CButtonListCtrl::OnSize(UINT nType, int cx, int cy)
{
    CListCtrl::OnSize(nType, cx, cy);
    

}

void CButtonListCtrl::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    CListCtrl::OnHScroll(nSBCode, nPos, pScrollBar);
    
    // 更新所有按钮位置
    for (int i = 0; i < GetItemCount(); i++)
    {
        UpdateButtonPos(i);
    }
}

void CButtonListCtrl::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    CListCtrl::OnVScroll(nSBCode, nPos, pScrollBar);
    
    // 更新所有按钮位置
    for (int i = 0; i < GetItemCount(); i++)
    {
        UpdateButtonPos(i);
    }
}

LRESULT CButtonListCtrl::OnButtonClickHandler(WPARAM wParam, LPARAM lParam)
{
    int nItem = (int)wParam;
    // 这里可以添加按钮点击的具体处理逻辑
    TRACE(_T("按钮被点击，项目索引: %d\n"), nItem);
    return 0;
}

void    CButtonListCtrl::RemoveAllButtons()
{
    // 清理按钮
    POSITION pos = m_mapButtons.GetStartPosition();
    int nKey = 0;
    CListButton* pButton;
    while (pos != NULL)
    {
        m_mapButtons.GetNextAssoc(pos, nKey, pButton);
        if (pButton && ::IsWindow(pButton->m_hWnd))
        {
            pButton->DestroyWindow();
            delete pButton;
        }
    }

    m_mapButtons.RemoveAll();
}

void CButtonListCtrl::SetRowHeight(int nHeight)
{
    m_nButtonHeight = nHeight;
    CRect rcWin;
    GetWindowRect(&rcWin);
    WINDOWPOS wp;
    wp.hwnd = m_hWnd;
    wp.cx = rcWin.Width();
    wp.cy = rcWin.Height();
    wp.flags = SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER;
    SendMessage(WM_WINDOWPOSCHANGED, 0, (LPARAM)&wp);
}
