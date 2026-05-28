#pragma once
#include <map>
#include <mutex>
#include <thread>
#include <vector>
#include <memory>
#include "Salakhova_Session.h"
#include "Salakhova_Interfaces.h"

using namespace std;

// Локальная маршрутизация: SRLocal : public ITransport
class SRLocal : public ITransport
{
public:
    int id;

    static map<int, shared_ptr<Session>> sessions_map;
    static mutex mx;
    static vector<thread> threads;
    static mutex threadOpMx;

    SRLocal(int id = -1);

    virtual void send(Message& m) override;
    virtual void receive(Message& m) override;

    static void addThread(int sessionID);
    static void removeLastThread();
    static int  threadCount();
    static shared_ptr<Session> getSession(int id);
    static wstring getThreadIds();
};

void MyThread(shared_ptr<Session> session);