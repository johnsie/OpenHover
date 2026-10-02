// SPDX-License-Identifier: MIT OR Apache-2.0
#include "TcpLobbyClient.h"

#include <algorithm>
#include <csignal>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace
{
bool Contains(const std::vector<std::string>& pMessages, const std::string& pExpected)
{
    return std::find(pMessages.begin(), pMessages.end(), pExpected) != pMessages.end();
}

bool TickUntil(TcpLobbyClient& pClient, const std::string& pExpected)
{
    for (int attempt = 0; attempt < 200; ++attempt)
    {
        pClient.Tick();
        const std::vector<std::string> messages = pClient.TakeMessages();
        if (Contains(messages, pExpected))
            return true;
        if (pClient.State() == TcpLobbyClientState::Failed)
        {
            std::cerr << "client failed while waiting for " << pExpected << '\n';
            return false;
        }
        usleep(10000);
    }
    std::cerr << "client timed out waiting for " << pExpected << '\n';
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
    const int port = 19701;
    const pid_t serverProcess = fork();
    if (serverProcess == 0)
    {
        execl(pArguments[1], pArguments[1], "--port", "19701", static_cast<char*>(nullptr));
        _exit(127);
    }
    if (serverProcess < 0)
    {
        std::cerr << "could not start server\n";
        return 1;
    }

    TcpLobbyClient client;
    bool connected = false;
    bool greeted = false;
    for (int attempt = 0; attempt < 100 && !greeted; ++attempt)
    {
        connected = client.Connect("127.0.0.1", port);
        if (connected)
            greeted = TickUntil(client, "OPENHOVER 1");
        if (!greeted)
        {
            client.Disconnect();
            usleep(10000);
        }
    }
    const bool queuedHello = greeted && client.SendCommand("HELLO SmokeClient");
    const bool welcomed = queuedHello && TickUntil(client, "WELCOME 1");
    client.Disconnect();
    kill(serverProcess, SIGTERM);
    int serverStatus = 0;
    waitpid(serverProcess, &serverStatus, 0);
    if (!welcomed)
    {
        std::cerr << "tcp lobby client did not complete server handshake\n";
        return 1;
    }
    return 0;
}