#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <io.h>
#include <fcntl.h>
#include <conio.h>
#include <thread>
#include <memory>
#include <string>
#include <map>
#include <mutex>
#include <boost/asio.hpp>

#include "Salakhova_SysProgh.h"
#include "Salakhova_ThreadManager.h"
#include "Salakhova_ServerTransport.h"
#include "Salakhova_SocketTransport.h"

using boost::asio::ip::tcp;

namespace
{
    constexpr int kPort = 12345;
    int g_nextSessionId = 0;
    std::mutex g_sessionIdMx;

    wstring handleCommand(const Message& msg, bool& threadsChanged)
    {
        threadsChanged = false;

        if (msg.header.to == SR_BROKER)
        {
            switch (static_cast<MessageTypes>(msg.header.messageType))
            {
            case MT_START:
            {
                int newId;
                {
                    std::lock_guard<std::mutex> lg(g_sessionIdMx);
                    newId = g_nextSessionId++;
                }
                SRLocal::addThread(newId);
                threadsChanged = true;
                break;
            }
            case MT_STOP:
            {
                if (SRLocal::threadCount() > 0)
                {
                    SRLocal::removeLastThread();
                    threadsChanged = true;
                }
                break;
            }
            case MT_DATA:
                SafeWrite(L"[main]:", msg.data.c_str());
                break;

            default:
                break;
            }
        }
        else
        {
            if (msg.header.to == SR_ALL && msg.header.messageType == MT_DATA)
                SafeWrite(L"[main] broadcast:", msg.data.c_str());

            // Маршрутизация через SRLocal
            SRLocal local(msg.header.to);
            Message copy = msg;
            copy.send(&local);
        }

        return SRLocal::getThreadIds();
    }

    void clientWorker(std::shared_ptr<tcp::socket> sock)
    {
        int peerId = ServerTransport::add(sock);
        SafeWrite(L"[server] client", peerId, L"connected");

        // Отправляем список ID активных потоков при подключении
        wstring initialIds = SRLocal::getThreadIds();
        Message confirmMsg(SR_BROKER, MT_CONFIRM, initialIds, peerId);
        ServerTransport::sendTo(peerId, confirmMsg);

        std::mutex writeMx;

        try
        {
            SocketTransport tr(*sock, writeMx);

            while (true)
            {
                Message msg;
                msg.receive(&tr);

                bool threadsChanged = false;
                wstring ids = handleCommand(msg, threadsChanged);

                // Подтверждение каждому клиенту — список ID потоков
                Message confirm(SR_BROKER, MT_CONFIRM, ids, peerId);
                ServerTransport::sendTo(peerId, confirm);

                // Если количество потоков изменилось — оповестить всех остальных
                if (threadsChanged)
                {
                    Message bcast(SR_BROKER, MT_CONFIRM, ids);
                    ServerTransport::broadcast(bcast);
                }
            }
        }
        catch (...)
        {
            // Клиент отключился — нормально
        }

        ServerTransport::remove(peerId);
        SafeWrite(L"[server] client", peerId, L"disconnected");
    }
}

int wmain()
{
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    SafeWrite(L"[server] Салахова Lab3 server on port", kPort);

    try
    {
        boost::asio::io_context io;
        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), kPort));

        SafeWrite(L"[server] waiting for clients...");

        while (true)
        {
            auto sock = std::make_shared<tcp::socket>(io);
            acceptor.accept(*sock);
            std::thread(clientWorker, sock).detach();
        }
    }
    catch (std::exception& e)
    {
        std::wcerr << L"[server] fatal: " << e.what() << std::endl;
        SafeWrite(L"[server] press any key to exit...");
        _getwch();
    }

    return 0;
}