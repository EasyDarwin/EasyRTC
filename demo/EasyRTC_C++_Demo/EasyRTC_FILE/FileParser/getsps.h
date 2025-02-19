
#ifndef __GET_SPS_H__
#define __GET_SPS_H__


int MP4_GetH265VPSandSPSandPPS(char *pbuf, int bufsize, char *_vps, int *_vpslen, char *_sps, int *_spslen, char *_pps, int *_ppslen, char *_sei, int *_seilen, int* idrOffset, int* startCodeSize);
int MP4_GetH264SPSandPPS(char *pbuf, int bufsize, char *_sps, int *_spslen, char *_pps, int *_ppslen, char *_sei, int *_seilen, int *idrOffset);


#endif
