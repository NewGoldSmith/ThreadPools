
#include <iostream>
#include "RingBuf.h"
    char ch[] = "abcd";
    RingBuf<char> buf(ch, 4);

int main()
{
    using namespace std;
    char* pch = buf.Pull();
    char* pch2=buf.Pull();
    char* pch3 = buf.Pull();
    char* pch4 = buf.Pull();
    //buf.Push(pch4);
    //buf.Push(pch3);
    //buf.Push(pch2);
    //buf.Push(pch);
    //pch = buf.Pull();
    //pch2 = buf.Pull();
    //pch3 = buf.Pull();
    //pch4 = buf.Pull();
    cout << *pch << "\r\n";
    cout << *pch2 << "\r\n";
    cout << *pch3 << "\r\n";
    cout << *pch4 << "\r\n";
}
