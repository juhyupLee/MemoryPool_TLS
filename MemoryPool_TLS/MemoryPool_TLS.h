#pragma once
#include "FreeList.h"
#include "Global.h"

struct ChunkMark
{
	// MarkID �� �ʿ���� �����Ҽ��� ûũ���� ChunkPtr�� �������ָ�ȴ�  
	void* _ChunkPtr;
	int32_t _MarkValue;
	int32_t _FreeFlag;
};

template <typename T>
struct ChunkAllocMemory
{
	ChunkMark _FrontMark;
	T _Data;
	ChunkMark _RearMark;
};

template <typename T>
class ChunkMemory
{
public:
	ChunkMemory();
	~ChunkMemory();
public:

public:
	void Crash();
	T* Alloc();
	bool Free(T* data);
	void SetPlacementNew(bool placementNew);
	void SetObjectCount(DWORD objectCount);

private:
	void FreeInit();
public:
	void AllocInit(bool placementNew, DWORD objectCount);
public:
	ChunkAllocMemory<T>* m_Chunk;
	DWORD m_AllocIndex;
	bool m_bPlacementNew;
	bool m_bFree;
	DWORD m_ObjectCount;

	char buffer[64];
	LONG  m_FreeCount;
};

template <typename T>
class MemoryPool_TLS
{
public:
	MemoryPool_TLS(DWORD objectCount,bool bPlacementNew = false);
	~MemoryPool_TLS();
public:
	T* Alloc();
	bool Free(T* data);

	int32_t GetChunkCount();

private:
	void Crash();
private:

	FreeList <ChunkMemory<T>> m_ChunkMemoryPool;
	DWORD m_TLSChunkIndex;
	DWORD m_TLSAllocCountIndex;

	bool m_bPlacementNew;
	DWORD m_ObjectCount;
};


template<typename T>
inline ChunkMemory<T>::ChunkMemory()
{
	m_Chunk = nullptr;
}

template<typename T>
inline ChunkMemory<T>::~ChunkMemory()
{
	delete[] m_Chunk;
}

template<typename T>
inline void ChunkMemory<T>::Crash()
{
	int* p = nullptr;
	*p = 10;
}

template<typename T>
inline T* ChunkMemory<T>::Alloc()
{

	MemoryPoolTLS_Log log;

	//-------------------------------------
	// Chunk�� Mark�κ��� ������ ��¥  TŸ���� �����͸� �����ش�.
	//------------------------------------
	if (m_AllocIndex >= m_ObjectCount)
	{
		Crash();
	}
	//-------------------------------------------------------------------------------
	// Alloc�� �� �����忡�� ���� �Ǳ� ������, ���� Interlock���� ++ ���ʿ����
	//-------------------------------------------------------------------------------
	T* rtnData = (T*)((char*)&m_Chunk[m_AllocIndex++] + sizeof(ChunkMark));

	if (m_bPlacementNew && m_bFree)
	{
		new(rtnData) T;
	}

	//log.DataSettiong(InterlockedIncrement64(&g_TLSPoolMemoryNo), eMemoryPoolTLS::ALLOC_DATA_CHUNK, GetCurrentThreadId(), (int64_t)this, m_AllocIndex, m_FreeCount, m_bFree, (int64_t)rtnData);
	//g_MemoryLog_TLSPool.MemoryLogging(log);

	return rtnData;
}

template<typename T>
inline bool ChunkMemory<T>::Free(T* data)
{
	MemoryPoolTLS_Log log;

	ChunkAllocMemory<T>* dataPtr = (ChunkAllocMemory<T>*)((char*)data - sizeof(ChunkMark));

	//----------------------------------------------------
	// �ݳ��� �����Ͱ� ����÷ο� �� ���
	//----------------------------------------------------
	if (dataPtr->_FrontMark._ChunkPtr != this || dataPtr->_FrontMark._MarkValue != MARK_FRONT)
	{
		throw(FreeListException(L"Underflow Violation", __LINE__));
		return false;
	}
	//----------------------------------------------------
	// �ݳ��� �����Ͱ� �����÷ο� �� ���
	//----------------------------------------------------
	if (dataPtr->_RearMark._ChunkPtr != this || dataPtr->_RearMark._MarkValue != MARK_REAR)
	{
		throw(FreeListException(L"Overflow Violation", __LINE__));
		return false;
	}

	if (0 != InterlockedExchange((LONG*)&(dataPtr->_FrontMark._FreeFlag), 1))
	{
		throw(FreeListException(L"Free X 2", __LINE__));
		return false;
	}

	//--------------------------------------------
	// ����ڰ� placement �� ����Ѵٸ�
	// Free�Ҷ� �Ҹ��ڸ� ȣ�����ش�
	// �Ⱦ��ٸ� ���� �Ҹ��ڸ� ȣ���� �� �ʿ�� ����.
	//--------------------------------------------
	if (m_bPlacementNew)
	{
		data->~T();
	}


	//-------------------------------------
	// Alloc�������� TLS���� �����͸� Alloc�ϱ⶧���� ���������忡�� ������ Ȯ���̾�����
	// Free�� ���ٴϴ� �����Ϳ� �������� ���ٰ����ϱ⶧���� ���������忡�� ������ �� �ִ�.
	//-------------------------------------
	if (InterlockedIncrement(&m_FreeCount) == m_ObjectCount)
	{
		//log.DataSettiong(InterlockedIncrement64(&g_TLSPoolMemoryNo), eMemoryPoolTLS::FREE_FULL_COUNT_CHUNK, GetCurrentThreadId(), (int64_t)this, m_AllocIndex, m_FreeCount, m_bFree, (int64_t)data);
		//g_MemoryLog_TLSPool.MemoryLogging(log);
		//--------------------------------------
		// Free Count�� 100�̵Ǵ¼���,
		// �ʱ�ȭ�� ���ְ� , ûũ�� �ݳ��Ѵ�.
		// �ݳ��� g_MemoryPool����
		//--------------------------------------
		FreeInit();
		return true;
	}

	//log.DataSettiong(InterlockedIncrement64(&g_TLSPoolMemoryNo), eMemoryPoolTLS::FREE_DATA_CHUNK, GetCurrentThreadId(), (int64_t)this, m_AllocIndex, m_FreeCount, m_bFree, (int64_t)data);
	//g_MemoryLog_TLSPool.MemoryLogging(log);
	return false;
}

template<typename T>
inline void ChunkMemory<T>::SetPlacementNew(bool placementNew)
{
	m_bPlacementNew = placementNew;
}

template<typename T>
inline void ChunkMemory<T>::SetObjectCount(DWORD objectCount)
{
	m_ObjectCount = objectCount;
}

template<typename T>
inline void ChunkMemory<T>::FreeInit()
{
	for (size_t i = 0; i < m_ObjectCount; ++i)
	{
		//-----------------------------------------
		// �ݳ�(Free)�� �ι����� ��츦 üũ�ϱ����� Flag
		// Chunk�� �ٻ���������� 0���� �ʱ�ȭ ���� �� �ȴ�
		//-----------------------------------------
		m_Chunk[i]._FrontMark._FreeFlag = 0;
	}
	m_FreeCount = 0;
	m_AllocIndex = 0;
	m_bFree = true;

	/*MemoryPoolTLS_Log log;
	log.DataSettiong(InterlockedIncrement64(&g_TLSPoolMemoryNo), eMemoryPoolTLS::FREE_INIT_DATA_CHUNK, GetCurrentThreadId(), (int64_t)this, m_AllocIndex, m_FreeCount, m_bFree, -1);
	g_MemoryLog_TLSPool.MemoryLogging(log);*/
}

template<typename T>
inline void ChunkMemory<T>::AllocInit(bool placementNew, DWORD objectCount)
{
	if (m_Chunk == nullptr)
	{
		m_ObjectCount = objectCount;
		m_bPlacementNew = placementNew;
		m_bFree = false;

		m_AllocIndex = 0;
		m_FreeCount = 0;

		m_Chunk = new ChunkAllocMemory<T>[m_ObjectCount];

		for (size_t i = 0; i < m_ObjectCount; ++i)
		{
			m_Chunk[i]._FrontMark._ChunkPtr = this;
			m_Chunk[i]._FrontMark._MarkValue = MARK_FRONT;

			//-----------------------------------------
			// �ݳ�(Free)�� �ι����� ��츦 üũ�ϱ����� Flag
			// Chunk�� �ٻ���������� 0���� �ʱ�ȭ ���� �� �ȴ�
			//-----------------------------------------
			m_Chunk[i]._FrontMark._FreeFlag = 0;

			m_Chunk[i]._RearMark._ChunkPtr = this;
			m_Chunk[i]._RearMark._MarkValue = MARK_REAR;
		}
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

	m_TLSAllocCountIndex = TlsAlloc();
	if (m_TLSAllocCountIndex == TLS_OUT_OF_INDEXES)
	{
		Crash();
	}

	m_bPlacementNew = bPlacementNew;
}

template<typename T>
inline MemoryPool_TLS<T>::~MemoryPool_TLS()
{
	TlsFree(m_TLSChunkIndex);
	TlsFree(m_TLSAllocCountIndex);
}

template<typename T>
inline T* MemoryPool_TLS<T>::Alloc()
{
	ChunkMemory<T>* chunkPtr = (ChunkMemory<T> * )TlsGetValue(m_TLSChunkIndex);
	DWORD* allocIndex = (DWORD*)TlsGetValue(m_TLSAllocCountIndex);

	//MemoryPoolTLS_Log log;
	//if (allocIndex != nullptr)
	//{
	//	log.DataSettiong(InterlockedIncrement64(&g_TLSPoolMemoryNo), eMemoryPoolTLS::ALLOC_ENTRY_MEMTLS, GetCurrentThreadId(), (int64_t)chunkPtr, *allocIndex, -1, -1, -1);
	//}
	//else
	//{
	//	log.DataSettiong(InterlockedIncrement64(&g_TLSPoolMemoryNo), eMemoryPoolTLS::ALLOC_ENTRY_MEMTLS, GetCurrentThreadId(), (int64_t)chunkPtr, -1, -1, -1, -1);
	//}
	//g_MemoryLog_TLSPool.MemoryLogging(log);

	if (chunkPtr == nullptr)
	{
		chunkPtr = m_ChunkMemoryPool.Alloc();
		chunkPtr->AllocInit(m_bPlacementNew,m_ObjectCount);

		TlsSetValue(m_TLSChunkIndex,chunkPtr);

		//--------------------------------------------
		// Alloc Index�� ��ó������ TLS �Ҵ��ϸ� ��ü�� �� �� ����
		//--------------------------------------------
		if (allocIndex == nullptr)
		{
			allocIndex = new DWORD;
			*allocIndex = 0;
			TlsSetValue(m_TLSAllocCountIndex, allocIndex);
		}

		//log.DataSettiong(InterlockedIncrement64(&g_TLSPoolMemoryNo), eMemoryPoolTLS::ALLOC_TLS_FIRST_MEMTLS, GetCurrentThreadId(), (int64_t)chunkPtr, *allocIndex, -1, -1, -1);
		//g_MemoryLog_TLSPool.MemoryLogging(log);
	}
	

	T* rtnData = nullptr;

	if (*allocIndex >= m_ObjectCount)
	{
		*allocIndex = 0;
		chunkPtr = m_ChunkMemoryPool.Alloc();
		chunkPtr->AllocInit(m_bPlacementNew, m_ObjectCount);

		if (chunkPtr->m_AllocIndex > 0)
		{
			Crash();
		}
		chunkPtr->SetPlacementNew(m_bPlacementNew);
		chunkPtr->SetObjectCount(m_ObjectCount);
		//----------------------------------------------------
		// �޸�Ǯ�κ��� ���Ӱ� �Ҵ���� ChunkPtr�� �ٽ� TLS �� �־��ش�
		//----------------------------------------------------
		TlsSetValue(m_TLSChunkIndex, chunkPtr);

		rtnData = chunkPtr->Alloc();
		//log.DataSettiong(InterlockedIncrement64(&g_TLSPoolMemoryNo), eMemoryPoolTLS::ALLOC_ALLOCINDEXFULL_MEMTLS, GetCurrentThreadId(), (int64_t)chunkPtr, chunkPtr->m_AllocIndex, chunkPtr->m_FreeCount, chunkPtr->m_bFree, (int64_t)rtnData);
		//g_MemoryLog_TLSPool.MemoryLogging(log);
	}
	else
	{
		rtnData = chunkPtr->Alloc();
	}
	
	(*allocIndex)++;

	//log.DataSettiong(InterlockedIncrement64(&g_TLSPoolMemoryNo), eMemoryPoolTLS::ALLOC_DATA_MEMTLS, GetCurrentThreadId(), (int64_t)chunkPtr, chunkPtr->m_AllocIndex, chunkPtr->m_FreeCount, chunkPtr->m_bFree, (int64_t)rtnData);
	//g_MemoryLog_TLSPool.MemoryLogging(log);

	return rtnData;
}

template<typename T>
inline bool MemoryPool_TLS<T>::Free(T* data)
{
	ChunkAllocMemory<T>* dataPtr= (ChunkAllocMemory<T>*)((char*)data - sizeof(ChunkMark));
	ChunkMemory<T>* chunkPtr = (ChunkMemory<T> * )dataPtr->_FrontMark._ChunkPtr;


	//MemoryPoolTLS_Log log;
	//log.DataSettiong(InterlockedIncrement64(&g_TLSPoolMemoryNo), eMemoryPoolTLS::FREE_ENTRY_MEMTLS, GetCurrentThreadId(), (int64_t)chunkPtr, chunkPtr->m_AllocIndex, chunkPtr->m_FreeCount, chunkPtr->m_bFree, (int64_t)data);
	//g_MemoryLog_TLSPool.MemoryLogging(log);

	if (chunkPtr->Free(data))
	{
		
		m_ChunkMemoryPool.Free(chunkPtr);

		//log.DataSettiong(InterlockedIncrement64(&g_TLSPoolMemoryNo), eMemoryPoolTLS::FREE_FULL_COUNT_MEMTLS, GetCurrentThreadId(), (int64_t)chunkPtr, chunkPtr->m_AllocIndex, chunkPtr->m_FreeCount, chunkPtr->m_bFree, (int64_t)data);
		//g_MemoryLog_TLSPool.MemoryLogging(log);
	}
	return true;
}

template<typename T>
inline int32_t MemoryPool_TLS<T>::GetChunkCount()
{
	return  m_ChunkMemoryPool.GetAllocCount();
}

template<typename T>
inline void MemoryPool_TLS<T>::Crash()
{
	int* p = nullptr;
	*p = 10;
}
