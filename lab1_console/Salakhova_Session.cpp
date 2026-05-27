#include "Salakhova_Session.h"

Session::Session(int sessionID)
    : sessionID(sessionID)
{
}

void Session::addMessage(Message& m)
{
    {
        lock_guard<mutex> lg(mx);
        messages.push(m);
    }
    cv.notify_one();
}

bool Session::getMessage(Message& m)
{
    unique_lock<mutex> ul(mx);
    if (cv.wait_for(ul, chrono::milliseconds(100), [&] { return !messages.empty(); }))
    {
        m = messages.front();
        messages.pop();
        return true;
    }
    return false;
}