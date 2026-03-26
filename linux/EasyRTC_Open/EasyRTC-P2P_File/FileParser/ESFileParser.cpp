#include "ESFileParser.h"


ESFileParser::ESFileParser(void)
{
	memset(&esParser, 0x00, sizeof(ES_PARSER_T));
}


ESFileParser::~ESFileParser(void)
{
	if (NULL != esParser.fIn)
	{
		fclose(esParser.fIn);
		esParser.fIn = NULL;
	}
}


int		ESFileParser::OpenEsFile(const char *filename, int loop)
{
	esParser.fIn = fopen(filename, "rb");
	if (NULL == esParser.fIn)
	{
		printf("OpenFile fail: %s\n", filename);
		return -2;
	}

	int len = (int)strlen(filename);
	if (0 == strncmp(filename + len - 4, "h264", 4))
	{
		videoCodec = 0x1C;
	}
	else if (0 == strncmp(filename + len - 4, "h265", 4))
	{
		videoCodec = 0xAE;
	}

	esParser.loop = loop;
	return 0;
}

unsigned int	ESFileParser::GetVideoCodec()
{
	return videoCodec;
}

int		ESFileParser::ReadFrame(char **out_pbuf, int *out_bufsize, int *out_frameType)
{
	if (NULL == esParser.fIn)		return -1;

	int  nRet = -1;
	char pbuf[1024] = {0};
	int bufsize = sizeof(pbuf);
	while (! feof(esParser.fIn))
	{
		int readBytes = fread(pbuf, 1, bufsize, esParser.fIn);
		if (readBytes < bufsize)
		{

			break;
		}

		bool bRet = GetFrame(pbuf, readBytes, out_pbuf, out_bufsize, out_frameType);
		if (bRet)
		{
			nRet = 0;
			break;
		}
	}

	if (feof(esParser.fIn) && esParser.loop)
	{
		fseek(esParser.fIn, 0, SEEK_SET);
	}

	return nRet;
}
