// ButtonListCtrl.h
#pragma once
#include "ListButton.h"
extern "C"
{
#include "osmutex.h"
}
#define WM_CLICK_BUTTON     (WM_USER+5001)

class CButtonListCtrl : public CListCtrl
{
    DECLARE_DYNAMIC(CButtonListCtrl)
public:
    CButtonListCtrl();
    virtual ~CButtonListCtrl();
    
    // 添加带按钮的行
    int AddItem(LPCTSTR lpszText, LPCTSTR lpszButtonText, DWORD_PTR dwData = 0);
    
    // 设置按钮文本
    void SetButtonText(int nItem, LPCTSTR lpszButtonText);
    
    // 获取按钮文本
    CString GetButtonText(int nItem);

    void    UpdateButtonPosition();

    void    RemoveAllButtons();
    
    void SetRowHeight(int nHeight);

protected:
    DECLARE_MESSAGE_MAP()
    
    virtual void PreSubclassWindow();
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
    virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
    
    // 创建按钮
    void CreateButton(int nItem);
    
    // 更新按钮位置
    void UpdateButtonPos(int nItem);
    
    // 按钮点击处理
    void OnButtonClick(int nItem);
    
    // 获取按钮矩形
    CRect GetButtonRect(int nItem);
    
    afx_msg void OnDestroy();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg LRESULT OnButtonClickHandler(WPARAM wParam, LPARAM lParam);
    
private:
    CMap<int, int, CListButton*, CListButton*> m_mapButtons;  // 存储按钮控件
    CFont m_font;  // 按钮字体
    int m_nButtonWidth;  // 按钮宽度
    int m_nButtonHeight; // 按钮高度
    int m_nButtonMargin; // 按钮边距
};