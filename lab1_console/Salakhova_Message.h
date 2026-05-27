#pragma once
#include <string>
#include "Salakhova_Interfaces.h"

using namespace std;

enum MessageTypes
{
    MT_CLOSE   = 0,
    MT_DATA    = 1,
    MT_START   = 2,
    MT_STOP    = 3,
    MT_QUIT    = 4,
    MT_INIT    = 5,
    MT_CONFIRM = 6,
    MT_NODATA  = 7
};

constexpr int SR_ALL    = -1; // всем потокам (broadcast)
constexpr int SR_BROKER = -2; // серверу

struct MessageHeader
{
    int messageType;
    int size;
    int to;
    int from;
};

struct Message
{
    MessageHeader header = { 0 };
    wstring data;

    Message() = default;
    Message(MessageTypes messageType, const wstring& data = L"");
    Message(int to, MessageTypes messageType, const wstring& data = L"", int from = 0);

    // Strategy Pattern: сообщение делегирует транспортной стратегии
    void send(ITransport* transport);
    void receive(ITransport* transport);

    static void sendMessage(ITransport* transport, int to, MessageTypes messageType, const wstring& data = L"");
    static Message receiveMessage(ITransport* transport);
};