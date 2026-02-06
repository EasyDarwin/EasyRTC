#include "buff.h"

int BUFF_MALLOC(BUFF_T *pBuff, int bufsize)
{
    if (NULL == pBuff)      return -1;
    if (bufsize < 1)            return -1;

    if (NULL == pBuff->pbuf)
    {
        pBuff->pbuf = (char *) malloc(bufsize);
        if (NULL == pBuff->pbuf)        return -2;

        pBuff->bufsize = bufsize;
    }
    memset(pBuff->pbuf, 0x00, pBuff->bufsize);
    pBuff->bufpos = 0;

    return 0;
}

int BUFF_CLEAR(BUFF_T *pBuff)
{
	if (NULL == pBuff)		return -1;

	if (NULL != pBuff->pbuf && pBuff->bufsize > 0)
	{
		memset(pBuff->pbuf, 0x00, pBuff->bufsize);
		pBuff->bufpos = 0;
	}
	return 0;
}

int BUFF_FREE(BUFF_T *pBuff)
{
    if (NULL == pBuff)      return -1;

    if (NULL != pBuff->pbuf)
    {
        free(pBuff->pbuf);
        pBuff->pbuf = NULL;
    }

    pBuff->bufpos = 0;
    pBuff->bufsize = 0;

    return 0;
}


int BUFF_ENQUEUE(BUFF_T *pBuff, char *buf, int size)
{
	int ret = -1;
	if (NULL == pBuff)			return -1;
	if (NULL == pBuff->pbuf)	return -1;
	if (size < 1)					return -1;

	if (pBuff->bufpos + size <= pBuff->bufsize)
	{
		memcpy(pBuff->pbuf+pBuff->bufpos, buf, size);
		pBuff->bufpos += size;
		ret = 0;
	}
	return ret;
}


int BUFF_SAVE2FILE(BUFF_T *pBuff, char *filename)
{
	FILE *f = NULL;
	int ret = -1;
	if (NULL == pBuff)			return -1;
	if (pBuff->bufpos < 1)		return -1;
	

	f = fopen(filename, "wb");
	if (NULL != f)
	{
		fwrite(pBuff->pbuf, 1, pBuff->bufpos, f);
		fclose(f);
	}

	return 0;
}