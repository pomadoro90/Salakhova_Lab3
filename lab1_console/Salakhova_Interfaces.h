#pragma once

struct Message;

// Транспортная стратегия (Strategy Pattern) — как в Lab5 Вельгана
// Единый интерфейс для локальной маршрутизации и сокетного I/O
class ITransport
{
public:
    virtual void send(Message& m) = 0;
    virtual void receive(Message& m) = 0;
    virtual ~ITransport() = default;
};