//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php
#pragma once
#include <exception>
//#define NO_CONFIRM_RINGBUF
template <class T>class RingBuf
{
public:
	RingBuf() = delete;
	RingBuf(T* pBufIn, size_t sizeIn)
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
	RingBuf(RingBuf& obj) = delete;
	RingBuf(RingBuf&& obj) = delete;
	~RingBuf()
	{
		delete[]ppBuf;
	}

	T* Pop()
	{
#ifndef NO_CONFIRM_RINGBUF
		try {
			if (front+1  < end)
			{
				throw std::runtime_error("Err! RingBuf.Pop (front&mask)+1 == (end&mask)\r\n"); // 例外送出
			}
		}
		catch (std::exception& e) {
			// 例外を捕捉
			// エラー理由を出力する
			std::cout << e.what() << std::endl;
			std::exit(1);
		}
#endif // !NO_CONFIRM_RINGBUF
		T** ppT = &ppBuf[end & mask];
		++end;
		return *ppT;
	}

	void Push(T* pT)
	{
#ifndef NO_CONFIRM_RINGBUF
		try {
			if (front + 1 == end +size)
			{
				throw std::runtime_error("Err! RingBuf.Push (front&mask) + 1 == (end&mask)\r\n"); // 例外送出
			}
		}
		catch (std::exception& e) {
			// 例外を捕捉
			// エラー理由を出力する
			std::cout << e.what() << "\r\n";
			std::exit(1);
		}
#endif // !NO_CONFIRM_RINGBUF
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

