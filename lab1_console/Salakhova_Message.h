#pragma once
#include <string>
#include "Salakhova_Interfaces.h"

using namespace std;

enum MessageTypes
{
    MT_CLOSE,
    MT_DATA,
    MT_START,
    MT_STOP,
    MT_QUIT,
    MT_INFO,
    MT_CONFIRM
};

// Специальные адресаты
constexpr int ADDR_BROADCAST = -1;  // Отправить всем
constexpr int ADDR_SERVER    = -2;  // Системное сообщение от/к серверу

// Таймауты (секунды)
constexpr int SERVER_TIMEOUT_SECONDS    = 30;
constexpr int SERVER_TIMEOUT_CHECK_SEC  =  5;

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
    Message(int to, MessageTypes messageType, const wstring& data = L"");

    void send(const ITransport& transport);
    void receive(const ITransport& transport);

    static void sendMessage(const ITransport& transport, int to, MessageTypes messageType, const wstring& data = L"");
    static Message receiveMessage(const ITransport& transport);
};