//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php
#pragma once
#include <exception>
#include <semaphore>
#include <atomic>
//#define NO_CONFIRM_RINGBUF
//#define NO_SUPPORT_MULTI_THREAD_RINGBUF
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
#ifndef NO_SUPPORT_MULTI_THREAD_RINGBUF
		, sem(1)
#endif // !NO_SUPPORT_MULTI_THREAD_RINGBUF
	{
#ifndef NO_CONFIRM_RINGBUF
		try {

			if ((sizeIn & mask) != 0)
			{
				throw std::invalid_argument("Err! The RingBuf must be Power of Two.\r\n");
			}
		}
		catch (std::invalid_argument& e)
		{
			std::cerr << e.what();
			std::exception_ptr ep = std::current_exception();
			std::rethrow_exception(ep);
		}
#endif // !NO_CONFIRM_RINGBUF
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

	T* Pull()
	{
#ifndef NO_SUPPORT_MULTI_THREAD_RINGBUF
		sem.acquire();
#endif // !NO_SUPPORT_MULTI_THREAD_RINGBUF
#ifndef NO_CONFIRM_RINGBUF
		try {
			if (front+1  < end)
			{
				throw std::runtime_error("Err! RingBuf.Pull (front&mask)+1 == (end&mask)\r\n"); // 例外送出
			}
		}
		catch (std::exception& e) {
			// 例外を捕捉
			// エラー理由を出力する
			std::cerr << e.what() ;
			std::exception_ptr ep = std::current_exception();
			std::rethrow_exception(ep);
		}
#endif // !NO_CONFIRM_RINGBUF
		T** ppT = &ppBuf[end & mask];
		++end;
#ifndef NO_SUPPORT_MULTI_THREAD_RINGBUF
		sem.release();
#endif // !NO_SUPPORT_MULTI_THREAD_RINGBUF
		return *ppT;
	}

	void Push(T* pT)
	{
#ifndef NO_SUPPORT_MULTI_THREAD_RINGBUF
		sem.acquire();
#endif // !NO_SUPPORT_MULTI_THREAD_RINGBUF
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
			std::cerr << e.what() ;
			std::exception_ptr ep = std::current_exception();
			std::rethrow_exception(ep);
		}
#endif // !NO_CONFIRM_RINGBUF
		++front;
		ppBuf[front & mask] = pT;
#ifndef NO_SUPPORT_MULTI_THREAD_RINGBUF
		sem.release();
#endif // !NO_SUPPORT_MULTI_THREAD_RINGBUF
	}

protected:
	T** ppBuf;
	std::atomic_size_t size;
	std::atomic_size_t front;
	std::atomic_size_t end;
	std::atomic_size_t mask;
#ifndef NO_SUPPORT_MULTI_THREAD_RINGBUF
	std::binary_semaphore sem;
#endif // !NO_SUPPORT_MULTI_THREAD_RINGBUF

};

