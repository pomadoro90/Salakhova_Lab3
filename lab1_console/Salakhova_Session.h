#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include "Salakhova_Message.h"

using namespace std;

class Session
{
    queue<Message> messages;
    mutex mx;
    condition_variable cv;

public:
    int sessionID;

    Session(int sessionID);
    ~Session() = default;

    void addMessage(Message& m);
    bool getMessage(Message& m);
};