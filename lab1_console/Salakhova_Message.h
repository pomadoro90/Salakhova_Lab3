#pragma once
#include <string>
#include "Salakhova_Interfaces.h"

using namespace std;

enum MessageTypes
{
    MT_CLOSE,    // 0
    MT_DATA,     // 1
    MT_START,    // 2
    MT_STOP,     // 3
    MT_QUIT,     // 4
    MT_INFO,     // 5
    MT_CONFIRM,  // 6
    MT_GETDATA,  // 7 — клиент запрашивает данные у брокера
    MT_NODATA    // 8 — брокер отвечает: данных нет
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