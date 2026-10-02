// SPDX-License-Identifier: MIT OR Apache-2.0
#include "TcpLobbyClient.h"

#include <cerrno>
#include <cstring>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
const std::size_t kMaximumBufferSize = 16384;
}

TcpLobbyClient::~TcpLobbyClient()
{
    Disconnect();
}

bool TcpLobbyClient::Connect(const std::string& pHost, int pPort)
{
    Disconnect();
    if (pHost.empty() || pPort < 1 || pPort > 65535)
        return false;
    addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* results = nullptr;
    const std::string port = std::to_string(pPort);
    if (getaddrinfo(pHost.c_str(), port.c_str(), &hints, &results) != 0)
        return false;
    for (addrinfo* result = results; result != nullptr; result = result->ai_next)
    {
        const int socketHandle = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (socketHandle < 0)
            continue;
        const int flags = fcntl(socketHandle, F_GETFL, 0);
        if (flags < 0 || fcntl(socketHandle, F_SETFL, flags | O_NONBLOCK) < 0)
        {
            close(socketHandle);
            continue;
        }
        const int resultCode = connect(socketHandle, result->ai_addr, result->ai_addrlen);
        if (resultCode == 0 || errno == EINPROGRESS)
        {
            mSocket = socketHandle;
            mState = resultCode == 0 ? TcpLobbyClientState::Connected : TcpLobbyClientState::Connecting;
            freeaddrinfo(results);
            return true;
        }
        close(socketHandle);
    }
    freeaddrinfo(results);
    return false;
}

void TcpLobbyClient::Disconnect()
{
    if (mSocket >= 0)
        close(mSocket);
    mSocket = -1;
    mState = TcpLobbyClientState::Disconnected;
    mSendBuffer.clear();
    mReceiveBuffer.clear();
}

void TcpLobbyClient::Tick()
{
    if (mSocket < 0 || mState == TcpLobbyClientState::Failed)
        return;
    if (mState == TcpLobbyClientState::Connecting)
    {
        fd_set writable;
        FD_ZERO(&writable);
        FD_SET(mSocket, &writable);
        timeval timeout = {0, 0};
        const int selected = select(mSocket + 1, nullptr, &writable, nullptr, &timeout);
        if (selected < 0)
        {
            Fail();
            return;
        }
        if (selected == 0)
            return;
        int error = 0;
        socklen_t errorLength = sizeof(error);
        if (getsockopt(mSocket, SOL_SOCKET, SO_ERROR, &error, &errorLength) != 0 || error != 0)
        {
            Fail();
            return;
        }
        mState = TcpLobbyClientState::Connected;
    }
    FlushSendBuffer();
    ReceiveMessages();
}

bool TcpLobbyClient::SendCommand(const std::string& pCommand)
{
    if (mState != TcpLobbyClientState::Connected || pCommand.empty()
        || pCommand.find_first_of("\r\n") != std::string::npos
        || mSendBuffer.size() + pCommand.size() + 1 > kMaximumBufferSize)
        return false;
    mSendBuffer += pCommand;
    mSendBuffer += '\n';
    return true;
}

std::vector<std::string> TcpLobbyClient::TakeMessages()
{
    std::vector<std::string> messages;
    messages.swap(mMessages);
    return messages;
}

void TcpLobbyClient::Fail()
{
    if (mSocket >= 0)
        close(mSocket);
    mSocket = -1;
    mState = TcpLobbyClientState::Failed;
    mSendBuffer.clear();
    mReceiveBuffer.clear();
}

void TcpLobbyClient::FlushSendBuffer()
{
    while (!mSendBuffer.empty())
    {
        const ssize_t sent = send(mSocket, mSendBuffer.data(), mSendBuffer.size(), MSG_NOSIGNAL);
        if (sent > 0)
        {
            mSendBuffer.erase(0, static_cast<std::size_t>(sent));
            continue;
        }
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return;
        Fail();
        return;
    }
}

void TcpLobbyClient::ReceiveMessages()
{
    char buffer[1024];
    while (mState == TcpLobbyClientState::Connected)
    {
        const ssize_t received = recv(mSocket, buffer, sizeof(buffer), 0);
        if (received > 0)
        {
            mReceiveBuffer.append(buffer, static_cast<std::size_t>(received));
            if (mReceiveBuffer.size() > kMaximumBufferSize)
            {
                Fail();
                return;
            }
            std::size_t lineEnd = 0;
            while ((lineEnd = mReceiveBuffer.find('\n')) != std::string::npos)
            {
                std::string line = mReceiveBuffer.substr(0, lineEnd);
                mReceiveBuffer.erase(0, lineEnd + 1);
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                mMessages.push_back(line);
            }
            continue;
        }
        if (received == 0)
            Fail();
        else if (errno != EAGAIN && errno != EWOULDBLOCK)
            Fail();
        return;
    }
}