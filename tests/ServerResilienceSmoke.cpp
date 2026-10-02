// SPDX-License-Identifier: MIT OR Apache-2.0
// Real-server checks for the failures players meet: a racer dropping out mid-race, and the whole
// server being restarted underneath connected clients.
#include "Protocol.h"
#include "TcpLobbyClient.h"

#include <csignal>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace
{
bool HasPrefix(const std::vector<std::string>& pMessages, const std::string& pPrefix)
{
    for (const std::string& message : pMessages)
    {
        if (message.compare(0, pPrefix.size(), pPrefix) == 0)
            return true;
    }
    return false;
}

// Reads from pClient until a message with the prefix arrives (everything seen is kept in pMessages).
bool WaitFor(TcpLobbyClient& pClient, std::vector<std::string>& pMessages, const std::string& pPrefix,
             int pAttempts = 500)
{
    for (int attempt = 0; attempt < pAttempts; ++attempt)
    {
        pClient.Tick();
        for (const std::string& message : pClient.TakeMessages())
            pMessages.push_back(message);
        if (HasPrefix(pMessages, pPrefix))
            return true;
        if (pClient.State() == TcpLobbyClientState::Failed)
            return false;
        usleep(10000);
    }
    return false;
}

bool WaitForFailure(TcpLobbyClient& pClient, int pAttempts = 500)
{
    for (int attempt = 0; attempt < pAttempts; ++attempt)
    {
        pClient.Tick();
        pClient.TakeMessages();
        if (pClient.State() == TcpLobbyClientState::Failed)
            return true;
        usleep(10000);
    }
    return false;
}

bool Hello(TcpLobbyClient& pClient, int pPort, const std::string& pName, std::vector<std::string>& pMessages)
{
    for (int attempt = 0; attempt < 200; ++attempt)
    {
        pMessages.clear();
        if (pClient.Connect("127.0.0.1", pPort) && WaitFor(pClient, pMessages, "OPENHOVER ", 100)
            && pClient.SendCommand("HELLO " + std::to_string(kOpenHoverProtocolVersion) + "|"
                                   + std::to_string(kOpenHoverContentVersion) + "|" + pName)
            && WaitFor(pClient, pMessages, "WELCOME ", 100))
            return true;
        pClient.Disconnect();
        usleep(20000);
    }
    return false;
}

pid_t StartServer(const char* pPath, int pPort)
{
    const std::string portText = std::to_string(pPort);
    const pid_t child = fork();
    if (child == 0)
    {
        execl(pPath, pPath, "--port", portText.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    return child;
}

void StopServer(pid_t pChild)
{
    kill(pChild, SIGTERM);
    waitpid(pChild, nullptr, 0);
}
}

int main(int pArgumentCount, char* pArguments[])
{
    if (pArgumentCount != 2)
    {
        std::cerr << "server path argument missing\n";
        return 1;
    }
    bool ok = true;
    const auto expect = [&](bool pCondition, const char* pMessage)
    {
        if (!pCondition)
        {
            std::cerr << pMessage << '\n';
            ok = false;
        }
    };
    const int port = 20000 + static_cast<int>((getpid() + 4241) % 20000);

    pid_t server = StartServer(pArguments[1], port);
    TcpLobbyClient host;
    TcpLobbyClient guest;
    std::vector<std::string> hostMessages;
    std::vector<std::string> guestMessages;
    if (!Hello(host, port, "Host", hostMessages) || !Hello(guest, port, "Guest", guestMessages))
    {
        std::cerr << "could not connect two clients\n";
        StopServer(server);
        return 1;
    }

    // Two players start a race.
    host.SendCommand("CREATE Resilience|0|0|3|2|0|1|0");
    expect(WaitFor(host, hostMessages, "ROOM "), "the host creates a room");
    guest.SendCommand("JOIN 1");
    expect(WaitFor(guest, guestMessages, "JOINED 1"), "the guest joins");
    // Wait for a lobby update that arrives after READY, so the host starts only once the guest is
    // really ready.
    guestMessages.clear();
    guest.SendCommand("READY 1");
    bool bothReady = false;
    for (int attempt = 0; attempt < 500 && !bothReady; ++attempt)
    {
        guest.Tick();
        for (const std::string& message : guest.TakeMessages())
        {
            // The room entry ends with the ready count and the track hash ("-" for built-in).
            const std::string ending = ",2,-";
            bothReady = bothReady || (message.compare(0, 5, "LOBBY") == 0 && message.size() > ending.size()
                                      && message.compare(message.size() - ending.size(), ending.size(), ending) == 0);
        }
        usleep(10000);
    }
    expect(bothReady, "a lobby update shows both players ready");
    hostMessages.clear();
    host.SendCommand("START 1");
    expect(WaitFor(host, hostMessages, "RACE "), "the race starts for the host");
    expect(WaitFor(guest, guestMessages, "RACE "), "the race starts for the guest");

    // One racer drops out: the other is told, kept in the room, and can leave the race cleanly.
    hostMessages.clear();
    guest.Disconnect();
    expect(WaitFor(host, hostMessages, "RACEABORT 1|PLAYER DISCONNECTED"),
           "the remaining racer is told the race was aborted and why");
    host.SendCommand("LEAVE");
    expect(WaitFor(host, hostMessages, "LOBBY"), "the remaining racer can still use the lobby afterwards");

    // The server itself is restarted: the connected client sees the connection fail...
    StopServer(server);
    expect(WaitForFailure(host), "a client notices when the server goes away");
    host.Disconnect();

    // ...and a fresh server on the same port accepts a reconnect with clean state.
    server = StartServer(pArguments[1], port);
    TcpLobbyClient returning;
    std::vector<std::string> returningMessages;
    expect(Hello(returning, port, "Host", returningMessages), "the client can reconnect after a restart");
    expect(HasPrefix(returningMessages, "WELCOME 1|"), "the restarted server starts a fresh session");
    returning.SendCommand("CREATE Again|0|1|3|2|0|1|0");
    expect(WaitFor(returning, returningMessages, "ROOM 1"), "the reconnected client can host a new room");
    returningMessages.clear();
    WaitFor(returning, returningMessages, "LOBBY", 100);
    bool oldRoomGone = true;
    for (const std::string& message : returningMessages)
        oldRoomGone = oldRoomGone && message.find("Resilience") == std::string::npos;
    expect(oldRoomGone, "rooms from before the restart do not come back");
    StopServer(server);
    return ok ? 0 : 1;
}
