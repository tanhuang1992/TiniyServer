#pragma once

/************************************************************************/
/*      消息队列基于内存的数据包读取类
		内存自动调整大小，貌似需要内存池
*/
/************************************************************************/

#define DEFAULT_MEM_SIZE 102400
#define  SAFE_FREE(p) (if(p){free(p);(p)=nullptr})

class CDataPacketReader
{
public:
	CDataPacketReader()
	{
		m_pMemory=(char*)malloc(DEFAULT_MEM_SIZE);
		m_pMemory=m_pMemoryEnd=m_pOffset=m_pDataEnd=nullptr;
	}

	CDataPacketReader(void* pBuffer,size_t size)
	{
		m_pMemory=m_pOffset=(char*)pBuffer;
		m_pMemoryEnd=m_pDataEnd=m_pMemory+size;
	}

	~CDataPacketReader()
	{
		SAFE_FREE(m_pMemory);
		m_pDataEnd=m_pMemoryEnd=m_pOffset=nullptr;
	}

	size_t readBuf(void* pBuffer,size_t size)
	{
		size_t avliableSize=m_pDataEnd-m_pOffset;
		if(avliableSize<size)
		{
			size=avliableSize;
		}

		if (size>0)
		{
			memcpy(pBuffer,m_pOffset,size);
			m_pOffset+=size;
		}

		return size;
	}

	void wirteBuf(void* pBuffer,size_t size)
	{
		size_t avliableMemSize=m_pMemoryEnd-m_pDataEnd;
		if(avliableMemSize<size)
		{
			reserveSize(size);
		}
		
		memcpy(m_pDataEnd+1,pBuffer,size);
		m_pDataEnd+=size;
	}


	template<typename _Ty>
	_Ty readAtom()
	{
		_Ty val;
		size_t avliableSize=m_pDataEnd-m_pOffset;

		if(avliableSize>sizeof(_Ty))
		{
			val=*(_Ty*)m_pOffset;
			m_pOffset+=sizeof(_Ty);
		}
		else if(avliableSize>0) //内存数据不够读
		{
			memset(&val,0,sizeof(_Ty));
			memcpy(&val,m_pOffset,avliableSize);
			m_pOffset+=avliableSize;
		}
		else
		{
			memset(&val,0,sizeof(_Ty));
		}

		return val;
	}

	template<typename _Ty>
	void writeAtom(_Ty& val)
	{
		size_t avliableMemSize=m_pMemoryEnd-m_pDataEnd;
		if(avliableMemSize<sizeof(_Ty))
		{
			reserveSize(sizeof(_Ty));
		}

		memcpy(m_pDataEnd+1,&val,sizeof(_Ty));
		m_pDataEnd+=sizeof(_Ty);
	}

public:

	template<typename _Ty>
	inline CDataPacketReader& operator <<(_Ty& val)
	{
		if(sizeof(val)>sizeof(INT_PTR))
		{
			wirteBuf(&val,sizeof(_Ty));
		}
		else
		{
			writeAtom<_Ty>(val);
		}

		return *this;
	}

	template<typename _Ty>
	inline CDataPacketReader& operator >>(_Ty& val)
	{
		if(sizeof(val)>sizeof(INT_PTR))
		{
			readBuf(&val,sizeof(_Ty));
		}
		else
		{
			readAtom<_Ty>();
		}

		return *this;
	}



private:
	//扩充
	void reserveSize(size_t size)
	{
		if(size==0)
			return;
		realloc(m_pMemory,m_pMemoryEnd-m_pMemory+size+1);
		m_pDataEnd+=size;
	}

private:
	char* m_pMemory;	
	char* m_pMemoryEnd;	
	char* m_pOffset;	
	char* m_pDataEnd;	

}