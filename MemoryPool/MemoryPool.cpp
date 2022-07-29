#include <iostream>
#include "MemoryPool.h"
#define MAX_CONTEXT 16000
Context pContext[MAX_CONTEXT];
std::vector<Context*> Pool=[]();
std::vector<Context*> Connected;
int main()
{
    for (int i = 0; i < MAX_CONTEXT; ++i)
    {
        Pool.push_back(&pContext[i]);
    }
    Context* p = Pool.front();
    Pool.erase(Pool.begin());
    Connected.push_back(p);
    p = Connected.front();
    Connected.erase(Connected.begin());
    Pool.push_back(p);
}

