#include "Global.h"

Profiler g_Profiler;
LONG g_IOPostCount = 0;
LONG g_IOCompleteCount = 0;
LONG g_IOIncDecCount = 0;

MemoryLogging_New<IOCP_Log, 5000> g_MemoryLog_IOCP;
MemoryLogging_New<MemoryPoolTLS_Log, 5000> g_MemoryLog_TLSPool;

int64_t g_IOCPMemoryNo = -1;
int64_t g_TLSPoolMemoryNo = -1;

