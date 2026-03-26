#include "ESParser.h"


ESParser::ESParser(void)
{
	memset(&esParser, 0x00, sizeof(esParser));
	esParser.startCodeOffset = -1;
	videoCodec = 0;
}


ESParser::~ESParser(void)
{
	BUFF_Free(&esParser.inBuff);
	BUFF_Free(&esParser.outBuff);
}

int		ESParser::BUFF_Malloc(BUFF_T *pbuf, int bufsize)
{
	if (NULL == pbuf->pbuf)
	{
		pbuf->bufsize = bufsize;
		pbuf->bufpos = 0;
		pbuf->pbuf = new char[bufsize];
		memset(pbuf->pbuf, 0x00, bufsize);
	}

	return 0;
}
int		ESParser::BUFF_Free(BUFF_T *pbuf)
{
	if (NULL != pbuf->pbuf)
	{
		delete []pbuf->pbuf;
	}
	memset(pbuf, 0x00, sizeof(BUFF_T));
	return 0;
}

int		ESParser::BUFF_AddData(BUFF_T *pbuf, char *data, int datasize)
{
	if (NULL == pbuf)		return -1;
	if (NULL == data || datasize < 1)		return -1;

	if (pbuf->bufpos + datasize > pbuf->bufsize)
	{
		char *tmp = NULL;
		if (NULL != pbuf->pbuf)
		{
			tmp = new char[pbuf->bufsize];
			memcpy(tmp, pbuf->pbuf, pbuf->bufpos);

			delete []pbuf->pbuf;
			pbuf->pbuf = NULL;
		}
		pbuf->bufsize += (datasize+1024 * 512);
		pbuf->pbuf = new char[pbuf->bufsize];
		if (NULL != tmp)
		{
			memcpy(pbuf->pbuf, tmp, pbuf->bufpos);
		}
	}
	memcpy(pbuf->pbuf+pbuf->bufpos, data, datasize);
	pbuf->bufpos += datasize;
	return 0;
}

bool		ESParser::GetFrame(char *in_buf, int in_bufsize, char **out_pbuf, int *out_bufsize, int *out_frameType)
{
	if (NULL == in_buf || in_bufsize<1)		return false;

	bool bGetFrame = false;
	BUFF_AddData(&esParser.inBuff, in_buf, in_bufsize);

	int procSize = 0;
	for (int i=esParser.procOffset; i<esParser.inBuff.bufpos-4; i++)
	{
		if (    ((unsigned char)esParser.inBuff.pbuf[i] == 0x00)     && ((unsigned char)esParser.inBuff.pbuf[i+1] == 0x00) &&
				((unsigned char)esParser.inBuff.pbuf[i+2] == 0x00) && ((unsigned char)esParser.inBuff.pbuf[i+3] == 0x01) )
		{

			bool skip = false;
			if (esParser.startCodeOffset >= 0)
			{
				int framesize = i - esParser.startCodeOffset;
				if (esParser.outBuff.bufsize < framesize)
				{
					int offset = esParser.outBuff.bufpos;
					if (offset > 0)
					{

						BUFF_T	tmpBuf;
						memset(&tmpBuf, 0x00, sizeof(BUFF_T));
						BUFF_Malloc(&tmpBuf, esParser.outBuff.bufsize);
						memcpy(tmpBuf.pbuf, esParser.outBuff.pbuf, esParser.outBuff.bufpos);
						tmpBuf.bufpos = esParser.outBuff.bufpos;

						BUFF_Free(&esParser.outBuff);
						BUFF_Malloc(&esParser.outBuff, framesize + 1024 * 1024);

						memcpy(esParser.outBuff.pbuf, tmpBuf.pbuf, tmpBuf.bufpos);
						esParser.outBuff.bufpos = tmpBuf.bufpos;

						BUFF_Free(&tmpBuf);
					}
					else
					{
						BUFF_Free(&esParser.outBuff);
						BUFF_Malloc(&esParser.outBuff, framesize + 1024 * 1024);
					}
				}

				int nalType = 0;
				if (videoCodec == 0x1C)
				{
					nalType = (esParser.inBuff.pbuf[esParser.startCodeOffset + 4] & 0x1F);
					if (nalType == 1 || nalType == 5)
					{
						bGetFrame = true;
					}
					else if (nalType == 6)
					{
						//esParser.outBuff.bufpos = 0;
						skip = true;
					}
				}
				else if (videoCodec == 0xAE)
				{
					unsigned char b1 = (unsigned char)esParser.inBuff.pbuf[esParser.startCodeOffset + 4];
					nalType = 1;
					if (b1 == 0x26 || b1 == 0x02)
					{
						if (b1 == 0x26)	nalType = 0;
						bGetFrame = true;
					}
				}

				if (!skip)
				{
#ifdef _DEBUG__
					FILE* f = fopen("1.txt", "wb");
					if (f)
					{
						fwrite(esParser.inBuff.pbuf+ esParser.startCodeOffset, 1, framesize, f);
						fclose(f);
					}
#endif
					BUFF_AddData(&esParser.outBuff, esParser.inBuff.pbuf + esParser.startCodeOffset, framesize);
				}

				if (bGetFrame)
				{
					if (NULL != out_pbuf)		*out_pbuf = esParser.outBuff.pbuf;
					if (NULL != out_bufsize)	*out_bufsize = esParser.outBuff.bufpos;
					if (NULL != out_frameType)	*out_frameType = (nalType==1?0:1);

					memmove(esParser.inBuff.pbuf, esParser.inBuff.pbuf+esParser.outBuff.bufpos, esParser.inBuff.bufpos - esParser.outBuff.bufpos);
					esParser.inBuff.bufpos -= esParser.outBuff.bufpos;
					procSize = 0;

					esParser.outBuff.bufpos = 0;
					esParser.startCodeOffset = -1;
				}
			}
			if (bGetFrame)		break;
			esParser.startCodeOffset = i;
		}
		procSize = i; 
	}

	esParser.procOffset = procSize;

	return bGetFrame;
}
