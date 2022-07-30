
#include <iostream>
#include "RingBuf.h"
    char ch[] = "abcd";
    RingBuf<char> buf(ch, 4);

int main()
{
    using namespace std;
    char* pch = buf.Pop();
    char* pch2=buf.Pop();
    char* pch3 = buf.Pop();
    char* pch4 = buf.Pop();
    buf.Push(pch4);
    buf.Push(pch3);
    buf.Push(pch2);
    buf.Push(pch);
    pch = buf.Pop();
    pch2 = buf.Pop();
    pch3 = buf.Pop();
    pch4 = buf.Pop();
    cout << *pch << "\r\n";
    cout << *pch2 << "\r\n";
    cout << *pch3 << "\r\n";
    cout << *pch4 << "\r\n";
}
