// SPDX-License-Identifier: MIT OR Apache-2.0
#include "AuthoritativeRace.h"
#include "Championship.h"
#include "Lobby.h"
#include "Protocol.h"
#include "TrackDefinition.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <random>
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
const std::size_t kMaximumConnections = 64;

#ifndef OPENHOVER_VERSION
#define OPENHOVER_VERSION "unknown"
#endif

#ifndef OPENHOVER_SOURCE_REVISION
#define OPENHOVER_SOURCE_REVISION "unknown"
#endif

struct ClientConnection
{
    int mSocket = -1;
    LobbyPlayerId mPlayerId = 0;
    std::string mReceiveBuffer;
    LobbyChatRateLimiter mChatRateLimiter;
    LobbyReportRateLimiter mReportRateLimiter;
};

double MonotonicSeconds()
{
    typedef std::chrono::steady_clock Clock;
    static const Clock::time_point start = Clock::now();
    return std::chrono::duration<double>(Clock::now() - start).count();
}

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

std::string LobbySnapshotForPlayer(const Lobby& pLobby, LobbyPlayerId pPlayerId)
{
    std::ostringstream snapshot;
    snapshot << "LOBBY";
    for (const LobbyPlayer& player : pLobby.Players())
    {
        LobbyRoomId visibleRoomId = 0;
        bool host = false;
        bool ready = false;
        for (const LobbyRoom& room : pLobby.Rooms())
        {
            const bool playerIsMember = std::find(room.mPlayerIds.begin(), room.mPlayerIds.end(), player.mId)
                != room.mPlayerIds.end();
            const bool viewerIsMember = std::find(room.mPlayerIds.begin(), room.mPlayerIds.end(), pPlayerId)
                != room.mPlayerIds.end();
            if (playerIsMember && (!room.mPrivate || viewerIsMember))
            {
                visibleRoomId = room.mId;
                host = room.mHostId == player.mId;
                ready = std::find(room.mReadyPlayerIds.begin(), room.mReadyPlayerIds.end(), player.mId)
                    != room.mReadyPlayerIds.end();
                break;
            }
        }
        snapshot << "|P," << player.mId << ',' << player.mDisplayName << ',' << visibleRoomId
                 << ',' << (host ? 1 : 0) << ',' << (ready ? 1 : 0);
    }
    for (const LobbyRoom& room : pLobby.Rooms())
    {
        const bool member = std::find(room.mPlayerIds.begin(), room.mPlayerIds.end(), pPlayerId)
            != room.mPlayerIds.end();
        if (room.mPrivate && !member)
            continue;
        snapshot << "|R," << room.mId << ',' << room.mName << ',' << room.mHostId << ','
                 << room.mPlayerIds.size() << ',' << room.mSettings.mPlayerCapacity << ','
                 << (room.mRaceRunning ? 1 : 0) << ',' << static_cast<int>(room.mSettings.mRaceMode)
                 << ',' << room.mSettings.mTrackIndex << ',' << room.mSettings.mLapCount << ','
                 << room.mSettings.mRivalCount << ',' << (room.mSettings.mWeaponsAllowed ? 1 : 0)
                 << ',' << (room.mPrivate ? 1 : 0) << ',' << room.mReadyPlayerIds.size();
    }
    return snapshot.str();
}

void BroadcastLobbySnapshot(const Lobby& pLobby, const std::vector<ClientConnection>& pClients)
{
    for (const ClientConnection& client : pClients)
        SendLine(client, LobbySnapshotForPlayer(pLobby, client.mPlayerId));
}

std::string GenerateRoomCode()
{
    static std::mt19937 generator(std::random_device{}());
    static const char characters[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    std::string code(6, 'A');
    for (char& character : code)
        character = characters[generator() % (sizeof(characters) - 1)];
    return code;
}

void BroadcastLobbyNotice(const std::string& pText, const std::vector<ClientConnection>& pClients)
{
    for (const ClientConnection& client : pClients)
        SendLine(client, "CHAT 0 " + pText);
}

void BroadcastRaceSnapshot(LobbyRoomId pRoomId, const AuthoritativeRace& pRace,
                           const Lobby& pLobby, const std::vector<ClientConnection>& pClients)
{
    const RaceSnapshot snapshot = pRace.Snapshot();
    std::ostringstream message;
    message << "RACE " << pRoomId << '|' << snapshot.mTick;
    for (const RaceRacerSnapshot& racer : snapshot.mRacers)
    {
        message << "|R," << racer.mPlayerId << ',' << racer.mState.mX << ',' << racer.mState.mY
                << ',' << racer.mState.mHeading << ',' << racer.mState.mSpeed << ','
            << racer.mState.mHeight << ',' << (racer.mState.mBoosting ? 1 : 0) << ','
            << racer.mState.mSpinOutSeconds << ',' << static_cast<int>(racer.mCraftClass);
    }
    for (const RaceMissileSnapshot& missile : snapshot.mMissiles)
    {
        message << "|M," << missile.mPlayerId << ',' << missile.mState.mX << ','
                << missile.mState.mY << ',' << missile.mState.mHeading << ','
                << missile.mState.mSpeed << ',' << missile.mState.mHeight;
    }
    for (std::size_t index = 0; index < snapshot.mMineTriggered.size(); ++index)
        message << "|N," << index << ',' << (snapshot.mMineTriggered[index] ? 1 : 0);
    for (const ClientConnection& client : pClients)
    {
        if (pLobby.RoomForPlayer(client.mPlayerId) == pRoomId)
            SendLine(client, message.str());
    }
}

void BroadcastRaceHudSnapshot(LobbyRoomId pRoomId, const AuthoritativeRace& pRace,
                              const Lobby& pLobby, const std::vector<ClientConnection>& pClients)
{
    const RaceSnapshot snapshot = pRace.Snapshot();
    std::ostringstream message;
    message << "RACEHUD " << pRoomId << '|' << snapshot.mTargetLaps << "|S,"
            << snapshot.mStartLights << ',' << (snapshot.mCountdownActive ? 1 : 0) << ','
            << snapshot.mCountdownSeconds;
    for (const RaceRacerSnapshot& racer : snapshot.mRacers)
    {
        message << '|' << racer.mPlayerId << ',' << racer.mProgress.mCompletedLaps << ','
                << racer.mProgress.mNextCheckpoint << ',' << racer.mProgress.mElapsedSeconds << ','
                << (racer.mProgress.mFinished ? 1 : 0) << ',' << racer.mPosition << ','
                << racer.mLapTiming.mCurrentSeconds << ',' << racer.mLapTiming.mLastSeconds << ','
                << racer.mLapTiming.mBestSeconds;
    }
    for (const ClientConnection& client : pClients)
    {
        if (pLobby.RoomForPlayer(client.mPlayerId) == pRoomId)
            SendLine(client, message.str());
    }
}

void BroadcastRaceFinished(LobbyRoomId pRoomId, const Lobby& pLobby,
                           const std::vector<ClientConnection>& pClients)
{
    const std::string message = "RACEFINISH " + std::to_string(pRoomId);
    for (const ClientConnection& client : pClients)
    {
        if (pLobby.RoomForPlayer(client.mPlayerId) == pRoomId)
            SendLine(client, message);
    }
}

void BroadcastRaceAborted(LobbyRoomId pRoomId, const std::string& pReason,
                          const Lobby& pLobby, const std::vector<ClientConnection>& pClients)
{
    const std::string message = "RACEABORT " + std::to_string(pRoomId) + "|" + pReason;
    for (const ClientConnection& client : pClients)
    {
        if (pLobby.RoomForPlayer(client.mPlayerId) == pRoomId)
            SendLine(client, message);
    }
}

void BroadcastRaceEvent(LobbyRoomId pRoomId, int pTrackIndex, const Championship& pChampionship,
                        const AuthoritativeRace& pRace, const Lobby& pLobby,
                        const std::vector<ClientConnection>& pClients)
{
    std::ostringstream message;
    message << "RACEEVENT " << pRoomId << '|' << pTrackIndex << '|'
            << pChampionship.CurrentEvent() + 1 << '|' << pChampionship.EventCount();
    const RaceSnapshot snapshot = pRace.Snapshot();
    for (int competitor = 0; competitor < pChampionship.CompetitorCount()
         && competitor < static_cast<int>(snapshot.mRacers.size()); ++competitor)
    {
        message << "|P," << snapshot.mRacers[competitor].mPlayerId << ','
                << pChampionship.CompetitorPoints(competitor);
    }
    for (const ClientConnection& client : pClients)
    {
        if (pLobby.RoomForPlayer(client.mPlayerId) == pRoomId)
            SendLine(client, message.str());
    }
}

void StopRace(Lobby& pLobby, std::map<LobbyRoomId, AuthoritativeRace>& pRaces,
              std::map<LobbyRoomId, Championship>& pChampionships, LobbyRoomId pRoomId)
{
    if (pRaces.erase(pRoomId) != 0)
        pLobby.FinishRace(pRoomId);
    pChampionships.erase(pRoomId);
}

void RemoveClient(std::vector<ClientConnection>& pClients, std::size_t pIndex, Lobby& pLobby,
                  std::map<LobbyRoomId, AuthoritativeRace>& pRaces,
                  std::map<LobbyRoomId, Championship>& pChampionships,
                  const char* pReason)
{
    const LobbyPlayerId removedPlayerId = pClients[pIndex].mPlayerId;
    const int removedSocket = pClients[pIndex].mSocket;
    const LobbyRoomId removedRoomId = pLobby.RoomForPlayer(removedPlayerId);
    std::cout << "connection_closed socket=" << removedSocket
              << " player_id=" << removedPlayerId
              << " room_id=" << removedRoomId
              << " reason=" << pReason << std::endl;
    if (pClients[pIndex].mPlayerId != 0)
    {
        std::string displayName;
        for (const LobbyPlayer& player : pLobby.Players())
        {
            if (player.mId == pClients[pIndex].mPlayerId)
            {
                displayName = player.mDisplayName;
                break;
            }
        }
        const LobbyRoomId roomId = pLobby.RoomForPlayer(pClients[pIndex].mPlayerId);
        const bool raceWasRunning = pRaces.find(roomId) != pRaces.end();
        pLobby.Disconnect(pClients[pIndex].mPlayerId);
        StopRace(pLobby, pRaces, pChampionships, roomId);
        close(pClients[pIndex].mSocket);
        pClients.erase(pClients.begin() + pIndex);
        if (!displayName.empty())
            BroadcastLobbyNotice(displayName + " LEFT LOBBY", pClients);
        if (raceWasRunning)
            BroadcastRaceAborted(roomId, "PLAYER DISCONNECTED", pLobby, pClients);
        return;
    }
    close(pClients[pIndex].mSocket);
    pClients.erase(pClients.begin() + pIndex);
}

void HandleCommand(ClientConnection& pClient, const std::string& pLine, Lobby& pLobby,
                   std::map<LobbyRoomId, AuthoritativeRace>& pRaces,
                   std::map<LobbyRoomId, Championship>& pChampionships,
                   const std::vector<ClientConnection>& pClients)
{
    const std::size_t separator = pLine.find(' ');
    const std::string command = pLine.substr(0, separator);
    const std::string argument = separator == std::string::npos ? "" : pLine.substr(separator + 1);
    if (command == "HELLO")
    {
        std::vector<std::string> helloFields;
        std::stringstream helloStream(argument);
        std::string helloField;
        while (std::getline(helloStream, helloField, '|'))
            helloFields.push_back(helloField);
        int protocolVersion = 0;
        int contentVersion = 0;
        if (helloFields.size() != 3
            || !ParseInteger(helloFields[0], protocolVersion)
            || !ParseInteger(helloFields[1], contentVersion))
            SendLine(pClient, "ERROR INCOMPATIBLE CLIENT UPDATE REQUIRED");
        else if (protocolVersion != kOpenHoverProtocolVersion)
            SendLine(pClient, "ERROR INCOMPATIBLE PROTOCOL UPDATE REQUIRED");
        else if (contentVersion != kOpenHoverContentVersion)
            SendLine(pClient, "ERROR INCOMPATIBLE CONTENT UPDATE REQUIRED");
        else if (pClient.mPlayerId != 0)
            SendLine(pClient, "ERROR invalid hello");
        else if (!IsValidLobbyDisplayName(helloFields[2]))
            SendLine(pClient, "ERROR invalid name");
        else if (pLobby.Connect(helloFields[2], pClient.mPlayerId))
        {
            SendLine(pClient, "WELCOME " + std::to_string(pClient.mPlayerId) + "|"
                + std::to_string(kOpenHoverProtocolVersion) + "|"
                + std::to_string(kOpenHoverContentVersion) + "|" + OPENHOVER_VERSION);
            BroadcastLobbySnapshot(pLobby, pClients);
            BroadcastLobbyNotice(helloFields[2] + " JOINED LOBBY", pClients);
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
    if (command == "CHAT")
    {
        if (ContainsProtocolDelimiter(argument) || argument.empty())
        {
            SendLine(pClient, "ERROR invalid chat");
            return;
        }
        if (!pClient.mChatRateLimiter.Allow(MonotonicSeconds()))
        {
            SendLine(pClient, "ERROR chat rate limit");
            return;
        }
        if (!pLobby.SendChat(pClient.mPlayerId, argument))
        {
            SendLine(pClient, "ERROR invalid chat");
            return;
        }
        const LobbyRoomId roomId = pLobby.RoomForPlayer(pClient.mPlayerId);
        bool raceChat = false;
        for (const LobbyRoom& room : pLobby.Rooms())
        {
            if (room.mId == roomId)
            {
                raceChat = room.mRaceRunning;
                break;
            }
        }
        for (const ClientConnection& client : pClients)
        {
            if (!raceChat || pLobby.RoomForPlayer(client.mPlayerId) == roomId)
                SendLine(client, "CHAT " + std::to_string(pClient.mPlayerId) + " " + argument);
        }
        return;
    }
    if (command == "REPORT")
    {
        const std::size_t reasonSeparator = argument.find('|');
        int targetId = 0;
        const std::string reason = reasonSeparator == std::string::npos
            ? "" : argument.substr(reasonSeparator + 1);
        const LobbyPlayer* target = nullptr;
        if (reasonSeparator != std::string::npos
            && ParseInteger(argument.substr(0, reasonSeparator), targetId))
        {
            for (const LobbyPlayer& player : pLobby.Players())
            {
                if (player.mId == static_cast<LobbyPlayerId>(targetId))
                {
                    target = &player;
                    break;
                }
            }
        }
        if (target == nullptr || target->mId == pClient.mPlayerId
            || !IsValidLobbyReportReason(reason))
            SendLine(pClient, "ERROR invalid report");
        else if (!pClient.mReportRateLimiter.Allow(MonotonicSeconds()))
            SendLine(pClient, "ERROR report rate limit");
        else
        {
            std::cout << "moderation_report reporter_id=" << pClient.mPlayerId
                      << " target_id=" << target->mId << " reason=" << reason << std::endl;
            SendLine(pClient, "REPORTACK " + std::to_string(target->mId));
        }
        return;
    }
    if (command == "CREATE")
    {
        LobbyRaceSettings settings;
        LobbyRoomId roomId = 0;
        std::string roomName;
        const std::vector<TrackDefinition>& tracks = BuiltInTracks();
        const std::size_t privacySeparator = argument.rfind('|');
        int privateRoom = 0;
        const bool parsedPrivacy = privacySeparator != std::string::npos
            && ParseInteger(argument.substr(privacySeparator + 1), privateRoom)
            && (privateRoom == 0 || privateRoom == 1);
        std::string joinCode;
        if (privateRoom != 0)
        {
            bool duplicate = false;
            do
            {
                joinCode = GenerateRoomCode();
                duplicate = false;
                for (const LobbyRoom& room : pLobby.Rooms())
                    duplicate = duplicate || room.mJoinCode == joinCode;
            }
            while (duplicate);
        }
        if (parsedPrivacy && ParseRoomFields(argument.substr(0, privacySeparator), roomName, settings)
            && settings.mTrackIndex >= 0
            && settings.mTrackIndex < static_cast<int>(tracks.size()))
        {
            roomName = tracks[settings.mTrackIndex].mName;
            if (pLobby.CreateRoom(pClient.mPlayerId, roomName, settings, roomId,
                                  privateRoom != 0, joinCode))
            {
                SendLine(pClient, "ROOM " + std::to_string(roomId)
                    + (joinCode.empty() ? "" : "|" + joinCode));
                BroadcastLobbySnapshot(pLobby, pClients);
                return;
            }
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
        if (!ParseInteger(argument, roomId) || requestedRoom == nullptr
            || requestedRoom->mPrivate)
            SendLine(pClient, "ERROR room unavailable");
        else if (requestedRoom->mRaceRunning)
            SendLine(pClient, "ERROR race running");
        else if (pLobby.RoomForPlayer(pClient.mPlayerId) != 0)
            SendLine(pClient, "ERROR already in room");
        else if (static_cast<int>(requestedRoom->mPlayerIds.size()) >= requestedRoom->mSettings.mPlayerCapacity)
            SendLine(pClient, "ERROR room full");
        else if (pLobby.JoinRoom(pClient.mPlayerId, requestedRoom->mId))
        {
            SendLine(pClient, "JOINED " + std::to_string(requestedRoom->mId));
            BroadcastLobbySnapshot(pLobby, pClients);
            return;
        }
    }
    else if (command == "JOINCODE")
    {
        const LobbyRoom* requestedRoom = nullptr;
        for (const LobbyRoom& room : pLobby.Rooms())
        {
            if (room.mPrivate && room.mJoinCode == argument)
            {
                requestedRoom = &room;
                break;
            }
        }
        if (requestedRoom == nullptr)
            SendLine(pClient, "ERROR room unavailable");
        else if (requestedRoom->mRaceRunning)
            SendLine(pClient, "ERROR race running");
        else if (pLobby.RoomForPlayer(pClient.mPlayerId) != 0)
            SendLine(pClient, "ERROR already in room");
        else if (static_cast<int>(requestedRoom->mPlayerIds.size())
                 >= requestedRoom->mSettings.mPlayerCapacity)
            SendLine(pClient, "ERROR room full");
        else if (pLobby.JoinRoom(pClient.mPlayerId, requestedRoom->mId))
        {
            SendLine(pClient, "JOINED " + std::to_string(requestedRoom->mId));
            BroadcastLobbySnapshot(pLobby, pClients);
            return;
        }
    }
    else if (command == "LEAVE")
    {
        const LobbyRoomId roomId = pLobby.RoomForPlayer(pClient.mPlayerId);
        if (argument.empty() && pLobby.LeaveRoom(pClient.mPlayerId))
        {
            StopRace(pLobby, pRaces, pChampionships, roomId);
            BroadcastLobbySnapshot(pLobby, pClients);
            return;
        }
    }
    else if (command == "READY")
    {
        int ready = 0;
        if (ParseInteger(argument, ready) && (ready == 0 || ready == 1)
            && pLobby.SetReady(pClient.mPlayerId, ready != 0))
        {
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
            if (requestedRoom != nullptr && !pLobby.AllPlayersReady(requestedRoom->mId))
            {
                SendLine(pClient, "ERROR players not ready");
                return;
            }
            if (requestedRoom != nullptr
                && race.Start(requestedRoom->mPlayerIds, requestedRoom->mSettings.mTrackIndex,
                              requestedRoom->mSettings.mLapCount,
                              requestedRoom->mSettings.mWeaponsAllowed,
                              requestedRoom->mSettings.mRivalCount,
                              requestedRoom->mSettings.mRaceMode)
                && pLobby.StartRace(pClient.mPlayerId, requestedRoom->mId))
            {
                pRaces[requestedRoom->mId] = std::move(race);
                if (requestedRoom->mSettings.mRaceMode == RaceMode::Championship)
                {
                    pChampionships.emplace(requestedRoom->mId,
                                            Championship(static_cast<int>(BuiltInTracks().size())));
                    BroadcastRaceEvent(requestedRoom->mId, requestedRoom->mSettings.mTrackIndex,
                                       pChampionships.find(requestedRoom->mId)->second,
                                       pRaces[requestedRoom->mId], pLobby, pClients);
                }
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
        int fire = 0;
        int recover = 0;
        int steeringAssist = 0;
        int brakingAssist = 0;
        int craftClass = 0;
        const LobbyRoomId roomId = pLobby.RoomForPlayer(pClient.mPlayerId);
        std::map<LobbyRoomId, AuthoritativeRace>::iterator race = pRaces.find(roomId);
        if (fields.size() == 9 && race != pRaces.end()
            && ParseDouble(fields[0], throttle) && ParseDouble(fields[1], steering)
            && ParseInteger(fields[2], jump) && ParseInteger(fields[3], reverseFacing)
            && ParseInteger(fields[4], fire) && ParseInteger(fields[5], recover)
            && ParseInteger(fields[6], steeringAssist) && ParseInteger(fields[7], brakingAssist)
            && ParseInteger(fields[8], craftClass)
            && (jump == 0 || jump == 1)
            && (reverseFacing == 0 || reverseFacing == 1) && (fire == 0 || fire == 1)
            && (recover == 0 || recover == 1)
            && (steeringAssist == 0 || steeringAssist == 1) && (brakingAssist == 0 || brakingAssist == 1)
            && craftClass >= static_cast<int>(CraftClass::Balanced)
            && craftClass <= static_cast<int>(CraftClass::Control)
            && std::isfinite(throttle) && std::isfinite(steering)
            && std::fabs(throttle) <= 1.0 && std::fabs(steering) <= 1.0
            && race->second.SubmitInput({pClient.mPlayerId, throttle, steering,
                                         jump != 0, reverseFacing != 0, fire != 0, recover != 0,
                                         steeringAssist != 0, brakingAssist != 0, craftClass}))
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
    if (pArgumentCount == 2 && std::string(pArguments[1]) == "--version")
    {
        std::cout << "OpenHoverServer " << OPENHOVER_VERSION << " (" << OPENHOVER_SOURCE_REVISION
                  << ") protocol=" << kOpenHoverProtocolVersion
                  << " content=" << kOpenHoverContentVersion << "\n";
        return 0;
    }
    if (pArgumentCount == 3 && std::string(pArguments[1]) == "--port"
        && !ParseInteger(pArguments[2], port))
    {
        std::cerr << "Invalid port\n";
        return 1;
    }
    if (pArgumentCount != 1 && pArgumentCount != 3)
    {
        std::cerr << "Usage: OpenHoverServer [--port PORT] [--version]\n";
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

    std::cout << "OpenHoverServer " << OPENHOVER_VERSION << " (" << OPENHOVER_SOURCE_REVISION
              << ") listening on TCP port " << port << std::endl;
    Lobby lobby;
    std::map<LobbyRoomId, AuthoritativeRace> races;
    std::map<LobbyRoomId, Championship> championships;
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
                if (clients.size() >= kMaximumConnections)
                {
                    ClientConnection rejected = {clientSocket, 0, "", LobbyChatRateLimiter(),
                                                 LobbyReportRateLimiter()};
                    SendLine(rejected, "ERROR server full");
                    close(clientSocket);
                    std::cout << "connection_rejected reason=server_full active_connections="
                              << clients.size() << std::endl;
                }
                else
                {
                    clients.push_back({clientSocket, 0, "", LobbyChatRateLimiter(),
                                       LobbyReportRateLimiter()});
                    std::cout << "connection_open socket=" << clientSocket
                              << " active_connections=" << clients.size() << std::endl;
                    SendLine(clients.back(), "OPENHOVER " + std::to_string(kOpenHoverProtocolVersion)
                        + " " + std::to_string(kOpenHoverContentVersion) + " " + OPENHOVER_VERSION);
                }
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
                RemoveClient(clients, index, lobby, races, championships,
                             received == 0 ? "peer_closed" : "receive_error");
                BroadcastLobbySnapshot(lobby, clients);
                continue;
            }
            client.mReceiveBuffer.append(buffer, static_cast<std::size_t>(received));
            if (client.mReceiveBuffer.size() > kMaximumReceiveBuffer)
            {
                RemoveClient(clients, index, lobby, races, championships, "receive_limit");
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
                HandleCommand(client, line, lobby, races, championships, clients);
            }
            ++index;
        }
        std::vector<LobbyRoomId> completedRaces;
        for (std::map<LobbyRoomId, AuthoritativeRace>::iterator race = races.begin(); race != races.end(); ++race)
        {
            race->second.Step();
            if (race->second.Snapshot().mTick % 4 == 0)
            {
                BroadcastRaceSnapshot(race->first, race->second, lobby, clients);
                BroadcastRaceHudSnapshot(race->first, race->second, lobby, clients);
            }
            if (race->second.Complete())
            {
                BroadcastRaceSnapshot(race->first, race->second, lobby, clients);
                BroadcastRaceHudSnapshot(race->first, race->second, lobby, clients);
                std::map<LobbyRoomId, Championship>::iterator championship = championships.find(race->first);
                if (championship != championships.end())
                {
                    std::vector<int> positions;
                    const RaceSnapshot snapshot = race->second.Snapshot();
                    for (const RaceRacerSnapshot& racer : snapshot.mRacers)
                        positions.push_back(racer.mPosition);
                    championship->second.RecordResults(positions);
                    if (championship->second.AdvanceEvent())
                    {
                        const LobbyRoom* room = nullptr;
                        for (const LobbyRoom& candidate : lobby.Rooms())
                        {
                            if (candidate.mId == race->first)
                            {
                                room = &candidate;
                                break;
                            }
                        }
                        const int nextTrack = championship->second.CurrentEvent()
                            % static_cast<int>(BuiltInTracks().size());
                        AuthoritativeRace nextRace;
                        if (room != nullptr && nextRace.Start(room->mPlayerIds, nextTrack,
                            room->mSettings.mLapCount, room->mSettings.mWeaponsAllowed,
                            room->mSettings.mRivalCount, room->mSettings.mRaceMode))
                        {
                            race->second = std::move(nextRace);
                            BroadcastRaceEvent(race->first, nextTrack, championship->second,
                                               race->second, lobby, clients);
                            continue;
                        }
                    }
                    championships.erase(championship);
                }
                BroadcastRaceFinished(race->first, lobby, clients);
                completedRaces.push_back(race->first);
            }
        }
        for (LobbyRoomId roomId : completedRaces)
            StopRace(lobby, races, championships, roomId);
        if (!completedRaces.empty())
            BroadcastLobbySnapshot(lobby, clients);
    }
    for (const ClientConnection& client : clients)
        close(client.mSocket);
    close(listenSocket);
    return 0;
}
