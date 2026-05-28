#include "pch.h"
#include "Salakhova_Exports.h"
#include "Salakhova_SocketTransport.h"

#include <windows.h>
#include <boost/asio.hpp>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <memory>
#include <string>

namespace
{
    using boost::asio::ip::tcp;

    boost::asio::io_context        g_io;
    std::shared_ptr<tcp::socket>   g_sock;
    std::mutex                     g_writeMx;
    std::mutex                     g_qMx;         // Мьютекс для очереди сообщений
    std::queue<Message>            g_inbox;       // Очередь входящих сообщений
    std::thread                    g_reader;      // Фоновый поток для чтения
    std::atomic<bool>              g_alive{ false };

    void readerLoop()
    {
        try
        {
            SocketTransport tr(*g_sock, g_writeMx);
            while (g_alive.load())
            {
                Message m;
                // Твой метод receive из Salakhova_Message.cpp
                m.receive(tr);
                {
                    std::lock_guard<std::mutex> lg(g_qMx);
                    g_inbox.push(std::move(m));
                }
            }
        }
        catch (...)
        {
            g_alive.store(false);
        }
    }
}

extern "C"
{
    __declspec(dllexport) bool Salakhova_Connect(const wchar_t* host, int port)
    {
        if (g_alive.load()) return true;

        try
        {
            std::string h;
            if (host)
            {
                while (*host) h.push_back(static_cast<char>(*host++));
            }
            else
            {
                h = "127.0.0.1";
            }

            g_sock = std::make_shared<tcp::socket>(g_io);
            tcp::resolver r(g_io);
            boost::asio::connect(*g_sock, r.resolve(h, std::to_string(port)));

            g_alive.store(true);
            g_reader = std::thread(readerLoop);
            return true;
        }
        catch (...)
        {
            g_sock.reset();
            g_alive.store(false);
            return false;
        }
    }

    __declspec(dllexport) void Salakhova_Disconnect()
    {
        g_alive.store(false);

        if (g_sock)
        {
            boost::system::error_code ec;
            g_sock->shutdown(tcp::socket::shutdown_both, ec);
            g_sock->close(ec);
        }

        if (g_reader.joinable()) g_reader.join();

        g_sock.reset();

        std::lock_guard<std::mutex> lg(g_qMx);
        std::queue<Message> empty;
        g_inbox.swap(empty); // Очищаем очередь
    }

    __declspec(dllexport) bool Salakhova_IsConnected()
    {
        return g_alive.load();
    }

    __declspec(dllexport) void Salakhova_Send(int target, int command, const wchar_t* text)
    {
        if (!g_alive.load() || !g_sock) return;

        try
        {
            std::wstring str = text ? text : L"";
            // Используем твой конструктор Message(int to, MessageTypes messageType, const wstring& data)
            Message m(target, static_cast<MessageTypes>(command), str);

            SocketTransport tr(*g_sock, g_writeMx);
            m.send(tr); // Твой метод send
        }
        catch (...)
        {
            g_alive.store(false);
        }
    }

    __declspec(dllexport) bool Salakhova_Poll(int* outCommand, int* outTarget, int* outSource, wchar_t* outText, int outCapacity)
    {
        std::lock_guard<std::mutex> lg(g_qMx);
        if (g_inbox.empty()) return false;

        Message m = std::move(g_inbox.front());
        g_inbox.pop();

        if (outCommand) *outCommand = m.header.messageType;
        if (outTarget)  *outTarget = m.header.to;
        if (outSource)  *outSource = m.header.from; // Передаем ID отправителя

        if (outText && outCapacity > 0)
        {
            int n = static_cast<int>(m.data.size());
            if (n >= outCapacity) n = outCapacity - 1;
            for (int i = 0; i < n; ++i) outText[i] = m.data[i];
            outText[n] = L'\0';
        }
        return true;
    }
}