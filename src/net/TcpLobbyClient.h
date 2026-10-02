// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_TCP_LOBBY_CLIENT_H
#define OPENHOVER_TCP_LOBBY_CLIENT_H

#include <string>
#include <vector>

enum class TcpLobbyClientState
{
    Disconnected,
    Connecting,
    Connected,
    Failed
};

class TcpLobbyClient
{
public:
    ~TcpLobbyClient();

    bool Connect(const std::string& pHost, int pPort = 9700);
    void Disconnect();
    void Tick();
    bool SendCommand(const std::string& pCommand);

    TcpLobbyClientState State() const { return mState; }
    std::vector<std::string> TakeMessages();

private:
    void Fail();
    void FlushSendBuffer();
    void ReceiveMessages();

    int mSocket = -1;
    TcpLobbyClientState mState = TcpLobbyClientState::Disconnected;
    std::string mSendBuffer;
    std::string mReceiveBuffer;
    std::vector<std::string> mMessages;
};

#endif