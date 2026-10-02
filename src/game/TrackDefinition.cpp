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
    return std::string();
}

RaceGate TrackDefinition::Finish() const
{
    return mWaypoints.empty() ? RaceGate() : mWaypoints.front();
}

std::vector<RaceGate> TrackDefinition::Checkpoints() const
{
    return mCheckpoints;
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
           harborLoop.mWaypoints = {{-52.0, -92.0, 5.0}, {-52.0, -48.0, 5.0},
               {-52.0, 0.0, 5.0}, {-52.0, 48.0, 5.0}, {-52.0, 96.0, 5.0},
               {-16.0, 96.0, 5.0}, {-16.0, 54.0, 5.0}, {0.0, 26.0, 5.0},
               {16.0, 54.0, 5.0}, {16.0, 96.0, 5.0}, {52.0, 96.0, 5.0},
               {52.0, 48.0, 5.0}, {52.0, 0.0, 5.0}, {52.0, -48.0, 5.0},
               {52.0, -92.0, 5.0}, {16.0, -92.0, 5.0}, {16.0, -50.0, 5.0},
               {0.0, -22.0, 5.0}, {-16.0, -50.0, 5.0}, {-16.0, -92.0, 5.0}};
            harborLoop.mCheckpoints = {{-52.0, 96.0, 5.0}, {16.0, 96.0, 5.0},
                           {52.0, -92.0, 5.0}, {-16.0, -92.0, 5.0}};
           harborLoop.mBoostPads = {{-52.0, -24.0, 1.8}, {-52.0, 70.0, 1.8},
               {0.0, 26.0, 1.8}, {52.0, 70.0, 1.8}, {52.0, -24.0, 1.8},
               {0.0, -22.0, 1.8}};
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
        switchback.mWaypoints = {{0.0, 0.0, 5.0}, {54.0, -12.0, 5.0},
             {96.0, -4.0, 5.0}, {112.0, 18.0, 5.0},
             {92.0, 42.0, 5.0}, {48.0, 32.0, 5.0},
             {20.0, 48.0, 5.0}, {32.0, 72.0, 5.0},
             {76.0, 66.0, 5.0}, {104.0, 82.0, 5.0},
             {120.0, 112.0, 5.0}, {90.0, 136.0, 5.0},
             {44.0, 128.0, 5.0}, {8.0, 102.0, 5.0},
             {-12.0, 70.0, 5.0}, {-20.0, 36.0, 5.0}};
            switchback.mCheckpoints = {{112.0, 18.0, 5.0}, {32.0, 72.0, 5.0},
                           {90.0, 136.0, 5.0}, {-20.0, 36.0, 5.0}};
        switchback.mBoostPads = {{28.0, -6.0, 1.8}, {104.0, 7.0, 1.8},
             {26.0, 60.0, 1.8}, {92.0, 75.0, 1.8}, {66.0, 132.0, 1.8}};
        switchback.mMines = {{48.0, 32.0, 2.1}};
           switchback.mRaisedSections = {{38.0, -8.0, 1.25, 7.2, -0.22, 1.45},
               {104.0, 7.0, 1.25, 7.2, 0.94, 1.45}, {102.0, 30.0, 1.25, 7.2, 2.27, 1.45},
               {70.0, 37.0, 1.25, 7.2, -2.92, 1.45}, {26.0, 60.0, 1.25, 7.2, 1.11, 1.45},
               {54.0, 69.0, 16.0, 5.4, -0.14, 1.45, true}, {112.0, 97.0, 1.25, 7.2, 1.08, 1.45},
               {67.0, 132.0, 15.0, 5.4, -2.97, 1.45, true}};
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
        velocityRing.mWaypoints = {{0.0, 0.0, 5.0}, {56.0, -18.0, 5.0},
               {104.0, -2.0, 5.0}, {126.0, 26.0, 5.0},
               {96.0, 52.0, 5.0}, {128.0, 84.0, 5.0},
               {102.0, 116.0, 5.0}, {58.0, 100.0, 5.0},
               {32.0, 136.0, 5.0}, {-8.0, 120.0, 5.0},
               {-42.0, 90.0, 5.0}, {-24.0, 60.0, 5.0},
               {-58.0, 32.0, 5.0}, {-34.0, 6.0, 5.0}};
            velocityRing.mCheckpoints = {{126.0, 26.0, 5.0}, {102.0, 116.0, 5.0},
                             {-8.0, 120.0, 5.0}, {-58.0, 32.0, 5.0}};
        velocityRing.mBoostPads = {{28.0, -9.0, 1.9}, {112.0, 8.0, 1.9},
               {114.0, 100.0, 1.9}, {42.0, 122.0, 1.9}, {-34.0, 75.0, 1.9}};
         velocityRing.mHazardZones = {{78.0, -11.0, 4.0, 0.75}, {112.0, 66.0, 4.0, 0.75},
             {80.0, 108.0, 4.0, 0.75}, {-32.0, 75.0, 4.0, 0.75}};
         velocityRing.mRaisedSections = {{38.0, -12.0, 1.35, 7.8, -0.31, 1.38},
             {113.0, 10.0, 1.35, 7.8, 0.9, 1.38}, {112.0, 68.0, 1.35, 7.8, 0.79, 1.38},
             {80.0, 108.0, 17.0, 6.0, -2.79, 1.38, true}, {14.0, 128.0, 1.35, 7.8, -2.76, 1.38}};
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