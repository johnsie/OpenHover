// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_TRACK_DEFINITION_H
#define OPENHOVER_TRACK_DEFINITION_H

#include "BoostPad.h"
#include "HazardZone.h"
#include "RaisedSection.h"
#include "Race.h"

#include <string>
#include <vector>

struct TrackProvenance
{
    std::string mAuthor;
    std::string mLicense;
    std::string mAssetOrigin;
};

struct TrackDefinition
{
    int mFormatVersion = 1;
    std::string mId;
    std::string mName;
    TrackProvenance mProvenance;
    std::vector<RaceGate> mWaypoints;
    std::vector<BoostPad> mBoostPads;
    std::vector<HazardZone> mHazardZones;
    std::vector<RaisedSection> mRaisedSections;
    double mRoadHalfWidth = 5.0;
    float mAtmosphereRed = 0.14f;
    float mAtmosphereGreen = 0.28f;
    float mAtmosphereBlue = 0.32f;
    float mRoadRed = 0.68f;
    float mRoadGreen = 0.76f;
    float mRoadBlue = 0.78f;
    float mWallRed = 0.84f;
    float mWallGreen = 0.9f;
    float mWallBlue = 0.91f;

    std::string Validate() const;
    RaceGate Finish() const;
    std::vector<RaceGate> Checkpoints() const;
};

const std::vector<TrackDefinition>& BuiltInTracks();

#endif