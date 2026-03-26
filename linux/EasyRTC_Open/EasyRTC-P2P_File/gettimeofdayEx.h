#ifndef __GET_TIME_OF_DAY_EX_H__
#define __GET_TIME_OF_DAY_EX_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifndef _WIN32
#include <sys/time.h>
#endif

int gettimeofdayEx(struct timeval* tp, int* /*tz*/);


#endif