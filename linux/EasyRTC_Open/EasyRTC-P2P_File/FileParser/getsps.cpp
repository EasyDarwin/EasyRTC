#include "getsps.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>



//输入的pbuf必须包含start code(00 00 00 01)或者(00 00 01)

//找到尾且拷贝数据
int MP4_FindTailCopy(int frome, int to, char *pbuf, char *pDst, int *pDstLen, int dstSize)
{
	for (int n = frome; n < to; n++)
	{
		if ( ((unsigned char)pbuf[n] == 0x00 && (unsigned char)pbuf[n + 1] == 0x00 &&
				(unsigned char)pbuf[n + 2] == 0x00 && (unsigned char)pbuf[n + 3] == 0x01)		||
			 ((unsigned char)pbuf[n] == 0x00 && (unsigned char)pbuf[n + 1] == 0x00 &&
				//(unsigned char)pbuf[n + 2] == 0x01 )																||
				 (unsigned char)pbuf[n + 2] == 0x01 && (unsigned char)pbuf[n+3]!=0x90) ||
			(n+1==to))
		{
			*pDstLen = n - frome;
			if (*pDstLen >= dstSize)	return -1;
			memset(pDst, 0x00, dstSize);
			memcpy(pDst, pbuf + frome, *pDstLen);
			break;
		}
	}

	return 0;
}


int MP4_GetH265VPSandSPSandPPS(char *pbuf, int bufsize, char *_vps, int *_vpslen, char *_sps, int *_spslen, char *_pps, int *_ppslen, char *_sei, int *_seilen, int* idrOffset, int *startCodeSize)
{
	char vps[512]={0}, sps[512] = {0}, pps[128] = {0}, sei[128] = {0};
	int vpslen=0, spslen=0, ppslen=0, seilen=0, i=0, iStartPos=0, ret=-1;
	int iFoundVPS=0, iFoundSPS=0, iFoundPPS=0, iFoundSEI=0, iFoundIDR=0;
	if (NULL == pbuf || bufsize < 4)	return -1;

#ifdef _DEBUG_
	FILE *f = fopen("vpsspspps.txt", "wb");
	if (NULL != f)
	{
		fwrite(pbuf, 1, bufsize, f);
		fclose(f);
	}
#endif
	int startCode = 0;
	unsigned char *pData = (unsigned char*)pbuf;
	for (i = 0; i < bufsize; i++)
	{
		startCode = -1;
		if (pData[i] == 0x00 && pData[i + 1] == 0x00 && pData[i + 2] == 0x00 && pData[i + 3] == 0x01)
			startCode = 4;
		else if (pData[i] == 0x00 && pData[i + 1] == 0x00 && pData[i + 2] == 0x01)
		{
			startCode = 3;
			if (startCodeSize)	*startCodeSize = startCode;

			if (idrOffset && *idrOffset>0)	*idrOffset -= 1;
		}

		

		if(startCode > 0)
		{
			printf("Get StartCode: %d    0x%X\n", startCode, pbuf[i + startCode]);

			switch ((unsigned char)pbuf[i + startCode])
			{
			case 0x40:		//VPS
				{
					iStartPos = i + startCode;
					ret = MP4_FindTailCopy(iStartPos, bufsize, pbuf, vps, &vpslen, sizeof(vps));
					if (!ret)
					{
						iFoundVPS = 1;
						i = iStartPos + vpslen - 1;
					}
				}
				break;
			case 0x42:		//SPS
				{
					iStartPos = i + startCode;
					ret = MP4_FindTailCopy(iStartPos, bufsize, pbuf, sps, &spslen, sizeof(sps));
					if (!ret)
					{
						iFoundSPS = 1;
						i = iStartPos + spslen - 1;
					}
				}
				break;
			case 0x44:		//PPS
				{
					iStartPos = i + startCode;
					ret = MP4_FindTailCopy(iStartPos, bufsize, pbuf, pps, &ppslen, sizeof(pps));
					if (!ret)
					{
						iFoundPPS = 1;
						i = iStartPos + ppslen - 1;
					}	

					//if (idrOffset)	*idrOffset = i + 1 + startCode;
					if (idrOffset)	*idrOffset = i + startCode + (startCode == 4 ? 1 : 0);
					if (startCodeSize)	*startCodeSize = startCode;

					//printf("Get PPS.  startCode: %d\n", startCode);
				}
break;
			case 0x4E:		//SEI
			case 0x50:
			{
				iStartPos = i + startCode;
				ret = MP4_FindTailCopy(iStartPos, bufsize, pbuf, sei, &seilen, sizeof(sei));
				if (!ret)
				{
					iFoundSEI = 1;
					i = iStartPos + seilen - 1;
				}

				//if (idrOffset)	*idrOffset = i + 1 + startCode;
				if (idrOffset)	*idrOffset = i + startCode + (startCode == 4 ? 1 : 0);
				if (startCodeSize)	*startCodeSize = startCode;
			}
			break;
			case 0x26:
			{
				iFoundIDR = 1;
				iStartPos = i + startCode;
				if (startCodeSize)	*startCodeSize = startCode;

				break;
			}
			break;
			default:
			{
				iFoundIDR = 1;
				iStartPos = i + startCode;
			}
			break;
			}
		}

		if (iFoundIDR == 0x01)
		{
			if (i < 4)
			{
				i = 4;
				iFoundIDR = 0x00;
				continue;
			}

			break;
		}
	}

	if (iFoundVPS == 0x01)
	{
		if (vpslen > 0)
		{
			if (NULL != _vps)   memcpy(_vps, vps, vpslen);
			if (NULL != _vpslen)    *_vpslen = vpslen;
		}

		ret = 0;
	}

	if (iFoundSPS == 0x01)
	{
		if (spslen > 0)
		{
			if (NULL != _sps)   memcpy(_sps, sps, spslen);
			if (NULL != _spslen)    *_spslen = spslen;
		}

		ret = 0;
	}

	if (iFoundPPS == 0x01)
	{
		if (ppslen > 0)
		{
			if (NULL != _pps)   memcpy(_pps, pps, ppslen);
			if (NULL != _ppslen)    *_ppslen = ppslen;
		}
		ret = 0;
	}

	if (iFoundSEI == 0x01)
	{
		if (seilen > 0)
		{
			if (NULL != _sei)   memcpy(_sei, sei, seilen);
			if (NULL != _seilen)    *_seilen = seilen;
		}
		ret = 0;
	}

	return ret;
}



int MP4_GetH264SPSandPPS(char* pbuf, int bufsize, char* _sps, int* _spslen, char* _pps, int* _ppslen, char* _sei, int* _seilen, int* idrOffset)
{
	char sps[512] = { 0 }, pps[128] = { 0 }, sei[128] = { 0 };
	int spslen = 0, ppslen = 0, seilen = 0, i = 0, iStartPos = 0, ret = -1;
	int iFoundSPS = 0, iFoundPPS = 0, iFoundSEI = 0, iFoundIDR = 0;
	if (NULL == pbuf || bufsize < 4)	return -1;

#ifdef _DEBUG__
	FILE* f = fopen("1.txt", "wb");
	if (f)
	{
		fwrite(pbuf, 1, bufsize, f);
		fclose(f);
	}

#endif

	int startCodeOffset = -1;
	int offsetLen = 0;
	for (i = 0; i < bufsize; i++)
	{

		if ((unsigned char)pbuf[i] == 0x00 && (unsigned char)pbuf[i + 1] == 0x00 &&
			(unsigned char)pbuf[i + 2] == 0x00 && (unsigned char)pbuf[i + 3] == 0x01)
		{
			startCodeOffset = 4;
		}
		else if ((unsigned char)pbuf[i] == 0x00 && (unsigned char)pbuf[i + 1] == 0x00 &&
			(unsigned char)pbuf[i + 2] == 0x01)
		{
			startCodeOffset = 3;
		}

		if (startCodeOffset >= 0)
		{
			unsigned char naltype = ((unsigned char)pbuf[i + startCodeOffset] & 0x1F);
			if (naltype == 7)       //sps
			{
				iFoundSPS = 1;
				iStartPos = i + startCodeOffset;

				offsetLen += startCodeOffset;
				i += 1;
			}
			else if (naltype == 8)	//pps
			{
				//copy sps
				if (iFoundSPS == 0x01 && i > 4 && spslen < 1)
				{
					spslen = i - startCodeOffset;
					if (spslen > 256)	return -1;          //sps长度超出范围
					memset(sps, 0x00, sizeof(sps));
					memcpy(sps, pbuf + startCodeOffset, spslen);
				}

				iFoundPPS = 1;
				offsetLen += startCodeOffset;
				i += 1;
			}
			else if (naltype == 6)		//sei
			{
				if (iFoundPPS == 0x01 && i > 4 && ppslen<1)
				{
					ppslen = i - spslen - offsetLen;
					if (ppslen > 0 && ppslen < sizeof(pps))
					{
						memset(pps, 0x00, sizeof(pps));
						memcpy(pps, pbuf + spslen + offsetLen, ppslen);	//pps
					}
				}

				else if (iFoundSEI == 1 && seilen < 1)
				{
					seilen = i - spslen - offsetLen - ppslen;
					if (seilen > 0 && seilen < sizeof(sei))
					{
						memset(sei, 0x00, sizeof(sei));
						memcpy(sei, pbuf + spslen + offsetLen + ppslen, seilen);	//sei
					}
					else
					{
						seilen = 0;
					}
				}

				iFoundSEI = 1;
				offsetLen += startCodeOffset;
				i += 1;
			}
			else if (naltype == 5)	//idr
			{
				iFoundIDR = 1;

				if (iFoundPPS == 1 && iFoundSEI == 0 && ppslen<1)
				{
					ppslen = i - spslen - offsetLen;
					if (ppslen > 0 && ppslen < sizeof(pps))
					{
						memset(pps, 0x00, sizeof(pps));
						memcpy(pps, pbuf + spslen + offsetLen, ppslen);	//pps
					}
				}
				else if (iFoundSEI==1 && seilen<1)
				{
					seilen = i - spslen - offsetLen - ppslen;
					if (seilen > 0 && seilen < sizeof(sei))
					{
						memset(sei, 0x00, sizeof(sei));
						memcpy(sei, pbuf + spslen + offsetLen + ppslen, seilen);	//sei
					}
					else
					{
						seilen = 0;
					}
				}

				if (idrOffset)	*idrOffset = i + startCodeOffset;

				break;
			}
			else if (naltype == 1)	//Slice
			{
				iFoundIDR = 1;

				if (iFoundPPS == 1 && iFoundSEI == 0 && ppslen < 1)
				{
					ppslen = i - spslen - offsetLen;
					if (ppslen > 0 && ppslen < sizeof(pps))
					{
						memset(pps, 0x00, sizeof(pps));
						memcpy(pps, pbuf + spslen + offsetLen, ppslen);	//pps
					}
				}
				else if (iFoundSEI == 1)// && seilen < 1)
				{
					int offset = 0;
					if (seilen > 0)
					{
						offset = seilen;
						seilen = i - spslen - offsetLen - ppslen - seilen;
					}
					else
					{
						seilen = i - spslen - offsetLen - ppslen;
					}

					if (seilen > 0 && seilen < sizeof(sei))
					{
						memset(sei, 0x00, sizeof(sei));
						memcpy(sei, pbuf + spslen + offsetLen + ppslen + offset, seilen);	//sei
					}
					else
					{
						seilen = 0;
					}
				}
				break;




			}


			
			startCodeOffset = -1;

		}
		else
		{

		}
	}


	if (iFoundSPS == 0x01)
	{
		if (spslen < 1)
		{
			if (bufsize < sizeof(sps))
			{
				spslen = bufsize - 4;
				memset(sps, 0x00, sizeof(sps));
				memcpy(sps, pbuf + 4, spslen);
			}
		}

		if (spslen > 0)
		{
			if (NULL != _sps)   memcpy(_sps, sps, spslen);
			if (NULL != _spslen)    *_spslen = spslen;
		}

		ret = 0;
	}

	if (iFoundPPS == 0x01)
	{
		if (ppslen < 1)
		{
			if (bufsize < sizeof(pps))
			{
				ppslen = bufsize - 4;
				memset(pps, 0x00, sizeof(pps));
				memcpy(pps, pbuf + 4, ppslen);	//pps
			}
		}
		if (ppslen > 0)
		{
			if (NULL != _pps)   memcpy(_pps, pps, ppslen);
			if (NULL != _ppslen)    *_ppslen = ppslen;
		}
		ret = 0;
	}

	if (iFoundSEI == 0x01)
	{
		if (seilen > 0)
		{
			if (NULL != _sei)   memcpy(_sei, sei, seilen);
			if (NULL != _seilen)    *_seilen = seilen;
		}
		ret = 0;
	}



#ifdef _DEBUG__
	f = fopen("1sps.txt", "wb");
	if (f)
	{
		fwrite(sps, 1, spslen, f);
		fclose(f);
	}

	f = fopen("1pps.txt", "wb");
	if (f)
	{
		fwrite(pps, 1, ppslen, f);
		fclose(f);
	}

#endif

	return ret;
}


int MP4_GetH264SPSandPPS_(char *pbuf, int bufsize, char *_sps, int *_spslen, char *_pps, int *_ppslen, char *_sei, int *_seilen)
{
	char sps[512] = {0}, pps[128] = {0}, sei[128] = {0};
	int spslen=0, ppslen=0, seilen=0, i=0, iStartPos=0, ret=-1;
	int iFoundSPS=0, iFoundPPS=0, iFoundSEI=0, iFoundIDR=0;
	if (NULL == pbuf || bufsize < 4)	return -1;

	unsigned char *pData = (unsigned char*)pbuf;
	for (i = 0; i < bufsize; i++)
	{
		if ((unsigned char)pbuf[i] == 0x00 && (unsigned char)pbuf[i + 1] == 0x00 &&
			(unsigned char)pbuf[i + 2] == 0x00 && (unsigned char)pbuf[i + 3] == 0x01)
		{
			unsigned char naltype = (pData[i + 4] & 0x1F);
			if (naltype == 7)       //sps
			{
                iStartPos = i + 4;
				iFoundSPS = 1;
			}
			else if (naltype == 8)	//pps
			{ 
				//copy sps
				if (iFoundSPS == 0x01 && i > 4)
				{
					spslen = i - iStartPos;
					if (spslen > 256)	return -1;          //sps长度超出范围
					memset(sps, 0x00, sizeof(sps));
					memcpy(sps, pbuf + iStartPos, spslen);
				}

				iStartPos = i + 4;
				iFoundPPS = 1;
			}
			else if (naltype == 6)	//sei
			{
				if (iFoundPPS == 0x01 && i > 4)
				{
					ppslen = i - iStartPos;
					if (ppslen > 0 && ppslen < sizeof(pps))
					{
						memset(pps, 0x00, sizeof(pps));
						memcpy(pps, pbuf + iStartPos, ppslen);	//pps
					}
				}

				iFoundSEI = 1;
				iStartPos = i + 4;
			}
			else if (naltype == 5) //到达关键帧位置
			{
				if (iFoundSEI == 0x01 && i > 4)
				{
					seilen = i - iStartPos;
					if (seilen > 0 && seilen < sizeof(sei))
					{
						memset(sei, 0x00, sizeof(sei));
						memcpy(sei, pbuf + iStartPos, seilen);
					}
					else
					{
						seilen = 0;
					}
				}

				break;
			}
		}
	}

    if (iFoundSPS == 0x01)
    {
        if (spslen > 0)
        {
            if (NULL != _sps)   memcpy(_sps, sps, spslen);
            if (NULL != _spslen)    *_spslen = spslen;
        }
        ret = 0;
    }

    if (iFoundPPS == 0x01)
    {
        if (ppslen > 0)
        {
            if (NULL != _pps)   memcpy(_pps, pps, ppslen);
            if (NULL != _ppslen)    *_ppslen = ppslen;
        }
        ret = 0;
    }

	if (iFoundSEI == 0x01)
	{
        if (seilen > 0)
        {
            if (NULL != _sei)   memcpy(_sei, sei, seilen);
            if (NULL != _seilen)    *_seilen =seilen;
        }
        ret = 0;
	}

    return ret;
}

