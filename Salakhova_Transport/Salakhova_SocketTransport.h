#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <boost/asio.hpp>
#include <mutex>
#include "../lab1_console/Salakhova_Interfaces.h"

class SocketTransport : public ITransport
{
    boost::asio::ip::tcp::socket& sock_;
    std::mutex&                    writeMx_;

public:
    SocketTransport(boost::asio::ip::tcp::socket& sock, std::mutex& writeMx)
        : sock_(sock), writeMx_(writeMx) {}

    virtual void send(Message& m) override;
    virtual void receive(Message& m) override;
};