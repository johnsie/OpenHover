// SPDX-License-Identifier: MIT OR Apache-2.0
#include "AuthoritativeRace.h"
#include "Lobby.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
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

bool ParseDouble(const std::string& pText, double& pValue)
{
    char* end = nullptr;
    pValue = std::strtod(pText.c_str(), &end);
    return end != pText.c_str() && *end == '\0';
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

void BroadcastRaceSnapshot(LobbyRoomId pRoomId, const AuthoritativeRace& pRace,
                           const Lobby& pLobby, const std::vector<ClientConnection>& pClients)
{
    const RaceSnapshot snapshot = pRace.Snapshot();
    std::ostringstream message;
    message << "RACE " << pRoomId << '|' << snapshot.mTick;
    for (const RaceRacerSnapshot& racer : snapshot.mRacers)
    {
        message << '|' << racer.mPlayerId << ',' << racer.mState.mX << ',' << racer.mState.mY
                << ',' << racer.mState.mHeading << ',' << racer.mState.mSpeed << ','
                << racer.mState.mHeight;
    }
    for (const ClientConnection& client : pClients)
    {
        if (pLobby.RoomForPlayer(client.mPlayerId) == pRoomId)
            SendLine(client, message.str());
    }
}

void StopRace(Lobby& pLobby, std::map<LobbyRoomId, AuthoritativeRace>& pRaces,
              LobbyRoomId pRoomId)
{
    if (pRaces.erase(pRoomId) != 0)
        pLobby.FinishRace(pRoomId);
}

void RemoveClient(std::vector<ClientConnection>& pClients, std::size_t pIndex, Lobby& pLobby,
                  std::map<LobbyRoomId, AuthoritativeRace>& pRaces)
{
    if (pClients[pIndex].mPlayerId != 0)
    {
        const LobbyRoomId roomId = pLobby.RoomForPlayer(pClients[pIndex].mPlayerId);
        pLobby.Disconnect(pClients[pIndex].mPlayerId);
        StopRace(pLobby, pRaces, roomId);
    }
    close(pClients[pIndex].mSocket);
    pClients.erase(pClients.begin() + pIndex);
}

void HandleCommand(ClientConnection& pClient, const std::string& pLine, Lobby& pLobby,
                   std::map<LobbyRoomId, AuthoritativeRace>& pRaces,
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
    else if (command == "JOIN")
    {
        int roomId = 0;
        const LobbyRoom* requestedRoom = nullptr;
        if (ParseInteger(argument, roomId))
        {
            for (const LobbyRoom& room : pLobby.Rooms())
            {
                if (room.mId == static_cast<LobbyRoomId>(roomId))
                {
                    requestedRoom = &room;
                    break;
                }
            }
        }
        if (!ParseInteger(argument, roomId) || requestedRoom == nullptr)
            SendLine(pClient, "ERROR room unavailable");
        else if (requestedRoom->mRaceRunning)
            SendLine(pClient, "ERROR race running");
        else if (pLobby.RoomForPlayer(pClient.mPlayerId) != 0)
            SendLine(pClient, "ERROR already in room");
        else if (static_cast<int>(requestedRoom->mPlayerIds.size()) >= requestedRoom->mSettings.mPlayerCapacity)
            SendLine(pClient, "ERROR room full");
        else if (pLobby.JoinRoom(pClient.mPlayerId, requestedRoom->mId))
        {
            BroadcastLobbySnapshot(pLobby, pClients);
            return;
        }
    }
    else if (command == "LEAVE")
    {
        const LobbyRoomId roomId = pLobby.RoomForPlayer(pClient.mPlayerId);
        if (argument.empty() && pLobby.LeaveRoom(pClient.mPlayerId))
        {
            StopRace(pLobby, pRaces, roomId);
            BroadcastLobbySnapshot(pLobby, pClients);
            return;
        }
    }
    else if (command == "START")
    {
        int roomId = 0;
        if (ParseInteger(argument, roomId))
        {
            const LobbyRoom* requestedRoom = nullptr;
            for (const LobbyRoom& room : pLobby.Rooms())
            {
                if (room.mId == static_cast<LobbyRoomId>(roomId))
                {
                    requestedRoom = &room;
                    break;
                }
            }
            AuthoritativeRace race;
            if (requestedRoom != nullptr
                && race.Start(requestedRoom->mPlayerIds, requestedRoom->mSettings.mTrackIndex)
                && pLobby.StartRace(pClient.mPlayerId, requestedRoom->mId))
            {
                pRaces[requestedRoom->mId] = std::move(race);
                BroadcastLobbySnapshot(pLobby, pClients);
                return;
            }
        }
    }
    else if (command == "INPUT")
    {
        std::vector<std::string> fields;
        std::stringstream stream(argument);
        std::string field;
        while (std::getline(stream, field, '|'))
            fields.push_back(field);
        double throttle = 0.0;
        double steering = 0.0;
        int jump = 0;
        int reverseFacing = 0;
        const LobbyRoomId roomId = pLobby.RoomForPlayer(pClient.mPlayerId);
        std::map<LobbyRoomId, AuthoritativeRace>::iterator race = pRaces.find(roomId);
        if (fields.size() == 4 && race != pRaces.end()
            && ParseDouble(fields[0], throttle) && ParseDouble(fields[1], steering)
            && ParseInteger(fields[2], jump) && ParseInteger(fields[3], reverseFacing)
            && (jump == 0 || jump == 1) && (reverseFacing == 0 || reverseFacing == 1)
            && std::isfinite(throttle) && std::isfinite(steering)
            && std::fabs(throttle) <= 1.0 && std::fabs(steering) <= 1.0
            && race->second.SubmitInput({pClient.mPlayerId, throttle, steering,
                                         jump != 0, reverseFacing != 0}))
            return;
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
    std::map<LobbyRoomId, AuthoritativeRace> races;
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
        timeval timeout = {0, 8333};
        if (select(maximumSocket + 1, &readable, nullptr, nullptr, &timeout) < 0)
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
                RemoveClient(clients, index, lobby, races);
                BroadcastLobbySnapshot(lobby, clients);
                continue;
            }
            client.mReceiveBuffer.append(buffer, static_cast<std::size_t>(received));
            if (client.mReceiveBuffer.size() > kMaximumReceiveBuffer)
            {
                RemoveClient(clients, index, lobby, races);
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
                HandleCommand(client, line, lobby, races, clients);
            }
            ++index;
        }
        for (std::map<LobbyRoomId, AuthoritativeRace>::iterator race = races.begin(); race != races.end(); ++race)
        {
            race->second.Step();
            if (race->second.Snapshot().mTick % 4 == 0)
                BroadcastRaceSnapshot(race->first, race->second, lobby, clients);
        }
    }
    for (const ClientConnection& client : clients)
        close(client.mSocket);
    close(listenSocket);
    return 0;
}