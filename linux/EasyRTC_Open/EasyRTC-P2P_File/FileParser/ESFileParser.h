#ifndef __ES_FILE_PARSER_H__
#define __ES_FILE_PARSER_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ESParser.h"

class ESFileParser : public ESParser
{
	typedef struct __ES_PARSER_T
	{
		FILE		*fIn;

		bool	loop;
		int		offset;
		int		totalSize;
	}ES_PARSER_T;
public:
	ESFileParser(void);
	~ESFileParser(void);

	int		OpenEsFile(const char *filename, int loop);
	int		ReadFrame(char **out_pbuf, int *out_bufsize, int *out_frameType);

	unsigned int	GetVideoCodec();
protected:
	ES_PARSER_T		esParser;
};

#endif
