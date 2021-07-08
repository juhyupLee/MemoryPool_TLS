#pragma once
#include <iostream>
#include <Windows.h>
#include "MemoryLog.h"

#define MARK_FRONT 0x12345678
#define MARK_REAR 0x87654321


extern MemoryLogging_New<FreeList_Log,10000> g_MemoryLogFreeList;
extern long long g_Mark;
extern int64_t g_FreeListMemoryCount;

class FreeListException
{
public:
	FreeListException(const wchar_t* str, int line)
		:m_Line(line)
	{
		wcscpy_s(m_String,str);
	}
	void what()
	{
		wprintf(L"%s  [File:FreeList.h] [Line:%d]\n", m_String, m_Line);
	}
	int m_Line;
	wchar_t m_String[128];
};

template <typename T>
class FreeList
{
public:
	struct Mark
	{
		int64_t _MarkID;
		int32_t _MarkValue;
		int32_t _FreeFlag;
	};
	struct Node
	{
		Node()
			:_Next(nullptr)
		{

		}
		T _Data;
		Node* _Next;
	};

	struct TopCheck
	{
		Node* _TopPtr;
		int64_t _ID;
	};
	struct AllocMemory
	{
		Mark _FrontMark;
		Node _Node;
		Mark _RearMark;
	};

	FreeList(bool placementNew = false)
		:
		m_bPlacementNew(placementNew),
		m_UseCount(0),
		m_PoolCount(0),
		m_AllocCount(0),
		m_MarkValue(++g_Mark)
	{
		m_TopCheck = (TopCheck*)_aligned_malloc(sizeof(TopCheck), 16);
		m_TopCheck->_TopPtr = nullptr;
		m_TopCheck->_ID = 0;

	}
	FreeList(int32_t blockNum, bool placementNew=false)
		:
		m_bPlacementNew(placementNew),
		m_MarkValue(++g_Mark),
		m_UseCount(0),
		m_PoolCount(0),
		m_AllocCount(0)
	{
		m_TopCheck = (TopCheck*)_aligned_malloc(sizeof(TopCheck), 16);
		m_TopCheck->_TopPtr = nullptr;
		m_TopCheck->_ID = 0;

		for (int i = 0; i < blockNum; i++)
		{
			T* data = AllocateNewMemory();
			Free(data);
		}
	}
	~FreeList()
	{
		//--------------------------------------------------------------------------
		// ���̳�带 ������, ������ ������ ���� �����Ϳ��� 128����Ʈ ��ŭ ����, delete�� �ؾ��Ѵ�
		//--------------------------------------------------------------------------
		Node* curNode = m_TopCheck->_TopPtr;

		while (curNode != nullptr)
		{
			char* delNode = (char*)curNode;

			if (!m_bPlacementNew)
			{
				//---------------------------------------------------------------------------------
				// ��ó�� �޸� malloc���� �Ҵ��Ҷ�, ��üȭ�� ���� ����, PlacementNew�� ���� ������ ȣ�⶧����,
				// �Ҹ��ڸ� �Ǹ��������� ȣ�����ش�.
				//---------------------------------------------------------------------------------
				((T*)delNode)->~T();
			}

			delNode = delNode - (sizeof(int64_t)*2);

			curNode = curNode->_Next;
			
			//free(delNode);

			_aligned_free(delNode);
		}

		_aligned_free(m_TopCheck);
	}

	int32_t GetPoolCount();
	int32_t GetUseCount();
	int32_t GetAllocCount();
	bool Free(T* data);
	T* Alloc();

private:
	T* AllocateNewMemory();

private:
	TopCheck* m_TopCheck;

	char padding[64];
	bool m_bPlacementNew;

	LONG m_UseCount;
	LONG m_AllocCount;
	LONG m_PoolCount;

	const int64_t m_MarkValue;

};


//
//template <int size>
//class Data
//{
//public:
//	Data() { memset(m_Buffer, 7, size); };
//	~Data() {};
//private:
//	char m_Buffer[size];
//};


//class MemoryPool
//{
//
//public:
//	MemoryPool() {};
//
//	Data<16>* Alloc(int size)
//	{
//		m_128 = new FreeList<Data<16>>();
//		
//		return m_128->Alloc();
//	}
//private:
//	FreeList<Data<16>>* m_128;
//
//};

template<typename T>
inline int32_t FreeList<T>::GetPoolCount()
{
	return m_PoolCount;
}

template<typename T>
inline int32_t FreeList<T>::GetUseCount()
{
	return m_UseCount;
}

template<typename T>
inline int32_t FreeList<T>::GetAllocCount()
{
	return m_AllocCount;
}

template<typename T>
inline T* FreeList<T>::AllocateNewMemory()
{
	AllocMemory* allocMemory;
	//--------------------------------------------------------------------------
	// ����÷ο� üũ�� mark ID  + data(Payload)  + �����÷ο� üũ�� mark ID  �Ҵ�
	//--------------------------------------------------------------------------
	//allocMemory = (AllocMemory*)malloc(sizeof( AllocMemory));
	allocMemory = (AllocMemory*)_aligned_malloc(sizeof(AllocMemory), 64);
	new(&allocMemory->_Node)T;
	allocMemory->_FrontMark._FreeFlag = 0;
	allocMemory->_FrontMark._MarkID = m_MarkValue;
	allocMemory->_FrontMark._MarkValue = MARK_FRONT;
	allocMemory->_RearMark._MarkID = m_MarkValue;
	allocMemory->_RearMark._MarkValue = MARK_REAR;

	Node* tempNode = &allocMemory->_Node;

	//FreeList_Log logData;
	//logData.DataSettiong(InterlockedIncrement64(&g_FreeListMemoryCount), eFreeListPos::ALLOC_MEMORY, GetCurrentThreadId(), (int64_t)m_TopCheck->_TopPtr,  -1,-1, (int64_t)tempNode);
	//g_MemoryLogFreeList.MemoryLogging(logData);


	InterlockedIncrement(&m_AllocCount);
	//InterlockedIncrement(&m_UseCount);

	return (T*)&allocMemory->_Node;
}

template<typename T>
bool FreeList<T>::Free(T* data)
{
	Node* freeNode = (Node*)data;
	AllocMemory* allocMemory = (AllocMemory*)((char*)data - sizeof(Mark));
	//AllocMemory* allocMemory = (AllocMemory*)(byteDataPtr - sizeof(int64_t) * 2);

	//----------------------------------------------------
	// �ݳ��� �����Ͱ� ����÷ο� �� ���
	//----------------------------------------------------
	if (allocMemory->_FrontMark._MarkID != m_MarkValue || allocMemory->_FrontMark._MarkValue != MARK_FRONT)
	{
		throw(FreeListException(L"Underflow Violation", __LINE__));
		return false;
	}
	//----------------------------------------------------
	// �ݳ��� �����Ͱ� �����÷ο� �� ���
	//----------------------------------------------------
	if (allocMemory->_RearMark._MarkID != m_MarkValue || allocMemory->_RearMark._MarkValue != MARK_REAR)
	{
		throw(FreeListException(L"Overflow Violation", __LINE__));
		return false;
	}

	if (0 != InterlockedExchange((LONG*)&(allocMemory->_FrontMark._FreeFlag), 1))
	{
		throw(FreeListException(L"Free X 2", __LINE__));
		return false;
	}

	if (m_bPlacementNew)
	{
		((T*)freeNode)->~T();
	}

	TopCheck tempTop;

	tempTop._TopPtr = m_TopCheck->_TopPtr;
	tempTop._ID = m_TopCheck->_ID;
	do
	{
		//-------------------------------------------------------------------------------------------
		//  FreeNode(�ݳ��� ���) ->  CurrentTop  �ݳ��� ����� Next�����͸� ������ Top������Ű����
		//-------------------------------------------------------------------------------------------
		freeNode->_Next = tempTop._TopPtr;

	} while (!InterlockedCompareExchange128((LONG64*)m_TopCheck, (LONG64)tempTop._ID + 1, (LONG64)freeNode, (LONG64*)&tempTop));

	//InterlockedDecrement(&m_UseCount);
	//InterlockedIncrement(&m_PoolCount);

	//FreeList_Log logData;
	//logData.DataSettiong(InterlockedIncrement64(&g_FreeListMemoryCount), eFreeListPos::FREE_OKAY, GetCurrentThreadId(), (int64_t)m_TopCheck->_TopPtr, (int64_t)tempTop._TopPtr, (int64_t)freeNode,-1);
	//g_MemoryLogFreeList.MemoryLogging(logData);

	return true;
}

template <typename T>
T* FreeList<T>::Alloc()
{
	TopCheck tempTop;
	Node* nextNode = nullptr;

	tempTop._TopPtr = m_TopCheck->_TopPtr;
	tempTop._ID = m_TopCheck->_ID;

	do
	{
		if (tempTop._TopPtr == nullptr)
		{
			return AllocateNewMemory();
		}

		nextNode = tempTop._TopPtr->_Next;

	} while (!InterlockedCompareExchange128((LONG64*)m_TopCheck, (LONG64)tempTop._ID + 1, (LONG64)nextNode, (LONG64*)&tempTop));

	//InterlockedIncrement(&m_UseCount);
	//InterlockedDecrement(&m_PoolCount);

	Node* rtnNode = tempTop._TopPtr;

	//--------------------------------------------------------------------------
	// ����, Placement New���, �׳� �����ڸ� ȣ�����ش�.
	//--------------------------------------------------------------------------
	if (m_bPlacementNew)
	{
		new(rtnNode) T;
	}

	char* byteDataPtr = (char*)rtnNode;
	AllocMemory* allocMemory = (AllocMemory*)(byteDataPtr - sizeof(int64_t) * 2);
	InterlockedExchange((LONG*)&(allocMemory->_FrontMark._FreeFlag), 0);

	//FreeList_Log logData;
	//logData.DataSettiong(InterlockedIncrement64(&g_FreeListMemoryCount), eFreeListPos::ALLOC_OKAY, GetCurrentThreadId(), (int64_t)m_TopCheck->_TopPtr, (int64_t)tempTop._TopPtr, -1, (int64_t)rtnNode);
	//g_MemoryLogFreeList.MemoryLogging(logData);

	return &rtnNode->_Data;
}
