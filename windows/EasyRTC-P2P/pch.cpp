// pch.cpp: 与预编译标头对应的源文件

#include "pch.h"

// 当使用预编译的头时，需要使用此源文件，编译才能成功。

#include<iostream>
#include "strmif.h"
//#include <initguid.h>


#pragma comment(lib, "setupapi.lib")
#define VI_MAX_CAMERAS 20
DEFINE_GUID(CLSID_SystemDeviceEnum, 0x62be5d10, 0x60eb, 0x11d0, 0xbd, 0x3b, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86);
//DEFINE_GUID(CLSID_VideoInputDeviceCategory, 0x860bb310, 0x5d01, 0x11d0, 0xbd, 0x3b, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86);
DEFINE_GUID(IID_ICreateDevEnum, 0x29840822, 0x5b84, 0x11d0, 0xbd, 0x3b, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86);

#include <dshow.h>
#include <uuids.h>
#pragma comment(lib, "strmiids.lib")



bool MByteToWChar(LPCSTR lpcszStr, LPWSTR lpwszStr, DWORD dwSize)
{
	// Get the required size of the buffer that receives the Unicode
	// string.
	DWORD dwMinSize;
	dwMinSize = MultiByteToWideChar(CP_ACP, 0, lpcszStr, -1, NULL, 0);

	if (dwSize < dwMinSize)
	{
		return false;
	}

	// Convert headers from ASCII to Unicode.
	MultiByteToWideChar(CP_ACP, 0, lpcszStr, -1, lpwszStr, dwMinSize);
	return true;
}
bool WCharToMByte(LPCWSTR lpcwszStr, LPSTR lpszStr, DWORD dwSize)
{
	DWORD dwMinSize;
	dwMinSize = WideCharToMultiByte(CP_OEMCP, NULL, lpcwszStr, -1, NULL, 0, NULL, FALSE);
	if (dwSize < dwMinSize)
	{
		return false;
	}
	WideCharToMultiByte(CP_OEMCP, NULL, lpcwszStr, -1, lpszStr, dwSize, NULL, FALSE);
	return true;
}

void GetEditText(char* out, int size, CEdit* pEdt)
{
	if (NULL == pEdt || NULL == out)		return;
	wchar_t wszURL[1024] = { 0 };
	pEdt->GetWindowText(wszURL, sizeof(wszURL));
	WCharToMByte(wszURL, out, size);
}
void SetEditText(CEdit* pEdt, const char* str)
{
	wchar_t wszText[512] = { 0 };
	MByteToWChar(str, wszText, sizeof(wszText) / sizeof(wszText[0]));

	pEdt->SetWindowTextW(wszText);
}
void SetEditText(CEdit* pEdt, const int val)
{
	wchar_t wszText[512] = { 0 };
	wsprintf(wszText, L"%d", val);

	pEdt->SetWindowTextW(wszText);
}


#define MAX_FRIENDLY_NAME_LENGTH	128
#define MAX_MONIKER_NAME_LENGTH		256
LOCAL_DEVICE_MAP LocalVideoDeviceMap;
LOCAL_DEVICE_MAP LocalAudioDeviceMap;

typedef struct tagVIDEOINFOHEADER2 {
	RECT rcSource;
	RECT rcTarget;
	DWORD dwBitRate;
	DWORD dwBitErrorRate;
	REFERENCE_TIME AvgTimePerFrame;
	DWORD dwInterlaceFlags;
	DWORD dwCopyProtectFlags;
	DWORD dwPictAspectRatioX;
	DWORD dwPictAspectRatioY;
	DWORD dwReserved1;
	DWORD dwReserved2;
	BITMAPINFOHEADER bmiHeader;
} VIDEOINFOHEADER2;
void GetResolution(IMoniker* pMoniker, MEDIA_PROPERTY_VECTOR* pVector)
{
	USES_CONVERSION;
	HRESULT hr = NULL;
	IBaseFilter* pFilter;
	hr = pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void**)&pFilter);
	if (!pFilter)
	{
		return;
	}

	IEnumPins* pinEnum = NULL;
	IPin* pin = NULL;
	if (FAILED(pFilter->EnumPins(&pinEnum)))
	{
		pinEnum->Release();
		return;
	}

	pinEnum->Reset();
	ULONG pinFetched = 0;
	while (SUCCEEDED(pinEnum->Next(1, &pin, &pinFetched)) && pinFetched)
	{
		if (!pin)
		{
			continue;
		}

		PIN_INFO pinInfo;
		if (FAILED(pin->QueryPinInfo(&pinInfo)))
		{
			continue;
		}

		if (pinInfo.dir != PINDIR_OUTPUT)
		{
			continue;
		}

		IEnumMediaTypes* mtEnum = NULL;
		AM_MEDIA_TYPE* mt = NULL;
		if (FAILED(pin->EnumMediaTypes(&mtEnum)))
		{
			break;
		}

		mtEnum->Reset();
		ULONG mtFetched = 0;

		while (SUCCEEDED(mtEnum->Next(1, &mt, &mtFetched)) && mtFetched)
		{
			BITMAPINFOHEADER* bmi = NULL;
			int avgTime;

			if (mt->formattype == FORMAT_VideoInfo)
			{
				if (mt->cbFormat >= sizeof(VIDEOINFOHEADER))
				{
					VIDEOINFOHEADER* pVih = reinterpret_cast<VIDEOINFOHEADER*>(mt->pbFormat);
					bmi = &(pVih->bmiHeader);
					avgTime = pVih->AvgTimePerFrame;
				}
			}
			else if (mt->formattype == FORMAT_VideoInfo2)
			{
				if (mt->cbFormat >= sizeof(VIDEOINFOHEADER2))
				{
					VIDEOINFOHEADER2* pVih = reinterpret_cast<VIDEOINFOHEADER2*>(mt->pbFormat);
					bmi = &(pVih->bmiHeader);
					avgTime = pVih->AvgTimePerFrame;
				}
			}
			if (bmi)
			{
				int echo = false;

				printf("%dx%d", bmi->biWidth, bmi->biHeight);

				char str[32] = { 0 };
				sprintf(str, "%dx%d", bmi->biWidth, bmi->biHeight);
				if (pVector)	pVector->push_back(str);

				//for (int i = 0; i < ResolutionList.size(); i++)
				//{
				//	if (ResolutionList[i].width == bmi->biWidth && ResolutionList[i].height == bmi->biHeight)
				//	{
				//		echo = true;
				//		break;
				//	}
				//}
				//if (!echo)
				//{
				//	StuResolution stuResolution;
				//	stuResolution.width = bmi->biWidth;
				//	stuResolution.height = bmi->biHeight;
				//	ResolutionList.append(stuResolution);
				//	qDebug() << "llt cam Resolution:" << bmi->biWidth << "x" << bmi->biHeight;
				//}
			}
		}
		pin->Release();
	}
	return;
}


//列出硬件设备
int listDevices(LOCAL_DEVICE_MAP& list, REFGUID guidValue)
{
	ICreateDevEnum* pDevEnum = NULL;
	IEnumMoniker* pEnum = NULL;
	int deviceCounter = 0;


	HRESULT hr = CoCreateInstance(
		CLSID_SystemDeviceEnum,
		NULL,
		CLSCTX_INPROC_SERVER,
		IID_ICreateDevEnum,
		reinterpret_cast<void**>(&pDevEnum)
	);

	if (SUCCEEDED(hr))
	{
		hr = pDevEnum->CreateClassEnumerator(guidValue, &pEnum, 0);
		if (hr == S_OK) {

			IMoniker* pMoniker = NULL;
			while (pEnum->Next(1, &pMoniker, NULL) == S_OK)
			{
				IPropertyBag* pPropBag;
				hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag,
					(void**)(&pPropBag));

				if (FAILED(hr)) {
					pMoniker->Release();
					continue; // Skip this one, maybe the next one will work.
				}

				VARIANT varName;
				VariantInit(&varName);
				hr = pPropBag->Read(L"Description", &varName, 0);
				if (FAILED(hr))
				{
					hr = pPropBag->Read(L"FriendlyName", &varName, 0);
				}

				if (SUCCEEDED(hr))
				{
					hr = pPropBag->Read(L"FriendlyName", &varName, 0);
					//int count = 0;
					//char tmp[255] = { 0 };
					//while (varName.bstrVal[count] != 0x00 && count < 255)
					//{
					//	tmp[count] = (char)varName.bstrVal[count];
					//	count++;
					//}

					MEDIA_PROPERTY_VECTOR	mediaPropertyVector;

					if (guidValue == CLSID_VideoInputDeviceCategory)
					{
						//GetResolution(pMoniker, &mediaPropertyVector);
					}
					else
					{
						LPOLESTR pOleDisplayName = reinterpret_cast<LPOLESTR>(CoTaskMemAlloc(MAX_MONIKER_NAME_LENGTH * 2));
						if (pOleDisplayName != NULL)
						{
							hr = pMoniker->GetDisplayName(NULL, NULL, &pOleDisplayName);
							if (SUCCEEDED(hr))
							{
								// 拷贝设备Moniker名到name.MonikerName
								wchar_t wszMonikerName[MAX_MONIKER_NAME_LENGTH] = { 0 };
								StringCchCopy(wszMonikerName, MAX_MONIKER_NAME_LENGTH, pOleDisplayName);
								//vectorDevices.push_back(name);

								char szMonikerName[MAX_MONIKER_NAME_LENGTH] = { 0 };
								WCharToMByte(wszMonikerName, szMonikerName, sizeof(szMonikerName) / sizeof(szMonikerName[0]));

								mediaPropertyVector.push_back(szMonikerName);
							}
							CoTaskMemFree(pOleDisplayName);
						}
					}

					char szDeviceName[MAX_FRIENDLY_NAME_LENGTH] = { 0 };


					//char* lptstrValue;

					//Value.vt = VT_BSTR;
					//USES_CONVERSION;
					LPTSTR lptstrValue = W2T(varName.bstrVal);
					WCharToMByte(lptstrValue, szDeviceName, sizeof(szDeviceName) / sizeof(szDeviceName[0]));
					//memcpy(szDeviceName, lptstrValue, (int)wcslen(lptstrValue));

					list.insert(LOCAL_DEVICE_MAP::value_type(szDeviceName, mediaPropertyVector));
				}

				pPropBag->Release();
				pPropBag = NULL;
				pMoniker->Release();
				pMoniker = NULL;

				deviceCounter++;
			}

			pDevEnum->Release();
			pDevEnum = NULL;
			pEnum->Release();
			pEnum = NULL;
		}
	}
	return deviceCounter;
}


