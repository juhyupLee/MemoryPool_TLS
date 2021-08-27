#pragma once
#include <iostream>
#include <Windows.h>
#include "MemoryLog.h"
#include "Profiler.h"

extern Profiler g_Profiler;
extern MemoryLogging_New<IOCP_Log, 5000> g_MemoryLog_IOCP;
extern MemoryLogging_New<MemoryPoolTLS_Log, 5000> g_MemoryLog_TLSPool;

extern int64_t g_IOCPMemoryNo;
extern int64_t g_TLSPoolMemoryNo;

extern LONG g_IOPostCount;
extern LONG g_IOCompleteCount;
extern LONG g_IOIncDecCount;