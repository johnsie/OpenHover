// SPDX-License-Identifier: MIT OR Apache-2.0
#include "TrackHash.h"

#include "Sha256.h"
#include "TrackFile.h"

#include <cmath>

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

std::string CheckOnlineTrackLimits(const TrackDefinition& pTrack)
{
    if (pTrack.mWaypoints.size() > 256)
        return "too many waypoints (limit 256)";
    if (pTrack.mBoostPads.size() > 128 || pTrack.mHazardZones.size() > 64 || pTrack.mMines.size() > 64
        || pTrack.mRaisedSections.size() > 64)
        return "too many pads, hazards, mines, or raised sections";
    if (pTrack.mRoadHalfWidth < 2.0 || pTrack.mRoadHalfWidth > 30.0)
        return "road half-width must be between 2 and 30";
    const double kLimit = 5000.0;
    const auto inRange = [kLimit](double pX, double pY, double pRadius)
    {
        return std::fabs(pX) <= kLimit && std::fabs(pY) <= kLimit && pRadius > 0.0 && pRadius <= 200.0;
    };
    for (const RaceGate& gate : pTrack.mWaypoints)
        if (!inRange(gate.mX, gate.mY, gate.mRadius))
            return "a waypoint is outside the allowed range";
    for (const RaceGate& gate : pTrack.mCheckpoints)
        if (!inRange(gate.mX, gate.mY, gate.mRadius))
            return "a checkpoint is outside the allowed range";
    for (const BoostPad& pad : pTrack.mBoostPads)
        if (!inRange(pad.mX, pad.mY, pad.mRadius))
            return "a boost pad is outside the allowed range";
    for (const HazardZone& zone : pTrack.mHazardZones)
        if (!inRange(zone.mX, zone.mY, zone.mRadius) || zone.mSpeedLossPerSecond > 50.0)
            return "a hazard zone is outside the allowed range";
    for (const Mine& mine : pTrack.mMines)
        if (!inRange(mine.mX, mine.mY, mine.mRadius))
            return "a mine is outside the allowed range";
    for (const RaisedSection& section : pTrack.mRaisedSections)
    {
        if (!inRange(section.mX, section.mY, 1.0) || section.mHalfLength > 200.0
            || section.mHalfWidth > 100.0 || section.mClearHeight > 50.0)
            return "a raised section is outside the allowed range";
    }
    double routeLength = 0.0;
    for (std::size_t index = 0; index < pTrack.mWaypoints.size(); ++index)
    {
        const RaceGate& next = pTrack.mWaypoints[(index + 1) % pTrack.mWaypoints.size()];
        routeLength += std::hypot(next.mX - pTrack.mWaypoints[index].mX,
                                  next.mY - pTrack.mWaypoints[index].mY);
    }
    if (routeLength > 20000.0)
        return "route is longer than 20 km";
    return std::string();
}
