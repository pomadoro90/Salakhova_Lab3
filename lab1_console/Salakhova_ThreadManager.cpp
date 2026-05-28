#include "Salakhova_ThreadManager.h"

map<int, Session*> SRLocal::sessions_map;
mutex SRLocal::mx;
