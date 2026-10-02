// SPDX-License-Identifier: MIT OR Apache-2.0
#include "TrackDefinition.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace
{
RaceGate GateBeforeTurn(const RaceGate& pGate, const std::vector<RaceGate>& pWaypoints,
                        double pRoadHalfWidth)
{
    if (pWaypoints.size() < 2)
        return pGate;

    int closestWaypoint = 0;
    double closestDistanceSquared = -1.0;
    for (int index = 0; index < static_cast<int>(pWaypoints.size()); ++index)
    {
        const double deltaX = pGate.mX - pWaypoints[index].mX;
        const double deltaY = pGate.mY - pWaypoints[index].mY;
        const double distanceSquared = deltaX * deltaX + deltaY * deltaY;
        if (closestDistanceSquared < 0.0 || distanceSquared < closestDistanceSquared)
        {
            closestDistanceSquared = distanceSquared;
            closestWaypoint = index;
        }
    }

    const RaceGate& turn = pWaypoints[closestWaypoint];
    const RaceGate& previous = pWaypoints[(closestWaypoint + pWaypoints.size() - 1)
        % pWaypoints.size()];
    const double directionX = turn.mX - previous.mX;
    const double directionY = turn.mY - previous.mY;
    const double length = std::sqrt(directionX * directionX + directionY * directionY);
    if (length == 0.0)
        return pGate;

    const double clearance = std::min(length * 0.45, pRoadHalfWidth + 2.0);
    RaceGate result = pGate;
    result.mX = turn.mX - directionX / length * clearance;
    result.mY = turn.mY - directionY / length * clearance;
    result.mRadius = std::max(result.mRadius, pRoadHalfWidth + 0.5);
    return result;
}

struct RoutePoint
{
    double mDistanceFromRoute = 0.0;
    double mProgress = 0.0; // distance along the closed route from the finish waypoint
};

RoutePoint NearestRoutePoint(const std::vector<RaceGate>& pWaypoints, double pX, double pY)
{
    RoutePoint best;
    best.mDistanceFromRoute = -1.0;
    double travelled = 0.0;
    for (std::size_t index = 0; index < pWaypoints.size(); ++index)
    {
        const RaceGate& start = pWaypoints[index];
        const RaceGate& end = pWaypoints[(index + 1) % pWaypoints.size()];
        const double segmentX = end.mX - start.mX;
        const double segmentY = end.mY - start.mY;
        const double length = std::sqrt(segmentX * segmentX + segmentY * segmentY);
        double fraction = 0.0;
        if (length > 0.0)
        {
            fraction = ((pX - start.mX) * segmentX + (pY - start.mY) * segmentY) / (length * length);
            fraction = std::max(0.0, std::min(1.0, fraction));
        }
        const double nearestX = start.mX + segmentX * fraction;
        const double nearestY = start.mY + segmentY * fraction;
        const double distance = std::sqrt((pX - nearestX) * (pX - nearestX)
                                          + (pY - nearestY) * (pY - nearestY));
        if (best.mDistanceFromRoute < 0.0 || distance < best.mDistanceFromRoute)
        {
            best.mDistanceFromRoute = distance;
            best.mProgress = travelled + length * fraction;
        }
        travelled += length;
    }
    return best;
}
}

// Intersection of segments a-b and c-d, if they cross.
bool SegmentCrossing(const RaceGate& pA, const RaceGate& pB, const RaceGate& pC,
                     const RaceGate& pD, double& pX, double& pY)
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
    pX = pA.mX + t * rX;
    pY = pA.mY + t * rY;
    return true;
}

std::string TrackDefinition::Validate() const
{
    if (mFormatVersion != 1)
        return "unsupported track format version";
    if (mId.empty() || mName.empty())
        return "track id and name are required";
    if (mProvenance.mAuthor.empty() || mProvenance.mLicense.empty()
        || mProvenance.mAssetOrigin.empty())
    {
        return "track provenance author, license, and asset origin are required";
    }
    if (mWaypoints.size() < 4)
        return "at least four route waypoints are required";
    if (mCheckpoints.size() != 4)
        return "exactly four race checkpoints are required";
    if (mRoadHalfWidth <= 0.0)
        return "road half-width must be positive";
    if (mRoadRed < 0.0f || mRoadRed > 1.0f || mRoadGreen < 0.0f || mRoadGreen > 1.0f
        || mRoadBlue < 0.0f || mRoadBlue > 1.0f || mWallRed < 0.0f || mWallRed > 1.0f
        || mWallGreen < 0.0f || mWallGreen > 1.0f || mWallBlue < 0.0f || mWallBlue > 1.0f)
    {
        return "track material colors must be normalized";
    }

    for (const RaceGate& waypoint : mWaypoints)
    {
        if (waypoint.mRadius <= 0.0)
            return "route waypoint radius must be positive";
    }
    for (const RaceGate& checkpoint : mCheckpoints)
    {
        if (checkpoint.mRadius <= 0.0)
            return "checkpoint radius must be positive";
    }
    for (const BoostPad& boostPad : mBoostPads)
    {
        if (boostPad.mRadius <= 0.0)
            return "boost pad radius must be positive";
    }
    for (const HazardZone& hazardZone : mHazardZones)
    {
        if (hazardZone.mRadius <= 0.0 || hazardZone.mSpeedLossPerSecond <= 0.0)
            return "hazard zone radius and speed loss must be positive";
    }
    for (const Mine& mine : mMines)
    {
        if (mine.mRadius <= 0.0)
            return "mine radius must be positive";
    }
    for (const RaisedSection& section : mRaisedSections)
    {
        if (section.mHalfLength <= 0.0 || section.mHalfWidth <= 0.0
            || section.mClearHeight <= 0.0)
        {
            return "raised section dimensions and clearance must be positive";
        }
    }

    for (std::size_t index = 0; index < mWaypoints.size(); ++index)
    {
        const RaceGate& next = mWaypoints[(index + 1) % mWaypoints.size()];
        if (std::fabs(mWaypoints[index].mX - next.mX) + std::fabs(mWaypoints[index].mY - next.mY)
            < 0.5)
            return "consecutive route waypoints must be distinct";
    }
    double routeLength = 0.0;
    for (std::size_t index = 0; index < mWaypoints.size(); ++index)
    {
        const RaceGate& next = mWaypoints[(index + 1) % mWaypoints.size()];
        routeLength += std::hypot(next.mX - mWaypoints[index].mX, next.mY - mWaypoints[index].mY);
    }
    double previousProgress = 0.0;
    for (const RaceGate& checkpoint : mCheckpoints)
    {
        const RoutePoint point = NearestRoutePoint(mWaypoints, checkpoint.mX, checkpoint.mY);
        if (point.mDistanceFromRoute > mRoadHalfWidth)
            return "every checkpoint must lie on the route";
        // A checkpoint on the finish point itself is the end of the lap, not its start.
        const double progress = point.mProgress < 0.001 ? routeLength : point.mProgress;
        if (progress + 0.001 < previousProgress)
            return "checkpoints must follow the route in driving order";
        previousProgress = progress;
    }
    // Where the route crosses itself, one road must pass over the other on a driveable raised
    // section; otherwise the crossing is an ambiguous junction that lap validation cannot trust.
    const std::size_t waypointCount = mWaypoints.size();
    for (std::size_t first = 0; first < waypointCount; ++first)
    {
        for (std::size_t second = first + 2; second < waypointCount; ++second)
        {
            if (first == 0 && second == waypointCount - 1)
                continue; // adjacent through the closing segment
            double crossX = 0.0;
            double crossY = 0.0;
            if (!SegmentCrossing(mWaypoints[first], mWaypoints[(first + 1) % waypointCount],
                                 mWaypoints[second], mWaypoints[(second + 1) % waypointCount],
                                 crossX, crossY))
                continue;
            bool sealed = false;
            for (const RaisedSection& section : mRaisedSections)
            {
                if (section.mDriveable
                    && std::hypot(section.mX - crossX, section.mY - crossY) <= section.mHalfLength)
                    sealed = true;
            }
            if (!sealed)
                return "every route crossing must be sealed by a driveable raised section";
        }
    }
    // The starting grid (three abreast, three rows, as laid out by the race start) must be on the
    // road and clear of mines and hazard zones.
    {
        const RaceGate finish = Finish();
        double forwardX = mWaypoints.front().mX - finish.mX;
        double forwardY = mWaypoints.front().mY - finish.mY;
        const double forwardLength = std::hypot(forwardX, forwardY);
        if (forwardLength > 0.0)
        {
            forwardX /= forwardLength;
            forwardY /= forwardLength;
        }
        for (int slot = 0; slot < 9; ++slot)
        {
            const double ahead = 7.0 - slot / 3 * 4.0;
            const double aside = (slot % 3 - 1) * 3.2;
            const double x = finish.mX + forwardX * ahead - forwardY * aside;
            const double y = finish.mY + forwardY * ahead + forwardX * aside;
            if (NearestRoutePoint(mWaypoints, x, y).mDistanceFromRoute > mRoadHalfWidth)
                return "the starting grid must lie on the road";
            for (const Mine& mine : mMines)
            {
                if (std::hypot(mine.mX - x, mine.mY - y) < mine.mRadius + 2.0)
                    return "the starting grid must be clear of mines";
            }
            for (const HazardZone& hazardZone : mHazardZones)
            {
                if (std::hypot(hazardZone.mX - x, hazardZone.mY - y) < hazardZone.mRadius + 2.0)
                    return "the starting grid must be clear of hazard zones";
            }
        }
    }
    for (const BoostPad& boostPad : mBoostPads)
    {
        if (NearestRoutePoint(mWaypoints, boostPad.mX, boostPad.mY).mDistanceFromRoute
            > mRoadHalfWidth)
            return "every boost pad must lie on the road";
    }
    for (const HazardZone& hazardZone : mHazardZones)
    {
        if (NearestRoutePoint(mWaypoints, hazardZone.mX, hazardZone.mY).mDistanceFromRoute
            > mRoadHalfWidth)
            return "every hazard zone must lie on the road";
    }
    for (const Mine& mine : mMines)
    {
        if (NearestRoutePoint(mWaypoints, mine.mX, mine.mY).mDistanceFromRoute > mRoadHalfWidth)
            return "every mine must lie on the road";
    }
    return std::string();
}

RaceGate TrackDefinition::Finish() const
{
    return mWaypoints.empty() ? RaceGate()
        : GateBeforeTurn(mWaypoints.front(), mWaypoints, mRoadHalfWidth);
}

std::vector<RaceGate> TrackDefinition::Checkpoints() const
{
    std::vector<RaceGate> checkpoints;
    checkpoints.reserve(mCheckpoints.size());
    for (const RaceGate& checkpoint : mCheckpoints)
        checkpoints.push_back(GateBeforeTurn(checkpoint, mWaypoints, mRoadHalfWidth));
    return checkpoints;
}

const std::vector<TrackDefinition>& BuiltInTracks()
{
    static const std::vector<TrackDefinition> tracks = []()
    {
        std::vector<TrackDefinition> result;
        TrackDefinition harborLoop;
        harborLoop.mId = "harbor-loop";
        harborLoop.mName = "Harbor Loop";
        harborLoop.mProvenance = {"OpenHover contributors", "CC BY 4.0",
                      "Original coordinates and procedural runtime materials"};
        harborLoop.mRoadHalfWidth = 7.8;
           harborLoop.mWaypoints = {{-78.2, -211.6, 5.0},
               {-119.6, -211.6, 5.0}, {-119.6, -147.2, 5.0},
               {-69.0, -92.0, 5.0}, {-119.6, -36.8, 5.0}, {-170.2, 23.0, 5.0},
               {-119.6, 78.2, 5.0}, {-69.0, 133.4, 5.0}, {-119.6, 184.0, 5.0},
               {-119.6, 220.8, 5.0},
               {-36.8, 220.8, 5.0}, {-36.8, 124.2, 5.0}, {0.0, 59.8, 5.0},
               {36.8, 124.2, 5.0}, {36.8, 220.8, 5.0}, {119.6, 220.8, 5.0},
               {119.6, 147.2, 5.0}, {69.0, 92.0, 5.0}, {119.6, 36.8, 5.0},
               {170.2, -23.0, 5.0}, {119.6, -78.2, 5.0}, {69.0, -133.4, 5.0},
               {119.6, -184.0, 5.0}, {119.6, -211.6, 5.0}, {36.8, -211.6, 5.0},
               {36.8, -115.0, 5.0}, {0.0, -50.6, 5.0}, {-36.8, -115.0, 5.0},
               {-36.8, -211.6, 5.0}};
            harborLoop.mCheckpoints = {{-119.6, 220.8, 5.0}, {36.8, 220.8, 5.0},
                           {119.6, -211.6, 5.0}, {-36.8, -211.6, 5.0}};
           harborLoop.mBoostPads = {{-94.3, -119.6, 1.8}, {-94.3, 105.8, 1.8},
               {0.0, 59.8, 1.8}, {94.3, 119.6, 1.8}, {94.3, -105.8, 1.8},
               {0.0, -50.6, 1.8}};
        harborLoop.mAtmosphereRed = 0.3f;
        harborLoop.mAtmosphereGreen = 0.52f;
        harborLoop.mAtmosphereBlue = 0.58f;
        harborLoop.mRoadRed = 0.84f;
        harborLoop.mRoadGreen = 0.86f;
        harborLoop.mRoadBlue = 0.8f;
        harborLoop.mWallRed = 0.31f;
        harborLoop.mWallGreen = 0.36f;
        harborLoop.mWallBlue = 0.39f;
        result.push_back(harborLoop);

        TrackDefinition switchback;
        switchback.mId = "glass-switchback";
        switchback.mName = "Glass Switchback";
        switchback.mProvenance = {"OpenHover contributors", "CC BY 4.0",
                      "Original coordinates and procedural runtime materials"};
        switchback.mRoadHalfWidth = 8.0;
        switchback.mWaypoints = {{-32.0, 57.6, 5.0},
               {0.0, 0.0, 5.0}, {172.8, -38.4, 5.0},
             {307.2, -12.8, 5.0}, {358.4, 57.6, 5.0},
             {294.4, 134.4, 5.0}, {153.6, 102.4, 5.0},
             {64.0, 153.6, 5.0}, {102.4, 230.4, 5.0},
             {243.2, 211.2, 5.0}, {332.8, 262.4, 5.0},
             {384.0, 358.4, 5.0}, {288.0, 435.2, 5.0},
             {140.8, 409.6, 5.0}, {25.6, 326.4, 5.0},
             {-38.4, 224.0, 5.0}, {-64.0, 115.2, 5.0}};
            switchback.mCheckpoints = {{358.4, 57.6, 5.0}, {102.4, 230.4, 5.0},
                           {288.0, 435.2, 5.0}, {-64.0, 115.2, 5.0}};
        switchback.mBoostPads = {{89.6, -19.2, 1.8}, {332.8, 22.4, 1.8},
             {83.2, 192.0, 1.8}, {294.4, 240.0, 1.8}, {211.2, 422.4, 1.8},
             {240.0, -25.6, 1.8}, {-6.4, 275.2, 1.8}, {-32.0, 57.6, 1.8}};
        switchback.mMines = {{153.6, 102.4, 2.1}, {83.2, 368.0, 2.1}, {-51.2, 169.6, 2.1}};
           switchback.mRaisedSections = {{121.6, -25.6, 1.25, 7.2, -0.22, 1.45},
               {332.8, 22.4, 1.25, 7.2, 0.94, 1.45}, {326.4, 96.0, 1.25, 7.2, 2.27, 1.45},
               {224.0, 118.4, 1.25, 7.2, -2.92, 1.45}, {83.2, 192.0, 1.25, 7.2, 1.11, 1.45},
               {172.8, 220.8, 16.0, 5.4, -0.14, 1.45, true}, {358.4, 310.4, 1.25, 7.2, 1.08, 1.45},
               {214.4, 422.4, 15.0, 5.4, -2.97, 1.45, true}};
        switchback.mAtmosphereRed = 0.48f;
        switchback.mAtmosphereGreen = 0.34f;
        switchback.mAtmosphereBlue = 0.28f;
        switchback.mRoadRed = 0.9f;
        switchback.mRoadGreen = 0.84f;
        switchback.mRoadBlue = 0.72f;
        switchback.mWallRed = 0.56f;
        switchback.mWallGreen = 0.28f;
        switchback.mWallBlue = 0.2f;
        result.push_back(switchback);

        TrackDefinition velocityRing;
        velocityRing.mId = "velocity-ring";
        velocityRing.mName = "Velocity Ring";
        velocityRing.mProvenance = {"OpenHover contributors", "CC BY 4.0",
                       "Original coordinates and procedural runtime materials"};
        velocityRing.mRoadHalfWidth = 9.0;
        velocityRing.mWaypoints = {{-59.5, 10.5, 5.0},
               {0.0, 0.0, 5.0}, {196.0, -63.0, 5.0},
               {364.0, -7.0, 5.0}, {441.0, 91.0, 5.0},
               {336.0, 182.0, 5.0}, {448.0, 294.0, 5.0},
               {357.0, 406.0, 5.0}, {203.0, 350.0, 5.0},
               {112.0, 476.0, 5.0}, {-28.0, 420.0, 5.0},
               {-147.0, 315.0, 5.0}, {-84.0, 210.0, 5.0},
               {-203.0, 112.0, 5.0}, {-119.0, 21.0, 5.0}};
            velocityRing.mCheckpoints = {{441.0, 91.0, 5.0}, {357.0, 406.0, 5.0},
                             {-28.0, 420.0, 5.0}, {-203.0, 112.0, 5.0}};
        velocityRing.mBoostPads = {{98.0, -31.5, 1.9}, {392.0, 28.0, 1.9},
               {399.0, 350.0, 1.9}, {147.0, 427.0, 1.9}, {-119.0, 262.5, 1.9},
               {388.5, 136.5, 1.9}, {-87.5, 367.5, 1.9}, {-161.0, 66.5, 1.9}};
         velocityRing.mHazardZones = {{273.0, -38.5, 4.0, 0.75}, {392.0, 231.0, 4.0, 0.75},
             {280.0, 378.0, 4.0, 0.75}, {-112.0, 262.5, 4.0, 0.75},
             {-143.5, 161.0, 4.0, 0.75}};
         velocityRing.mRaisedSections = {{133.0, -42.0, 1.35, 7.8, -0.31, 1.38},
             {395.5, 35.0, 1.35, 7.8, 0.9, 1.38}, {392.0, 238.0, 1.35, 7.8, 0.79, 1.38},
             {280.0, 378.0, 17.0, 6.0, -2.79, 1.38, true}, {49.0, 448.0, 1.35, 7.8, -2.76, 1.38}};
        velocityRing.mAtmosphereRed = 0.28f;
        velocityRing.mAtmosphereGreen = 0.44f;
        velocityRing.mAtmosphereBlue = 0.4f;
        velocityRing.mRoadRed = 0.74f;
        velocityRing.mRoadGreen = 0.84f;
        velocityRing.mRoadBlue = 0.78f;
        velocityRing.mWallRed = 0.55f;
        velocityRing.mWallGreen = 0.48f;
        velocityRing.mWallBlue = 0.18f;
        result.push_back(velocityRing);
        for (TrackDefinition& track : result)
        {
            const double minimumGateRadius = track.mRoadHalfWidth + 0.5;
            for (RaceGate& waypoint : track.mWaypoints)
            {
                if (waypoint.mRadius < minimumGateRadius)
                    waypoint.mRadius = minimumGateRadius;
            }
            for (RaceGate& checkpoint : track.mCheckpoints)
            {
                if (checkpoint.mRadius < minimumGateRadius)
                    checkpoint.mRadius = minimumGateRadius;
            }
        }
        return result;
    }();
    return tracks;
}