#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <boost/asio.hpp>
#include <map>
#include <memory>
#include <mutex>
#include "Salakhova_Message.h"
#include "Salakhova_Interfaces.h"

using boost::asio::ip::tcp;

// Управление сокетными подключениями клиентов на сервере
struct ServerPeer
{
    std::shared_ptr<tcp::socket> sock;
    std::shared_ptr<std::mutex>  writeMx;
};

class ServerTransport
{
public:
    static std::map<int, ServerPeer> peers;
    static std::mutex                 peersMx;
    static int                        nextPeerId;

    static int  add(std::shared_ptr<tcp::socket> s);
    static void remove(int id);
    static void sendTo(int peerId, Message m);
    static void broadcast(Message m);
};