#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <memory>
#include <boost/asio.hpp>
#include "Salakhova_Message.h"

using namespace std;
using boost::asio::ip::tcp;

class Session
{
    queue<Message> messages;
    mutex mtx;
    condition_variable cv;
    wstring clientName;
    chrono::steady_clock::time_point lastActivity;
    shared_ptr<tcp::socket> sock_;
    shared_ptr<mutex> writeMx_;

public:
    int sessionID;

    Session(int sessionID);
    Session(int sessionID, const wstring& name);
    ~Session() = default;

    void addMessage(Message& m);
    bool getMessage(Message& m);
    void updateActivity();
    bool isTimedOut(int timeoutSeconds) const;
    void setSocket(shared_ptr<tcp::socket> s, shared_ptr<mutex> mx);
    void closeSocket();
};
