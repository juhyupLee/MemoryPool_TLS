#pragma once
#include "FreeList.h"
#include "Global.h"

template <typename T>
class MemoryPool_TLS
{
	struct ChunkMark
	{
		// MarkID 가 필요없다 누구소속의 청크인지 ChunkPtr로 구분해주면된다  
		void* _ChunkPtr;
		int64_t _MarkValue;
	};

	struct ChunkAllocMemory
	{
		ChunkMark _FrontMark;
		T _Data;
	};
	class ChunkMemory
	{
	public:
		
	public:
		ChunkMemory();
		~ChunkMemory();
	public:
		T* Alloc();
		bool Free(T* data);

	public:
		void AllocInit(bool placementNew, DWORD objectCount, MemoryPool_TLS<T>* centerPool);
	public:
		ChunkAllocMemory* m_Chunk;
		MemoryPool_TLS<T>* m_CenterMemoryPool;
		bool m_bPlacementNew;
		DWORD m_ObjectCount;
		DWORD m_AllocIndex;

		char m_Padding2[64];
		LONG  m_FreeCount;
	};


public:
	MemoryPool_TLS(DWORD objectCount,bool bPlacementNew = false);
	~MemoryPool_TLS();
public:
	T* Alloc();
	bool Free(T* data);
	void Crash();
	ChunkMemory* ChunkSetting()
	{
		//g_Profiler.ProfileBegin(L"ChunkSetting");
		MemoryPool_TLS<T>::ChunkMemory* chunkPtr = m_ChunkMemoryPool.Alloc();
		chunkPtr->AllocInit(m_bPlacementNew, m_ObjectCount, this);
		TlsSetValue(m_TLSChunkIndex, chunkPtr);

		//g_Profiler.ProfileEnd(L"ChunkSetting");
		return chunkPtr;
	}
	int32_t GetChunkCount();

private:

	FreeList <ChunkMemory> m_ChunkMemoryPool;
	DWORD m_TLSChunkIndex;
	bool m_bPlacementNew;
	DWORD m_ObjectCount;
};


template<typename T>
inline MemoryPool_TLS<T>::ChunkMemory::ChunkMemory()
{
	m_Chunk = nullptr;
	m_CenterMemoryPool = nullptr;

}

template<typename T>
inline MemoryPool_TLS<T>::ChunkMemory::~ChunkMemory()
{
	delete[] m_Chunk;
}



template<typename T>
inline T* MemoryPool_TLS<T>::ChunkMemory::Alloc()
{

	//wprintf(L"Debug Cache Line m_AllocIndex:%lld  namuji :%d\n", (int64_t)&m_AllocIndex / 64, (int64_t)&m_AllocIndex % 64);
	//wprintf(L"Debug Cache Line m_FreeCount:%lld  namuji :%d\n", (int64_t)&m_FreeCount / 64, (int64_t)&m_FreeCount % 64);
	//-------------------------------------
	// Chunk의 Mark부분을 제외한 진짜  T타입의 포인터를 던져준다.
	//------------------------------------
	//g_Profiler.ProfileBegin(L"Chunk Alloc");
	//-------------------------------------------------------------------------------
	// Alloc은 각 스레드에서 진행 되기 때문에, 굳이 Interlock으로 ++ 할필요없다
	//-------------------------------------------------------------------------------

	//g_Profiler.ProfileBegin(L"Chunk Alloc_RtnDataSetting");
	T* rtnData = (T*)((char*)&m_Chunk[--m_AllocIndex] + sizeof(ChunkMark));
	//g_Profiler.ProfileEnd(L"Chunk Alloc_RtnDataSetting");

	//g_Profiler.ProfileBegin(L"Chunk Alloc_PlacementNewCheck");
	//-----------------------------------------------------------
	// placement New flag true인경우, 맨 처음 할당일땐 생성자가 두번 호출된다
	//-----------------------------------------------------------
	if (m_bPlacementNew )
	{
		new(rtnData) T;
	}
	//g_Profiler.ProfileEnd(L"Chunk Alloc_PlacementNewCheck");

	//---------------------------------------
	// Return 하는순간 다른스레드에서 바로 반납할수있기때문에
	// Return 하기전에 TLS의 포인터를 새로 세팅해준다.
	//---------------------------------------
	if (m_AllocIndex == 0)
	{
		m_CenterMemoryPool->ChunkSetting();
	}

	//log.DataSettiong(InterlockedIncrement64(&g_TLSPoolMemoryNo), eMemoryPoolTLS::ALLOC_DATA_CHUNK, GetCurrentThreadId(), (int64_t)this, m_AllocIndex, m_FreeCount, m_bFree, (int64_t)rtnData);
	//g_MemoryLog_TLSPool.MemoryLogging(log);

	//g_Profiler.ProfileEnd(L"Chunk Alloc");
	return rtnData;
}

template<typename T>
inline bool MemoryPool_TLS<T>::ChunkMemory::Free(T* data)
{

	//g_Profiler.ProfileBegin(L"Chunk Free");


	//g_Profiler.ProfileBegin(L"Chunk Free DataPtrSetting");
	ChunkAllocMemory* dataPtr = (ChunkAllocMemory*)((char*)data - sizeof(ChunkMark));
	//g_Profiler.ProfileEnd(L"Chunk Free DataPtrSetting");
	//----------------------------------------------------
	// 반납된 포인터가 언더플로우 한 경우
	//----------------------------------------------------

	//g_Profiler.ProfileBegin(L"Chunk Free Overflow");
	if (dataPtr->_FrontMark._MarkValue != MARK_FRONT)
	{
		throw(FreeListException(L"Underflow Violation", __LINE__));
		return false;
	}
	////----------------------------------------------------
	//// 반납된 포인터가 오버플로우 한 경우
	////----------------------------------------------------
	//if (dataPtr->_RearMark._ChunkPtr != this || dataPtr->_RearMark._MarkValue != MARK_REAR)
	//{
	//	throw(FreeListException(L"Overflow Violation", __LINE__));
	//	return false;
	//}

	//g_Profiler.ProfileEnd(L"Chunk Free Overflow");
	//--------------------------------------------
	// 사용자가 placement 를 사용한다면
	// Free할때 소멸자를 호출해준다
	// 안쓴다면 굳이 소멸자를 호출해 줄 필요는 없다.
	//--------------------------------------------
	if (m_bPlacementNew)
	{
		data->~T();
	}


	//-------------------------------------
	// Alloc같은경우는 TLS에서 데이터를 Alloc하기때문에 여러스레드에서 접근할 확률이없지만
	// Free는 떠다니는 포인터에 어떤스레드든 접근가능하기때문에 여러스레드에서 접근할 수 있다.
	//-------------------------------------
	
	//g_Profiler.ProfileBegin(L"Chunk Free Interlock");
	if (InterlockedDecrement(&m_FreeCount) == 0)
	{
		//log.DataSettiong(InterlockedIncrement64(&g_TLSPoolMemoryNo), eMemoryPoolTLS::FREE_FULL_COUNT_CHUNK, GetCurrentThreadId(), (int64_t)this, m_AllocIndex, m_FreeCount, m_bFree, (int64_t)data);
		//g_MemoryLog_TLSPool.MemoryLogging(log);
		//--------------------------------------
		// Free Count가 0 이되는순간,
		// 초기화를 해주고 , 청크를 반납한다.
		// 반납은 g_MemoryPool에서
		//--------------------------------------
		//FreeInit();
		//g_Profiler.ProfileBegin(L"Chunk Free INit");
		//m_FreeCount = m_ObjectCount;
		//m_AllocIndex = m_ObjectCount;
		//m_bFree = true;
		//g_Profiler.ProfileEnd(L"Chunk Free INit");

		m_CenterMemoryPool->m_ChunkMemoryPool.Free(this);
		return true;
	}
	//g_Profiler.ProfileEnd(L"Chunk Free Interlock");

	//g_Profiler.ProfileEnd(L"Chunk Free");
	//log.DataSettiong(InterlockedIncrement64(&g_TLSPoolMemoryNo), eMemoryPoolTLS::FREE_DATA_CHUNK, GetCurrentThreadId(), (int64_t)this, m_AllocIndex, m_FreeCount, m_bFree, (int64_t)data);
	//g_MemoryLog_TLSPool.MemoryLogging(log);
	return false;
}

template<typename T>
__forceinline void MemoryPool_TLS<T>::ChunkMemory::AllocInit(bool placementNew, DWORD objectCount, MemoryPool_TLS<T>* centerPool)
{
	if (m_Chunk == nullptr)
	{
		m_CenterMemoryPool = centerPool;
		m_ObjectCount = objectCount;
		m_bPlacementNew = placementNew;

		m_AllocIndex = m_ObjectCount;
		m_FreeCount = m_ObjectCount;

		if (m_bPlacementNew)
		{
			m_Chunk = (ChunkAllocMemory*)malloc(sizeof(ChunkAllocMemory) * m_ObjectCount);
		}
		else
		{
			m_Chunk = new ChunkAllocMemory[m_ObjectCount];
		}
		

		for (size_t i = 0; i < m_ObjectCount; ++i)
		{
			m_Chunk[i]._FrontMark._ChunkPtr = this;
			m_Chunk[i]._FrontMark._MarkValue = MARK_FRONT;

			//m_Chunk[i]._RearMark._ChunkPtr = this;
			//m_Chunk[i]._RearMark._MarkValue = MARK_REAR;
		}
	}
	else
	{
		m_FreeCount = m_ObjectCount;
		m_AllocIndex = m_ObjectCount;
	}
}

template<typename T>
inline MemoryPool_TLS<T>::MemoryPool_TLS(DWORD objectCount,bool bPlacementNew)
{
	m_ObjectCount = objectCount;
	m_TLSChunkIndex = TlsAlloc();
	if (m_TLSChunkIndex == TLS_OUT_OF_INDEXES)
	{
		Crash();
	}

	m_bPlacementNew = bPlacementNew;
}

template<typename T>
inline MemoryPool_TLS<T>::~MemoryPool_TLS()
{
	TlsFree(m_TLSChunkIndex);
}

template<typename T>
inline T* MemoryPool_TLS<T>::Alloc()
{
	ChunkMemory* chunkPtr= (ChunkMemory *)TlsGetValue(m_TLSChunkIndex);

	if (chunkPtr == nullptr)
	{
	
		chunkPtr = ChunkSetting();
	}
	
	//log.DataSettiong(InterlockedIncrement64(&g_TLSPoolMemoryNo), eMemoryPoolTLS::ALLOC_DATA_MEMTLS, GetCurrentThreadId(), (int64_t)chunkPtr, chunkPtr->m_AllocIndex, chunkPtr->m_FreeCount, chunkPtr->m_bFree, (int64_t)rtnData);
	//g_MemoryLog_TLSPool.MemoryLogging(log);

	return chunkPtr->Alloc();
}

template<typename T>
inline bool MemoryPool_TLS<T>::Free(T* data)
{

	//g_Profiler.ProfileBegin(L"TLS_FREE_DataCpy");
	MemoryPool_TLS<T>::ChunkAllocMemory* dataPtr= (MemoryPool_TLS<T>::ChunkAllocMemory*)((char*)data - sizeof(ChunkMark));
	//ChunkMemory* chunkPtr = (ChunkMemory*)dataPtr->_FrontMark._ChunkPtr;
	//g_Profiler.ProfileEnd(L"TLS_FREE_DataCpy");
	

	//ChunkMemory* chunkPtr = (((MemoryPool_TLS<T>::ChunkAllocMemory*)((char*)data - sizeof(ChunkMark)))->_FrontMark._ChunkPtr;

	//MemoryPoolTLS_Log log;
	//log.DataSettiong(InterlockedIncrement64(&g_TLSPoolMemoryNo), eMemoryPoolTLS::FREE_ENTRY_MEMTLS, GetCurrentThreadId(), (int64_t)chunkPtr, chunkPtr->m_AllocIndex, chunkPtr->m_FreeCount, chunkPtr->m_bFree, (int64_t)data);
	//g_MemoryLog_TLSPool.MemoryLogging(log);

	//chunkPtr->Free(data);

	
	//((ChunkMemory*)(dataPtr->_FrontMark._ChunkPtr))->Free(data);
//	((ChunkMemory*)(dataPtr->_FrontMark._ChunkPtr))->Free(data);

	///g_Profiler.ProfileBegin(L"Chunk_FREE");
	//((ChunkMemory*)(((ChunkAllocMemory*)((char*)data - sizeof(ChunkMark)))->_FrontMark._ChunkPtr))->Free(data);
	//chunkPtr->Free(data);
	((ChunkMemory*)(dataPtr->_FrontMark._ChunkPtr))->Free(data);
	//g_Profiler.ProfileEnd(L"Chunk_FREE");
	//chunkPtr->Free(data);
	

	return true;
}

template<typename T>
inline void MemoryPool_TLS<T>::Crash()
{
	int* p = nullptr;
	*p = 10;
}


//template<typename T>
//inline MemoryPool_TLS<T>::ChunkMemory* MemoryPool_TLS<T>::ChunkSetting()
//{
//	MemoryPool_TLS<T>::ChunkMemory* chunkPtr = m_ChunkMemoryPool.Alloc();
//	chunkPtr->AllocInit(m_bPlacementNew, m_ObjectCount,this);
//	TlsSetValue(m_TLSChunkIndex, chunkPtr);
//
//	return chunkPtr;
//
//}

template<typename T>
inline int32_t MemoryPool_TLS<T>::GetChunkCount()
{
	return  m_ChunkMemoryPool.GetAllocCount();
}
