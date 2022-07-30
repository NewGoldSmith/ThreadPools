//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php
#pragma once
//#pragma comment(lib, "ws2_32.lib")
//#include <WinSock2.h>
//#include <MSWSock.h>
//#include <ws2tcpip.h>
//#include <Windows.h>

	template <class T>class RingBuf
	{
	public:
		RingBuf() = delete;
		RingBuf(T* pBufIn,size_t sizeIn )
			:ppBuf(nullptr)
			, size(sizeIn)
			, front(sizeIn - 1)
			, end(0)
			, mask(sizeIn - 1)
		{
			ppBuf = new T * [sizeIn];
			for (size_t i(0); i < size; ++i)
			{

				ppBuf[i] = &pBufIn[i];
			}
		}

		RingBuf(RingBuf& obj)
			:ppBuf(obj.ppBuf)
			, size(obj.size)
			, front(obj.front)
			, end(obj.end)
			, mask(obj.mask)
		{
		}
		~RingBuf()
		{
			delete[]ppBuf;
		}
		T* Pop()
		{
			T** ppT = &ppBuf[end & mask];
			++end;
			return *ppT;
		}
		void Push(T* pT)
		{
			++front;
			ppBuf[front & mask] = pT;
		}

	protected:
		T** ppBuf;
		size_t size;
		size_t front;
		size_t end;
		size_t mask;
	};

