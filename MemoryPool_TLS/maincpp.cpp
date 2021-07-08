#include <process.h>

#include "MemoryPool_TLS.h"
#include "MemoryDump.h"
#include "Profiler.h"


CrashDump g_CrashDump;
Profiler g_Profiler;

#define INIT_DATA 0x0000000055555555
#define INIT_COUNT  0
#define DATA_COUNT 100000
#define THREAD_NUM 8

#define POOL_MODE 0

struct TestData
{
	TestData()
	{
		_Data = INIT_DATA;
		_RefCount = INIT_COUNT;
	}
	int64_t _Data;
	LONG _RefCount;
};

CrashDump memoryDump;

HANDLE g_Thread[THREAD_NUM];
bool g_Exit = false;

MemoryPool_TLS<TestData> g_MemoryPool(DATA_COUNT);

void Crash()
{
	int* p = nullptr;
	*p = 10;
}


unsigned int __stdcall TestThread_NewDelete(LPVOID param)
{
	TestData** dataArray = (TestData**)param;

	while (1)
	{
		if (g_Exit)
		{
			break;
		}
		
		g_Profiler.ProfileBegin(L"NewDelete");
		for (int i = 0; i < DATA_COUNT; ++i)
		{
			InterlockedIncrement64(&dataArray[i]->_Data);
			InterlockedIncrement(&dataArray[i]->_RefCount);
		}
		//Sleep(0);

		for (int i = 0; i < DATA_COUNT; ++i)
		{
			if (dataArray[i]->_Data != INIT_DATA + 1)
			{
				Crash();
			}
			if (dataArray[i]->_RefCount != INIT_COUNT + 1)
			{
				Crash();
			}
		}
		//Sleep(0);

		for (int i = 0; i < DATA_COUNT; ++i)
		{
			InterlockedDecrement64(&dataArray[i]->_Data);
			InterlockedDecrement(&dataArray[i]->_RefCount);

		}
		//Sleep(0);
		for (int i = 0; i < DATA_COUNT; ++i)
		{
			if (dataArray[i]->_Data != INIT_DATA)
			{
				Crash();
			}
			if (dataArray[i]->_RefCount != INIT_COUNT)
			{
				Crash();
			}
		}

		//----------------------------------------------------
		// Free 内靛
		//----------------------------------------------------
		for (int i = 0; i < DATA_COUNT; ++i)
		{
			//g_Profiler.ProfileBegin(L"Delete");
			delete dataArray[i];
			//g_Profiler.ProfileEnd(L"Delete");
		}

		//Sleep(0);


		//memset(dataArray, 0, sizeof(TestData*) * DATA_COUNT);

		//----------------------------------------------------
		// Alloc 内靛
		//----------------------------------------------------
		for (int i = 0; i < DATA_COUNT; ++i)
		{
			//g_Profiler.ProfileBegin(L"New");
			dataArray[i] = new TestData;
			//g_Profiler.ProfileEnd(L"New");
			if (dataArray[i]->_Data != INIT_DATA)
			{
				Crash();
			}
			if (dataArray[i]->_RefCount != INIT_COUNT)
			{
				Crash();
			}
		}


		//Sleep(2);
		for (int i = 0; i < DATA_COUNT; ++i)
		{
			if (dataArray[i] == nullptr)
			{
				Crash();
			}
			if (dataArray[i]->_Data != INIT_DATA)
			{
				Crash();
			}
			if (dataArray[i]->_RefCount != INIT_COUNT)
			{
				Crash();
			}
		}

		//Sleep(0);

		g_Profiler.ProfileEnd(L"NewDelete");
	}
	return 0;
}

unsigned int __stdcall TestThread_Pool(LPVOID param)
{
	TestData** dataArray = (TestData**)param;

	while (1)
	{
		if (g_Exit)
		{
			break;
		}
		
		g_Profiler.ProfileBegin(L"Pool");
		for (int i = 0; i < DATA_COUNT; ++i)
		{
			InterlockedIncrement64(&dataArray[i]->_Data);
			InterlockedIncrement(&dataArray[i]->_RefCount);
		}
		//Sleep(0);

		for (int i = 0; i < DATA_COUNT; ++i)
		{
			if (dataArray[i]->_Data != INIT_DATA + 1)
			{
				Crash();
			}
			if (dataArray[i]->_RefCount != INIT_COUNT + 1)
			{
				Crash();
			}
		}
		//Sleep(0);

		for (int i = 0; i < DATA_COUNT; ++i)
		{
			InterlockedDecrement64(&dataArray[i]->_Data);
			InterlockedDecrement(&dataArray[i]->_RefCount);

		}
		//Sleep(0);
		for (int i = 0; i < DATA_COUNT; ++i)
		{
			if (dataArray[i]->_Data != INIT_DATA)
			{
				Crash();
			}
			if (dataArray[i]->_RefCount != INIT_COUNT)
			{
				Crash();
			}
		}

		//----------------------------------------------------
		// Free 内靛
		//----------------------------------------------------
		for (int i = 0; i < DATA_COUNT; ++i)
		{
			//g_Profiler.ProfileBegin(L"PoolFree");
			g_MemoryPool.Free(dataArray[i]);
			//g_Profiler.ProfileEnd(L"PoolFree");
		}

		//Sleep(0);


		//memset(dataArray, 0, sizeof(TestData*) * DATA_COUNT);

		//----------------------------------------------------
		// Alloc 内靛
		//----------------------------------------------------
		for (int i = 0; i < DATA_COUNT; ++i)
		{
			//g_Profiler.ProfileBegin(L"PoolAlloc");
			dataArray[i] = g_MemoryPool.Alloc();
			//g_Profiler.ProfileEnd(L"PoolAlloc");
			if (dataArray[i]->_Data != INIT_DATA)
			{
				Crash();
			}
			if (dataArray[i]->_RefCount != INIT_COUNT)
			{
				Crash();
			}
		}

		
		//Sleep(2);
		for (int i = 0; i < DATA_COUNT; ++i)
		{
			if (dataArray[i] == nullptr)
			{
				Crash();
			}
			if (dataArray[i]->_Data != INIT_DATA)
			{
				Crash();
			}
			if (dataArray[i]->_RefCount != INIT_COUNT)
			{
				Crash();
			}
		}
		g_Profiler.ProfileEnd(L"Pool");
		//Sleep(0)
	}
	return 0;
}

int main()
{
#if POOL_MODE ==1
	TestData* dataArray[THREAD_NUM][DATA_COUNT];

	for (int i = 0; i < THREAD_NUM; ++i)
	{
		for (int j = 0; j < DATA_COUNT; ++j)
		{
			//Test
			dataArray[i][j] = g_MemoryPool.Alloc();

			if (dataArray[i][j] == nullptr)
			{
				Crash();
			}
			if (dataArray[i][j]->_Data != INIT_DATA)
			{
				Crash();
			}
			if (dataArray[i][j]->_RefCount != INIT_COUNT)
			{
				Crash();
			}
		}

	}

	for (int i = 0; i < THREAD_NUM; ++i)
	{
		g_Thread[i] = (HANDLE)_beginthreadex(NULL, 0, TestThread_Pool, (void*)dataArray[i], 0, NULL);
	}

#endif

#if POOL_MODE ==0
	TestData* dataArray[THREAD_NUM][DATA_COUNT];

	for (int i = 0; i < THREAD_NUM; ++i)
	{
		for (int j = 0; j < DATA_COUNT; ++j)
		{
			dataArray[i][j] = new TestData;

			if (dataArray[i][j] == nullptr)
			{
				Crash();
			}
			if (dataArray[i][j]->_Data != INIT_DATA)
			{
				Crash();
			}
			if (dataArray[i][j]->_RefCount != INIT_COUNT)
			{
				Crash();
			}
		}

	}

	for (int i = 0; i < THREAD_NUM; ++i)
	{
		g_Thread[i] = (HANDLE)_beginthreadex(NULL, 0, TestThread_NewDelete, (void*)dataArray[i], 0, NULL);
	}
#endif

	while (true)
	{
		if (GetAsyncKeyState(VK_F4))
		{
			g_Exit = true;
			break;
		}
		wprintf(L"Chunk Count:%d\n", g_MemoryPool.GetChunkCount());
		Sleep(1000);
	}

	WaitForMultipleObjects(THREAD_NUM, g_Thread, TRUE, INFINITE);

	g_Profiler.ProfileDataOutText(L"ProfileReport.txt");
}