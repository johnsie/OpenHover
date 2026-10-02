// SPDX-License-Identifier: MIT OR Apache-2.0
#include "TcpLobbyClient.h"

#include <algorithm>
#include <csignal>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace
{
bool Contains(const std::vector<std::string>& pMessages, const std::string& pExpected)
{
    return std::find(pMessages.begin(), pMessages.end(), pExpected) != pMessages.end();
}

bool ContainsPrefix(const std::vector<std::string>& pMessages, const std::string& pPrefix)
{
    for (const std::string& message : pMessages)
    {
        if (message.compare(0, pPrefix.size(), pPrefix) == 0)
            return true;
    }
    return false;
}

bool ContainsText(const std::vector<std::string>& pMessages, const std::string& pText)
{
    for (const std::string& message : pMessages)
    {
        if (message.find(pText) != std::string::npos)
            return true;
    }
    return false;
}

bool TickUntil(TcpLobbyClient& pClient, const std::string& pExpected)
{
    for (int attempt = 0; attempt < 200; ++attempt)
    {
        pClient.Tick();
        const std::vector<std::string> messages = pClient.TakeMessages();
        if (Contains(messages, pExpected))
            return true;
        if (pClient.State() == TcpLobbyClientState::Failed)
        {
            std::cerr << "client failed while waiting for " << pExpected << '\n';
            return false;
        }
        usleep(10000);
    }
    std::cerr << "client timed out waiting for " << pExpected << '\n';
    return false;
}

bool TickUntilPrefix(TcpLobbyClient& pClient, const std::string& pExpectedPrefix)
{
    for (int attempt = 0; attempt < 200; ++attempt)
    {
        pClient.Tick();
        if (ContainsPrefix(pClient.TakeMessages(), pExpectedPrefix))
            return true;
        if (pClient.State() == TcpLobbyClientState::Failed)
        {
            std::cerr << "client failed while waiting for " << pExpectedPrefix << '\n';
            return false;
        }
        usleep(10000);
    }
    std::cerr << "client timed out waiting for " << pExpectedPrefix << '\n';
    return false;
}

bool TickUntilContaining(TcpLobbyClient& pClient, const std::string& pExpectedText)
{
    for (int attempt = 0; attempt < 200; ++attempt)
    {
        pClient.Tick();
        if (ContainsText(pClient.TakeMessages(), pExpectedText))
            return true;
        if (pClient.State() == TcpLobbyClientState::Failed)
            return false;
        usleep(10000);
    }
    return false;
}

bool ConnectAndHello(TcpLobbyClient& pClient, int pPort, const std::string& pName, int pExpectedId)
{
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        if (pClient.Connect("127.0.0.1", pPort) && TickUntil(pClient, "OPENHOVER 1")
            && pClient.SendCommand("HELLO " + pName)
            && TickUntil(pClient, "WELCOME " + std::to_string(pExpectedId)))
            return true;
        pClient.Disconnect();
        usleep(10000);
    }
    return false;
}
}

int main(int pArgumentCount, char* pArguments[])
{
    if (pArgumentCount != 2)
    {
        std::cerr << "server path argument missing\n";
        return 1;
    }
    const int port = 20000 + static_cast<int>(getpid() % 20000);
    const std::string portText = std::to_string(port);
    const pid_t serverProcess = fork();
    if (serverProcess == 0)
    {
        execl(pArguments[1], pArguments[1], "--port", portText.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    if (serverProcess < 0)
    {
        std::cerr << "could not start server\n";
        return 1;
    }

    TcpLobbyClient host;
    TcpLobbyClient guest;
    TcpLobbyClient spectator;
    const bool hostConnected = ConnectAndHello(host, port, "Host", 1);
    const bool guestConnected = hostConnected && ConnectAndHello(guest, port, "Guest", 2);
    const bool guestAnnounced = guestConnected && TickUntil(host, "CHAT 0 Guest JOINED LOBBY");
    const bool namedChatDelivered = guestAnnounced && host.SendCommand("CHAT Welcome")
        && TickUntil(host, "CHAT 1 Welcome") && TickUntil(guest, "CHAT 1 Welcome");
    const bool spectatorConnected = namedChatDelivered && ConnectAndHello(spectator, port, "Spectator", 3);
    const bool roomCreated = namedChatDelivered && spectatorConnected
        && host.SendCommand("CREATE Smoke race|0|0|3|2|0|1")
        && TickUntil(host, "ROOM 1");
    if (roomCreated)
    {
        guest.Tick();
        guest.TakeMessages();
    }
    const bool guestJoined = roomCreated && guest.SendCommand("JOIN 1")
        && TickUntilContaining(guest, "|R,1,Smoke race,1,2,2,");
    const bool raceStarted = guestJoined && host.SendCommand("START 1")
        && TickUntilPrefix(host, "RACE 1|") && TickUntilPrefix(guest, "RACE 1|");
    const bool raceHudReceived = raceStarted && TickUntilPrefix(host, "RACEHUD 1|3|");
    bool spectatorReceivedRace = false;
    for (int attempt = 0; attempt < 20 && raceHudReceived; ++attempt)
    {
        spectator.Tick();
        spectatorReceivedRace = spectatorReceivedRace
            || ContainsPrefix(spectator.TakeMessages(), "RACE 1|");
        usleep(10000);
    }
    spectator.Disconnect();
    const bool spectatorDepartureAnnounced = TickUntil(host, "CHAT 0 Spectator LEFT LOBBY");
    host.Disconnect();
    guest.Disconnect();
    kill(serverProcess, SIGTERM);
    int serverStatus = 0;
    waitpid(serverProcess, &serverStatus, 0);
    if (!raceHudReceived || spectatorReceivedRace || !spectatorDepartureAnnounced)
    {
        std::cerr << "tcp lobby server did not isolate authoritative race snapshots\n";
        return 1;
    }
    return 0;
}