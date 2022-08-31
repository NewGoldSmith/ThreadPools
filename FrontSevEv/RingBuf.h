//Copyright (c) 2022, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php
#pragma once
#include <synchapi.h>
#include <exception>
#include <semaphore>
#include <atomic>
#include <string>
#define MyTRACE(lpsz) OutputDebugStringA(lpsz);

//#define USING_CRITICAL_SECTION
//#define NO_CONFIRM_RINGBUF
//#define NOT_USING_SEMAPHORE_RINGBUF

#ifdef USING_CRITICAL_SECTION
#ifndef NOT_USING_SEMAPHORE_RINGBUF
#define NOT_USING_SEMAPHORE_RINGBUF
#endif
#endif
template <class T>class RingBuf
{
public:
	RingBuf() = delete;
	RingBuf(T* pBufIn, size_t sizeIn)
		:ppBuf(nullptr)
		, size(sizeIn)
		, front(0)
		, end(0)
		, mask(sizeIn - 1)
#ifdef USING_CRITICAL_SECTION
		, cs{}		
#endif // USING_CRITICAL_SECTION

#ifndef NOT_USING_SEMAPHORE_RINGBUF
		, sem(1)
#endif // !NOT_USING_SEMAPHORE_RINGBUF
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

#ifdef USING_CRITICAL_SECTION
		InitializeCriticalSectionAndSpinCount(&cs, 400);
#endif // USING_CRITICAL_SECTION

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
#ifdef USING_CRITICAL_SECTION
		EnterCriticalSection(&cs);
#endif // USING_CRITICAL_SECTION
#ifndef NOT_USING_SEMAPHORE_RINGBUF
		sem.acquire();
#endif // !NOT_USING_SEMAPHORE_RINGBUF
#ifndef NO_CONFIRM_RINGBUF
		MyTRACE(("Pull front" + to_string(front) + " end " + to_string(end) + "\r\n").c_str());
		try {
			if (front+size  < end)
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
#ifndef NOT_USING_SEMAPHORE_RINGBUF
		sem.release();
#endif // !NOT_USING_SEMAPHORE_RINGBUF
#ifdef USING_CRITICAL_SECTION
		LeaveCriticalSection(&cs);
#endif // USING_CRITICAL_SECTION
		return *ppT;
	}

	void Push(T* pT)
	{
#ifdef USING_CRITICAL_SECTION
		EnterCriticalSection(&cs);
#endif // USING_CRITICAL_SECTION
#ifndef NOT_USING_SEMAPHORE_RINGBUF
		sem.acquire();
#endif // !NOT_USING_SEMAPHORE_RINGBUF
#ifndef NO_CONFIRM_RINGBUF
		MyTRACE(("Push front " + to_string(front)+ " end " + to_string(end)+"\r\n").c_str());
		try {
			if (front  == end +size)
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
		ppBuf[front & mask] = pT;
		++front;
#ifndef NOT_USING_SEMAPHORE_RINGBUF
		sem.release();
#endif // !NOT_USING_SEMAPHORE_RINGBUF
#ifdef USING_CRITICAL_SECTION
		LeaveCriticalSection(&cs);
#endif // USING_CRITICAL_SECTION
	}

protected:
	T** ppBuf;
	std::atomic_size_t size;
	std::atomic_size_t front;
	std::atomic_size_t end;
	std::atomic_size_t mask;
#ifdef USING_CRITICAL_SECTION
	CRITICAL_SECTION cs;
#endif // USING_CRITICAL_SECTION

#ifndef NOT_USING_SEMAPHORE_RINGBUF
	std::binary_semaphore sem;
#endif // !NOT_USING_SEMAPHORE_RINGBUF

};

