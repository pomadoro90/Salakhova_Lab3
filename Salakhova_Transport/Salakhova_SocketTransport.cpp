#include "pch.h"
#include "../lab1_console/Salakhova_Interfaces.h"
#include "../lab1_console/Salakhova_Message.h"
#include "../lab1_console/Salakhova_Message.cpp"
#include "Salakhova_SocketTransport.h"
#include "Salakhova_Exports.h"

namespace ba = boost::asio;

void SocketTransport::send(Message& m)
{
    std::lock_guard<std::mutex> lg(writeMx_);

    boost::system::error_code ec;
    ba::write(sock_, ba::buffer(&m.header, sizeof(MessageHeader)), ec);
    if (ec) throw boost::system::system_error(ec);

    if (m.header.size > 0 && !m.data.empty())
    {
        ba::write(sock_, ba::buffer(m.data.data(), m.header.size), ec);
        if (ec) throw boost::system::system_error(ec);
    }
}

void SocketTransport::receive(Message& m)
{
    boost::system::error_code ec;

    ba::read(sock_, ba::buffer(&m.header, sizeof(MessageHeader)), ec);
    if (ec) throw boost::system::system_error(ec);

    if (m.header.size > 0)
    {
        m.data.resize(m.header.size / sizeof(wchar_t));
        ba::read(sock_, ba::buffer(&m.data[0], m.header.size), ec);
        if (ec) throw boost::system::system_error(ec);
    }
    else
    {
        m.data.clear();
    }
}

// ===== Глобальное состояние DLL =====
namespace
{
    boost::asio::io_context        g_io;
    std::shared_ptr<ba::ip::tcp::socket> g_sock;
    std::mutex                      g_writeMx;
    std::mutex                      g_qMx;
    std::queue<Message>             g_inbox;
    std::thread                     g_reader;
    std::atomic<bool>               g_alive{ false };
    int                             g_lastErrCode = 0;

    void readerLoop()
    {
        try
        {
            SocketTransport tr(*g_sock, g_writeMx);
            while (g_alive.load())
            {
                Message m;
                m.receive(&tr);
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

// ===== Экспортируемые функции =====

extern "C" __declspec(dllexport) bool Salakhova_Connect(const wchar_t* host, int port)
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

        g_sock = std::make_shared<ba::ip::tcp::socket>(g_io);
        ba::ip::tcp::resolver resolver(g_io);
        ba::connect(*g_sock, resolver.resolve(h, std::to_string(port)));

        g_alive.store(true);
        g_lastErrCode = 0;
        g_reader = std::thread(readerLoop);
        return true;
    }
    catch (boost::system::system_error& ex)
    {
        g_lastErrCode = ex.code().value();
        g_sock.reset();
        g_alive.store(false);
        return false;
    }
    catch (...)
    {
        g_lastErrCode = -1;
        g_sock.reset();
        g_alive.store(false);
        return false;
    }
}

extern "C" __declspec(dllexport) void Salakhova_Disconnect()
{
    g_alive.store(false);

    if (g_sock)
    {
        boost::system::error_code ec;
        g_sock->shutdown(ba::ip::tcp::socket::shutdown_both, ec);
        g_sock->close(ec);
    }

    if (g_reader.joinable()) g_reader.join();
    g_sock.reset();

    std::lock_guard<std::mutex> lg(g_qMx);
    std::queue<Message> empty;
    g_inbox.swap(empty);
}

extern "C" __declspec(dllexport) bool Salakhova_IsConnected()
{
    return g_alive.load();
}

extern "C" __declspec(dllexport) void Salakhova_Send(int target, int command, const wchar_t* text)
{
    if (!g_alive.load() || !g_sock) return;

    try
    {
        std::wstring str = text ? text : L"";
        Message msg(target, static_cast<MessageTypes>(command), str);

        SocketTransport tr(*g_sock, g_writeMx);
        msg.send(&tr);
    }
    catch (...)
    {
        g_alive.store(false);
    }
}

extern "C" __declspec(dllexport) bool Salakhova_Poll(int* outCommand, int* outTarget, wchar_t* outText, int outCapacity)
{
    std::lock_guard<std::mutex> lg(g_qMx);
    if (g_inbox.empty()) return false;

    Message m = std::move(g_inbox.front());
    g_inbox.pop();

    if (outCommand) *outCommand = m.header.messageType;
    if (outTarget)  *outTarget  = m.header.to;

    if (outText && outCapacity > 0)
    {
        int n = static_cast<int>(m.data.size());
        if (n >= outCapacity) n = outCapacity - 1;
        for (int i = 0; i < n; ++i) outText[i] = m.data[i];
        outText[n] = L'\0';
    }
    return true;
}

extern "C" __declspec(dllexport) int Salakhova_LastErrorCode()
{
    return g_lastErrCode;
}