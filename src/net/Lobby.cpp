// SPDX-License-Identifier: MIT OR Apache-2.0
#include "Lobby.h"

#include <algorithm>

namespace
{
const std::size_t kMaximumDisplayNameLength = 24;
const std::size_t kMaximumRoomNameLength = 40;
const std::size_t kMaximumChatMessageLength = 256;
const std::size_t kMaximumChatHistory = 100;
const std::size_t kMaximumChatMessagesPerWindow = 4;
const double kChatRateLimitWindowSeconds = 2.0;
const std::size_t kMaximumReportsPerWindow = 2;
const double kReportRateLimitWindowSeconds = 60.0;
}

bool IsValidLobbyNameCharacter(char pCharacter)
{
    return (pCharacter >= 'A' && pCharacter <= 'Z')
        || (pCharacter >= 'a' && pCharacter <= 'z')
        || (pCharacter >= '0' && pCharacter <= '9')
        || pCharacter == '_' || pCharacter == '-';
}

bool IsValidLobbyDisplayName(const std::string& pDisplayName)
{
    if (pDisplayName.empty() || pDisplayName.size() > kMaximumDisplayNameLength)
        return false;
    return std::all_of(pDisplayName.begin(), pDisplayName.end(), IsValidLobbyNameCharacter);
}

bool IsValidLobbyReportReason(const std::string& pReason)
{
    if (pReason.empty() || pReason.size() > 120)
        return false;
    return std::all_of(pReason.begin(), pReason.end(), [](char pCharacter)
    {
        return pCharacter >= 32 && pCharacter <= 126 && pCharacter != '|';
    });
}

bool LobbyChatRateLimiter::Allow(double pNowSeconds)
{
    mAcceptedMessageTimes.erase(
        std::remove_if(mAcceptedMessageTimes.begin(), mAcceptedMessageTimes.end(),
                       [pNowSeconds](double pMessageTime)
                       {
                           return pNowSeconds - pMessageTime >= kChatRateLimitWindowSeconds;
                       }),
        mAcceptedMessageTimes.end());
    if (mAcceptedMessageTimes.size() >= kMaximumChatMessagesPerWindow)
        return false;
    mAcceptedMessageTimes.push_back(pNowSeconds);
    return true;
}

void LobbyMuteList::Mute(LobbyPlayerId pPlayerId)
{
    if (pPlayerId != 0 && !IsMuted(pPlayerId))
        mPlayerIds.push_back(pPlayerId);
}

void LobbyMuteList::Unmute(LobbyPlayerId pPlayerId)
{
    mPlayerIds.erase(std::remove(mPlayerIds.begin(), mPlayerIds.end(), pPlayerId), mPlayerIds.end());
}

bool LobbyMuteList::IsMuted(LobbyPlayerId pPlayerId) const
{
    return std::find(mPlayerIds.begin(), mPlayerIds.end(), pPlayerId) != mPlayerIds.end();
}

void LobbyMuteList::Clear()
{
    mPlayerIds.clear();
}

bool LobbyReportRateLimiter::Allow(double pNowSeconds)
{
    mAcceptedReportTimes.erase(
        std::remove_if(mAcceptedReportTimes.begin(), mAcceptedReportTimes.end(),
                       [pNowSeconds](double pReportTime)
                       {
                           return pNowSeconds - pReportTime >= kReportRateLimitWindowSeconds;
                       }),
        mAcceptedReportTimes.end());
    if (mAcceptedReportTimes.size() >= kMaximumReportsPerWindow)
        return false;
    mAcceptedReportTimes.push_back(pNowSeconds);
    return true;
}

bool Lobby::Connect(const std::string& pDisplayName, LobbyPlayerId& pPlayerId)
{
    if (!IsValidLobbyDisplayName(pDisplayName))
        return false;
    for (const LobbyPlayer& player : mPlayers)
    {
        if (player.mDisplayName == pDisplayName)
            return false;
    }
    pPlayerId = mNextPlayerId++;
    mPlayers.push_back({pPlayerId, pDisplayName});
    return true;
}

bool Lobby::Disconnect(LobbyPlayerId pPlayerId)
{
    if (!HasPlayer(pPlayerId))
        return false;
    LeaveRoom(pPlayerId);
    mPlayers.erase(std::remove_if(mPlayers.begin(), mPlayers.end(),
                                  [pPlayerId](const LobbyPlayer& pPlayer)
                                  {
                                      return pPlayer.mId == pPlayerId;
                                  }),
                   mPlayers.end());
    return true;
}

bool Lobby::SendChat(LobbyPlayerId pSenderId, const std::string& pText)
{
    if (!HasPlayer(pSenderId) || pText.empty() || pText.size() > kMaximumChatMessageLength)
        return false;
    mChatMessages.push_back({pSenderId, pText});
    if (mChatMessages.size() > kMaximumChatHistory)
        mChatMessages.erase(mChatMessages.begin());
    return true;
}

bool Lobby::CreateRoom(LobbyPlayerId pHostId, const std::string& pRoomName,
                       const LobbyRaceSettings& pSettings, LobbyRoomId& pRoomId,
                       bool pPrivate, const std::string& pJoinCode)
{
    if (!HasPlayer(pHostId) || RoomForPlayer(pHostId) != 0 || pRoomName.empty()
        || pRoomName.size() > kMaximumRoomNameLength || !IsValidSettings(pSettings)
        || (pPrivate && pJoinCode.empty()))
        return false;
    pRoomId = mNextRoomId++;
    LobbyRoom room;
    room.mId = pRoomId;
    room.mName = pRoomName;
    room.mHostId = pHostId;
    room.mSettings = pSettings;
    room.mPrivate = pPrivate;
    room.mJoinCode = pPrivate ? pJoinCode : "";
    room.mPlayerIds.push_back(pHostId);
    room.mReadyPlayerIds.push_back(pHostId);
    mRooms.push_back(room);
    return true;
}

bool Lobby::JoinRoom(LobbyPlayerId pPlayerId, LobbyRoomId pRoomId)
{
    LobbyRoom* room = FindRoom(pRoomId);
    if (!HasPlayer(pPlayerId) || room == nullptr || room->mRaceRunning
        || RoomForPlayer(pPlayerId) != 0
        || static_cast<int>(room->mPlayerIds.size()) >= room->mSettings.mPlayerCapacity)
        return false;
    room->mPlayerIds.push_back(pPlayerId);
    return true;
}

bool Lobby::LeaveRoom(LobbyPlayerId pPlayerId)
{
    const LobbyRoomId roomId = RoomForPlayer(pPlayerId);
    LobbyRoom* room = FindRoom(roomId);
    if (room == nullptr)
        return false;
    room->mPlayerIds.erase(std::remove(room->mPlayerIds.begin(), room->mPlayerIds.end(), pPlayerId),
                           room->mPlayerIds.end());
    room->mReadyPlayerIds.erase(
        std::remove(room->mReadyPlayerIds.begin(), room->mReadyPlayerIds.end(), pPlayerId),
        room->mReadyPlayerIds.end());
    if (room->mPlayerIds.empty())
    {
        mRooms.erase(std::remove_if(mRooms.begin(), mRooms.end(),
                                    [roomId](const LobbyRoom& pRoom)
                                    {
                                        return pRoom.mId == roomId;
                                    }),
                     mRooms.end());
    }
    else if (room->mHostId == pPlayerId)
    {
        room->mHostId = room->mPlayerIds.front();
        if (std::find(room->mReadyPlayerIds.begin(), room->mReadyPlayerIds.end(), room->mHostId)
            == room->mReadyPlayerIds.end())
            room->mReadyPlayerIds.push_back(room->mHostId);
    }
    return true;
}

bool Lobby::SetReady(LobbyPlayerId pPlayerId, bool pReady)
{
    LobbyRoom* room = FindRoom(RoomForPlayer(pPlayerId));
    if (room == nullptr || room->mRaceRunning || room->mHostId == pPlayerId)
        return false;
    const std::vector<LobbyPlayerId>::iterator ready = std::find(
        room->mReadyPlayerIds.begin(), room->mReadyPlayerIds.end(), pPlayerId);
    if (pReady && ready == room->mReadyPlayerIds.end())
        room->mReadyPlayerIds.push_back(pPlayerId);
    else if (!pReady && ready != room->mReadyPlayerIds.end())
        room->mReadyPlayerIds.erase(ready);
    return true;
}

bool Lobby::AllPlayersReady(LobbyRoomId pRoomId) const
{
    for (const LobbyRoom& room : mRooms)
    {
        if (room.mId == pRoomId)
            return room.mReadyPlayerIds.size() == room.mPlayerIds.size();
    }
    return false;
}

bool Lobby::UpdateRoomSettings(LobbyPlayerId pHostId, LobbyRoomId pRoomId,
                               const LobbyRaceSettings& pSettings)
{
    LobbyRoom* room = FindRoom(pRoomId);
    if (room == nullptr || room->mRaceRunning || room->mHostId != pHostId
        || !IsValidSettings(pSettings) || static_cast<int>(room->mPlayerIds.size()) > pSettings.mPlayerCapacity)
        return false;
    room->mSettings = pSettings;
    room->mReadyPlayerIds.clear();
    room->mReadyPlayerIds.push_back(room->mHostId);
    return true;
}

bool Lobby::StartRace(LobbyPlayerId pHostId, LobbyRoomId pRoomId)
{
    LobbyRoom* room = FindRoom(pRoomId);
    if (room == nullptr || room->mRaceRunning || room->mHostId != pHostId
        || !AllPlayersReady(pRoomId))
        return false;
    room->mRaceRunning = true;
    return true;
}

bool Lobby::FinishRace(LobbyRoomId pRoomId)
{
    LobbyRoom* room = FindRoom(pRoomId);
    if (room == nullptr || !room->mRaceRunning)
        return false;
    room->mRaceRunning = false;
    return true;
}

LobbyRoomId Lobby::RoomForPlayer(LobbyPlayerId pPlayerId) const
{
    for (const LobbyRoom& room : mRooms)
    {
        if (std::find(room.mPlayerIds.begin(), room.mPlayerIds.end(), pPlayerId)
            != room.mPlayerIds.end())
            return room.mId;
    }
    return 0;
}

bool Lobby::HasPlayer(LobbyPlayerId pPlayerId) const
{
    return std::find_if(mPlayers.begin(), mPlayers.end(),
                        [pPlayerId](const LobbyPlayer& pPlayer)
                        {
                            return pPlayer.mId == pPlayerId;
                        }) != mPlayers.end();
}

bool Lobby::IsValidSettings(const LobbyRaceSettings& pSettings) const
{
    return pSettings.mTrackIndex >= 0 && pSettings.mLapCount >= 1 && pSettings.mLapCount <= 5
        && pSettings.mPlayerCapacity >= 2 && pSettings.mPlayerCapacity <= 8
        && pSettings.mRivalCount >= 0 && pSettings.mRivalCount <= 7;
}

LobbyRoom* Lobby::FindRoom(LobbyRoomId pRoomId)
{
    for (LobbyRoom& room : mRooms)
    {
        if (room.mId == pRoomId)
            return &room;
    }
    return nullptr;
}
