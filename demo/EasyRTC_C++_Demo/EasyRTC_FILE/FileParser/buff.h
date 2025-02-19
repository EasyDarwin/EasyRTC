
#ifndef __BUFF_H__
#define __BUFF_H__


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct __BUFF_T
{
    int     bufsize;
    int     bufpos;
    char    *pbuf;
}BUFF_T;

int BUFF_MALLOC(BUFF_T *pBuff, int bufsize);
int BUFF_CLEAR(BUFF_T *pBuff);
int BUFF_FREE(BUFF_T *pBuff);

int BUFF_ENQUEUE(BUFF_T *pBuff, char *buf, int size);


int BUFF_SAVE2FILE(BUFF_T *pBuff, char *filename);

#endif
