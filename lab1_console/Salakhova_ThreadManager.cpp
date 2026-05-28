#include "Salakhova_ThreadManager.h"
#include "Salakhova_SysProgh.h"
#include <iostream>
#include <fstream>
#include <sstream>

map<int, shared_ptr<Session>> SRLocal::sessions_map;
mutex SRLocal::mx;
vector<thread> SRLocal::threads;
mutex SRLocal::threadOpMx;

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
        auto it = sessions_map.find(m.header.to);
        if (it != sessions_map.end())
            it->second->addMessage(m);
    }
}

void SRLocal::receive(Message& m)
{
    shared_ptr<Session> targetSession;
    {
        lock_guard<mutex> lg(mx);
        auto it = sessions_map.find(id);
        if (it != sessions_map.end())
            targetSession = it->second;
    }

    if (targetSession)
    {
        targetSession->getMessage(m);
    }
}

shared_ptr<Session> SRLocal::getSession(int id)
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

wstring SRLocal::getThreadIds()
{
    lock_guard<mutex> lg(mx);
    if (sessions_map.empty()) return L"";

    wostringstream oss;
    bool first = true;
    for (const auto& pair : sessions_map)
    {
        if (!first) oss << L",";
        oss << pair.first;
        first = false;
    }
    return oss.str();
}

void SRLocal::addThread(int sessionID)
{
    auto s = make_shared<Session>(sessionID);
    {
        lock_guard<mutex> lg(mx);
        sessions_map[sessionID] = s;
    }

    lock_guard<mutex> lg(threadOpMx);
    threads.emplace_back(MyThread, s);
}

void SRLocal::removeLastThread()
{
    int idToClose = -1;

    {
        lock_guard<mutex> lg(mx);
        if (sessions_map.empty()) return;
        idToClose = sessions_map.rbegin()->first;
    }

    auto s = getSession(idToClose);
    if (s)
    {
        Message m(idToClose, MT_CLOSE);
        s->addMessage(m);
    }

    lock_guard<mutex> lg(threadOpMx);
    if (threads.back().joinable())
        threads.back().join();
    threads.pop_back();
}

void MyThread(shared_ptr<Session> session)
{
    int id = session->sessionID;

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