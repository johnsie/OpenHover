// SPDX-License-Identifier: MIT OR Apache-2.0
#include "TrackHash.h"

#include "Sha256.h"
#include "TrackFile.h"

std::string TrackHash(const TrackDefinition& pTrack)
{
    const std::string text = SerializeTrack(pTrack);
    return Sha256::HashBytes(text.data(), text.size());
}

bool IsOnlineSafeTrackText(const std::string& pText)
{
    if (pText.empty() || pText.size() > 24 || pText.front() == ' ' || pText.back() == ' ')
        return false;
    for (char c : pText)
    {
        const bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
            || c == ' ' || c == '.' || c == '_' || c == '-';
        if (!allowed)
            return false;
    }
    return true;
}

bool IsOnlineSafeTrack(const TrackDefinition& pTrack)
{
    return IsOnlineSafeTrackText(pTrack.mId) && IsOnlineSafeTrackText(pTrack.mName);
}
