#include "Salakhova_ThreadManager.h"
#include "Salakhova_SysProgh.h"
#include <iostream>
#include <fstream>

map<int, Session*> SRLocal::sessions_map;
mutex SRLocal::mx;
vector<thread> SRLocal::threads;
CRITICAL_SECTION SRLocal::threadOpMx;
bool SRLocal::csInited = false;

SRLocal::SRLocal(int id) : id(id) {}

void SRLocal::send(Message& m)
{
    lock_guard<mutex> lg(mx);
    if (id < 0) // broadcast
    {
        for (auto& pair : sessions_map)
            pair.second->addMessage(m);
    }
    else
    {
        if (sessions_map.find(m.header.to) != sessions_map.end())
            sessions_map[m.header.to]->addMessage(m);
    }
}

void SRLocal::receive(Message& m)
{
    Session* targetSession = nullptr;
    {
        lock_guard<mutex> lg(mx);
        targetSession = sessions_map[id];
    }

    if (targetSession != nullptr)
    {
        targetSession->getMessage(m);
    }
}

Session* SRLocal::getSession(int id)
{
    lock_guard<mutex> lg(mx);
    auto it = sessions_map.find(id);
    return (it != sessions_map.end()) ? it->second : nullptr;
}

int SRLocal::threadCount()
{
    lock_guard<mutex> lg(mx);
    return (int)sessions_map.size();
}

void SRLocal::addThread(int sessionID)
{
    if (!csInited)
    {
        InitializeCriticalSection(&threadOpMx);
        csInited = true;
    }
    EnterCriticalSection(&threadOpMx);
    threads.emplace_back(MyThread, (LPVOID)new Session(sessionID));
    LeaveCriticalSection(&threadOpMx);
}

void SRLocal::removeLastThread()
{
    if (!csInited)
    {
        InitializeCriticalSection(&threadOpMx);
        csInited = true;
    }
    EnterCriticalSection(&threadOpMx);

    if (threads.empty())
    {
        LeaveCriticalSection(&threadOpMx);
        return;
    }

    int idToClose = -1;
    {
        lock_guard<mutex> lg(mx);
        if (!sessions_map.empty())
            idToClose = sessions_map.rbegin()->first;
    }

    if (idToClose >= 0)
    {
        Session* s = getSession(idToClose);
        if (s)
        {
            Message m(idToClose, MT_CLOSE);
            s->addMessage(m);
        }
    }

    if (threads.back().joinable())
        threads.back().join();
    threads.pop_back();

    LeaveCriticalSection(&threadOpMx);
}

void MyThread(LPVOID lpParameter)
{
    auto* session = static_cast<Session*>(lpParameter);
    int id = session->sessionID;

    // Добавляем сессию в карту
    {
        lock_guard<mutex> lg(SRLocal::mx);
        SRLocal::sessions_map[id] = session;
    }

    SafeWrite(L"session", id, L"is created.");

    while (true)
    {
        SRLocal local(id);
        Message m = Message::receiveMessage(&local);

        switch (m.header.messageType)
        {
        case MT_CLOSE:
        {
            SafeWrite(L"session", id, L"is closed.");

            {
                lock_guard<mutex> lg(SRLocal::mx);
                SRLocal::sessions_map.erase(id);
            }

            delete session;
            return;
        }
        case MT_DATA:
        {
            wstring fileName = to_wstring(id) + L".txt";
            wofstream fout(fileName, ios::app);
            fout.imbue(locale("ru_RU.UTF-8"));
            fout << m.data << endl;
            fout.close();

            SafeWrite(L"session", id, L"wrote data to file.");
        }
        break;
        }
    }
}