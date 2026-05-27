#pragma once
#include <windows.h>
#include <map>
#include <mutex>
#include <thread>
#include <vector>
#include "Salakhova_Session.h"
#include "Salakhova_Interfaces.h"

using namespace std;

// Локальная маршрутизация: SRLocal : public ITransport
class SRLocal : public ITransport
{
public:
    int id;

    static map<int, Session*> sessions_map;
    static mutex mx;
    static vector<thread> threads;
    static CRITICAL_SECTION threadOpMx;
    static bool csInited;

    SRLocal(int id = -1);

    virtual void send(Message& m) override;
    virtual void receive(Message& m) override;

    static void addThread(int sessionID);
    static void removeLastThread();
    static int  threadCount();
    static Session* getSession(int id);
};

void MyThread(LPVOID lpParameter);