// SPDX-License-Identifier: MIT OR Apache-2.0
#include "Lobby.h"

#include <iostream>

int main()
{
    Lobby lobby;
    LobbyPlayerId hostId = 0;
    LobbyPlayerId guestId = 0;
    LobbyPlayerId extraId = 0;
    if (!lobby.Connect("Host", hostId) || !lobby.Connect("Guest", guestId)
        || !lobby.Connect("Extra", extraId) || lobby.Connect("Host", extraId))
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
    if (!lobby.StartRace(hostId, roomId) || lobby.JoinRoom(extraId, roomId)
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