// SPDX-License-Identifier: MIT OR Apache-2.0
#include "AuthoritativeRace.h"
#include "TrackBuilder.h"
#include "TrackFile.h"
#include "TrackHash.h"

#include <cmath>
#include <iostream>

namespace
{
const double kPi = 3.14159265358979323846;

std::vector<EditorPoint> Ring(int pCount, double pRadiusX, double pRadiusY)
{
    std::vector<EditorPoint> points;
    for (int index = 0; index < pCount; ++index)
    {
        const double angle = 2.0 * kPi * index / pCount;
        points.push_back({std::cos(angle) * pRadiusX, std::sin(angle) * pRadiusY});
    }
    return points;
}

// Seconds for a full-pace rival to finish one lap, or 0 if it never does.
double RivalLapSeconds(const TrackDefinition& pTrack)
{
    std::vector<TrackDefinition> tracks = BuiltInTracks();
    tracks.push_back(pTrack);
    AuthoritativeRace race;
    if (!race.Start({11}, static_cast<int>(tracks.size()) - 1, 1, false, 1, RaceMode::SingleRace, &tracks))
        return 0.0;
    for (int step = 0; step < 120 * 300; ++step)
    {
        race.Step();
        if (step % 60 == 0)
        {
            for (const RaceRacerSnapshot& racer : race.Snapshot().mRacers)
            {
                if (racer.mPlayerId >= 1000000 && racer.mProgress.mFinished)
                    return racer.mProgress.mElapsedSeconds;
            }
        }
    }
    return 0.0;
}
}

int main()
{
    bool ok = true;
    const auto expect = [&](bool pCondition, const char* pMessage)
    {
        if (!pCondition)
        {
            std::cerr << pMessage << '\n';
            ok = false;
        }
    };
    expect(MakeTrackId("My Great Track!") == "my-great-track", "ids are lower-case slugs");
    expect(MakeTrackId("  --A  B-- ") == "a-b", "ids trim and collapse hyphens");
    expect(MakeTrackId("!!!").empty(), "a name with no letters has no id");
    expect(MakeTrackId(std::string(40, 'a')).size() == 24, "ids are at most 24 characters");

    expect(!BuildTrackFromPoints("T", "me", Ring(4, 200, 200), 8.0).mOk, "too few points");
    expect(BuildTrackFromPoints("T", "me", Ring(4, 200, 200), 8.0).mProblem.find("5 POINTS") != std::string::npos,
           "the reason says how many points are needed");
    expect(!BuildTrackFromPoints("", "me", Ring(12, 200, 200), 8.0).mOk, "a name is required");
    expect(!BuildTrackFromPoints("Tiny", "me", Ring(8, 5, 5), 8.0).mOk, "a tiny track is refused");

    // A plain oval and a figure eight both build into valid tracks a rival can finish.
    const BuiltTrack oval = BuildTrackFromPoints("Plain Oval", "tester", Ring(14, 260, 170), 8.0);
    expect(oval.mOk && oval.mTrack.Validate().empty(), "an oval builds into a valid track");
    expect(oval.mTrack.mCheckpoints.size() == 4 && !oval.mTrack.mBoostPads.empty(),
           "the builder adds four checkpoints and some boost pads");
    expect(oval.mTrack.mRaisedSections.empty(), "an oval needs no bridges");
    const double ovalLap = RivalLapSeconds(oval.mTrack);
    expect(ovalLap > 15.0 && ovalLap < 200.0, "a rival can drive a lap of the built oval");

    std::vector<EditorPoint> eight;
    for (int index = 0; index < 16; ++index)
    {
        const double angle = 2.0 * kPi * index / 16.0;
        eight.push_back({std::sin(angle) * 260.0, std::sin(2.0 * angle) * 140.0});
    }
    const BuiltTrack figureEight = BuildTrackFromPoints("Figure Eight", "tester", eight, 8.0);
    expect(figureEight.mOk, "a figure eight builds");
    expect(!figureEight.mTrack.mRaisedSections.empty() && figureEight.mTrack.mRaisedSections[0].mDriveable,
           "the crossing is bridged");
    expect(RivalLapSeconds(figureEight.mTrack) > 15.0, "a rival can drive a lap of the figure eight");

    // The result survives the file format and the online checks.
    TrackDefinition reloaded;
    expect(ParseTrack(SerializeTrack(oval.mTrack), reloaded).empty() && reloaded.Validate().empty()
               && TrackHash(reloaded) == TrackHash(oval.mTrack),
           "a built track saves and reloads unchanged");
    expect(IsOnlineSafeTrack(oval.mTrack) && CheckOnlineTrackLimits(oval.mTrack).empty(),
           "a built track can be hosted online");

    // Optional mines and hazard zones: absent by default, present when asked, always on the road and
    // clear of the start grid (Validate checks that), and a rival can still finish a lap.
    expect(BuildTrackFromPoints("Plain", "tester", Ring(10, 300, 200), 8.0).mTrack.mMines.empty()
               && oval.mTrack.mHazardZones.empty(),
           "no hazards unless asked for");
    BuilderOptions both;
    both.mMines = true;
    both.mHazards = true;
    const BuiltTrack hazardous = BuildTrackFromPoints("Hazard Oval", "tester", Ring(10, 300, 200), 8.0, both);
    expect(hazardous.mOk && !hazardous.mTrack.mMines.empty() && !hazardous.mTrack.mHazardZones.empty(),
           "asking for both gives mines and hazard zones");
    BuilderOptions minesOnly;
    minesOnly.mMines = true;
    const BuiltTrack mined = BuildTrackFromPoints("Mined Oval", "tester", Ring(10, 300, 200), 8.0, minesOnly);
    expect(mined.mOk && !mined.mTrack.mMines.empty() && mined.mTrack.mHazardZones.empty(),
           "mines only gives no hazard zones");
    BuilderOptions hazardsOnly;
    hazardsOnly.mHazards = true;
    const BuiltTrack zoned = BuildTrackFromPoints("Zoned Oval", "tester", Ring(10, 300, 200), 8.0, hazardsOnly);
    expect(zoned.mOk && zoned.mTrack.mMines.empty() && !zoned.mTrack.mHazardZones.empty(),
           "hazards only gives no mines");
    const double hazardLap = RivalLapSeconds(hazardous.mTrack);
    expect(hazardLap > 15.0 && hazardLap < 250.0, "a rival can still drive a lap through mines and hazards");
    const BuiltTrack hazardEight = BuildTrackFromPoints("Hazard Eight", "tester", eight, 8.0, both);
    expect(hazardEight.mOk, "a figure eight with hazards builds");
    TrackDefinition hazardReloaded;
    expect(ParseTrack(SerializeTrack(hazardous.mTrack), hazardReloaded).empty()
               && hazardReloaded.Validate().empty() && IsOnlineSafeTrack(hazardous.mTrack)
               && CheckOnlineTrackLimits(hazardous.mTrack).empty(),
           "a track with hazards saves, reloads and can be hosted online");

    // Two roads crossing at a shallow angle cannot be bridged sensibly.
    std::vector<EditorPoint> shallow = {{-300, 0}, {300, 20}, {300, 80}, {-300, -10}, {-300, -200}, {0, -250}};
    const BuiltTrack shallowResult = BuildTrackFromPoints("Shallow", "me", shallow, 8.0);
    expect(!shallowResult.mOk && shallowResult.mProblem.find("SHALLOW") != std::string::npos,
           "roads crossing at a shallow angle are refused with a reason");
    return ok ? 0 : 1;
}
