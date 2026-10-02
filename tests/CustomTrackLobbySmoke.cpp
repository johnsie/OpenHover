// SPDX-License-Identifier: MIT OR Apache-2.0
// Starts a real race server with a custom track installed and checks that the track is only
// playable by clients that declare the same file (by hash).
#include "Protocol.h"
#include "TcpLobbyClient.h"
#include "TrackFile.h"
#include "TrackHash.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace
{
bool Collect(TcpLobbyClient& pClient, std::vector<std::string>& pMessages, const std::string& pPrefix)
{
    for (int attempt = 0; attempt < 300; ++attempt)
    {
        pClient.Tick();
        for (const std::string& message : pClient.TakeMessages())
            pMessages.push_back(message);
        for (const std::string& message : pMessages)
        {
            if (message.compare(0, pPrefix.size(), pPrefix) == 0)
                return true;
        }
        if (pClient.State() == TcpLobbyClientState::Failed)
            return false;
        usleep(10000);
    }
    return false;
}

bool Hello(TcpLobbyClient& pClient, int pPort, const std::string& pName, std::vector<std::string>& pMessages)
{
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        pMessages.clear();
        if (pClient.Connect("127.0.0.1", pPort)
            && Collect(pClient, pMessages, "OPENHOVER ")
            && pClient.SendCommand("HELLO " + std::to_string(kOpenHoverProtocolVersion) + "|"
                                   + std::to_string(kOpenHoverContentVersion) + "|" + pName)
            && Collect(pClient, pMessages, "WELCOME "))
            return true;
        pClient.Disconnect();
        usleep(10000);
    }
    return false;
}

void Expect(bool pCondition, const char* pMessage, bool& pOk)
{
    if (!pCondition)
    {
        std::cerr << pMessage << '\n';
        pOk = false;
    }
}

bool Has(const std::vector<std::string>& pMessages, const std::string& pText)
{
    for (const std::string& message : pMessages)
    {
        if (message.find(pText) != std::string::npos)
            return true;
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
    bool ok = true;
    char pattern[] = "/tmp/openhover-customtrack-XXXXXX";
    const char* directory = mkdtemp(pattern);
    if (directory == nullptr)
        return 1;
    const std::string base = directory;
    TrackDefinition custom = BuiltInTracks()[0];
    custom.mId = "friends-track";
    custom.mName = "Friends Track";
    const std::string hash = TrackHash(custom);
    std::ofstream(base + "/friends.ohtrack") << SerializeTrack(custom);
    TrackDefinition unsafe = BuiltInTracks()[0];
    unsafe.mId = "bad|id";
    unsafe.mName = "Unsafe Track";
    std::ofstream(base + "/unsafe.ohtrack") << SerializeTrack(unsafe);

    const int port = 20000 + static_cast<int>((getpid() + 7919) % 20000);
    const std::string portText = std::to_string(port);
    const pid_t server = fork();
    if (server == 0)
    {
        execl(pArguments[1], pArguments[1], "--port", portText.c_str(), "--tracks", base.c_str(),
              static_cast<char*>(nullptr));
        _exit(127);
    }

    TcpLobbyClient host;
    TcpLobbyClient guest;
    TcpLobbyClient liar;
    std::vector<std::string> hostMessages;
    std::vector<std::string> guestMessages;
    std::vector<std::string> liarMessages;
    const int customIndex = static_cast<int>(BuiltInTracks().size());
    if (!Hello(host, port, "Host", hostMessages))
    {
        std::cerr << "host could not connect\n";
        kill(server, SIGTERM);
        waitpid(server, nullptr, 0);
        return 1;
    }
    Collect(host, hostMessages, "TRACKS ");
    Collect(host, hostMessages, "TRACK " + std::to_string(customIndex) + "|");
    Expect(Has(hostMessages, "TRACKS " + std::to_string(customIndex + 1)),
           "the server lists built-in plus one valid custom track (the unsafe one is skipped)", ok);
    Expect(Has(hostMessages, "TRACK " + std::to_string(customIndex) + "|friends-track|Friends Track|" + hash),
           "the server publishes the custom track's id, name and hash", ok);

    // A host without the file cannot create a room on the custom track.
    host.SendCommand("CREATE Friends Track|0|" + std::to_string(customIndex) + "|3|2|0|1|0");
    Collect(host, hostMessages, "ERROR MISSING TRACK Friends Track");
    Expect(Has(hostMessages, "ERROR MISSING TRACK Friends Track"), "creating without the file is refused", ok);

    // Declaring a malformed hash is rejected; declaring the real one is accepted.
    host.SendCommand("OWN not-a-hash");
    Collect(host, hostMessages, "ERROR invalid track declaration");
    Expect(Has(hostMessages, "ERROR invalid track declaration"), "a malformed declaration is rejected", ok);
    host.SendCommand("OWN " + hash);
    host.SendCommand("CREATE Friends Track|0|" + std::to_string(customIndex) + "|3|2|0|1|0");
    Expect(Collect(host, hostMessages, "ROOM "), "a host with the file creates the room", ok);

    // A guest without the file, or with a different file's hash, cannot join.
    Hello(guest, port, "Guest", guestMessages);
    guest.SendCommand("JOIN 1");
    Collect(guest, guestMessages, "ERROR MISSING TRACK Friends Track");
    Expect(Has(guestMessages, "ERROR MISSING TRACK Friends Track"), "joining without the file is refused", ok);
    TrackDefinition altered = custom;
    altered.mWaypoints[2].mX += 0.5;
    guest.SendCommand("OWN " + TrackHash(altered));
    guestMessages.clear();
    guest.SendCommand("JOIN 1");
    Collect(guest, guestMessages, "ERROR MISSING TRACK Friends Track");
    Expect(Has(guestMessages, "ERROR MISSING TRACK Friends Track"),
           "a slightly different copy of the track is refused", ok);
    guestMessages.clear();
    guest.SendCommand("OWN " + hash);
    guest.SendCommand("JOIN 1");
    Expect(Collect(guest, guestMessages, "JOINED 1"), "a guest with the identical file joins", ok);

    // The host cannot switch a room with players in it to a track one of them lacks.
    // Built-in track 0 is available to everyone, so switching to it works.
    Hello(liar, port, "Third", liarMessages);
    host.SendCommand("SET 1 0|3|2|0|1|0");
    hostMessages.clear();
    Collect(host, hostMessages, "LOBBY");
    Expect(Has(hostMessages, "LOBBY"), "switching to a built-in track is allowed", ok);

    kill(server, SIGTERM);
    waitpid(server, nullptr, 0);
    for (const char* file : {"friends.ohtrack", "unsafe.ohtrack"})
        std::remove((base + "/" + file).c_str());
    rmdir(base.c_str());
    return ok ? 0 : 1;
}
