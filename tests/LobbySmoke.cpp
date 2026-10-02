// SPDX-License-Identifier: MIT OR Apache-2.0
#include "Lobby.h"

#include <iostream>

int main()
{
    LobbyChatRateLimiter chatRateLimiter;
    if (!chatRateLimiter.Allow(10.0) || !chatRateLimiter.Allow(10.1)
        || !chatRateLimiter.Allow(10.2) || !chatRateLimiter.Allow(10.3)
        || chatRateLimiter.Allow(10.4) || !chatRateLimiter.Allow(12.0))
    {
        std::cerr << "chat rate limiter did not enforce its sliding window\n";
        return 1;
    }
    LobbyMuteList muteList;
    muteList.Mute(7);
    muteList.Mute(7);
    muteList.Mute(0);
    if (!muteList.IsMuted(7) || muteList.IsMuted(0))
    {
        std::cerr << "mute list did not retain a valid player\n";
        return 1;
    }
    muteList.Unmute(7);
    muteList.Mute(9);
    muteList.Clear();
    if (muteList.IsMuted(7) || muteList.IsMuted(9))
    {
        std::cerr << "mute list did not remove players\n";
        return 1;
    }
    LobbyReportRateLimiter reportRateLimiter;
    if (!IsValidLobbyReportReason("Blocking and abusive chat")
        || IsValidLobbyReportReason("") || IsValidLobbyReportReason("bad|payload")
        || !reportRateLimiter.Allow(20.0) || !reportRateLimiter.Allow(21.0)
        || reportRateLimiter.Allow(22.0) || !reportRateLimiter.Allow(80.0))
    {
        std::cerr << "report validation or rate limiting failed\n";
        return 1;
    }

    Lobby lobby;
    LobbyPlayerId hostId = 0;
    LobbyPlayerId guestId = 0;
    LobbyPlayerId extraId = 0;
    if (!lobby.Connect("Host", hostId) || !lobby.Connect("Guest", guestId)
        || !lobby.Connect("Extra", extraId) || lobby.Connect("Host", extraId)
        || !IsValidLobbyDisplayName("Pilot_7-Test")
        || IsValidLobbyDisplayName("")
        || IsValidLobbyDisplayName("ThisNameIsFarTooLongToJoin")
        || IsValidLobbyDisplayName("Two Words")
        || IsValidLobbyDisplayName("Pipe|Name")
        || IsValidLobbyDisplayName(std::string("Control\nName"))
        || lobby.Connect("Invalid Name", extraId))
    {
        std::cerr << "lobby did not validate player names\n";
        return 1;
    }
    if (!lobby.SendChat(hostId, "Welcome") || lobby.ChatMessages().size() != 1
        || lobby.SendChat(999, "Invalid"))
    {
        std::cerr << "lobby did not validate chat messages\n";
        return 1;
    }

    LobbyRaceSettings settings;
    settings.mTrackIndex = 1;
    settings.mLapCount = 4;
    settings.mPlayerCapacity = 2;
    Lobby privateLobby;
    LobbyPlayerId privateHostId = 0;
    LobbyRoomId privateRoomId = 0;
    if (!privateLobby.Connect("PrivateHost", privateHostId)
        || !privateLobby.CreateRoom(privateHostId, "Invite race", settings, privateRoomId,
                                    true, "ABC234")
        || !privateLobby.Rooms()[0].mPrivate
        || privateLobby.Rooms()[0].mJoinCode != "ABC234")
    {
        std::cerr << "lobby did not retain private room access settings\n";
        return 1;
    }
    LobbyRoomId roomId = 0;
    if (!lobby.CreateRoom(hostId, "Evening race", settings, roomId)
        || roomId == 0 || lobby.Rooms().size() != 1 || lobby.Rooms()[0].mHostId != hostId)
    {
        std::cerr << "lobby did not create a hosted room\n";
        return 1;
    }
    if (!lobby.JoinRoom(guestId, roomId) || lobby.JoinRoom(extraId, roomId)
        || lobby.RoomForPlayer(guestId) != roomId)
    {
        std::cerr << "lobby did not enforce room capacity\n";
        return 1;
    }

    LobbyRaceSettings updatedSettings = settings;
    updatedSettings.mWeaponsAllowed = false;
    if (lobby.UpdateRoomSettings(guestId, roomId, updatedSettings)
        || !lobby.UpdateRoomSettings(hostId, roomId, updatedSettings)
        || lobby.Rooms()[0].mSettings.mWeaponsAllowed)
    {
        std::cerr << "lobby did not enforce host-only settings\n";
        return 1;
    }
    if (lobby.StartRace(hostId, roomId) || !lobby.SetReady(guestId, true)
        || !lobby.AllPlayersReady(roomId) || !lobby.StartRace(hostId, roomId)
        || lobby.JoinRoom(extraId, roomId)
        || lobby.UpdateRoomSettings(hostId, roomId, settings) || !lobby.FinishRace(roomId))
    {
        std::cerr << "lobby did not enforce race lifecycle\n";
        return 1;
    }
    if (!lobby.Disconnect(hostId) || lobby.Rooms()[0].mHostId != guestId
        || !lobby.LeaveRoom(guestId) || !lobby.Rooms().empty())
    {
        std::cerr << "lobby did not transfer or remove room ownership\n";
        return 1;
    }
    return 0;
}
