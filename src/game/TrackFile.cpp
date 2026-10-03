// SPDX-License-Identifier: MIT OR Apache-2.0
#include "TrackFile.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace
{
const char* const kHeader = "openhover-track";
constexpr std::size_t kMaximumItems = 4096;

// Shortest decimal text that reads back to exactly the same value, so files stay readable
// (7.8, not 7.7999999999999998).
std::string Number(double pValue)
{
    char buffer[40];
    for (int digits = 1; digits <= 17; ++digits)
    {
        std::snprintf(buffer, sizeof(buffer), "%.*g", digits, pValue);
        if (std::strtod(buffer, nullptr) == pValue)
            break;
    }
    return buffer;
}

// Same for single-precision colours.
std::string Number(float pValue)
{
    char buffer[40];
    for (int digits = 1; digits <= 9; ++digits)
    {
        std::snprintf(buffer, sizeof(buffer), "%.*g", digits, static_cast<double>(pValue));
        if (std::strtof(buffer, nullptr) == pValue)
            break;
    }
    return buffer;
}

bool ParseNumber(const std::string& pToken, double& pOut)
{
    char* end = nullptr;
    pOut = std::strtod(pToken.c_str(), &end);
    return end != pToken.c_str() && *end == '\0' && std::isfinite(pOut);
}

std::string Trim(const std::string& pText)
{
    std::size_t begin = 0;
    std::size_t end = pText.size();
    while (begin < end && (pText[begin] == ' ' || pText[begin] == '\t' || pText[begin] == '\r'))
        ++begin;
    while (end > begin && (pText[end - 1] == ' ' || pText[end - 1] == '\t' || pText[end - 1] == '\r'))
        --end;
    return pText.substr(begin, end - begin);
}

std::string Error(int pLine, const std::string& pMessage)
{
    return "line " + std::to_string(pLine) + ": " + pMessage;
}
}

std::string SerializeTrack(const TrackDefinition& pTrack)
{
    std::ostringstream out;
    out << kHeader << ' ' << pTrack.mFormatVersion << '\n';
    out << "id " << pTrack.mId << '\n';
    out << "name " << pTrack.mName << '\n';
    out << "author " << pTrack.mProvenance.mAuthor << '\n';
    out << "license " << pTrack.mProvenance.mLicense << '\n';
    out << "origin " << pTrack.mProvenance.mAssetOrigin << '\n';
    out << "road-half-width " << Number(pTrack.mRoadHalfWidth) << '\n';
    out << "atmosphere " << Number(pTrack.mAtmosphereRed) << ' ' << Number(pTrack.mAtmosphereGreen)
        << ' ' << Number(pTrack.mAtmosphereBlue) << '\n';
    out << "road-colour " << Number(pTrack.mRoadRed) << ' ' << Number(pTrack.mRoadGreen) << ' '
        << Number(pTrack.mRoadBlue) << '\n';
    out << "wall-colour " << Number(pTrack.mWallRed) << ' ' << Number(pTrack.mWallGreen) << ' '
        << Number(pTrack.mWallBlue) << '\n';
    for (const RaceGate& gate : pTrack.mWaypoints)
        out << "waypoint " << Number(gate.mX) << ' ' << Number(gate.mY) << ' ' << Number(gate.mRadius) << '\n';
    for (const RaceGate& gate : pTrack.mCheckpoints)
        out << "checkpoint " << Number(gate.mX) << ' ' << Number(gate.mY) << ' ' << Number(gate.mRadius) << '\n';
    for (const BoostPad& pad : pTrack.mBoostPads)
        out << "boost-pad " << Number(pad.mX) << ' ' << Number(pad.mY) << ' ' << Number(pad.mRadius) << '\n';
    for (const HazardZone& zone : pTrack.mHazardZones)
        out << "hazard " << Number(zone.mX) << ' ' << Number(zone.mY) << ' ' << Number(zone.mRadius)
            << ' ' << Number(zone.mSpeedLossPerSecond) << '\n';
    for (const Mine& mine : pTrack.mMines)
        out << "mine " << Number(mine.mX) << ' ' << Number(mine.mY) << ' ' << Number(mine.mRadius) << '\n';
    for (const RaisedSection& section : pTrack.mRaisedSections)
        out << "raised " << Number(section.mX) << ' ' << Number(section.mY) << ' '
            << Number(section.mHalfLength) << ' ' << Number(section.mHalfWidth) << ' '
            << Number(section.mHeading) << ' ' << Number(section.mClearHeight) << ' '
            << (section.mDriveable ? 1 : 0) << '\n';
    for (const double height : pTrack.mGroundHeights)
        out << "ground-height " << Number(height) << '\n';
    return out.str();
}

std::string ParseTrack(const std::string& pText, TrackDefinition& pOut)
{
    TrackDefinition track;
    track.mFormatVersion = 0;
    std::istringstream in(pText);
    std::string rawLine;
    int lineNumber = 0;
    bool sawHeader = false;
    while (std::getline(in, rawLine))
    {
        ++lineNumber;
        const std::string line = Trim(rawLine);
        if (line.empty() || line[0] == '#')
            continue;
        const std::size_t space = line.find_first_of(" \t");
        const std::string keyword = line.substr(0, space);
        const std::string rest = space == std::string::npos ? std::string() : Trim(line.substr(space));
        std::vector<double> values;
        {
            std::istringstream tokens(rest);
            std::string token;
            while (tokens >> token)
            {
                double value = 0.0;
                if (!ParseNumber(token, value))
                {
                    values.clear();
                    values.push_back(std::nan(""));
                    break;
                }
                values.push_back(value);
            }
        }
        const bool numeric = !(values.size() == 1 && std::isnan(values[0]));
        const auto expectCount = [&](std::size_t pCount) -> std::string
        {
            if (!numeric)
                return Error(lineNumber, "'" + keyword + "' needs numbers");
            if (values.size() != pCount)
                return Error(lineNumber, "'" + keyword + "' needs " + std::to_string(pCount) + " numbers");
            return std::string();
        };
        std::string problem;
        if (!sawHeader)
        {
            if (keyword != kHeader || !numeric || values.size() != 1 || values[0] != 1.0)
                return Error(lineNumber, std::string("first line must be '") + kHeader + " 1'");
            track.mFormatVersion = 1;
            sawHeader = true;
            continue;
        }
        if (keyword == "id")
            track.mId = rest;
        else if (keyword == "name")
            track.mName = rest;
        else if (keyword == "author")
            track.mProvenance.mAuthor = rest;
        else if (keyword == "license")
            track.mProvenance.mLicense = rest;
        else if (keyword == "origin")
            track.mProvenance.mAssetOrigin = rest;
        else if (keyword == "road-half-width")
        {
            problem = expectCount(1);
            if (problem.empty())
                track.mRoadHalfWidth = values[0];
        }
        else if (keyword == "atmosphere" || keyword == "road-colour" || keyword == "wall-colour")
        {
            problem = expectCount(3);
            if (problem.empty())
            {
                float* target = keyword == "atmosphere" ? &track.mAtmosphereRed
                    : keyword == "road-colour" ? &track.mRoadRed : &track.mWallRed;
                for (int i = 0; i < 3; ++i)
                    target[i] = static_cast<float>(values[i]);
            }
        }
        else if (keyword == "waypoint" || keyword == "checkpoint")
        {
            problem = expectCount(3);
            if (problem.empty())
            {
                std::vector<RaceGate>& list = keyword == "waypoint" ? track.mWaypoints : track.mCheckpoints;
                if (list.size() >= kMaximumItems)
                    return Error(lineNumber, "too many " + keyword + " lines");
                list.push_back({values[0], values[1], values[2]});
            }
        }
        else if (keyword == "boost-pad")
        {
            problem = expectCount(3);
            if (problem.empty())
            {
                if (track.mBoostPads.size() >= kMaximumItems)
                    return Error(lineNumber, "too many boost-pad lines");
                BoostPad pad;
                pad.mX = values[0];
                pad.mY = values[1];
                pad.mRadius = values[2];
                track.mBoostPads.push_back(pad);
            }
        }
        else if (keyword == "hazard")
        {
            problem = expectCount(4);
            if (problem.empty())
            {
                if (track.mHazardZones.size() >= kMaximumItems)
                    return Error(lineNumber, "too many hazard lines");
                HazardZone zone;
                zone.mX = values[0];
                zone.mY = values[1];
                zone.mRadius = values[2];
                zone.mSpeedLossPerSecond = values[3];
                track.mHazardZones.push_back(zone);
            }
        }
        else if (keyword == "mine")
        {
            problem = expectCount(3);
            if (problem.empty())
            {
                if (track.mMines.size() >= kMaximumItems)
                    return Error(lineNumber, "too many mine lines");
                Mine mine;
                mine.mX = values[0];
                mine.mY = values[1];
                mine.mRadius = values[2];
                track.mMines.push_back(mine);
            }
        }
        else if (keyword == "ground-height")
        {
            problem = expectCount(1);
            if (problem.empty())
            {
                if (track.mGroundHeights.size() >= kMaximumItems)
                    return Error(lineNumber, "too many ground-height lines");
                track.mGroundHeights.push_back(values[0]);
            }
        }
        else if (keyword == "raised")
        {
            problem = expectCount(7);
            if (problem.empty())
            {
                if (track.mRaisedSections.size() >= kMaximumItems)
                    return Error(lineNumber, "too many raised lines");
                RaisedSection section;
                section.mX = values[0];
                section.mY = values[1];
                section.mHalfLength = values[2];
                section.mHalfWidth = values[3];
                section.mHeading = values[4];
                section.mClearHeight = values[5];
                section.mDriveable = values[6] != 0.0;
                track.mRaisedSections.push_back(section);
            }
        }
        else
            return Error(lineNumber, "unknown directive '" + keyword + "'");
        if (!problem.empty())
            return problem;
    }
    if (!sawHeader)
        return "the file is empty";
    pOut = track;
    return std::string();
}
