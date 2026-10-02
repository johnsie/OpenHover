// SPDX-License-Identifier: MIT OR Apache-2.0
#include <cerrno>
#include <cstddef>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "TcpLobbyClient.h"

namespace
{
const std::size_t kMaximumBufferSize = 16384;

#ifdef _WIN32
bool InitializeSocketLibrary()
{
    static const bool initialized = []()
    {
        WSADATA data = {};
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    return initialized;
}

SOCKET NativeSocket(std::intptr_t pSocket)
{
    return static_cast<SOCKET>(pSocket);
}

bool SetNonBlocking(std::intptr_t pSocket)
{
    u_long enabled = 1;
    return ioctlsocket(NativeSocket(pSocket), FIONBIO, &enabled) == 0;
}

void CloseSocket(std::intptr_t pSocket)
{
    closesocket(NativeSocket(pSocket));
}

bool IsConnectInProgress()
{
    const int error = WSAGetLastError();
    return error == WSAEINPROGRESS || error == WSAEWOULDBLOCK;
}

bool IsWouldBlock()
{
    const int error = WSAGetLastError();
    return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS;
}

bool GetSocketError(std::intptr_t pSocket, int& pError)
{
    int errorLength = sizeof(pError);
    return getsockopt(NativeSocket(pSocket), SOL_SOCKET, SO_ERROR,
                      reinterpret_cast<char*>(&pError), &errorLength) == 0;
}

const int kSendFlags = 0;
#else
bool InitializeSocketLibrary()
{
    return true;
}

int NativeSocket(std::intptr_t pSocket)
{
    return static_cast<int>(pSocket);
}

bool SetNonBlocking(std::intptr_t pSocket)
{
    const int flags = fcntl(NativeSocket(pSocket), F_GETFL, 0);
    return flags >= 0 && fcntl(NativeSocket(pSocket), F_SETFL, flags | O_NONBLOCK) == 0;
}

void CloseSocket(std::intptr_t pSocket)
{
    close(NativeSocket(pSocket));
}

bool IsConnectInProgress()
{
    return errno == EINPROGRESS;
}

bool IsWouldBlock()
{
    return errno == EAGAIN || errno == EWOULDBLOCK;
}

bool GetSocketError(std::intptr_t pSocket, int& pError)
{
    socklen_t errorLength = sizeof(pError);
    return getsockopt(NativeSocket(pSocket), SOL_SOCKET, SO_ERROR, &pError, &errorLength) == 0;
}

const int kSendFlags = MSG_NOSIGNAL;
#endif
}

TcpLobbyClient::~TcpLobbyClient()
{
    Disconnect();
}

bool TcpLobbyClient::Connect(const std::string& pHost, int pPort)
{
    Disconnect();
    if (pHost.empty() || pPort < 1 || pPort > 65535 || !InitializeSocketLibrary())
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
        const std::intptr_t socketHandle = static_cast<std::intptr_t>(
            socket(result->ai_family, result->ai_socktype, result->ai_protocol));
        if (socketHandle < 0)
            continue;
        if (!SetNonBlocking(socketHandle))
        {
            CloseSocket(socketHandle);
            continue;
        }
        const int resultCode = connect(NativeSocket(socketHandle), result->ai_addr, result->ai_addrlen);
        if (resultCode == 0 || IsConnectInProgress())
        {
            mSocket = socketHandle;
            mState = resultCode == 0 ? TcpLobbyClientState::Connected : TcpLobbyClientState::Connecting;
            freeaddrinfo(results);
            return true;
        }
        CloseSocket(socketHandle);
    }
    freeaddrinfo(results);
    return false;
}

void TcpLobbyClient::Disconnect()
{
    if (mSocket >= 0)
        CloseSocket(mSocket);
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
        FD_SET(NativeSocket(mSocket), &writable);
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
        if (!GetSocketError(mSocket, error) || error != 0)
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
        CloseSocket(mSocket);
    mSocket = -1;
    mState = TcpLobbyClientState::Failed;
    mSendBuffer.clear();
    mReceiveBuffer.clear();
}

void TcpLobbyClient::FlushSendBuffer()
{
    while (!mSendBuffer.empty())
    {
        const std::ptrdiff_t sent = send(NativeSocket(mSocket), mSendBuffer.data(),
                         static_cast<int>(mSendBuffer.size()), kSendFlags);
        if (sent > 0)
        {
            mSendBuffer.erase(0, static_cast<std::size_t>(sent));
            continue;
        }
        if (sent < 0 && IsWouldBlock())
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
        const std::ptrdiff_t received = recv(NativeSocket(mSocket), buffer, sizeof(buffer), 0);
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
        else if (!IsWouldBlock())
            Fail();
        return;
    }
}