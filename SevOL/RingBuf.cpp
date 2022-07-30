//Copyright (c) 2021, Gold Smith
//Released under the MIT license
//https ://opensource.org/licenses/mit-license.php

#include "RingBuf.h"


    //template<class T>
    //RingBuf<T>::RingBuf(T* pBufIn, size_t sizeIn)
    //    :ppBuf(nullptr)
    //    , size(sizeIn)
    //    , front(sizeIn - 1)
    //    , end(0)
    //    , mask(sizeIn - 1)
    //{
    //    ppBuf = new T * [sizeIn];
    //    for (size_t i(0); i < size; ++i)
    //    {

    //        ppBuf[i] = &pBufIn[i];
    //    }
    //}

    //template<class T>
    //RingBuf<T>::RingBuf(RingBuf& obj)
    //    :ppBuf(obj.ppBuf)
    //    ,size(obj.size)
    //    ,front(obj.front)
    //    ,end(obj.end)
    //    ,mask(obj.mask)
    //{
    //}

    //template<class T>
    //RingBuf<T>::~RingBuf()
    //{
    //    delete[]ppBuf;
    //}

    //template<class T>
    //T* RingBuf<T>::Pop()
    //{
    //    T** ppT = &ppBuf[end & mask];
    //    ++end;
    //    return *ppT;
    //}

    //template<class T>
    //void RingBuf<T>::Push(T* pT)
    //{
    //    ++front;
    //    ppBuf[front & mask] = pT;
    //}

