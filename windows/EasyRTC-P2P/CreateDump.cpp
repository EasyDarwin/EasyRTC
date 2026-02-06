#include "CreateDump.h"
#ifdef _WIN32

//创建dump文件
void CreateDumpFile(LPCWSTR lpstrDumpFilePathName, EXCEPTION_POINTERS *pException)
{
	//创建Dump文件

	HANDLE hDumpFile = CreateFile(lpstrDumpFilePathName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	//Dump信息
	MINIDUMP_EXCEPTION_INFORMATION	dumpInfo;
	dumpInfo.ExceptionPointers	=	pException;
	dumpInfo.ThreadId			=	GetCurrentThreadId();
	dumpInfo.ClientPointers		=	TRUE;

	//写入Dump文件内容
	MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpWithFullMemory, &dumpInfo, NULL, NULL);

	CloseHandle(hDumpFile);
}



#endif
