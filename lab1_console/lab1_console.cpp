#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <boost/asio.hpp>
#include <windows.h>
#include <iostream>
#include <thread>
#include <map>
#include <mutex>
#include <vector>
#include <chrono>

#include "Salakhova_ThreadManager.h"
#include "Salakhova_SysProgh.h"
#include "Salakhova_Session.h"
#include "../Salakhova_Transport/Salakhova_SocketTransport.h"

using namespace boost::asio;
using boost::asio::ip::tcp;
using namespace std;

// Глобальный счетчик для выдачи ID клиентам
int nextClientId = 1;

void BroadcastClientList()
{
    lock_guard<mutex> lg(SRLocal::mx);
    wstring list;
    for (auto& pair : SRLocal::sessions_map)
    {
        if (!list.empty()) list += L';';
        list += to_wstring(pair.first) + L':' + pair.second->clientName;
    }
    for (auto& pair : SRLocal::sessions_map)
    {
        Message bcast(ADDR_SERVER, MT_CONFIRM, list);
        pair.second->addMessage(bcast);
    }
}

void SessionSender(Session* session, shared_ptr<tcp::socket> sock, shared_ptr<mutex> writeMx)
{
    int id = session->sessionID;
    SocketTransport tr(*sock, *writeMx);

    while (true)
    {
        Message m;
        if (session->getMessage(m))
        {
            if (m.header.messageType == MT_CLOSE)
                break; // Команда на завершение потока

            try { m.send(tr); }
            catch (...) { break; } // Если сеть отвалилась
        }
        else
        {
            // Если сообщений долго нет, проверяем, жива ли еще сессия
            lock_guard<mutex> lg(SRLocal::mx);
            if (SRLocal::sessions_map.find(id) == SRLocal::sessions_map.end())
                break;
        }
    }
}

void ClientWorker(shared_ptr<tcp::socket> sock)
{
    int clientId;
    shared_ptr<mutex> writeMx = make_shared<mutex>();
    wstring clientName = L"Client";

    {
        lock_guard<mutex> lg(SRLocal::mx);
        clientId = nextClientId++;
        Session* s = new Session(clientId, clientName);
        s->setSocket(sock);
        SRLocal::sessions_map[clientId] = s;

        // Запускаем поток-писатель в современном стиле C++
        thread t(SessionSender, s, sock, writeMx);
        t.detach();
    }

    SafeWrite("Client", clientId, "connected.");

    // Отправляем клиенту его собственный ID
    {
        Message idMsg(ADDR_SERVER, MT_CONFIRM, to_wstring(clientId));
        lock_guard<mutex> lg(SRLocal::mx);
        if (SRLocal::sessions_map.find(clientId) != SRLocal::sessions_map.end())
            SRLocal::sessions_map[clientId]->addMessage(idMsg);
    }

    try
    {
        SocketTransport tr(*sock, *writeMx);

        // Рассылаем всем новый список клиентов
        BroadcastClientList();

        while (true)
        {
            Message m;
            m.receive(tr); // Ожидаем сообщение от клиента

            {
                lock_guard<mutex> lg(SRLocal::mx);
                if (SRLocal::sessions_map.find(clientId) != SRLocal::sessions_map.end())
                    SRLocal::sessions_map[clientId]->updateActivity();
            }

            if (m.header.messageType == MT_QUIT)
            {
                break; // Клиент сам захотел отключиться
            }
            else if (m.header.messageType == MT_INFO)
            {
                // Регулярный пинг от клиента. Отвечаем ему свежим списком.
                lock_guard<mutex> lg(SRLocal::mx);
                wstring currentList;
                for (auto& pair : SRLocal::sessions_map)
                {
                    if (!currentList.empty()) currentList += L';';
                    currentList += to_wstring(pair.first) + L':' + pair.second->clientName;
                }
                Message confirm(ADDR_SERVER, MT_CONFIRM, currentList);
                if (SRLocal::sessions_map.find(clientId) != SRLocal::sessions_map.end())
                    SRLocal::sessions_map[clientId]->addMessage(confirm);
            }
            else if (m.header.messageType == MT_DATA)
            {
                if (m.header.to == ADDR_BROADCAST) // Всем (Broadcast)
                {
                    SafeWrite("Client", clientId, "sent broadcast.");
                    lock_guard<mutex> lg(SRLocal::mx);
                    for (auto& pair : SRLocal::sessions_map)
                    {
                        if (pair.first != clientId) // Не отправляем самому себе
                        {
                            Message copy = m;
                            copy.header.from = clientId; // Обязательно указываем отправителя
                            pair.second->addMessage(copy);
                        }
                    }
                }
                else // Конкретному адресату
                {
                    SafeWrite("Client", clientId, "sent message to", m.header.to);
                    lock_guard<mutex> lg(SRLocal::mx);
                    if (SRLocal::sessions_map.find(m.header.to) != SRLocal::sessions_map.end())
                    {
                        Message copy = m;
                        copy.header.from = clientId; // Обязательно указываем отправителя
                        SRLocal::sessions_map[m.header.to]->addMessage(copy);
                    }
                }
            }
        }
    }
    catch (...) {}

    SafeWrite("Client", clientId, "disconnected.");
    {
        lock_guard<mutex> lg(SRLocal::mx);
        if (SRLocal::sessions_map.find(clientId) != SRLocal::sessions_map.end())
        {
            Message closeMsg(ADDR_SERVER, MT_CLOSE, L"");
            SRLocal::sessions_map[clientId]->addMessage(closeMsg);
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    {
        lock_guard<mutex> lg(SRLocal::mx);
        if (SRLocal::sessions_map.find(clientId) != SRLocal::sessions_map.end())
        {
            delete SRLocal::sessions_map[clientId];
            SRLocal::sessions_map.erase(clientId);
        }
    }

    // Рассылаем остальным измененный список
    BroadcastClientList();
}

void TimeoutMonitor()
{
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(SERVER_TIMEOUT_CHECK_SEC)); // Проверяем каждые 5 секунд

        vector<int> timedOutClients;
        {
            lock_guard<mutex> lg(SRLocal::mx);
            for (auto& pair : SRLocal::sessions_map)
            {
                // Если клиент не пинговал 30 секунд — он "отвалился"
                if (pair.second->isTimedOut(SERVER_TIMEOUT_SECONDS))
                {
                    timedOutClients.push_back(pair.first);
                }
            }
        }

        if (!timedOutClients.empty())
        {
            for (int id : timedOutClients)
            {
                SafeWrite("Timeout alert: Client", id, "disconnected due to inactivity.");

                lock_guard<mutex> lg(SRLocal::mx);
                if (SRLocal::sessions_map.find(id) != SRLocal::sessions_map.end())
                {
                    SRLocal::sessions_map[id]->closeSocket();
                    Message closeMsg(ADDR_SERVER, MT_CLOSE, L"");
                    SRLocal::sessions_map[id]->addMessage(closeMsg);
                    delete SRLocal::sessions_map[id];
                    SRLocal::sessions_map.erase(id);
                }
            }

            // Оповещаем живых клиентов, что кто-то отвалился по таймауту
            BroadcastClientList();
        }
    }
}

int main()
{
    setlocale(LC_ALL, "Russian");
    SafeWrite("Message Broker Server started. Port: 12345");

    // Запускаем фоновый поток для проверки таймаутов
    thread(TimeoutMonitor).detach();

    try
    {
        io_context io;
        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 12345));

        while (true)
        {
            auto sock = make_shared<tcp::socket>(io);
            acceptor.accept(*sock);

            // Запускаем независимый обработчик для каждого нового клиента
            thread(ClientWorker, sock).detach();
        }
    }
    catch (exception& e)
    {
        cerr << "Server error: " << e.what() << endl;
    }

    return 0;
}