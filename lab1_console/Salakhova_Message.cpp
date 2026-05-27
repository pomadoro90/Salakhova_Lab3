#include "Salakhova_Message.h"

Message::Message(MessageTypes messageType, const wstring& data)
    : data(data)
{
    header = { messageType, int(data.length() * sizeof(wchar_t)), 0, 0 };
}

Message::Message(int to, MessageTypes messageType, const wstring& data, int from)
    : data(data)
{
    header = { messageType, int(data.length() * sizeof(wchar_t)), to, from };
}

void Message::send(ITransport* transport)
{
    transport->send(*this);
}

void Message::receive(ITransport* transport)
{
    transport->receive(*this);
}

void Message::sendMessage(ITransport* transport, int to, MessageTypes messageType, const wstring& data)
{
    Message m(to, messageType, data);
    m.send(transport);
}

Message Message::receiveMessage(ITransport* transport)
{
    Message m;
    m.receive(transport);
    return m;
}