#ifndef __ES_PARSER_H__
#define __ES_PARSER_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
class ESParser
{
	typedef struct __BUFF_T
	{
		char		*pbuf;
		int			bufsize;
		int			bufpos;
	}BUFF_T;

	typedef struct __ES_PARSER_T
	{
		BUFF_T		inBuff;
		BUFF_T		outBuff;
		int					startCodeOffset;
		int					procOffset;

		
	}ES_PARSER_T;

public:
	ESParser(void);
	~ESParser(void);

	bool		GetFrame(char *in_buf, int in_bufsize, char **out_pbuf, int *out_bufsize, int *out_frameType);

protected:
	ES_PARSER_T esParser;
	unsigned int	videoCodec;

	int		BUFF_Malloc(BUFF_T *pbuf, int bufsize);
	int		BUFF_AddData(BUFF_T *pbuf, char *data, int datasize);
	int		BUFF_Free(BUFF_T *pbuf);

};


#endif
