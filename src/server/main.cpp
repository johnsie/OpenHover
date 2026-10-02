// SPDX-License-Identifier: MIT OR Apache-2.0
#include "Lobby.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
const int kDefaultPort = 9700;
const std::size_t kMaximumReceiveBuffer = 4096;

struct ClientConnection
{
    int mSocket = -1;
    LobbyPlayerId mPlayerId = 0;
    std::string mReceiveBuffer;
};

bool ContainsProtocolDelimiter(const std::string& pText)
{
    return pText.find_first_of(",|\r\n") != std::string::npos;
}

bool ParseInteger(const std::string& pText, int& pValue)
{
    char* end = nullptr;
    const long value = std::strtol(pText.c_str(), &end, 10);
    if (end == pText.c_str() || *end != '\0')
        return false;
    pValue = static_cast<int>(value);
    return true;
}

bool ParseRoomFields(const std::string& pText, std::string& pRoomName, LobbyRaceSettings& pSettings)
{
    std::vector<std::string> fields;
    std::stringstream stream(pText);
    std::string field;
    while (std::getline(stream, field, '|'))
        fields.push_back(field);
    if (fields.size() != 7 || fields[0].empty())
        return false;
    int mode = 0;
    int weaponsAllowed = 0;
    if (!ParseInteger(fields[1], mode) || !ParseInteger(fields[2], pSettings.mTrackIndex)
        || !ParseInteger(fields[3], pSettings.mLapCount)
        || !ParseInteger(fields[4], pSettings.mPlayerCapacity)
        || !ParseInteger(fields[5], pSettings.mRivalCount)
        || !ParseInteger(fields[6], weaponsAllowed) || mode < 0 || mode > 3
        || (weaponsAllowed != 0 && weaponsAllowed != 1))
        return false;
    pRoomName = fields[0];
    pSettings.mRaceMode = static_cast<RaceMode>(mode);
    pSettings.mWeaponsAllowed = weaponsAllowed != 0;
    return !ContainsProtocolDelimiter(pRoomName);
}

void SendLine(const ClientConnection& pClient, const std::string& pText)
{
    const std::string line = pText + "\n";
    send(pClient.mSocket, line.c_str(), line.size(), MSG_NOSIGNAL);
}

void BroadcastLobbySnapshot(const Lobby& pLobby, const std::vector<ClientConnection>& pClients)
{
    std::ostringstream snapshot;
    snapshot << "LOBBY";
    for (const LobbyPlayer& player : pLobby.Players())
        snapshot << "|P," << player.mId << ',' << player.mDisplayName;
    for (const LobbyRoom& room : pLobby.Rooms())
    {
        snapshot << "|R," << room.mId << ',' << room.mName << ',' << room.mHostId << ','
                 << room.mPlayerIds.size() << ',' << room.mSettings.mPlayerCapacity << ','
                 << (room.mRaceRunning ? 1 : 0) << ',' << static_cast<int>(room.mSettings.mRaceMode)
                 << ',' << room.mSettings.mTrackIndex << ',' << room.mSettings.mLapCount << ','
                 << room.mSettings.mRivalCount << ',' << (room.mSettings.mWeaponsAllowed ? 1 : 0);
    }
    for (const ClientConnection& client : pClients)
        SendLine(client, snapshot.str());
}

void RemoveClient(std::vector<ClientConnection>& pClients, std::size_t pIndex, Lobby& pLobby)
{
    if (pClients[pIndex].mPlayerId != 0)
        pLobby.Disconnect(pClients[pIndex].mPlayerId);
    close(pClients[pIndex].mSocket);
    pClients.erase(pClients.begin() + pIndex);
}

void HandleCommand(ClientConnection& pClient, const std::string& pLine, Lobby& pLobby,
                   const std::vector<ClientConnection>& pClients)
{
    const std::size_t separator = pLine.find(' ');
    const std::string command = pLine.substr(0, separator);
    const std::string argument = separator == std::string::npos ? "" : pLine.substr(separator + 1);
    if (command == "HELLO")
    {
        if (pClient.mPlayerId != 0 || ContainsProtocolDelimiter(argument))
            SendLine(pClient, "ERROR invalid hello");
        else if (pLobby.Connect(argument, pClient.mPlayerId))
        {
            SendLine(pClient, "WELCOME " + std::to_string(pClient.mPlayerId));
            BroadcastLobbySnapshot(pLobby, pClients);
        }
        else
            SendLine(pClient, "ERROR name unavailable");
        return;
    }
    if (pClient.mPlayerId == 0)
    {
        SendLine(pClient, "ERROR hello required");
        return;
    }
    if (command == "CHAT" && !ContainsProtocolDelimiter(argument) && pLobby.SendChat(pClient.mPlayerId, argument))
    {
        for (const ClientConnection& client : pClients)
            SendLine(client, "CHAT " + std::to_string(pClient.mPlayerId) + " " + argument);
        return;
    }
    if (command == "CREATE")
    {
        LobbyRaceSettings settings;
        LobbyRoomId roomId = 0;
        std::string roomName;
        if (ParseRoomFields(argument, roomName, settings)
            && pLobby.CreateRoom(pClient.mPlayerId, roomName, settings, roomId))
        {
            SendLine(pClient, "ROOM " + std::to_string(roomId));
            BroadcastLobbySnapshot(pLobby, pClients);
            return;
        }
    }
    else if (command == "JOIN" || command == "START" || command == "LEAVE")
    {
        int roomId = 0;
        const bool success = command == "LEAVE" ? argument.empty() && pLobby.LeaveRoom(pClient.mPlayerId)
            : ParseInteger(argument, roomId) && (command == "JOIN"
                ? pLobby.JoinRoom(pClient.mPlayerId, static_cast<LobbyRoomId>(roomId))
                : pLobby.StartRace(pClient.mPlayerId, static_cast<LobbyRoomId>(roomId)));
        if (success)
        {
            BroadcastLobbySnapshot(pLobby, pClients);
            return;
        }
    }
    else if (command == "SET")
    {
        const std::size_t roomSeparator = argument.find(' ');
        int roomId = 0;
        LobbyRaceSettings settings;
        std::string ignoredName;
        if (roomSeparator != std::string::npos
            && ParseInteger(argument.substr(0, roomSeparator), roomId)
            && ParseRoomFields("settings|" + argument.substr(roomSeparator + 1), ignoredName, settings)
            && pLobby.UpdateRoomSettings(pClient.mPlayerId, static_cast<LobbyRoomId>(roomId), settings))
        {
            BroadcastLobbySnapshot(pLobby, pClients);
            return;
        }
    }
    SendLine(pClient, "ERROR invalid command");
}
}

int main(int pArgumentCount, char* pArguments[])
{
    int port = kDefaultPort;
    if (pArgumentCount == 3 && std::string(pArguments[1]) == "--port"
        && !ParseInteger(pArguments[2], port))
    {
        std::cerr << "Invalid port\n";
        return 1;
    }
    if (port < 1 || port > 65535)
    {
        std::cerr << "Port must be between 1 and 65535\n";
        return 1;
    }

    const int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0)
    {
        std::cerr << "Could not create TCP socket\n";
        return 1;
    }
    const int reuseAddress = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &reuseAddress, sizeof(reuseAddress));
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<unsigned short>(port));
    if (bind(listenSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0
        || listen(listenSocket, 16) != 0)
    {
        std::cerr << "Could not listen on TCP port " << port << ": " << std::strerror(errno) << '\n';
        close(listenSocket);
        return 1;
    }

    std::cout << "OpenHoverServer listening on TCP port " << port << '\n';
    Lobby lobby;
    std::vector<ClientConnection> clients;
    while (true)
    {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(listenSocket, &readable);
        int maximumSocket = listenSocket;
        for (const ClientConnection& client : clients)
        {
            FD_SET(client.mSocket, &readable);
            maximumSocket = std::max(maximumSocket, client.mSocket);
        }
        if (select(maximumSocket + 1, &readable, nullptr, nullptr, nullptr) < 0)
        {
            if (errno == EINTR)
                continue;
            break;
        }
        if (FD_ISSET(listenSocket, &readable))
        {
            const int clientSocket = accept(listenSocket, nullptr, nullptr);
            if (clientSocket >= 0)
            {
                clients.push_back({clientSocket, 0, ""});
                SendLine(clients.back(), "OPENHOVER 1");
            }
        }
        for (std::size_t index = 0; index < clients.size();)
        {
            ClientConnection& client = clients[index];
            if (!FD_ISSET(client.mSocket, &readable))
            {
                ++index;
                continue;
            }
            char buffer[1024];
            const ssize_t received = recv(client.mSocket, buffer, sizeof(buffer), 0);
            if (received <= 0)
            {
                RemoveClient(clients, index, lobby);
                BroadcastLobbySnapshot(lobby, clients);
                continue;
            }
            client.mReceiveBuffer.append(buffer, static_cast<std::size_t>(received));
            if (client.mReceiveBuffer.size() > kMaximumReceiveBuffer)
            {
                RemoveClient(clients, index, lobby);
                BroadcastLobbySnapshot(lobby, clients);
                continue;
            }
            std::size_t lineEnd = 0;
            while ((lineEnd = client.mReceiveBuffer.find('\n')) != std::string::npos)
            {
                std::string line = client.mReceiveBuffer.substr(0, lineEnd);
                client.mReceiveBuffer.erase(0, lineEnd + 1);
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                HandleCommand(client, line, lobby, clients);
            }
            ++index;
        }
    }
    for (const ClientConnection& client : clients)
        close(client.mSocket);
    close(listenSocket);
    return 0;
}