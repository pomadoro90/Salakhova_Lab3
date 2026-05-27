#include "Salakhova_ServerTransport.h"
#include "Salakhova_SocketTransport.h"

std::map<int, ServerPeer> ServerTransport::peers;
std::mutex                 ServerTransport::peersMx;
int                        ServerTransport::nextPeerId = 1;

int ServerTransport::add(std::shared_ptr<tcp::socket> s)
{
    std::lock_guard<std::mutex> lg(peersMx);
    int id = nextPeerId++;
    peers[id] = { s, std::make_shared<std::mutex>() };
    return id;
}

void ServerTransport::remove(int id)
{
    std::lock_guard<std::mutex> lg(peersMx);
    peers.erase(id);
}

void ServerTransport::sendTo(int peerId, Message m)
{
    ServerPeer p;
    {
        std::lock_guard<std::mutex> lg(peersMx);
        auto it = peers.find(peerId);
        if (it == peers.end()) return;
        p = it->second;
    }

    try
    {
        SocketTransport tr(*p.sock, *p.writeMx);
        m.send(&tr);
    }
    catch (...)
    {
        remove(peerId);
    }
}

void ServerTransport::broadcast(Message m)
{
    std::vector<std::pair<int, ServerPeer>> snapshot;
    {
        std::lock_guard<std::mutex> lg(peersMx);
        snapshot.reserve(peers.size());
        for (auto& kv : peers) snapshot.push_back(kv);
    }

    for (auto& [id, p] : snapshot)
    {
        try
        {
            SocketTransport tr(*p.sock, *p.writeMx);
            Message copy = m;
            copy.send(&tr);
        }
        catch (...)
        {
            remove(id);
        }
    }
}