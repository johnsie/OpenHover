// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_LOBBY_H
#define OPENHOVER_LOBBY_H

#include "RaceMode.h"

#include <string>
#include <vector>

using LobbyPlayerId = unsigned int;
using LobbyRoomId = unsigned int;

bool IsValidLobbyNameCharacter(char pCharacter);
bool IsValidLobbyDisplayName(const std::string& pDisplayName);
bool IsValidLobbyReportReason(const std::string& pReason);

class LobbyChatRateLimiter
{
public:
    bool Allow(double pNowSeconds);

private:
    std::vector<double> mAcceptedMessageTimes;
};

class LobbyMuteList
{
public:
    void Mute(LobbyPlayerId pPlayerId);
    void Unmute(LobbyPlayerId pPlayerId);
    bool IsMuted(LobbyPlayerId pPlayerId) const;
    void Clear();

private:
    std::vector<LobbyPlayerId> mPlayerIds;
};

class LobbyReportRateLimiter
{
public:
    bool Allow(double pNowSeconds);

private:
    std::vector<double> mAcceptedReportTimes;
};

struct LobbyRaceSettings
{
    RaceMode mRaceMode = RaceMode::SingleRace;
    int mTrackIndex = 0;
    int mLapCount = 3;
    int mPlayerCapacity = 8;
    int mRivalCount = 0;
    bool mWeaponsAllowed = true;
};

struct LobbyPlayer
{
    LobbyPlayerId mId = 0;
    std::string mDisplayName;
};

struct LobbyChatMessage
{
    LobbyPlayerId mSenderId = 0;
    std::string mText;
};

struct LobbyRoom
{
    LobbyRoomId mId = 0;
    std::string mName;
    LobbyPlayerId mHostId = 0;
    LobbyRaceSettings mSettings;
    std::vector<LobbyPlayerId> mPlayerIds;
    std::vector<LobbyPlayerId> mReadyPlayerIds;
    bool mRaceRunning = false;
    bool mPrivate = false;
    std::string mJoinCode;
};

class Lobby
{
public:
    bool Connect(const std::string& pDisplayName, LobbyPlayerId& pPlayerId);
    bool Disconnect(LobbyPlayerId pPlayerId);

    bool SendChat(LobbyPlayerId pSenderId, const std::string& pText);
    bool CreateRoom(LobbyPlayerId pHostId, const std::string& pRoomName,
                    const LobbyRaceSettings& pSettings, LobbyRoomId& pRoomId,
                    bool pPrivate = false, const std::string& pJoinCode = "");
    bool JoinRoom(LobbyPlayerId pPlayerId, LobbyRoomId pRoomId);
    bool LeaveRoom(LobbyPlayerId pPlayerId);
    bool SetReady(LobbyPlayerId pPlayerId, bool pReady);
    bool AllPlayersReady(LobbyRoomId pRoomId) const;
    bool UpdateRoomSettings(LobbyPlayerId pHostId, LobbyRoomId pRoomId,
                            const LobbyRaceSettings& pSettings);
    bool StartRace(LobbyPlayerId pHostId, LobbyRoomId pRoomId);
    bool FinishRace(LobbyRoomId pRoomId);

    const std::vector<LobbyPlayer>& Players() const { return mPlayers; }
    const std::vector<LobbyChatMessage>& ChatMessages() const { return mChatMessages; }
    const std::vector<LobbyRoom>& Rooms() const { return mRooms; }
    LobbyRoomId RoomForPlayer(LobbyPlayerId pPlayerId) const;

private:
    bool HasPlayer(LobbyPlayerId pPlayerId) const;
    bool IsValidSettings(const LobbyRaceSettings& pSettings) const;
    LobbyRoom* FindRoom(LobbyRoomId pRoomId);

    LobbyPlayerId mNextPlayerId = 1;
    LobbyRoomId mNextRoomId = 1;
    std::vector<LobbyPlayer> mPlayers;
    std::vector<LobbyChatMessage> mChatMessages;
    std::vector<LobbyRoom> mRooms;
};

#endif
