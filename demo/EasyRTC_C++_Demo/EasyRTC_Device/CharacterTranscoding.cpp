#include "CharacterTranscoding.h"

#include <stdio.h>


#include "iconv.h"
#pragma comment(lib, "iconv.lib")


bool	LIB_CHARACTER_TRANSCODING_API LIB_APICALL	libCharacterTranscoding_IsUTF8(const char* str)
{
  unsigned int nBytes = 0;//UFT8����1-6���ֽڱ���,ASCII��һ���ֽ�
  unsigned char chr = *str;
  bool bAllAscii = true;
  for (unsigned int i = 0; str[i] != '\0'; ++i){
    chr = *(str + i);
    //�ж��Ƿ�ASCII����,�������,˵���п�����UTF8,ASCII��7λ����,���λ���Ϊ0,0xxxxxxx
    if (nBytes == 0 && (chr & 0x80) != 0){
      bAllAscii = false;
    }
    if (nBytes == 0) {
      //�������ASCII��,Ӧ���Ƕ��ֽڷ�,�����ֽ���
      if (chr >= 0x80) {
        if (chr >= 0xFC && chr <= 0xFD){
          nBytes = 6;
        }
        else if (chr >= 0xF8){
          nBytes = 5;
        }
        else if (chr >= 0xF0){
          nBytes = 4;
        }
        else if (chr >= 0xE0){
          nBytes = 3;
        }
        else if (chr >= 0xC0){
          nBytes = 2;
        }
        else{
          return false;
        }
        nBytes--;
      }
    }
    else{
      //���ֽڷ��ķ����ֽ�,ӦΪ 10xxxxxx
      if ((chr & 0xC0) != 0x80){
        return false;
      }
      //����Ϊ��Ϊֹ
      nBytes--;
    }
  }
  //Υ��UTF8�������
  if (nBytes != 0) {
    return false;
  }
  if (bAllAscii){ //���ȫ������ASCII, Ҳ��UTF8
    return true;
  }
  return true;
}
bool	LIB_CHARACTER_TRANSCODING_API LIB_APICALL	libCharacterTranscoding_IsGBK(const char* str)
{
  unsigned int nBytes = 0;//GBK����1-2���ֽڱ���,�������� ,Ӣ��һ��
  unsigned char chr = *str;
  bool bAllAscii = true; //���ȫ������ASCII,
  for (unsigned int i = 0; str[i] != '\0'; ++i){
    chr = *(str + i);
    if ((chr & 0x80) != 0 && nBytes == 0){// �ж��Ƿ�ASCII����,�������,˵���п�����GBK
      bAllAscii = false;
    }
    if (nBytes == 0) {
      if (chr >= 0x80) {
        if (chr >= 0x81 && chr <= 0xFE){
          nBytes = +2;
        }
        else{
          return false;
        }
        nBytes--;
      }
    }
    else{
      if (chr < 0x40 || chr>0xFE){
        return false;
      }
      nBytes--;
    }//else end
  }
  if (nBytes != 0) {   //Υ������
    return false;
  }
  if (bAllAscii){ //���ȫ������ASCII, Ҳ��GBK
    return true;
  }
  return false;
}


int		LIB_CHARACTER_TRANSCODING_API LIB_APICALL	libCharacterTranscoding_UTF8ToGB2312(char* sOut, int iMaxOutLen, const char* sIn, int iInLen)
{
    if (NULL == sOut)       return 0;
    if (NULL == sIn)        return 0;
    if (0 == strcmp(sIn, "\0")) return 0;

    //���ΪGBK, ��ֱ�Ӹ�ֵ
    if (libCharacterTranscoding_IsGBK(sIn))
    {
        if (iMaxOutLen > iInLen)
        {
            strcpy(sOut, sIn);
            return iInLen;
        }

        strncpy(sOut, sIn, iMaxOutLen - 1);
        return iMaxOutLen - 1;
    }


    char *pIn = (char *)sIn;
    char *pOut = sOut;
    size_t ret;
    size_t iLeftLen=iMaxOutLen;
    iconv_t cd;

    cd = iconv_open("gb2312", "utf-8");
    if (cd == (iconv_t) - 1)
    {
        return -1;
    }
    size_t iSrcLen=iInLen;
	#ifdef _WIN32
#ifdef _DEBUG_
    ret = iconv(cd, (const char **)&pIn,&iSrcLen, &pOut,&iLeftLen);
#else
    ret = iconv(cd, (char**)&pIn, &iSrcLen, &pOut, &iLeftLen);
#endif

	#else
    ret = iconv(cd, ( char **)&pIn,&iSrcLen, &pOut,&iLeftLen);
	#endif
    if (ret == (size_t) - 1)
    {
        iconv_close(cd);
        return -1;
    }

    iconv_close(cd);

    return (iMaxOutLen - iLeftLen);
}

int		LIB_CHARACTER_TRANSCODING_API LIB_APICALL	libCharacterTranscoding_GB2312ToUTF8(char* sOut, int iMaxOutLen, const char* sIn, int iInLen)
{
    if (NULL == sOut)       return 0;
    if (NULL == sIn)        return 0;
    if (0 == strcmp(sIn, "\0")) return 0;

    //���ΪUTF8, ��ֱ�Ӹ�ֵ
    if (libCharacterTranscoding_IsUTF8(sIn))
    {
        if (iMaxOutLen > iInLen)
        {
            strcpy(sOut, sIn);
            return iInLen;
        }

        strncpy(sOut, sIn, iMaxOutLen - 1);
        return iMaxOutLen - 1;
    }

    char *pIn = (char *)sIn;
    char *pOut = sOut;
    size_t ret;
    size_t iLeftLen=iMaxOutLen;
    iconv_t cd;

    cd = iconv_open("utf-8", "gb2312");
    if (cd == (iconv_t) - 1)
    {
        return -1;
    }
    size_t iSrcLen=iInLen;
	#ifdef _WIN32
#ifdef _DEBUG_
    ret = iconv(cd, (const char **)&pIn,&iSrcLen, &pOut,&iLeftLen);
#else
    ret = iconv(cd, (char**)&pIn, &iSrcLen, &pOut, &iLeftLen);
#endif
	#else
    ret = iconv(cd, ( char **)&pIn,&iSrcLen, &pOut,&iLeftLen);
	#endif
    if (ret == (size_t) - 1)
    {
        iconv_close(cd);
        return -1;
    }

    iconv_close(cd);

    return (iMaxOutLen - iLeftLen);
}
