// SPDX-License-Identifier: MIT OR Apache-2.0
#include "TcpLobbyClient.h"
#include "Protocol.h"

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

bool TickUntilPrefix(TcpLobbyClient& pClient, const std::string& pExpectedPrefix,
                     std::string* pMatchedMessage = nullptr)
{
    for (int attempt = 0; attempt < 200; ++attempt)
    {
        pClient.Tick();
        const std::vector<std::string> messages = pClient.TakeMessages();
        for (const std::string& message : messages)
        {
            if (message.compare(0, pExpectedPrefix.size(), pExpectedPrefix) == 0)
            {
                if (pMatchedMessage != nullptr)
                    *pMatchedMessage = message;
                return true;
            }
        }
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

bool TickUntilFailed(TcpLobbyClient& pClient)
{
    for (int attempt = 0; attempt < 200; ++attempt)
    {
        pClient.Tick();
        if (pClient.State() == TcpLobbyClientState::Failed)
            return true;
        usleep(10000);
    }
    return false;
}

bool ConnectAndHello(TcpLobbyClient& pClient, int pPort, const std::string& pName, int pExpectedId)
{
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        if (pClient.Connect("127.0.0.1", pPort)
            && TickUntilPrefix(pClient, "OPENHOVER " + std::to_string(kOpenHoverProtocolVersion) + " ")
            && pClient.SendCommand("HELLO " + std::to_string(kOpenHoverProtocolVersion) + "|"
                                   + std::to_string(kOpenHoverContentVersion) + "|" + pName)
            && TickUntilPrefix(pClient, "WELCOME " + std::to_string(pExpectedId) + "|"))
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
    TcpLobbyClient incompatible;
    const bool incompatibleRejected = hostConnected && incompatible.Connect("127.0.0.1", port)
        && TickUntilPrefix(incompatible, "OPENHOVER " + std::to_string(kOpenHoverProtocolVersion) + " ")
        && incompatible.SendCommand("HELLO 999|" + std::to_string(kOpenHoverContentVersion)
                                    + "|OldClient")
        && TickUntil(incompatible, "ERROR INCOMPATIBLE PROTOCOL UPDATE REQUIRED");
    incompatible.Disconnect();
    TcpLobbyClient invalidName;
    const bool invalidNameRejected = incompatibleRejected && invalidName.Connect("127.0.0.1", port)
        && TickUntilPrefix(invalidName, "OPENHOVER " + std::to_string(kOpenHoverProtocolVersion) + " ")
        && invalidName.SendCommand("HELLO " + std::to_string(kOpenHoverProtocolVersion) + "|"
                                   + std::to_string(kOpenHoverContentVersion) + "|Invalid Name")
        && TickUntil(invalidName, "ERROR invalid name");
    invalidName.Disconnect();
    const bool guestConnected = invalidNameRejected && ConnectAndHello(guest, port, "Guest", 2);
    const bool guestAnnounced = guestConnected && TickUntil(host, "CHAT 0 Guest JOINED LOBBY");
    const bool reportAccepted = guestAnnounced && host.SendCommand("REPORT 2|Repeated abuse")
        && TickUntil(host, "REPORTACK 2");
    TcpLobbyClient oversizedClient;
    const bool oversizedClientRemoved = reportAccepted
        && oversizedClient.Connect("127.0.0.1", port)
        && TickUntilPrefix(oversizedClient,
                           "OPENHOVER " + std::to_string(kOpenHoverProtocolVersion) + " ")
        && oversizedClient.SendCommand(std::string(5000, 'X'))
        && TickUntilFailed(oversizedClient);
    oversizedClient.Disconnect();
    const bool namedChatDelivered = oversizedClientRemoved && host.SendCommand("CHAT Welcome?")
        && TickUntil(host, "CHAT 1 Welcome?") && TickUntil(guest, "CHAT 1 Welcome?");
    const bool spectatorConnected = namedChatDelivered && ConnectAndHello(spectator, port, "Spectator", 3);
    std::string roomCreatedMessage;
    const bool roomCreated = namedChatDelivered && spectatorConnected
        && host.SendCommand("CREATE Smoke race|0|0|3|2|0|1|1")
        && TickUntilPrefix(host, "ROOM 1|", &roomCreatedMessage);
    const std::string roomCode = roomCreated
        ? roomCreatedMessage.substr(roomCreatedMessage.find('|') + 1) : "";
    bool privateRoomHidden = false;
    if (roomCreated)
    {
        guest.Tick();
        const std::vector<std::string> guestMessages = guest.TakeMessages();
        privateRoomHidden = !ContainsText(guestMessages, "|R,1,")
            && ContainsText(guestMessages, "|P,1,Host,0,0,0");
    }
    const bool privateRoomRejectsDirectJoin = privateRoomHidden
        && guest.SendCommand("JOIN 1") && TickUntil(guest, "ERROR room unavailable");
    const bool guestJoined = privateRoomRejectsDirectJoin && roomCode.size() == 6
        && guest.SendCommand("JOINCODE " + roomCode)
        && TickUntilContaining(guest, "|R,1,Harbor Loop,1,2,2,");
    const bool unreadyStartRejected = guestJoined && host.SendCommand("START 1")
        && TickUntil(host, "ERROR players not ready");
    const bool guestReady = unreadyStartRejected && guest.SendCommand("READY 1")
        && TickUntilContaining(guest, "|P,2,Guest,1,0,1");
    const bool raceStarted = guestReady && host.SendCommand("START 1")
        && TickUntilPrefix(host, "RACE 1|") && TickUntilPrefix(guest, "RACE 1|");
    const bool raceHudReceived = raceStarted && TickUntilPrefix(host, "RACEHUD 1|3|");
    spectator.Tick();
    spectator.TakeMessages();
    const bool raceChatReceived = raceHudReceived && host.SendCommand("CHAT Race message")
        && TickUntil(host, "CHAT 1 Race message") && TickUntil(guest, "CHAT 1 Race message");
    bool spectatorReceivedRace = false;
    bool spectatorReceivedRaceChat = false;
    for (int attempt = 0; attempt < 20 && raceChatReceived; ++attempt)
    {
        spectator.Tick();
        const std::vector<std::string> spectatorMessages = spectator.TakeMessages();
        spectatorReceivedRace = spectatorReceivedRace || ContainsPrefix(spectatorMessages, "RACE 1|");
        spectatorReceivedRaceChat = spectatorReceivedRaceChat
            || Contains(spectatorMessages, "CHAT 1 Race message");
        usleep(10000);
    }
    const bool chatFloodRejected = raceChatReceived
        && spectator.SendCommand("CHAT Flood1")
        && spectator.SendCommand("CHAT Flood2")
        && spectator.SendCommand("CHAT Flood3")
        && spectator.SendCommand("CHAT Flood4")
        && spectator.SendCommand("CHAT Flood5")
        && TickUntil(spectator, "ERROR chat rate limit");
    spectator.Disconnect();
    const bool spectatorDepartureAnnounced = TickUntil(host, "CHAT 0 Spectator LEFT LOBBY");
    host.Disconnect();
    const bool disconnectEndedRace = TickUntil(guest, "RACEABORT 1|PLAYER DISCONNECTED");
    kill(serverProcess, SIGTERM);
    int serverStatus = 0;
    waitpid(serverProcess, &serverStatus, 0);
    const bool serverLossDetected = TickUntilFailed(guest);
    guest.Disconnect();
    if (!incompatibleRejected || !invalidNameRejected || !reportAccepted || !oversizedClientRemoved
        || !privateRoomHidden || !privateRoomRejectsDirectJoin || !unreadyStartRejected || !guestReady
        || !raceChatReceived || !chatFloodRejected
        || spectatorReceivedRace || spectatorReceivedRaceChat
        || !spectatorDepartureAnnounced || !disconnectEndedRace || !serverLossDetected)
    {
        std::cerr << "tcp lobby server did not isolate authoritative race snapshots\n";
        return 1;
    }
    return 0;
}
