// SPDX-License-Identifier: MIT OR Apache-2.0
#include "TrackDefinition.h"

#include <set>

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
    for (const RaisedSection& section : mRaisedSections)
    {
        if (section.mHalfLength <= 0.0 || section.mHalfWidth <= 0.0
            || section.mClearHeight <= 0.0)
        {
            return "raised section dimensions and clearance must be positive";
        }
    }
    return std::string();
}

RaceGate TrackDefinition::Finish() const
{
    return mWaypoints.empty() ? RaceGate() : mWaypoints.front();
}

std::vector<RaceGate> TrackDefinition::Checkpoints() const
{
    return mWaypoints.size() < 2
        ? std::vector<RaceGate>()
        : std::vector<RaceGate>(mWaypoints.begin() + 1, mWaypoints.end());
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
        harborLoop.mRoadHalfWidth = 8.5;
        harborLoop.mWaypoints = {{0.0, 0.0, 5.0}, {42.0, 0.0, 5.0},
                     {60.0, 18.0, 5.0}, {55.0, 48.0, 5.0},
                     {24.0, 64.0, 5.0}, {-10.0, 48.0, 5.0},
                     {-16.0, 20.0, 5.0}};
        harborLoop.mBoostPads = {{21.0, 0.0, 1.8}, {59.0, 33.0, 1.8},
                     {7.0, 56.0, 1.8}, {-13.0, 34.0, 1.8}};
        harborLoop.mHazardZones = {{34.0, 4.0, 2.6, 0.8}};
        harborLoop.mRaisedSections = {{31.0, 0.0, 1.3, 7.0, 0.0, 1.6}};
        harborLoop.mAtmosphereRed = 0.14f;
        harborLoop.mAtmosphereGreen = 0.3f;
        harborLoop.mAtmosphereBlue = 0.36f;
        harborLoop.mRoadRed = 0.48f;
        harborLoop.mRoadGreen = 0.67f;
        harborLoop.mRoadBlue = 0.7f;
        harborLoop.mWallRed = 0.52f;
        harborLoop.mWallGreen = 0.88f;
        harborLoop.mWallBlue = 0.86f;
        result.push_back(harborLoop);

        TrackDefinition switchback;
        switchback.mId = "glass-switchback";
        switchback.mName = "Glass Switchback";
        switchback.mProvenance = {"OpenHover contributors", "CC BY 4.0",
                      "Original coordinates and procedural runtime materials"};
        switchback.mRoadHalfWidth = 8.0;
        switchback.mWaypoints = {{0.0, 0.0, 5.0}, {56.0, 0.0, 5.0},
                     {68.0, 16.0, 5.0}, {68.0, 32.0, 5.0},
                     {22.0, 32.0, 5.0}, {10.0, 46.0, 5.0},
                     {10.0, 66.0, 5.0}, {64.0, 66.0, 5.0},
                     {78.0, 82.0, 5.0}, {78.0, 104.0, 5.0},
                                 {-28.0, 104.0, 5.0}};
        switchback.mBoostPads = {{28.0, 0.0, 1.8}, {68.0, 24.0, 1.8},
                     {16.0, 56.0, 1.8}, {48.0, 66.0, 1.8},
                     {39.0, 104.0, 1.8}};
        switchback.mHazardZones = {{55.0, 32.0, 2.7, 1.1}, {36.0, 66.0, 2.7, 1.1}};
        switchback.mAtmosphereRed = 0.36f;
        switchback.mAtmosphereGreen = 0.2f;
        switchback.mAtmosphereBlue = 0.28f;
        switchback.mRoadRed = 0.64f;
        switchback.mRoadGreen = 0.48f;
        switchback.mRoadBlue = 0.5f;
        switchback.mWallRed = 0.94f;
        switchback.mWallGreen = 0.62f;
        switchback.mWallBlue = 0.38f;
        result.push_back(switchback);

        TrackDefinition velocityRing;
        velocityRing.mId = "velocity-ring";
        velocityRing.mName = "Velocity Ring";
        velocityRing.mProvenance = {"OpenHover contributors", "CC BY 4.0",
                       "Original coordinates and procedural runtime materials"};
        velocityRing.mRoadHalfWidth = 9.0;
        velocityRing.mWaypoints = {{0.0, 0.0, 5.0}, {58.0, -4.0, 5.0},
                       {82.0, 22.0, 5.0}, {78.0, 58.0, 5.0},
                       {46.0, 82.0, 5.0}, {6.0, 78.0, 5.0},
                       {-18.0, 48.0, 5.0}, {-16.0, 16.0, 5.0}};
        velocityRing.mBoostPads = {{29.0, -2.0, 1.9}, {80.0, 40.0, 1.9},
                       {26.0, 80.0, 1.9}, {-17.0, 32.0, 1.9}};
        velocityRing.mHazardZones = {{68.0, 4.0, 2.8, 0.75}};
        velocityRing.mAtmosphereRed = 0.14f;
        velocityRing.mAtmosphereGreen = 0.34f;
        velocityRing.mAtmosphereBlue = 0.24f;
        velocityRing.mRoadRed = 0.46f;
        velocityRing.mRoadGreen = 0.66f;
        velocityRing.mRoadBlue = 0.44f;
        velocityRing.mWallRed = 0.86f;
        velocityRing.mWallGreen = 0.9f;
        velocityRing.mWallBlue = 0.46f;
        result.push_back(velocityRing);
        return result;
    }();
    return tracks;
}