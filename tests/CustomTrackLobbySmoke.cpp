// SPDX-License-Identifier: MIT OR Apache-2.0
// Starts a real race server and checks that a host can carry a custom track into a room, that
// other players download it and verify it by hash, and that bad uploads are refused.
#include "Protocol.h"
#include "TcpLobbyClient.h"
#include "TrackFile.h"
#include "TrackHash.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <signal.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace
{
bool HasPrefix(const std::vector<std::string>& pMessages, const std::string& pPrefix)
{
    for (const std::string& message : pMessages)
    {
        if (message.compare(0, pPrefix.size(), pPrefix) == 0)
            return true;
    }
    return false;
}

bool Collect(TcpLobbyClient& pClient, std::vector<std::string>& pMessages, const std::string& pPrefix)
{
    for (int attempt = 0; attempt < 400; ++attempt)
    {
        pClient.Tick();
        for (const std::string& message : pClient.TakeMessages())
            pMessages.push_back(message);
        if (HasPrefix(pMessages, pPrefix))
            return true;
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
        if (pClient.Connect("127.0.0.1", pPort) && Collect(pClient, pMessages, "OPENHOVER ")
            && pClient.SendCommand("HELLO " + std::to_string(kOpenHoverProtocolVersion) + "|"
                                   + std::to_string(kOpenHoverContentVersion) + "|" + pName)
            && Collect(pClient, pMessages, "WELCOME "))
            return true;
        pClient.Disconnect();
        usleep(10000);
    }
    return false;
}

std::string HexEncode(const std::string& pBytes)
{
    static const char* const digits = "0123456789abcdef";
    std::string out;
    for (unsigned char c : pBytes)
    {
        out += digits[c >> 4];
        out += digits[c & 15];
    }
    return out;
}

std::string HexDecode(const std::string& pHex)
{
    std::string out;
    for (std::size_t index = 0; index + 1 < pHex.size(); index += 2)
        out += static_cast<char>(std::stoi(pHex.substr(index, 2), nullptr, 16));
    return out;
}

// Sends an upload and returns the server's final answer line ("TRACKUP OK ..." or "ERROR ...").
std::string Upload(TcpLobbyClient& pClient, const std::string& pText, const std::string& pHash)
{
    std::vector<std::string> messages;
    pClient.SendCommand("TRACKUP " + std::to_string(pText.size()) + "|" + pHash);
    if (!Collect(pClient, messages, "TRACKUP READY"))
        return messages.empty() ? "" : messages.back();
    const std::string hex = HexEncode(pText);
    for (std::size_t offset = 0; offset < hex.size(); offset += kTrackChunkHexCharacters)
        pClient.SendCommand("TRACKDATA " + hex.substr(offset, kTrackChunkHexCharacters));
    messages.clear();
    for (int attempt = 0; attempt < 400; ++attempt)
    {
        pClient.Tick();
        for (const std::string& message : pClient.TakeMessages())
        {
            if (message.compare(0, 8, "TRACKUP ") == 0 || message.compare(0, 6, "ERROR ") == 0)
                return message;
        }
        usleep(10000);
    }
    return "";
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
    TrackDefinition custom = BuiltInTracks()[0];
    custom.mId = "friends-track";
    custom.mName = "Friends Track";
    const std::string text = SerializeTrack(custom);
    const std::string hash = TrackHash(custom);

    const int port = 20000 + static_cast<int>((getpid() + 7919) % 20000);
    const std::string portText = std::to_string(port);
    const pid_t server = fork();
    if (server == 0)
    {
        execl(pArguments[1], pArguments[1], "--port", portText.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }

    TcpLobbyClient host;
    TcpLobbyClient guest;
    TcpLobbyClient stranger;
    std::vector<std::string> hostMessages;
    std::vector<std::string> guestMessages;
    std::vector<std::string> strangerMessages;
    if (!Hello(host, port, "Host", hostMessages))
    {
        std::cerr << "host could not connect\n";
        kill(server, SIGTERM);
        waitpid(server, nullptr, 0);
        return 1;
    }
    const std::string customRoom = "Friends Track|0|" + std::to_string(kCustomTrackIndex) + "|3|2|0|1|0";

    // Creating a custom room needs an accepted upload first.
    host.SendCommand("CREATE " + customRoom);
    Collect(host, hostMessages, "ERROR TRACK REJECTED upload the track first");
    Expect(HasPrefix(hostMessages, "ERROR TRACK REJECTED upload the track first"),
           "creating a custom room without an upload is refused", ok);

    // Bad uploads are refused, each for its own reason.
    {
        TcpLobbyClient bad;
        std::vector<std::string> badMessages;
        Hello(bad, port, "Bad", badMessages);
        Expect(Has({Upload(bad, text, std::string(64, 'a'))}, "hash does not match"),
               "a wrong hash is refused", ok);
        Expect(Has({Upload(bad, "# note\n" + text, hash)}, "canonical"),
               "non-canonical text is refused", ok);
        TrackDefinition unsafe = custom;
        unsafe.mName = "Bad|Name";
        Expect(Has({Upload(bad, SerializeTrack(unsafe), TrackHash(unsafe))}, "TRACK REJECTED"),
               "an unsafe name is refused", ok);
        TrackDefinition broken = custom;
        broken.mCheckpoints.pop_back();
        Expect(Has({Upload(bad, SerializeTrack(broken), TrackHash(broken))}, "checkpoints"),
               "a track that fails validation is refused", ok);
        TrackDefinition clash = custom;
        clash.mName = BuiltInTracks()[1].mName;
        Expect(Has({Upload(bad, SerializeTrack(clash), TrackHash(clash))}, "built-in"),
               "a built-in name is refused", ok);
        TrackDefinition huge = custom;
        huge.mRoadHalfWidth = 100.0;
        Expect(Has({Upload(bad, SerializeTrack(huge), TrackHash(huge))}, "half-width"),
               "an out-of-range road width is refused", ok);
        bad.SendCommand("TRACKUP 99999999|" + hash);
        badMessages.clear();
        Collect(bad, badMessages, "ERROR TRACK REJECTED invalid upload header");
        Expect(HasPrefix(badMessages, "ERROR TRACK REJECTED invalid upload header"),
               "an oversized upload header is refused", ok);
    }

    // The real upload is accepted and the room carries the track's hash.
    const std::string accepted = Upload(host, text, hash);
    Expect(accepted == "TRACKUP OK " + hash, "the host's upload is accepted", ok);
    host.SendCommand("CREATE " + customRoom);
    Expect(Collect(host, hostMessages, "ROOM "), "the host creates the custom room", ok);
    hostMessages.clear();
    Collect(host, hostMessages, "LOBBY");
    Expect(Has(hostMessages, "," + hash), "the lobby snapshot advertises the room's track hash", ok);

    // A guest downloads the track from the room, and it is byte-for-byte the host's.
    Hello(guest, port, "Guest", guestMessages);
    guest.SendCommand("TRACKGET 1");
    Collect(guest, guestMessages, "TRACKEND 1");
    std::string downloaded;
    for (const std::string& message : guestMessages)
    {
        if (message.compare(0, 11, "TRACKCHUNK ") == 0)
            downloaded += HexDecode(message.substr(message.find('|') + 1));
    }
    Expect(downloaded == text, "the downloaded track is identical to the uploaded one", ok);
    Expect(Has(guestMessages, "TRACKDL 1|" + std::to_string(text.size()) + "|" + hash),
           "the download header carries the size and hash", ok);

    // Ready is refused until the guest has verified the track.
    guest.SendCommand("JOIN 1");
    Expect(Collect(guest, guestMessages, "JOINED 1"), "the guest joins the room", ok);
    guestMessages.clear();
    guest.SendCommand("READY 1");
    Collect(guest, guestMessages, "ERROR TRACK NOT VERIFIED YET");
    Expect(HasPrefix(guestMessages, "ERROR TRACK NOT VERIFIED YET"), "ready needs a verified track", ok);
    guest.SendCommand("HAVE " + std::string(64, 'b'));
    guestMessages.clear();
    guest.SendCommand("READY 1");
    Collect(guest, guestMessages, "ERROR TRACK NOT VERIFIED YET");
    Expect(HasPrefix(guestMessages, "ERROR TRACK NOT VERIFIED YET"), "a different track's hash does not count", ok);
    guest.SendCommand("HAVE " + hash);
    guestMessages.clear();
    guest.SendCommand("READY 1");
    Expect(Collect(guest, guestMessages, "LOBBY"), "ready is accepted once verified", ok);

    // The host starts the race on the custom track.
    hostMessages.clear();
    host.SendCommand("START 1");
    Expect(Collect(host, hostMessages, "RACE "), "the race starts on the custom track", ok);

    // Downloads are rate limited, and a stranger cannot ask for a track that does not exist.
    Hello(stranger, port, "Stranger", strangerMessages);
    stranger.SendCommand("TRACKGET 77");
    Collect(stranger, strangerMessages, "ERROR no custom track for that room");
    Expect(HasPrefix(strangerMessages, "ERROR no custom track for that room"),
           "asking for a missing room's track is refused", ok);
    strangerMessages.clear();
    for (int count = 0; count < 10; ++count)
        stranger.SendCommand("TRACKGET 1");
    Collect(stranger, strangerMessages, "ERROR too many track downloads");
    Expect(HasPrefix(strangerMessages, "ERROR too many track downloads"), "downloads are rate limited", ok);

    kill(server, SIGTERM);
    waitpid(server, nullptr, 0);
    return ok ? 0 : 1;
}
