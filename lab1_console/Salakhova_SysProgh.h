#pragma once

#include <windows.h>
#include <iostream>
#include <queue>
#include <vector>
#include <string>
#include <thread>
#include <fstream>
#include <tchar.h>

using namespace std;

inline void DoWrite()
{
    wcout << endl;
}

template <class T, typename... Args> inline void DoWrite(T& value, Args... args)
{
    wcout << value << L" ";
    DoWrite(args...);
}

static CRITICAL_SECTION cs;
static bool initCS = true;
template<typename... Args> inline void SafeWrite(Args... args)
{
    if (initCS)
    {
        InitializeCriticalSection(&cs);
        initCS = false;
    }
    EnterCriticalSection(&cs);
    DoWrite(args...);
    wcout.clear();
    LeaveCriticalSection(&cs);
}

#pragma warning(disable : 4302 4311 4312 6031)