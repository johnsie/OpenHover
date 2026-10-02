// SPDX-License-Identifier: MIT OR Apache-2.0
#include "GhostLibrary.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace
{
const char* const kMagic = "OPENHOVER-GHOSTS";
constexpr double kMaximumSeconds = 3600.0;
constexpr std::size_t kMaximumFrames = 60 * 3600;

bool ParseDouble(const std::string& pToken, double& pOut)
{
    char* end = nullptr;
    pOut = std::strtod(pToken.c_str(), &end);
    return end != pToken.c_str() && *end == '\0';
}
}

const GhostLibrary::Entry* GhostLibrary::FindEntry(const std::string& pTrackId,
                                                   int pCraftClass) const
{
    for (const Entry& entry : mEntries)
    {
        if (entry.mTrackId == pTrackId && entry.mCraftClass == pCraftClass)
            return &entry;
    }
    return nullptr;
}

bool GhostLibrary::Submit(const std::string& pTrackId, int pCraftClass, double pSeconds,
                          const InputRecording& pRecording)
{
    if (pRecording.Empty() || pSeconds <= 0.0 || pSeconds > kMaximumSeconds)
        return false;
    for (Entry& entry : mEntries)
    {
        if (entry.mTrackId == pTrackId && entry.mCraftClass == pCraftClass)
        {
            if (pSeconds >= entry.mSeconds)
                return false;
            entry.mSeconds = pSeconds;
            entry.mRecording = pRecording;
            return true;
        }
    }
    Entry entry;
    entry.mTrackId = pTrackId;
    entry.mCraftClass = pCraftClass;
    entry.mSeconds = pSeconds;
    entry.mRecording = pRecording;
    mEntries.push_back(entry);
    return true;
}

const InputRecording* GhostLibrary::Find(const std::string& pTrackId, int pCraftClass) const
{
    const Entry* entry = FindEntry(pTrackId, pCraftClass);
    return entry != nullptr ? &entry->mRecording : nullptr;
}

double GhostLibrary::BestSeconds(const std::string& pTrackId, int pCraftClass) const
{
    const Entry* entry = FindEntry(pTrackId, pCraftClass);
    return entry != nullptr ? entry->mSeconds : 0.0;
}

std::string GhostLibrary::Serialize(int pVersionTag) const
{
    std::ostringstream out;
    out << kMagic << ' ' << pVersionTag << ' ' << mEntries.size() << '\n';
    char buffer[96];
    for (const Entry& entry : mEntries)
    {
        out << entry.mTrackId << ' ' << entry.mCraftClass << ' ';
        std::snprintf(buffer, sizeof(buffer), "%a", entry.mSeconds);
        out << buffer << ' ' << entry.mRecording.FrameCount() << '\n';
        for (std::size_t frame = 0; frame < entry.mRecording.FrameCount(); ++frame)
        {
            HovercraftInput input;
            entry.mRecording.InputAt(frame, input);
            // Hexadecimal floats round-trip exactly, which replay determinism depends on.
            std::snprintf(buffer, sizeof(buffer), "%a %a %d\n", input.mThrottle, input.mSteering,
                          (input.mBoost ? 1 : 0) | (input.mJump ? 2 : 0) | (input.mFire ? 4 : 0)
                              | (input.mReverseFacing ? 8 : 0));
            out << buffer;
        }
    }
    return out.str();
}

bool GhostLibrary::Parse(const std::string& pText, int pVersionTag)
{
    std::istringstream in(pText);
    std::string magic;
    int tag = 0;
    std::size_t count = 0;
    if (!(in >> magic >> tag >> count) || magic != kMagic || tag != pVersionTag || count > 1000)
        return false;
    std::vector<Entry> parsed;
    for (std::size_t index = 0; index < count; ++index)
    {
        Entry entry;
        std::string secondsToken;
        std::size_t frames = 0;
        if (!(in >> entry.mTrackId >> entry.mCraftClass >> secondsToken >> frames)
            || !ParseDouble(secondsToken, entry.mSeconds) || entry.mSeconds <= 0.0
            || entry.mSeconds > kMaximumSeconds || frames == 0 || frames > kMaximumFrames)
            return false;
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            std::string throttleToken;
            std::string steeringToken;
            int flags = 0;
            HovercraftInput input;
            if (!(in >> throttleToken >> steeringToken >> flags)
                || !ParseDouble(throttleToken, input.mThrottle)
                || !ParseDouble(steeringToken, input.mSteering) || flags < 0 || flags > 15)
                return false;
            input.mBoost = (flags & 1) != 0;
            input.mJump = (flags & 2) != 0;
            input.mFire = (flags & 4) != 0;
            input.mReverseFacing = (flags & 8) != 0;
            entry.mRecording.Record(input);
        }
        parsed.push_back(entry);
    }
    std::string trailing;
    if (in >> trailing)
        return false;
    mEntries = parsed;
    return true;
}
