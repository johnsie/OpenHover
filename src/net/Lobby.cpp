// SPDX-License-Identifier: MIT OR Apache-2.0
#include "Lobby.h"

#include <algorithm>

namespace
{
const std::size_t kMaximumDisplayNameLength = 24;
const std::size_t kMaximumRoomNameLength = 40;
const std::size_t kMaximumChatMessageLength = 256;
const std::size_t kMaximumChatHistory = 100;
}

bool Lobby::Connect(const std::string& pDisplayName, LobbyPlayerId& pPlayerId)
{
    if (pDisplayName.empty() || pDisplayName.size() > kMaximumDisplayNameLength)
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
                       const LobbyRaceSettings& pSettings, LobbyRoomId& pRoomId)
{
    if (!HasPlayer(pHostId) || RoomForPlayer(pHostId) != 0 || pRoomName.empty()
        || pRoomName.size() > kMaximumRoomNameLength || !IsValidSettings(pSettings))
        return false;
    pRoomId = mNextRoomId++;
    LobbyRoom room;
    room.mId = pRoomId;
    room.mName = pRoomName;
    room.mHostId = pHostId;
    room.mSettings = pSettings;
    room.mPlayerIds.push_back(pHostId);
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
        room->mHostId = room->mPlayerIds.front();
    return true;
}

bool Lobby::UpdateRoomSettings(LobbyPlayerId pHostId, LobbyRoomId pRoomId,
                               const LobbyRaceSettings& pSettings)
{
    LobbyRoom* room = FindRoom(pRoomId);
    if (room == nullptr || room->mRaceRunning || room->mHostId != pHostId
        || !IsValidSettings(pSettings) || static_cast<int>(room->mPlayerIds.size()) > pSettings.mPlayerCapacity)
        return false;
    room->mSettings = pSettings;
    return true;
}

bool Lobby::StartRace(LobbyPlayerId pHostId, LobbyRoomId pRoomId)
{
    LobbyRoom* room = FindRoom(pRoomId);
    if (room == nullptr || room->mRaceRunning || room->mHostId != pHostId)
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