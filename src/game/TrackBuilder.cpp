// SPDX-License-Identifier: MIT OR Apache-2.0
#include "TrackBuilder.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kPi = 3.14159265358979323846;

struct Crossing
{
    double mX = 0.0;
    double mY = 0.0;
    double mHeading = 0.0;   // direction of the later segment, which becomes the bridge
    double mAngle = 0.0;     // angle between the two roads, 0 to pi/2
};

bool SegmentCrossing(const RaceGate& pA, const RaceGate& pB, const RaceGate& pC, const RaceGate& pD,
                     Crossing& pOut)
{
    const double rX = pB.mX - pA.mX;
    const double rY = pB.mY - pA.mY;
    const double sX = pD.mX - pC.mX;
    const double sY = pD.mY - pC.mY;
    const double denominator = rX * sY - rY * sX;
    if (std::fabs(denominator) < 1e-9)
        return false;
    const double t = ((pC.mX - pA.mX) * sY - (pC.mY - pA.mY) * sX) / denominator;
    const double u = ((pC.mX - pA.mX) * rY - (pC.mY - pA.mY) * rX) / denominator;
    if (t < 0.0 || t > 1.0 || u < 0.0 || u > 1.0)
        return false;
    pOut.mX = pA.mX + t * rX;
    pOut.mY = pA.mY + t * rY;
    pOut.mHeading = std::atan2(sY, sX);
    double angle = std::fabs(std::atan2(rX * sY - rY * sX, rX * sX + rY * sY));
    if (angle > kPi / 2.0)
        angle = kPi - angle;
    pOut.mAngle = angle;
    return true;
}

BuiltTrack Failure(const std::string& pProblem)
{
    BuiltTrack result;
    result.mProblem = pProblem;
    return result;
}
}

std::string MakeTrackId(const std::string& pName)
{
    std::string id;
    for (char c : pName)
    {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            id += c;
        else if (!id.empty() && id.back() != '-')
            id += '-';
    }
    while (!id.empty() && id.back() == '-')
        id.pop_back();
    if (id.size() > 24)
    {
        id.resize(24);
        while (!id.empty() && id.back() == '-')
            id.pop_back();
    }
    return id;
}

BuiltTrack BuildTrackFromPoints(const std::string& pName, const std::string& pAuthor,
                                const std::vector<EditorPoint>& pPoints, double pRoadHalfWidth,
                                const BuilderOptions& pOptions)
{
    if (pPoints.size() < 5)
        return Failure("ADD AT LEAST 5 POINTS");
    if (pName.empty() || MakeTrackId(pName).empty())
        return Failure("GIVE THE TRACK A NAME");

    // Start on the longest side so the grid has a straight to sit on.
    std::size_t longest = 0;
    double longestLength = -1.0;
    for (std::size_t index = 0; index < pPoints.size(); ++index)
    {
        const EditorPoint& next = pPoints[(index + 1) % pPoints.size()];
        const double length = std::hypot(next.mX - pPoints[index].mX, next.mY - pPoints[index].mY);
        if (length > longestLength)
        {
            longestLength = length;
            longest = index;
        }
    }
    if (pOptions.mStartSide >= 0 && pOptions.mStartSide < static_cast<int>(pPoints.size()))
    {
        // The caller chose where the start goes.
        longest = static_cast<std::size_t>(pOptions.mStartSide);
        const EditorPoint& next = pPoints[(longest + 1) % pPoints.size()];
        longestLength = std::hypot(next.mX - pPoints[longest].mX, next.mY - pPoints[longest].mY);
    }
    // The starting grid needs about 20 metres of straight road either side of the start; the
    // validation below checks that it really fits, so this only rules out hopeless cases.
    if (longestLength < 20.0)
        return Failure("THE TRACK IS TOO SMALL - SPREAD THE POINTS OUT");

    const double gateRadius = pRoadHalfWidth + 0.5;
    BuiltTrack result;
    TrackDefinition& track = result.mTrack;
    track.mId = MakeTrackId(pName);
    track.mName = pName;
    track.mProvenance = {pAuthor.empty() ? std::string("OpenHover player") : pAuthor, "CC BY 4.0",
                         "Created in the OpenHover track editor"};
    track.mRoadHalfWidth = pRoadHalfWidth;
    const EditorPoint& sideStart = pPoints[longest];
    const EditorPoint& sideEnd = pPoints[(longest + 1) % pPoints.size()];
    track.mWaypoints.push_back({(sideStart.mX + sideEnd.mX) * 0.5, (sideStart.mY + sideEnd.mY) * 0.5,
                                gateRadius});
    for (std::size_t offset = 1; offset <= pPoints.size(); ++offset)
    {
        const EditorPoint& point = pPoints[(longest + offset) % pPoints.size()];
        track.mWaypoints.push_back({point.mX, point.mY, gateRadius});
    }

    if (pOptions.mHeights.size() == pPoints.size())
    {
        // The start waypoint sits halfway along its side, the rest follow the points. The ground
        // must not step within reach of the starting grid.
        const std::size_t size = pPoints.size();
        if (pOptions.mHeights[(longest + size - 1) % size] != pOptions.mHeights[longest] && longestLength < 60.0)
            return Failure("A STEP IN THE GROUND IS TOO CLOSE BEHIND THE START");
        if (pOptions.mHeights[(longest + 1) % size] != pOptions.mHeights[longest] && longestLength < 20.0)
            return Failure("A STEP IN THE GROUND IS TOO CLOSE AHEAD OF THE START");
        track.mGroundHeights.push_back(pOptions.mHeights[longest]);
        for (std::size_t offset = 1; offset <= size; ++offset)
            track.mGroundHeights.push_back(pOptions.mHeights[(longest + offset) % size]);
    }

    // Four checkpoints at corners, spread evenly around the lap; the last is the final corner.
    const std::size_t count = track.mWaypoints.size();
    std::vector<double> progress(count, 0.0);
    for (std::size_t index = 1; index < count; ++index)
        progress[index] = progress[index - 1]
            + std::hypot(track.mWaypoints[index].mX - track.mWaypoints[index - 1].mX,
                         track.mWaypoints[index].mY - track.mWaypoints[index - 1].mY);
    const double lapLength = progress[count - 1]
        + std::hypot(track.mWaypoints[0].mX - track.mWaypoints[count - 1].mX,
                     track.mWaypoints[0].mY - track.mWaypoints[count - 1].mY);
    std::size_t previousChosen = 0;
    for (int checkpoint = 1; checkpoint <= 4; ++checkpoint)
    {
        std::size_t chosen = count - 1;
        if (checkpoint < 4)
        {
            const double wanted = lapLength * checkpoint / 4.0;
            double bestError = 1e18;
            for (std::size_t index = previousChosen + 1; index < count - 1; ++index)
            {
                const double error = std::fabs(progress[index] - wanted);
                if (error < bestError)
                {
                    bestError = error;
                    chosen = index;
                }
            }
        }
        if (chosen <= previousChosen)
            return Failure("ADD MORE POINTS SO THERE ARE FOUR CORNERS TO CHECK");
        track.mCheckpoints.push_back(track.mWaypoints[chosen]);
        previousChosen = chosen;
    }

    // Bridge every crossing: the later road passes over the earlier one.
    std::vector<Crossing> crossings;
    for (std::size_t first = 0; first < count; ++first)
    {
        for (std::size_t second = first + 2; second < count; ++second)
        {
            if (first == 0 && second == count - 1)
                continue;
            Crossing crossing;
            if (SegmentCrossing(track.mWaypoints[first], track.mWaypoints[(first + 1) % count],
                                track.mWaypoints[second], track.mWaypoints[(second + 1) % count], crossing))
                crossings.push_back(crossing);
        }
    }
    for (const Crossing& crossing : crossings)
    {
        if (crossing.mAngle < 25.0 * kPi / 180.0)
            return Failure("TWO ROADS CROSS AT TOO SHALLOW AN ANGLE");
        RaisedSection bridge;
        bridge.mX = crossing.mX;
        bridge.mY = crossing.mY;
        bridge.mHeading = crossing.mHeading;
        bridge.mHalfLength = std::max(2.0 * pRoadHalfWidth, pRoadHalfWidth / std::sin(crossing.mAngle) + 4.0);
        bridge.mHalfWidth = pRoadHalfWidth * 0.7;
        bridge.mClearHeight = 1.45;
        bridge.mDriveable = true;
        track.mRaisedSections.push_back(bridge);
    }

    // Boost pads in the middle of long straights, away from the start and from any bridge.
    for (std::size_t index = 1; pOptions.mPads && index + 1 < count && track.mBoostPads.size() < 8; ++index)
    {
        const RaceGate& a = track.mWaypoints[index];
        const RaceGate& b = track.mWaypoints[(index + 1) % count];
        if (std::hypot(b.mX - a.mX, b.mY - a.mY) < 80.0)
            continue;
        BoostPad pad;
        pad.mX = (a.mX + b.mX) * 0.5;
        pad.mY = (a.mY + b.mY) * 0.5;
        pad.mRadius = 1.8;
        bool nearBridge = false;
        for (const RaisedSection& bridge : track.mRaisedSections)
            nearBridge = nearBridge || std::hypot(bridge.mX - pad.mX, bridge.mY - pad.mY) < bridge.mHalfLength + 10.0;
        if (!nearBridge)
            track.mBoostPads.push_back(pad);
    }

    // Optional hazards: alternate mines and slowing zones along straights that have no pad and are
    // clear of every bridge, a third of the way along so they are met after the corner exits.
    if (pOptions.mMines || pOptions.mHazards)
    {
        int placed = 0;
        for (std::size_t index = 1; index + 1 < count && placed < 8; ++index)
        {
            const RaceGate& a = track.mWaypoints[index];
            const RaceGate& b = track.mWaypoints[(index + 1) % count];
            const double length = std::hypot(b.mX - a.mX, b.mY - a.mY);
            if (length < 110.0)
                continue;
            const double x = a.mX + (b.mX - a.mX) / 3.0;
            const double y = a.mY + (b.mY - a.mY) / 3.0;
            bool blocked = false;
            for (const RaisedSection& bridge : track.mRaisedSections)
                blocked = blocked || std::hypot(bridge.mX - x, bridge.mY - y) < bridge.mHalfLength + 15.0;
            for (const BoostPad& pad : track.mBoostPads)
                blocked = blocked || std::hypot(pad.mX - x, pad.mY - y) < 20.0;
            if (blocked)
                continue;
            const bool wantMine = pOptions.mMines && (!pOptions.mHazards || placed % 2 == 0);
            if (wantMine)
            {
                Mine mine;
                mine.mX = x;
                mine.mY = y;
                mine.mRadius = 2.1;
                track.mMines.push_back(mine);
            }
            else
            {
                HazardZone zone;
                zone.mX = x;
                zone.mY = y;
                zone.mRadius = pRoadHalfWidth * 0.5;
                zone.mSpeedLossPerSecond = 0.75;
                track.mHazardZones.push_back(zone);
            }
            ++placed;
        }
    }

    const std::string problem = track.Validate();
    if (!problem.empty())
    {
        std::string shouted = problem;
        for (char& c : shouted)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return Failure(shouted);
    }
    result.mOk = true;
    return result;
}
