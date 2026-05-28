#pragma once
#include <map>
#include <mutex>
#include "Salakhova_Session.h"

using namespace std;

struct SRLocal
{
    static map<int, Session*> sessions_map;
    static mutex mx;
};
