// pch.h: 这是预编译标头文件。
// 下方列出的文件仅编译一次，提高了将来生成的生成性能。
// 这还将影响 IntelliSense 性能，包括代码完成和许多代码浏览功能。
// 但是，如果此处列出的文件中的任何一个在生成之间有更新，它们全部都将被重新编译。
// 请勿在此处添加要频繁更新的文件，这将使得性能优势无效。

#ifndef PCH_H
#define PCH_H

// 添加要在此处预编译的标头
#include "framework.h"
#include <map>
#include <vector>
#include <string>
using namespace std;


#define __SetNull(x)    {x=NULL;}

#define __CREATE_WINDOW(_x, _class, _id) {if (NULL == _x) {_x = (_class*)GetDlgItem(_id);}}
#define __MOVE_WINDOW(x, _rect)	{if (NULL != x) {x->MoveWindow(&_rect);}}
#define __MOVE_WINDOW_INVALIDATE(x, _rect)	{if (NULL != x) {x->MoveWindow(&_rect); x->Invalidate();}}
#define __DELETE_WINDOW(x)	{if (NULL != x) {x->DestroyWindow(); delete x; x=NULL;}}

bool MByteToWChar(LPCSTR lpcszStr, LPWSTR lpwszStr, DWORD dwSize);
bool WCharToMByte(LPCWSTR lpcwszStr, LPSTR lpszStr, DWORD dwSize);

void GetEditText(char *out, int size, CEdit *pEdt);
void SetEditText(CEdit* pEdt, const char* str);
void SetEditText(CEdit* pEdt, const int val);

typedef vector<string>			MEDIA_PROPERTY_VECTOR;
typedef map<string, MEDIA_PROPERTY_VECTOR>       LOCAL_DEVICE_MAP;
extern LOCAL_DEVICE_MAP LocalVideoDeviceMap;
extern LOCAL_DEVICE_MAP LocalAudioDeviceMap;
int listDevices(LOCAL_DEVICE_MAP& list, REFGUID guidValue);

#endif //PCH_H
