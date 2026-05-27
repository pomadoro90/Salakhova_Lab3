#include "Salakhova_SocketTransport.h"
#include "Salakhova_Message.h"

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