// SPDX-License-Identifier: MIT OR Apache-2.0
#include <SDL.h>
#include <SDL_opengl.h>

#include "AudioFeedback.h"
#include "BoostPad.h"
#include "Championship.h"
#include "Course.h"
#include "CraftClass.h"
#include "FixedStepClock.h"
#include "Hovercraft.h"
#include "InputRecording.h"
#include "LapTiming.h"
#include "Missile.h"
#include "Race.h"
#include "RaceMode.h"
#include "RacePosition.h"
#include "RaceStart.h"
#include "RacerCollision.h"
#include "RecoveryAssist.h"
#include "RivalController.h"
#include "RouteGuidance.h"
#include "SteeringAssist.h"
#include "TrackDefinition.h"
#include "TcpLobbyClient.h"
#include "WallCollision.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace
{
const int kWindowWidth = 1280;
const int kWindowHeight = 720;
const int kRivalCount = 7;
const double kPi = 3.14159265358979323846;

enum class FrontScreen
{
    Welcome,
    Multiplayer,
    HostRaceSetup,
    OnlineRace,
    DisplayNameSetup,
    HowToPlay,
    Settings,
    LocalSetup,
    RaceSetup
};

struct LobbyRoomView
{
    int mId = 0;
    std::string mName;
    int mHostId = 0;
    int mPlayerCount = 0;
    int mPlayerCapacity = 0;
    bool mRaceRunning = false;
    int mRaceMode = 0;
    int mTrackIndex = 0;
    int mLapCount = 0;
    int mRivalCount = 0;
    bool mWeaponsAllowed = false;
};

struct LobbyPlayerView
{
    int mId = 0;
    std::string mDisplayName;
};

struct OnlineRacerView
{
    int mPlayerId = 0;
    HovercraftState mState;
    RaceProgress mProgress;
    LapTiming mLapTiming;
    int mPosition = 0;
};

std::vector<LobbyPlayerView> gLobbyPlayers;
std::vector<LobbyRoomView> gLobbyRooms;
std::vector<std::string> gLobbyChatMessages;
std::string gLobbyChatInput;
bool gLobbyChatInputFocused = false;
std::string gPlayerDisplayName;
std::string gLobbyStatus = "CONNECTING TO SERVER";
int gLobbySelectedRoom = -1;
int gLobbyPlayerId = 0;
int gLobbyJoinedRoomId = 0;
int gLobbyJoinPendingRoomId = 0;
int gHostSetupSelection = 0;
int gHostRaceMode = 0;
int gHostTrackIndex = 0;
int gHostLapCount = 3;
int gHostPlayerCapacity = 8;
int gHostRivalCount = 0;
bool gHostWeaponsAllowed = true;
bool gHostCreatePending = false;
bool gDisplayNameSetupConnectsToLobby = false;
int gOnlineRaceRoomId = 0;
unsigned int gOnlineRaceTick = 0;
int gOnlineTargetLaps = 0;
std::vector<OnlineRacerView> gOnlineRacers;

std::vector<std::string> SplitLobbyField(const std::string& pText, char pDelimiter)
{
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (start <= pText.size())
    {
        const std::size_t end = pText.find(pDelimiter, start);
        fields.push_back(pText.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return fields;
}

void ParseLobbySnapshot(const std::string& pMessage)
{
    gLobbyPlayers.clear();
    gLobbyRooms.clear();
    const std::vector<std::string> entries = SplitLobbyField(pMessage, '|');
    for (std::size_t index = 1; index < entries.size(); ++index)
    {
        const std::vector<std::string> fields = SplitLobbyField(entries[index], ',');
        if (fields.size() == 3 && fields[0] == "P")
            gLobbyPlayers.push_back({std::atoi(fields[1].c_str()), fields[2]});
        else if (fields.size() == 12 && fields[0] == "R")
        {
            LobbyRoomView room;
            room.mId = std::atoi(fields[1].c_str());
            room.mName = fields[2];
            room.mHostId = std::atoi(fields[3].c_str());
            room.mPlayerCount = std::atoi(fields[4].c_str());
            room.mPlayerCapacity = std::atoi(fields[5].c_str());
            room.mRaceRunning = std::atoi(fields[6].c_str()) != 0;
            room.mRaceMode = std::atoi(fields[7].c_str());
            room.mTrackIndex = std::atoi(fields[8].c_str());
            room.mLapCount = std::atoi(fields[9].c_str());
            room.mRivalCount = std::atoi(fields[10].c_str());
            room.mWeaponsAllowed = std::atoi(fields[11].c_str()) != 0;
            gLobbyRooms.push_back(room);
        }
    }
    if (gLobbyRooms.empty())
        gLobbySelectedRoom = -1;
    else if (gLobbySelectedRoom < 0 || gLobbySelectedRoom >= static_cast<int>(gLobbyRooms.size()))
        gLobbySelectedRoom = 0;
    bool joinedRoomExists = false;
    for (const LobbyRoomView& room : gLobbyRooms)
        joinedRoomExists = joinedRoomExists || room.mId == gLobbyJoinedRoomId;
    if (!joinedRoomExists)
        gLobbyJoinedRoomId = 0;
}

bool ParseRaceSnapshot(const std::string& pMessage)
{
    const std::vector<std::string> entries = SplitLobbyField(pMessage, '|');
    if (entries.size() < 3 || entries[0].compare(0, 5, "RACE ") != 0)
        return false;
    const int roomId = std::atoi(entries[0].substr(5).c_str());
    const int tick = std::atoi(entries[1].c_str());
    if (roomId <= 0 || tick < 0)
        return false;
    std::vector<OnlineRacerView> racers;
    for (std::size_t index = 2; index < entries.size(); ++index)
    {
        const std::vector<std::string> fields = SplitLobbyField(entries[index], ',');
        if (fields.size() != 6)
            return false;
        OnlineRacerView racer;
        racer.mPlayerId = std::atoi(fields[0].c_str());
        racer.mState.mX = std::strtod(fields[1].c_str(), nullptr);
        racer.mState.mY = std::strtod(fields[2].c_str(), nullptr);
        racer.mState.mHeading = std::strtod(fields[3].c_str(), nullptr);
        racer.mState.mTravelHeading = racer.mState.mHeading;
        racer.mState.mSpeed = std::strtod(fields[4].c_str(), nullptr);
        racer.mState.mHeight = std::strtod(fields[5].c_str(), nullptr);
        racer.mState.mPreviousX = racer.mState.mX;
        racer.mState.mPreviousY = racer.mState.mY;
        racer.mState.mHasPreviousPosition = true;
        if (racer.mPlayerId <= 0)
            return false;
        racers.push_back(racer);
    }
    gOnlineRaceRoomId = roomId;
    gOnlineRaceTick = static_cast<unsigned int>(tick);
    gOnlineRacers.swap(racers);
    return true;
}

bool ParseRaceHudSnapshot(const std::string& pMessage)
{
    const std::vector<std::string> entries = SplitLobbyField(pMessage, '|');
    if (entries.size() < 3 || entries[0].compare(0, 8, "RACEHUD ") != 0)
        return false;
    const int roomId = std::atoi(entries[0].substr(8).c_str());
    const int targetLaps = std::atoi(entries[1].c_str());
    if (roomId != gOnlineRaceRoomId || targetLaps < 1)
        return false;
    for (std::size_t index = 2; index < entries.size(); ++index)
    {
        const std::vector<std::string> fields = SplitLobbyField(entries[index], ',');
        if (fields.size() != 9)
            return false;
        const int playerId = std::atoi(fields[0].c_str());
        for (OnlineRacerView& racer : gOnlineRacers)
        {
            if (racer.mPlayerId != playerId)
                continue;
            racer.mProgress.mCompletedLaps = std::atoi(fields[1].c_str());
            racer.mProgress.mNextCheckpoint = std::atoi(fields[2].c_str());
            racer.mProgress.mElapsedSeconds = std::strtod(fields[3].c_str(), nullptr);
            racer.mProgress.mFinished = std::atoi(fields[4].c_str()) != 0;
            racer.mPosition = std::atoi(fields[5].c_str());
            racer.mLapTiming.mCurrentSeconds = std::strtod(fields[6].c_str(), nullptr);
            racer.mLapTiming.mLastSeconds = std::strtod(fields[7].c_str(), nullptr);
            racer.mLapTiming.mBestSeconds = std::strtod(fields[8].c_str(), nullptr);
            break;
        }
    }
    gOnlineTargetLaps = targetLaps;
    return true;
}

const char* RaceModeSetupLabel(RaceMode pRaceMode)
{
    switch (pRaceMode)
    {
    case RaceMode::SingleRace:
        return "SINGLE RACE RIVALS";
    case RaceMode::TimeTrial:
        return "TIME TRIAL SOLO";
    case RaceMode::Practice:
        return "PRACTICE OPEN DRIVE";
    case RaceMode::Championship:
        return "CHAMPIONSHIP SERIES";
    }
    return "SINGLE RACE RIVALS";
}

const char* RivalDifficultySetupLabel(RivalDifficulty pRivalDifficulty)
{
    switch (pRivalDifficulty)
    {
    case RivalDifficulty::Relaxed:
        return "RELAXED SLOWER AI";
    case RivalDifficulty::Standard:
        return "STANDARD BALANCED";
    case RivalDifficulty::Expert:
        return "EXPERT FASTER AI";
    }
    return "STANDARD BALANCED";
}

bool IsPointInRect(int pX, int pY, int pLeft, int pTop, int pWidth, int pHeight)
{
    return pX >= pLeft && pX < pLeft + pWidth && pY >= pTop && pY < pTop + pHeight;
}

bool IsLobbyNameCharacter(char pCharacter)
{
    return (pCharacter >= 'A' && pCharacter <= 'Z')
        || (pCharacter >= 'a' && pCharacter <= 'z')
        || (pCharacter >= '0' && pCharacter <= '9')
        || pCharacter == '_' || pCharacter == '-';
}

void AppendLobbyNameText(std::string& pName, const char* pText)
{
    for (const char* character = pText; *character != '\0' && pName.size() < 24; ++character)
    {
        if (IsLobbyNameCharacter(*character))
        {
            const char uppercaseCharacter = *character >= 'a' && *character <= 'z'
                ? static_cast<char>(*character - 'a' + 'A') : *character;
            pName += uppercaseCharacter;
        }
    }
}

double ControllerAxis(Sint16 pValue)
{
    const double normalized = pValue < 0 ? pValue / 32768.0 : pValue / 32767.0;
    return std::fabs(normalized) < 0.15 ? 0.0 : normalized;
}

bool BounceOffCourseWall(Hovercraft& pHovercraft, const Course& pCourse)
{
    HovercraftState state = pHovercraft.State();
    if (ResolveCourseWallCollision(state, pCourse))
    {
        pHovercraft.Reset(state);
        return true;
    }
    return false;
}

bool ApplyBoostPads(Hovercraft& pHovercraft, const std::vector<BoostPad>& pPads)
{
    HovercraftState state = pHovercraft.State();
    bool boosted = false;
    for (const BoostPad& pad : pPads)
    {
        if (ApplyBoostPad(state, pad))
            boosted = true;
    }
    if (boosted)
        pHovercraft.Reset(state);
    return boosted;
}

void ApplyHazardZones(Hovercraft& pHovercraft, const std::vector<HazardZone>& pZones,
                      double pSeconds)
{
    HovercraftState state = pHovercraft.State();
    bool affected = false;
    for (const HazardZone& zone : pZones)
        affected = ApplyHazardZone(state, zone, pSeconds) || affected;
    if (affected)
        pHovercraft.Reset(state);
}

bool ApplyMines(Hovercraft& pHovercraft, std::vector<Mine>& pMines)
{
    HovercraftState state = pHovercraft.State();
    for (Mine& mine : pMines)
    {
        if (ApplyMine(state, mine))
        {
            pHovercraft.Reset(state);
            return true;
        }
    }
    return false;
}

void ApplyRaisedSections(Hovercraft& pHovercraft, const std::vector<RaisedSection>& pSections)
{
    HovercraftState state = pHovercraft.State();
    const double previousSurfaceHeight = state.mSurfaceHeight;
    const double previousHeight = state.mHeight;
    const double previousVerticalSpeed = state.mVerticalSpeed;
    state.mSurfaceHeight = 0.0;
    for (const RaisedSection& section : pSections)
    {
        if (LandOnRaisedSection(state, section))
        {
            if (state.mSurfaceHeight != previousSurfaceHeight || state.mHeight != previousHeight
                || state.mVerticalSpeed != previousVerticalSpeed)
            {
                pHovercraft.Reset(state);
            }
            return;
        }
        if (ResolveRaisedSectionCollision(state, section))
        {
            pHovercraft.Reset(state);
            return;
        }
    }
    if (state.mSurfaceHeight != previousSurfaceHeight)
        pHovercraft.Reset(state);
}

bool ShouldJumpRaisedSection(const HovercraftState& pState,
                             const std::vector<RaisedSection>& pSections)
{
    const double travelX = std::cos(pState.mTravelHeading);
    const double travelY = std::sin(pState.mTravelHeading);
    for (const RaisedSection& section : pSections)
    {
        const double forwardX = std::cos(section.mHeading);
        const double forwardY = std::sin(section.mHeading);
        const double sideX = -forwardY;
        const double sideY = forwardX;
        const double deltaX = section.mX - pState.mX;
        const double deltaY = section.mY - pState.mY;
        const double forwardDistance = deltaX * travelX + deltaY * travelY;
        const double sidewaysDistance = std::fabs(deltaX * sideX + deltaY * sideY);
        const double jumpLead = std::fmax(4.0, std::fabs(pState.mSpeed) * 0.4);
        if (forwardDistance > 0.0 && forwardDistance <= section.mHalfLength + jumpLead
            && sidewaysDistance <= section.mHalfWidth + 0.9)
        {
            return true;
        }
    }
    return false;
}

void SetPerspective(double pAspect, double pSpeed)
{
    const double nearPlane = 0.2;
    const double farPlane = 400.0;
    const double speedFraction = std::fmin(1.0, std::fabs(pSpeed) / 55.0);
    const double fieldOfView = 54.0 + speedFraction * 7.0;
    const double top = nearPlane * std::tan(fieldOfView * kPi / 360.0);
    glFrustum(-top * pAspect, top * pAspect, -top, top, nearPlane, farPlane);
}

void SetChaseCamera(const HovercraftState& pState, double pDistance)
{
    const double forwardX = std::cos(pState.mHeading);
    const double forwardZ = std::sin(pState.mHeading);
    const double eyeX = pState.mX - forwardX * pDistance;
    const double eyeY = 2.3 + pDistance * 0.09;
    const double eyeZ = pState.mY - forwardZ * pDistance;
    const double targetX = pState.mX + forwardX * pDistance * 0.8;
    const double targetY = 1.55;
    const double targetZ = pState.mY + forwardZ * pDistance * 0.8;
    double viewX = targetX - eyeX;
    double viewY = targetY - eyeY;
    double viewZ = targetZ - eyeZ;
    const double viewLength = std::sqrt(viewX * viewX + viewY * viewY + viewZ * viewZ);
    viewX /= viewLength;
    viewY /= viewLength;
    viewZ /= viewLength;
    double sideX = -viewZ;
    double sideZ = viewX;
    const double sideLength = std::sqrt(sideX * sideX + sideZ * sideZ);
    sideX /= sideLength;
    sideZ /= sideLength;
    const double upX = -viewY * sideZ;
    const double upY = sideZ * viewX - sideX * viewZ;
    const double upZ = viewY * sideX;
    const GLdouble matrix[16] = {
        sideX, upX, -viewX, 0.0,
        0.0, upY, -viewY, 0.0,
        sideZ, upZ, -viewZ, 0.0,
        0.0, 0.0, 0.0, 1.0
    };
    glMultMatrixd(matrix);
    glTranslated(-eyeX, -eyeY, -eyeZ);
}

GLuint CreateRoadTexture()
{
    const int textureSize = 64;
    unsigned char pixels[textureSize * textureSize * 3];
    for (int y = 0; y < textureSize; ++y)
    {
        for (int x = 0; x < textureSize; ++x)
        {
            const bool seam = x % 21 < 2 || y % 16 < 2;
            const bool alternateTile = (x / 21 + y / 16) % 2 != 0;
            const int pixel = (y * textureSize + x) * 3;
            pixels[pixel] = static_cast<unsigned char>(seam ? 58 : (alternateTile ? 150 : 166));
            pixels[pixel + 1] = static_cast<unsigned char>(seam ? 86 : (alternateTile ? 178 : 194));
            pixels[pixel + 2] = static_cast<unsigned char>(seam ? 90 : (alternateTile ? 184 : 200));
        }
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureSize, textureSize, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, pixels);
    return texture;
}

GLuint CreateWallTexture()
{
    const int textureSize = 64;
    unsigned char pixels[textureSize * textureSize * 3];
    for (int y = 0; y < textureSize; ++y)
    {
        for (int x = 0; x < textureSize; ++x)
        {
            const bool panelSeam = x % 16 < 2 || y % 22 < 2;
            const bool warningBand = y >= 29 && y < 36;
            const int pixel = (y * textureSize + x) * 3;
            pixels[pixel] = static_cast<unsigned char>(panelSeam ? 26 : (warningBand ? 232 : 92));
            pixels[pixel + 1] = static_cast<unsigned char>(panelSeam ? 32 : (warningBand ? 146 : 108));
            pixels[pixel + 2] = static_cast<unsigned char>(panelSeam ? 36 : (warningBand ? 40 : 112));
        }
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureSize, textureSize, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, pixels);
    return texture;
}

void DrawCourseGrid(const HovercraftState& pState)
{
    const int centerX = static_cast<int>(pState.mX / 16.0) * 16;
    const int centerZ = static_cast<int>(pState.mY / 16.0) * 16;
    glColor3f(0.025f, 0.18f, 0.27f);
    glBegin(GL_QUADS);
    glNormal3d(0.0, 1.0, 0.0);
    glVertex3d(centerX - 96, -0.02, centerZ - 96);
    glVertex3d(centerX + 96, -0.02, centerZ - 96);
    glVertex3d(centerX + 96, -0.02, centerZ + 96);
    glVertex3d(centerX - 96, -0.02, centerZ + 96);
    glEnd();

    glColor3f(0.08f, 0.42f, 0.56f);
    glBegin(GL_LINES);
    for (int offset = -80; offset <= 80; offset += 16)
    {
        glVertex3d(centerX + offset, 0.0, centerZ - 80);
        glVertex3d(centerX + offset, 0.0, centerZ + 80);
        glVertex3d(centerX - 80, 0.0, centerZ + offset);
        glVertex3d(centerX + 80, 0.0, centerZ + offset);
    }
    glEnd();

    glColor3f(0.18f, 0.62f, 0.72f);
    glBegin(GL_LINES);
    for (int offset = -88; offset <= 88; offset += 11)
    {
        const double wave = std::sin((centerX + offset) * 0.16) * 1.8;
        glVertex3d(centerX + offset - 4.0, 0.012, centerZ + wave);
        glVertex3d(centerX + offset + 4.0, 0.012, centerZ + wave);
        glVertex3d(centerX + wave, 0.012, centerZ + offset - 4.0);
        glVertex3d(centerX + wave, 0.012, centerZ + offset + 4.0);
    }
    glEnd();
}

void DrawRoadSegment(double pStartX, double pStartZ, double pEndX, double pEndZ,
                     double pHalfWidth)
{
    const double roadHeight = 0.35;
    const double wallHeight = 5.4;
    const double directionX = pEndX - pStartX;
    const double directionZ = pEndZ - pStartZ;
    const double length = std::sqrt(directionX * directionX + directionZ * directionZ);
    const double sideX = -directionZ / length * pHalfWidth;
    const double sideZ = directionX / length * pHalfWidth;
    glColor3f(0.68f, 0.76f, 0.78f);
    glBegin(GL_QUADS);
    glVertex3d(pStartX + sideX, roadHeight, pStartZ + sideZ);
    glVertex3d(pStartX - sideX, roadHeight, pStartZ - sideZ);
    glVertex3d(pEndX - sideX, roadHeight, pEndZ - sideZ);
    glVertex3d(pEndX + sideX, roadHeight, pEndZ + sideZ);
    glEnd();

    glColor3f(0.84f, 0.9f, 0.91f);
    glBegin(GL_QUADS);
    glVertex3d(pStartX + sideX, 0.0, pStartZ + sideZ);
    glVertex3d(pStartX + sideX, wallHeight, pStartZ + sideZ);
    glVertex3d(pEndX + sideX, wallHeight, pEndZ + sideZ);
    glVertex3d(pEndX + sideX, 0.0, pEndZ + sideZ);
    glVertex3d(pStartX - sideX, wallHeight, pStartZ - sideZ);
    glVertex3d(pStartX - sideX, 0.0, pStartZ - sideZ);
    glVertex3d(pEndX - sideX, 0.0, pEndZ - sideZ);
    glVertex3d(pEndX - sideX, wallHeight, pEndZ - sideZ);
    glEnd();

    glColor3f(0.08f, 0.74f, 0.8f);
    glBegin(GL_LINES);
    glVertex3d(pStartX + sideX, wallHeight, pStartZ + sideZ);
    glVertex3d(pEndX + sideX, wallHeight, pEndZ + sideZ);
    glVertex3d(pStartX - sideX, wallHeight, pStartZ - sideZ);
    glVertex3d(pEndX - sideX, wallHeight, pEndZ - sideZ);
    for (double offset = 0.0; offset <= length; offset += 4.0)
    {
        const double postX = pStartX + directionX / length * std::fmin(offset, length);
        const double postZ = pStartZ + directionZ / length * std::fmin(offset, length);
        glVertex3d(postX + sideX, roadHeight, postZ + sideZ);
        glVertex3d(postX + sideX, wallHeight, postZ + sideZ);
        glVertex3d(postX - sideX, roadHeight, postZ - sideZ);
        glVertex3d(postX - sideX, wallHeight, postZ - sideZ);
    }
    glEnd();

    glColor3f(0.96f, 0.48f, 0.14f);
    glBegin(GL_QUADS);
    glVertex3d(pStartX + sideX * 0.82, roadHeight + 0.012, pStartZ + sideZ * 0.82);
    glVertex3d(pEndX + sideX * 0.82, roadHeight + 0.012, pEndZ + sideZ * 0.82);
    glVertex3d(pEndX + sideX, roadHeight + 0.012, pEndZ + sideZ);
    glVertex3d(pStartX + sideX, roadHeight + 0.012, pStartZ + sideZ);
    glVertex3d(pStartX - sideX, roadHeight + 0.012, pStartZ - sideZ);
    glVertex3d(pEndX - sideX, roadHeight + 0.012, pEndZ - sideZ);
    glVertex3d(pEndX - sideX * 0.82, roadHeight + 0.012, pEndZ - sideZ * 0.82);
    glVertex3d(pStartX - sideX * 0.82, roadHeight + 0.012, pStartZ - sideZ * 0.82);
    glEnd();

    glColor3f(0.45f, 0.54f, 0.57f);
    glBegin(GL_LINES);
    for (double offset = 0.0; offset <= length; offset += 2.0)
    {
        const double markerOffset = std::fmin(offset, length);
        const double centerX = pStartX + directionX / length * markerOffset;
        const double centerZ = pStartZ + directionZ / length * markerOffset;
        glVertex3d(centerX + sideX * 0.8, roadHeight + 0.016, centerZ + sideZ * 0.8);
        glVertex3d(centerX - sideX * 0.8, roadHeight + 0.016, centerZ - sideZ * 0.8);
    }
    glEnd();

    glColor3f(0.11f, 0.6f, 0.66f);
    glBegin(GL_QUADS);
    for (double offset = 3.0; offset < length; offset += 6.0)
    {
        const double centerX = pStartX + directionX / length * offset;
        const double centerZ = pStartZ + directionZ / length * offset;
        const double forwardX = directionX / length;
        const double forwardZ = directionZ / length;
        glVertex3d(centerX - forwardX * 0.8 + sideX * 1.01, 0.62,
                   centerZ - forwardZ * 0.8 + sideZ * 1.01);
        glVertex3d(centerX + forwardX * 0.8 + sideX * 1.01, 0.62,
                   centerZ + forwardZ * 0.8 + sideZ * 1.01);
        glVertex3d(centerX + forwardX * 0.8 + sideX * 1.01, 1.2,
                   centerZ + forwardZ * 0.8 + sideZ * 1.01);
        glVertex3d(centerX - forwardX * 0.8 + sideX * 1.01, 1.2,
                   centerZ - forwardZ * 0.8 + sideZ * 1.01);
    }
    glEnd();
}

void DrawConnectedTrack(const std::vector<RaceGate>& pWaypoints, double pHalfWidth,
                        float pRoadRed, float pRoadGreen, float pRoadBlue,
                        float pWallRed, float pWallGreen, float pWallBlue, GLuint pRoadTexture,
                        GLuint pWallTexture)
{
    if (pWaypoints.size() < 3)
        return;

    struct EdgePoint
    {
        double mLeftX;
        double mLeftZ;
        double mRightX;
        double mRightZ;
    };
    std::vector<EdgePoint> edges;
    for (int index = 0; index < static_cast<int>(pWaypoints.size()); ++index)
    {
        const RaceGate& previous = pWaypoints[(index + static_cast<int>(pWaypoints.size()) - 1)
                                               % pWaypoints.size()];
        const RaceGate& current = pWaypoints[index];
        const RaceGate& next = pWaypoints[(index + 1) % pWaypoints.size()];
        double previousX = current.mX - previous.mX;
        double previousZ = current.mY - previous.mY;
        double nextX = next.mX - current.mX;
        double nextZ = next.mY - current.mY;
        const double previousLength = std::sqrt(previousX * previousX + previousZ * previousZ);
        const double nextLength = std::sqrt(nextX * nextX + nextZ * nextZ);
        previousX /= previousLength;
        previousZ /= previousLength;
        nextX /= nextLength;
        nextZ /= nextLength;
        double normalX = -previousZ - nextZ;
        double normalZ = previousX + nextX;
        const double normalLength = std::sqrt(normalX * normalX + normalZ * normalZ);
        normalX /= normalLength;
        normalZ /= normalLength;
        const double miterScale = std::fmin(pHalfWidth * 1.6,
            pHalfWidth / std::fmax(0.45, normalX * -nextZ + normalZ * nextX));
        edges.push_back({current.mX + normalX * miterScale, current.mY + normalZ * miterScale,
                         current.mX - normalX * miterScale, current.mY - normalZ * miterScale});
    }

    const double roadHeight = 0.35;
    const double wallHeight = 5.4;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, pRoadTexture);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor3f(pRoadRed, pRoadGreen, pRoadBlue);
    glBegin(GL_QUAD_STRIP);
    glNormal3d(0.0, 1.0, 0.0);
    double textureDistance = 0.0;
    for (int index = 0; index <= static_cast<int>(edges.size()); ++index)
    {
        const EdgePoint& edge = edges[index % edges.size()];
        if (index > 0)
        {
            const EdgePoint& previous = edges[(index - 1) % edges.size()];
            const double previousCenterX = (previous.mLeftX + previous.mRightX) * 0.5;
            const double previousCenterZ = (previous.mLeftZ + previous.mRightZ) * 0.5;
            const double centerX = (edge.mLeftX + edge.mRightX) * 0.5;
            const double centerZ = (edge.mLeftZ + edge.mRightZ) * 0.5;
            const double deltaX = centerX - previousCenterX;
            const double deltaZ = centerZ - previousCenterZ;
            textureDistance += std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
        }
        glTexCoord2d(textureDistance / 12.0, 0.0);
        glVertex3d(edge.mLeftX, roadHeight, edge.mLeftZ);
        glTexCoord2d(textureDistance / 12.0, 3.0);
        glVertex3d(edge.mRightX, roadHeight, edge.mRightZ);
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);

    glColor3f(0.12f, 0.62f, 0.68f);
    glBegin(GL_LINE_LOOP);
    for (const EdgePoint& edge : edges)
    {
        glVertex3d(edge.mLeftX * 0.9 + edge.mRightX * 0.1, roadHeight + 0.014,
                   edge.mLeftZ * 0.9 + edge.mRightZ * 0.1);
    }
    glEnd();
    glBegin(GL_LINE_LOOP);
    for (const EdgePoint& edge : edges)
    {
        glVertex3d(edge.mLeftX * 0.1 + edge.mRightX * 0.9, roadHeight + 0.014,
                   edge.mLeftZ * 0.1 + edge.mRightZ * 0.9);
    }
    glEnd();
    glColor3f(0.48f, 0.56f, 0.59f);
    glBegin(GL_LINES);
    for (int index = 0; index < static_cast<int>(edges.size()); ++index)
    {
        const EdgePoint& start = edges[index];
        const EdgePoint& end = edges[(index + 1) % edges.size()];
        for (int column = 1; column < 3; ++column)
        {
            const double across = column / 3.0;
            glVertex3d(start.mLeftX + (start.mRightX - start.mLeftX) * across,
                       roadHeight + 0.012,
                       start.mLeftZ + (start.mRightZ - start.mLeftZ) * across);
            glVertex3d(end.mLeftX + (end.mRightX - end.mLeftX) * across,
                       roadHeight + 0.012,
                       end.mLeftZ + (end.mRightZ - end.mLeftZ) * across);
        }
        const double length = std::sqrt((end.mLeftX - start.mLeftX) * (end.mLeftX - start.mLeftX)
            + (end.mLeftZ - start.mLeftZ) * (end.mLeftZ - start.mLeftZ));
        for (double distance = 2.5; distance < length; distance += 2.5)
        {
            const double progress = distance / length;
            glVertex3d(start.mLeftX + (end.mLeftX - start.mLeftX) * progress, roadHeight + 0.012,
                       start.mLeftZ + (end.mLeftZ - start.mLeftZ) * progress);
            glVertex3d(start.mRightX + (end.mRightX - start.mRightX) * progress, roadHeight + 0.012,
                       start.mRightZ + (end.mRightZ - start.mRightZ) * progress);
        }
    }
    glEnd();

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, pWallTexture);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor3f(pWallRed, pWallGreen, pWallBlue);
    double wallTextureDistance = 0.0;
    glBegin(GL_QUAD_STRIP);
    for (int index = 0; index <= static_cast<int>(edges.size()); ++index)
    {
        const EdgePoint& edge = edges[index % edges.size()];
        if (index > 0)
        {
            const EdgePoint& previous = edges[(index - 1) % edges.size()];
            const double deltaX = edge.mLeftX - previous.mLeftX;
            const double deltaZ = edge.mLeftZ - previous.mLeftZ;
            wallTextureDistance += std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
        }
        const double normalLength = std::sqrt((edge.mLeftX - edge.mRightX) * (edge.mLeftX - edge.mRightX)
            + (edge.mLeftZ - edge.mRightZ) * (edge.mLeftZ - edge.mRightZ));
        glNormal3d((edge.mLeftX - edge.mRightX) / normalLength, 0.0,
                   (edge.mLeftZ - edge.mRightZ) / normalLength);
        glTexCoord2d(wallTextureDistance / 5.0, 0.0);
        glVertex3d(edge.mLeftX, roadHeight, edge.mLeftZ);
        glTexCoord2d(wallTextureDistance / 5.0, 1.0);
        glVertex3d(edge.mLeftX, wallHeight, edge.mLeftZ);
    }
    glEnd();
    wallTextureDistance = 0.0;
    glBegin(GL_QUAD_STRIP);
    for (int index = 0; index <= static_cast<int>(edges.size()); ++index)
    {
        const EdgePoint& edge = edges[index % edges.size()];
        if (index > 0)
        {
            const EdgePoint& previous = edges[(index - 1) % edges.size()];
            const double deltaX = edge.mRightX - previous.mRightX;
            const double deltaZ = edge.mRightZ - previous.mRightZ;
            wallTextureDistance += std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
        }
        const double normalLength = std::sqrt((edge.mRightX - edge.mLeftX) * (edge.mRightX - edge.mLeftX)
            + (edge.mRightZ - edge.mLeftZ) * (edge.mRightZ - edge.mLeftZ));
        glNormal3d((edge.mRightX - edge.mLeftX) / normalLength, 0.0,
                   (edge.mRightZ - edge.mLeftZ) / normalLength);
        glTexCoord2d(wallTextureDistance / 5.0, 1.0);
        glVertex3d(edge.mRightX, wallHeight, edge.mRightZ);
        glTexCoord2d(wallTextureDistance / 5.0, 0.0);
        glVertex3d(edge.mRightX, roadHeight, edge.mRightZ);
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);

    glColor3f(0.08f, 0.74f, 0.8f);
    glBegin(GL_LINE_LOOP);
    for (const EdgePoint& edge : edges)
        glVertex3d(edge.mLeftX, wallHeight, edge.mLeftZ);
    glEnd();
    glBegin(GL_LINE_LOOP);
    for (const EdgePoint& edge : edges)
        glVertex3d(edge.mRightX, wallHeight, edge.mRightZ);
    glEnd();

    glColor3f(0.22f, 0.9f, 1.0f);
    glBegin(GL_TRIANGLES);
    for (int side = 0; side < 2; ++side)
    {
        for (int index = 0; index < static_cast<int>(edges.size()); ++index)
        {
            const EdgePoint& start = edges[index];
            const EdgePoint& end = edges[(index + 1) % edges.size()];
            const double startX = side == 0 ? start.mLeftX : start.mRightX;
            const double startZ = side == 0 ? start.mLeftZ : start.mRightZ;
            const double endX = side == 0 ? end.mLeftX : end.mRightX;
            const double endZ = side == 0 ? end.mLeftZ : end.mRightZ;
            const double segmentX = endX - startX;
            const double segmentZ = endZ - startZ;
            const double segmentLength = std::sqrt(segmentX * segmentX + segmentZ * segmentZ);
            if (segmentLength == 0.0)
                continue;
            const double forwardX = segmentX / segmentLength;
            const double forwardZ = segmentZ / segmentLength;
            const double acrossX = start.mLeftX - start.mRightX;
            const double acrossZ = start.mLeftZ - start.mRightZ;
            const double acrossLength = std::sqrt(acrossX * acrossX + acrossZ * acrossZ);
            const double outwardX = (side == 0 ? acrossX : -acrossX) / acrossLength * 0.02;
            const double outwardZ = (side == 0 ? acrossZ : -acrossZ) / acrossLength * 0.02;
            for (double distance = 5.0; distance < segmentLength - 1.5; distance += 9.0)
            {
                const double centerX = startX + forwardX * distance + outwardX;
                const double centerZ = startZ + forwardZ * distance + outwardZ;
                glVertex3d(centerX + forwardX * 1.25, 1.15, centerZ + forwardZ * 1.25);
                glVertex3d(centerX - forwardX * 1.0, 0.7, centerZ - forwardZ * 1.0);
                glVertex3d(centerX - forwardX * 1.0, 1.6, centerZ - forwardZ * 1.0);
            }
        }
    }
    glEnd();
}

void DrawCityBuilding(double pX, double pZ, double pWidth, double pDepth, double pHeight,
                      float pRed, float pGreen, float pBlue)
{
    const double halfWidth = pWidth * 0.5;
    const double halfDepth = pDepth * 0.5;
    glColor3f(pRed, pGreen, pBlue);
    glBegin(GL_QUADS);
    glVertex3d(pX - halfWidth, 0.0, pZ - halfDepth);
    glVertex3d(pX + halfWidth, 0.0, pZ - halfDepth);
    glVertex3d(pX + halfWidth, pHeight, pZ - halfDepth);
    glVertex3d(pX - halfWidth, pHeight, pZ - halfDepth);
    glVertex3d(pX + halfWidth, 0.0, pZ - halfDepth);
    glVertex3d(pX + halfWidth, 0.0, pZ + halfDepth);
    glVertex3d(pX + halfWidth, pHeight, pZ + halfDepth);
    glVertex3d(pX + halfWidth, pHeight, pZ - halfDepth);
    glVertex3d(pX + halfWidth, pHeight, pZ + halfDepth);
    glVertex3d(pX - halfWidth, pHeight, pZ + halfDepth);
    glVertex3d(pX - halfWidth, pHeight, pZ - halfDepth);
    glVertex3d(pX + halfWidth, pHeight, pZ - halfDepth);
    glVertex3d(pX - halfWidth, pHeight, pZ - halfDepth);
    glVertex3d(pX + halfWidth, pHeight, pZ - halfDepth);
    glVertex3d(pX + halfWidth, pHeight, pZ + halfDepth);
    glVertex3d(pX - halfWidth, pHeight, pZ + halfDepth);
    glEnd();

    glColor3f(0.72f, 0.86f, 0.74f);
    glBegin(GL_LINES);
    for (double height = 4.0; height < pHeight - 2.0; height += 5.0)
    {
        glVertex3d(pX - halfWidth - 0.02, height, pZ - halfDepth);
        glVertex3d(pX + halfWidth + 0.02, height, pZ - halfDepth);
    }
    glEnd();
}

void DrawMountain(double pX, double pZ, double pRadius, double pHeight,
                  float pRed, float pGreen, float pBlue)
{
    glColor3f(pRed, pGreen, pBlue);
    glBegin(GL_TRIANGLES);
    glVertex3d(pX - pRadius, 0.0, pZ - pRadius);
    glVertex3d(pX + pRadius, 0.0, pZ - pRadius);
    glVertex3d(pX, pHeight, pZ);
    glVertex3d(pX + pRadius, 0.0, pZ - pRadius);
    glVertex3d(pX + pRadius, 0.0, pZ + pRadius);
    glVertex3d(pX, pHeight, pZ);
    glVertex3d(pX + pRadius, 0.0, pZ + pRadius);
    glVertex3d(pX - pRadius, 0.0, pZ + pRadius);
    glVertex3d(pX, pHeight, pZ);
    glVertex3d(pX - pRadius, 0.0, pZ + pRadius);
    glVertex3d(pX - pRadius, 0.0, pZ - pRadius);
    glVertex3d(pX, pHeight, pZ);
    glEnd();
}

void DrawIndustrialBeacon(double pX, double pZ, double pHeight)
{
    glColor3f(0.3f, 0.36f, 0.38f);
    glBegin(GL_QUADS);
    glVertex3d(pX - 0.9, 0.0, pZ - 0.9);
    glVertex3d(pX + 0.9, 0.0, pZ - 0.9);
    glVertex3d(pX + 0.32, pHeight, pZ - 0.32);
    glVertex3d(pX - 0.32, pHeight, pZ - 0.32);
    glVertex3d(pX + 0.9, 0.0, pZ + 0.9);
    glVertex3d(pX - 0.9, 0.0, pZ + 0.9);
    glVertex3d(pX - 0.32, pHeight, pZ + 0.32);
    glVertex3d(pX + 0.32, pHeight, pZ + 0.32);
    glEnd();
    glColor3f(1.0f, 0.68f, 0.12f);
    glBegin(GL_TRIANGLES);
    glVertex3d(pX, pHeight + 2.2, pZ);
    glVertex3d(pX - 0.65, pHeight, pZ);
    glVertex3d(pX + 0.65, pHeight, pZ);
    glEnd();
}

void DrawTrackEnvironment(const TrackDefinition& pTrack)
{
    if (pTrack.mId == "harbor-loop")
    {
        DrawCityBuilding(-220.0, 170.0, 24.0, 22.0, 42.0, 0.22f, 0.3f, 0.33f);
        DrawCityBuilding(-185.0, 220.0, 16.0, 18.0, 27.0, 0.28f, 0.36f, 0.38f);
        DrawCityBuilding(205.0, 160.0, 20.0, 24.0, 35.0, 0.18f, 0.28f, 0.32f);
        DrawCityBuilding(240.0, 215.0, 28.0, 20.0, 48.0, 0.24f, 0.34f, 0.36f);
    }
    else if (pTrack.mId == "glass-switchback")
    {
        DrawMountain(-210.0, 250.0, 45.0, 48.0, 0.22f, 0.31f, 0.28f);
        DrawMountain(235.0, 280.0, 58.0, 62.0, 0.3f, 0.38f, 0.32f);
        DrawMountain(45.0, 360.0, 68.0, 70.0, 0.18f, 0.27f, 0.26f);
    }
    else
    {
        DrawIndustrialBeacon(-185.0, 30.0, 32.0);
        DrawIndustrialBeacon(225.0, 42.0, 46.0);
        DrawIndustrialBeacon(190.0, 205.0, 38.0);
        DrawIndustrialBeacon(-145.0, 190.0, 42.0);
    }
}

void DrawFinishZone(const std::vector<RaceGate>& pWaypoints, double pTrackHalfWidth)
{
    if (pWaypoints.size() < 2 || pTrackHalfWidth <= 0.0)
        return;
    const RaceGate& finish = pWaypoints.front();
    const RaceGate& next = pWaypoints[1];
    double forwardX = next.mX - finish.mX;
    double forwardZ = next.mY - finish.mY;
    const double length = std::sqrt(forwardX * forwardX + forwardZ * forwardZ);
    if (length == 0.0)
        return;
    forwardX /= length;
    forwardZ /= length;
    const double sideX = -forwardZ;
    const double sideZ = forwardX;
    const double halfWidth = pTrackHalfWidth;
    const int columns = 8;
    const int rows = 4;
    for (int row = 0; row < rows; ++row)
    {
        const double start = -1.8 + row * 0.9;
        const double end = start + 0.9;
        for (int column = 0; column < columns; ++column)
        {
            const double left = -halfWidth + column * (halfWidth * 2.0 / columns);
            const double right = left + halfWidth * 2.0 / columns;
            const float color = (row + column) % 2 == 0 ? 0.94f : 0.04f;
            glColor3f(color, color, color);
            glBegin(GL_QUADS);
            glNormal3d(0.0, 1.0, 0.0);
            glVertex3d(finish.mX + forwardX * start + sideX * left, 0.4,
                       finish.mY + forwardZ * start + sideZ * left);
            glVertex3d(finish.mX + forwardX * end + sideX * left, 0.4,
                       finish.mY + forwardZ * end + sideZ * left);
            glVertex3d(finish.mX + forwardX * end + sideX * right, 0.4,
                       finish.mY + forwardZ * end + sideZ * right);
            glVertex3d(finish.mX + forwardX * start + sideX * right, 0.4,
                       finish.mY + forwardZ * start + sideZ * right);
            glEnd();
        }
    }
    const double bannerBottom = 4.1;
    const double bannerCellHeight = 0.5;
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < columns; ++column)
        {
            const double left = -halfWidth + column * (halfWidth * 2.0 / columns);
            const double right = left + halfWidth * 2.0 / columns;
            const double bottom = bannerBottom + row * bannerCellHeight;
            const double top = bottom + bannerCellHeight;
            const float color = (row + column) % 2 == 0 ? 0.96f : 0.03f;
            glColor3f(color, color, color);
            glBegin(GL_QUADS);
            glVertex3d(finish.mX + sideX * left - forwardX * 0.04, bottom,
                       finish.mY + sideZ * left - forwardZ * 0.04);
            glVertex3d(finish.mX + sideX * right - forwardX * 0.04, bottom,
                       finish.mY + sideZ * right - forwardZ * 0.04);
            glVertex3d(finish.mX + sideX * right - forwardX * 0.04, top,
                       finish.mY + sideZ * right - forwardZ * 0.04);
            glVertex3d(finish.mX + sideX * left - forwardX * 0.04, top,
                       finish.mY + sideZ * left - forwardZ * 0.04);
            glEnd();
        }
    }
}

void DrawGate(const RaceGate& pGate, double pDirectionX, double pDirectionZ, bool pActive,
              bool pFinish, double pTrackHalfWidth)
{
    const double length = std::sqrt(pDirectionX * pDirectionX + pDirectionZ * pDirectionZ);
    const double sideX = -pDirectionZ / length;
    const double sideZ = pDirectionX / length;
    if (pFinish)
        glColor3f(0.95f, 0.95f, 1.0f);
    else if (pActive)
        glColor3f(1.0f, 0.5f, 0.08f);
    else
        glColor3f(0.18f, 0.7f, 0.85f);
    const double forwardX = pDirectionX / length;
    const double forwardZ = pDirectionZ / length;
    const double gateHalfWidth = pTrackHalfWidth;
    const double postHeight = 3.8;
    const double postHalfWidth = 0.22;
    glBegin(GL_QUADS);
    glVertex3d(pGate.mX - forwardX * 0.28 - sideX * gateHalfWidth, 0.4,
               pGate.mY - forwardZ * 0.28 - sideZ * gateHalfWidth);
    glVertex3d(pGate.mX + forwardX * 0.28 - sideX * gateHalfWidth, 0.4,
               pGate.mY + forwardZ * 0.28 - sideZ * gateHalfWidth);
    glVertex3d(pGate.mX + forwardX * 0.28 + sideX * gateHalfWidth, 0.4,
               pGate.mY + forwardZ * 0.28 + sideZ * gateHalfWidth);
    glVertex3d(pGate.mX - forwardX * 0.28 + sideX * gateHalfWidth, 0.4,
               pGate.mY - forwardZ * 0.28 + sideZ * gateHalfWidth);
    glEnd();
    for (int side = -1; side <= 1; side += 2)
    {
        const double postX = pGate.mX + sideX * gateHalfWidth * side;
        const double postZ = pGate.mY + sideZ * gateHalfWidth * side;
        glBegin(GL_QUADS);
        glVertex3d(postX - forwardX * postHalfWidth, 0.36, postZ - forwardZ * postHalfWidth);
        glVertex3d(postX + forwardX * postHalfWidth, 0.36, postZ + forwardZ * postHalfWidth);
        glVertex3d(postX + forwardX * postHalfWidth, postHeight, postZ + forwardZ * postHalfWidth);
        glVertex3d(postX - forwardX * postHalfWidth, postHeight, postZ - forwardZ * postHalfWidth);
        glEnd();
    }
    glBegin(GL_QUADS);
    glVertex3d(pGate.mX - sideX * gateHalfWidth, postHeight - 0.32,
               pGate.mY - sideZ * gateHalfWidth);
    glVertex3d(pGate.mX + sideX * gateHalfWidth, postHeight - 0.32,
               pGate.mY + sideZ * gateHalfWidth);
    glVertex3d(pGate.mX + sideX * gateHalfWidth, postHeight,
               pGate.mY + sideZ * gateHalfWidth);
    glVertex3d(pGate.mX - sideX * gateHalfWidth, postHeight,
               pGate.mY - sideZ * gateHalfWidth);
    glEnd();
    glLineWidth(2.5f);
    glBegin(GL_LINE_LOOP);
    glVertex3d(pGate.mX - sideX * gateHalfWidth, 0.38, pGate.mY - sideZ * gateHalfWidth);
    glVertex3d(pGate.mX - sideX * gateHalfWidth, postHeight, pGate.mY - sideZ * gateHalfWidth);
    glVertex3d(pGate.mX + sideX * gateHalfWidth, postHeight, pGate.mY + sideZ * gateHalfWidth);
    glVertex3d(pGate.mX + sideX * gateHalfWidth, 0.38, pGate.mY + sideZ * gateHalfWidth);
    glEnd();
    glLineWidth(1.0f);
}

void DrawCheckpointGates(const std::vector<RaceGate>& pWaypoints,
                         const std::vector<RaceGate>& pCheckpoints,
                         int pActiveCheckpoint, double pTrackHalfWidth)
{
    if (pWaypoints.size() < 2)
        return;
    for (int checkpointIndex = 0; checkpointIndex < static_cast<int>(pCheckpoints.size()); ++checkpointIndex)
    {
        const RaceGate& checkpoint = pCheckpoints[checkpointIndex];
        int closestWaypoint = 0;
        double closestDistanceSquared = -1.0;
        for (int waypointIndex = 0; waypointIndex < static_cast<int>(pWaypoints.size()); ++waypointIndex)
        {
            const double deltaX = checkpoint.mX - pWaypoints[waypointIndex].mX;
            const double deltaZ = checkpoint.mY - pWaypoints[waypointIndex].mY;
            const double distanceSquared = deltaX * deltaX + deltaZ * deltaZ;
            if (closestDistanceSquared < 0.0 || distanceSquared < closestDistanceSquared)
            {
                closestDistanceSquared = distanceSquared;
                closestWaypoint = waypointIndex;
            }
        }
        const RaceGate& previous = pWaypoints[(closestWaypoint + pWaypoints.size() - 1)
            % pWaypoints.size()];
        DrawGate(checkpoint, checkpoint.mX - previous.mX, checkpoint.mY - previous.mY,
                 checkpointIndex == pActiveCheckpoint, false, pTrackHalfWidth);
    }
}

void DrawBoostPad(const BoostPad& pPad)
{
    const double innerRadius = pPad.mRadius * 0.55;
    glColor3f(0.08f, 0.82f, 1.0f);
    glBegin(GL_QUADS);
    glNormal3d(0.0, 1.0, 0.0);
    glVertex3d(pPad.mX - pPad.mRadius, 0.38, pPad.mY - pPad.mRadius);
    glVertex3d(pPad.mX + pPad.mRadius, 0.38, pPad.mY - pPad.mRadius);
    glVertex3d(pPad.mX + pPad.mRadius, 0.38, pPad.mY + pPad.mRadius);
    glVertex3d(pPad.mX - pPad.mRadius, 0.38, pPad.mY + pPad.mRadius);
    glEnd();
    glColor3f(0.8f, 0.98f, 1.0f);
    glBegin(GL_QUADS);
    glVertex3d(pPad.mX - innerRadius, 0.4, pPad.mY - innerRadius);
    glVertex3d(pPad.mX + innerRadius, 0.4, pPad.mY - innerRadius);
    glVertex3d(pPad.mX + innerRadius, 0.4, pPad.mY + innerRadius);
    glVertex3d(pPad.mX - innerRadius, 0.4, pPad.mY + innerRadius);
    glEnd();
}

void DrawMine(const Mine& pMine)
{
    if (pMine.mTriggered)
        return;
    glColor3f(0.1f, 0.08f, 0.06f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3d(pMine.mX, 0.62, pMine.mY);
    for (int degree = 0; degree <= 360; degree += 30)
    {
        const double angle = degree * kPi / 180.0;
        glVertex3d(pMine.mX + std::cos(angle) * pMine.mRadius,
                   0.45, pMine.mY + std::sin(angle) * pMine.mRadius);
    }
    glEnd();
    glColor3f(0.95f, 0.18f, 0.06f);
    glBegin(GL_TRIANGLES);
    for (int spike = 0; spike < 8; ++spike)
    {
        const double angle = spike * kPi * 0.25;
        glVertex3d(pMine.mX, 0.95, pMine.mY);
        glVertex3d(pMine.mX + std::cos(angle - 0.2) * pMine.mRadius * 1.25,
                   0.48, pMine.mY + std::sin(angle - 0.2) * pMine.mRadius * 1.25);
        glVertex3d(pMine.mX + std::cos(angle + 0.2) * pMine.mRadius * 1.25,
                   0.48, pMine.mY + std::sin(angle + 0.2) * pMine.mRadius * 1.25);
    }
    glEnd();
}

void DrawHazardZone(const HazardZone& pZone)
{
    const double waterHeight = 0.28;
    glColor3f(0.03f, 0.22f, 0.34f);
    glBegin(GL_QUADS);
    glNormal3d(0.0, 1.0, 0.0);
    glVertex3d(pZone.mX - pZone.mRadius, waterHeight, pZone.mY - pZone.mRadius);
    glVertex3d(pZone.mX + pZone.mRadius, waterHeight, pZone.mY - pZone.mRadius);
    glVertex3d(pZone.mX + pZone.mRadius, waterHeight, pZone.mY + pZone.mRadius);
    glVertex3d(pZone.mX - pZone.mRadius, waterHeight, pZone.mY + pZone.mRadius);
    glEnd();
    glColor3f(0.12f, 0.7f, 0.82f);
    glBegin(GL_LINE_LOOP);
    glVertex3d(pZone.mX - pZone.mRadius, waterHeight + 0.01, pZone.mY - pZone.mRadius);
    glVertex3d(pZone.mX + pZone.mRadius, waterHeight + 0.01, pZone.mY - pZone.mRadius);
    glVertex3d(pZone.mX + pZone.mRadius, waterHeight + 0.01, pZone.mY + pZone.mRadius);
    glVertex3d(pZone.mX - pZone.mRadius, waterHeight + 0.01, pZone.mY + pZone.mRadius);
    glEnd();
    glBegin(GL_LINES);
    for (double offset = -pZone.mRadius + 0.5; offset < pZone.mRadius; offset += 1.0)
    {
        glVertex3d(pZone.mX + offset - 0.24, waterHeight + 0.012, pZone.mY);
        glVertex3d(pZone.mX + offset + 0.24, waterHeight + 0.012, pZone.mY);
    }
    glEnd();
}

void DrawRaisedSection(const RaisedSection& pSection)
{
    const double baseHeight = 0.36;
    const double deckHeight = pSection.mClearHeight - 0.1;
    glPushMatrix();
    glTranslated(pSection.mX, 0.0, pSection.mY);
    glRotated(-pSection.mHeading * 180.0 / kPi, 0.0, 1.0, 0.0);
    glColor3f(0.94f, 0.76f, 0.1f);
    glBegin(GL_QUADS);
    glNormal3d(0.0, 1.0, 0.0);
    glVertex3d(-pSection.mHalfLength, deckHeight, -pSection.mHalfWidth);
    glVertex3d(pSection.mHalfLength, deckHeight, -pSection.mHalfWidth);
    glVertex3d(pSection.mHalfLength, deckHeight, pSection.mHalfWidth);
    glVertex3d(-pSection.mHalfLength, deckHeight, pSection.mHalfWidth);
    glColor3f(0.82f, 0.58f, 0.06f);
    glVertex3d(-pSection.mHalfLength, baseHeight, -pSection.mHalfWidth);
    glVertex3d(pSection.mHalfLength, baseHeight, -pSection.mHalfWidth);
    glVertex3d(pSection.mHalfLength, deckHeight, -pSection.mHalfWidth);
    glVertex3d(-pSection.mHalfLength, deckHeight, -pSection.mHalfWidth);
    glVertex3d(pSection.mHalfLength, baseHeight, pSection.mHalfWidth);
    glVertex3d(-pSection.mHalfLength, baseHeight, pSection.mHalfWidth);
    glVertex3d(-pSection.mHalfLength, deckHeight, pSection.mHalfWidth);
    glVertex3d(pSection.mHalfLength, deckHeight, pSection.mHalfWidth);
    glColor3f(0.72f, 0.48f, 0.04f);
    glVertex3d(-pSection.mHalfLength, baseHeight, pSection.mHalfWidth);
    glVertex3d(-pSection.mHalfLength, baseHeight, -pSection.mHalfWidth);
    glVertex3d(-pSection.mHalfLength, deckHeight, -pSection.mHalfWidth);
    glVertex3d(-pSection.mHalfLength, deckHeight, pSection.mHalfWidth);
    glVertex3d(pSection.mHalfLength, baseHeight, -pSection.mHalfWidth);
    glVertex3d(pSection.mHalfLength, baseHeight, pSection.mHalfWidth);
    glVertex3d(pSection.mHalfLength, deckHeight, pSection.mHalfWidth);
    glVertex3d(pSection.mHalfLength, deckHeight, -pSection.mHalfWidth);
    glColor3f(0.52f, 0.34f, 0.025f);
    glNormal3d(0.0, -1.0, 0.0);
    glVertex3d(-pSection.mHalfLength, baseHeight, pSection.mHalfWidth);
    glVertex3d(pSection.mHalfLength, baseHeight, pSection.mHalfWidth);
    glVertex3d(pSection.mHalfLength, baseHeight, -pSection.mHalfWidth);
    glVertex3d(-pSection.mHalfLength, baseHeight, -pSection.mHalfWidth);
    glEnd();
    glColor3f(0.62f, 0.42f, 0.03f);
    for (int end = -1; end <= 1; end += 2)
    {
        for (int side = -1; side <= 1; side += 2)
        {
            const double x = end * (pSection.mHalfLength - 0.28);
            const double z = side * (pSection.mHalfWidth - 0.28);
            glBegin(GL_QUADS);
            glVertex3d(x - 0.18, baseHeight, z - 0.18);
            glVertex3d(x + 0.18, baseHeight, z - 0.18);
            glVertex3d(x + 0.18, deckHeight, z - 0.18);
            glVertex3d(x - 0.18, deckHeight, z - 0.18);
            glVertex3d(x + 0.18, baseHeight, z + 0.18);
            glVertex3d(x - 0.18, baseHeight, z + 0.18);
            glVertex3d(x - 0.18, deckHeight, z + 0.18);
            glVertex3d(x + 0.18, deckHeight, z + 0.18);
            glEnd();
        }
    }
    glColor3f(0.74f, 0.84f, 0.88f);
    glBegin(GL_LINES);
    for (double x = -pSection.mHalfLength + 0.35; x < pSection.mHalfLength; x += 0.7)
    {
        glVertex3d(x, deckHeight + 0.01, -pSection.mHalfWidth);
        glVertex3d(x, deckHeight + 0.01, pSection.mHalfWidth);
    }
    glEnd();
    glPopMatrix();
}

void DrawEngineFlame(double pThrust, double pDuctZ, double pTime)
{
    const double thrust = std::fmax(0.0, std::fmin(1.0, pThrust));
    if (thrust < 0.015)
        return;
    const double flicker = 0.88 + std::sin(pTime * 17.0 + pDuctZ * 5.0) * 0.12;
    const double flameLength = (0.08 + thrust * 1.15) * flicker;

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glShadeModel(GL_SMOOTH);

    glBegin(GL_TRIANGLE_FAN);
    glColor4f(1.0f, 0.22f, 0.02f, 0.0f);
    glVertex3d(-0.88 - flameLength, 0.34, pDuctZ);
    for (int degree = 0; degree <= 360; degree += 30)
    {
        const double angle = degree * kPi / 180.0;
        glColor4f(1.0f, 0.3f, 0.025f, static_cast<float>(0.78 * thrust));
        glVertex3d(-0.88, 0.34 + std::sin(angle) * 0.24,
                   pDuctZ + std::cos(angle) * 0.25);
    }
    glEnd();

    glBegin(GL_TRIANGLE_FAN);
    glColor4f(1.0f, 0.72f, 0.15f, 0.0f);
    glVertex3d(-0.9 - flameLength * 0.72, 0.34, pDuctZ);
    for (int degree = 0; degree <= 360; degree += 30)
    {
        const double angle = degree * kPi / 180.0;
        glColor4f(0.26f, 0.76f, 1.0f, static_cast<float>(0.95 * thrust));
        glVertex3d(-0.9, 0.34 + std::sin(angle) * 0.12,
                   pDuctZ + std::cos(angle) * 0.13);
    }
    glEnd();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

void DrawCraftRaceNumber(int pNumber)
{
    static const bool segments[10][7] = {
        {true, true, true, true, true, true, false},
        {false, true, true, false, false, false, false},
        {true, true, false, true, true, false, true},
        {true, true, true, true, false, false, true},
        {false, true, true, false, false, true, true},
        {true, false, true, true, false, true, true},
        {true, false, true, true, true, true, true},
        {true, true, true, false, false, false, false},
        {true, true, true, true, true, true, true},
        {true, true, true, true, false, true, true}
    };
    const int number = pNumber >= 0 && pNumber <= 9 ? pNumber : 0;
    const double centerX = -0.65;
    const double centerZ = 0.0;
    const double segmentCenters[7][2] = {
        {0.3, 0.0}, {0.15, -0.18}, {-0.15, -0.18}, {-0.3, 0.0},
        {-0.15, 0.18}, {0.15, 0.18}, {0.0, 0.0}
    };

    glDisable(GL_LIGHTING);
    for (int layer = 0; layer < 2; ++layer)
    {
        glColor3f(layer == 0 ? 0.015f : 0.94f, layer == 0 ? 0.02f : 0.78f,
                  layer == 0 ? 0.025f : 0.18f);
        const double height = layer == 0 ? 0.172 : 0.18;
        const double thickness = layer == 0 ? 0.055 : 0.035;
        for (int segment = 0; segment < 7; ++segment)
        {
            if (!segments[number][segment])
                continue;
            const bool horizontal = segment == 0 || segment == 3 || segment == 6;
            const double x = centerX + segmentCenters[segment][0];
            const double z = centerZ + segmentCenters[segment][1];
            const double halfX = horizontal ? thickness : 0.17;
            const double halfZ = horizontal ? 0.17 : thickness;
            glBegin(GL_QUADS);
            glVertex3d(x - halfX, height, z - halfZ);
            glVertex3d(x + halfX, height, z - halfZ);
            glVertex3d(x + halfX, height, z + halfZ);
            glVertex3d(x - halfX, height, z + halfZ);
            glEnd();
        }
    }
    glEnable(GL_LIGHTING);
}

void DrawHovercraft(const HovercraftState& pState, bool pRival, bool pGhost = false,
                    CraftClass pCraftClass = CraftClass::Balanced, int pRaceNumber = 1,
                    int pOnlineColorIndex = -1)
{
    glShadeModel(GL_SMOOTH);
    const double hoverOffset = std::fmax(0.0, pState.mHeight - 1.2);
    const double shadowScale = std::fmax(0.42, 1.0 - hoverOffset * 0.5);
    const double craftScale = pRival || pGhost ? 1.0 : 1.12;
    const float shadowAlpha = static_cast<float>(std::fmax(0.1, 0.38 - hoverOffset * 0.14));
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glColor4f(0.01f, 0.02f, 0.025f, shadowAlpha);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3d(pState.mX, 0.382, pState.mY);
    for (int degree = 0; degree <= 360; degree += 15)
    {
        const double angle = degree * kPi / 180.0;
        glVertex3d(pState.mX + std::cos(angle) * 1.75 * craftScale * shadowScale, 0.382,
                   pState.mY + std::sin(angle) * 1.18 * craftScale * shadowScale);
    }
    glEnd();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    glPushMatrix();
    glTranslated(pState.mX, pState.mHeight, pState.mY);
    glRotated(-pState.mHeading * 180.0 / kPi + (pState.mReverseFacing ? 180.0 : 0.0),
              0.0, 1.0, 0.0);
    glRotated(-std::sin(pState.mHeading - pState.mTravelHeading) * 14.0, 1.0, 0.0, 0.0);
    const double lengthScale = pRival || pGhost ? craftScale : craftScale * 0.94;
    const double heightScale = pRival || pGhost ? craftScale : craftScale * 1.28;
    const double widthScale = pRival || pGhost ? craftScale : craftScale * 1.08;
    glScaled(lengthScale, heightScale, widthScale);
    const double verticalPitch = pState.mVerticalSpeed >= 0.0
        ? std::fmin(13.0, pState.mVerticalSpeed * 1.7)
        : std::fmax(-22.0, pState.mVerticalSpeed * 3.0);
    glRotated(verticalPitch, 0.0, 0.0, 1.0);
    static const float kOnlineAccentColors[][3] = {
        {0.9f, 0.08f, 0.12f}, {0.08f, 0.76f, 0.96f}, {0.98f, 0.72f, 0.1f},
        {0.84f, 0.2f, 0.9f}, {0.12f, 0.82f, 0.42f}, {1.0f, 0.4f, 0.08f},
        {0.28f, 0.44f, 1.0f}, {0.94f, 0.28f, 0.54f}
    };
    const int onlineColorCount = static_cast<int>(sizeof(kOnlineAccentColors) / sizeof(kOnlineAccentColors[0]));
    const int onlineColorIndex = pOnlineColorIndex < 0 ? 0 : pOnlineColorIndex % onlineColorCount;
    const float accentRed = pOnlineColorIndex >= 0 ? kOnlineAccentColors[onlineColorIndex][0]
        : (pGhost ? 0.12f : (pRival ? 0.16f : 0.9f));
    const float accentGreen = pOnlineColorIndex >= 0 ? kOnlineAccentColors[onlineColorIndex][1]
        : (pGhost ? 0.92f : (pRival ? 0.66f : 0.08f));
    const float accentBlue = pOnlineColorIndex >= 0 ? kOnlineAccentColors[onlineColorIndex][2]
        : (pGhost ? 0.82f : (pRival ? 0.82f : 0.12f));
    const GLfloat hullSpecular[] = {0.32f, 0.38f, 0.42f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, hullSpecular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 38.0f);
    DrawCraftRaceNumber(pRaceNumber);
    glColor3f(0.06f, 0.075f, 0.09f);
    glBegin(GL_TRIANGLE_FAN);
    glNormal3d(0.0, -1.0, 0.0);
    glVertex3d(0.0, -0.1, 0.0);
    for (int degree = 0; degree <= 360; degree += 15)
    {
        const double angle = degree * kPi / 180.0;
        glVertex3d(std::cos(angle) * 1.5, -0.1, std::sin(angle) * 1.02);
    }
    glEnd();
    glBegin(GL_QUAD_STRIP);
    for (int degree = 0; degree <= 360; degree += 15)
    {
        const double angle = degree * kPi / 180.0;
        const double skirtX = std::cos(angle) * 1.5;
        const double skirtZ = std::sin(angle) * 1.02;
        const float sideLighting = static_cast<float>(0.5 + (std::cos(angle) + 1.0) * 0.18);
        glColor3f(0.025f * sideLighting, 0.033f * sideLighting, 0.04f * sideLighting);
        glVertex3d(skirtX, -0.36, skirtZ);
        glColor3f(0.15f * sideLighting, 0.18f * sideLighting, 0.2f * sideLighting);
        glVertex3d(skirtX, -0.1, skirtZ);
    }
    glEnd();
    glBegin(GL_TRIANGLES);
    glNormal3d(0.0, 1.0, 0.0);
    glColor3f(accentRed, accentGreen, accentBlue);
    glVertex3d(1.42, 0.16, 0.0);
    glColor3f(accentRed * 0.8f, accentGreen * 0.8f, accentBlue * 0.8f);
    glVertex3d(0.48, 0.13, 0.72);
    glColor3f(accentRed * 0.5f, accentGreen * 0.5f, accentBlue * 0.5f);
    glVertex3d(-0.88, 0.08, 0.74);
    glColor3f(accentRed, accentGreen, accentBlue);
    glVertex3d(1.42, 0.16, 0.0);
    glColor3f(accentRed * 0.5f, accentGreen * 0.5f, accentBlue * 0.5f);
    glVertex3d(-0.88, 0.08, 0.74);
    glColor3f(accentRed * 0.28f, accentGreen * 0.28f, accentBlue * 0.28f);
    glVertex3d(-1.22, 0.02, 0.34);
    glColor3f(accentRed, accentGreen, accentBlue);
    glVertex3d(1.42, 0.16, 0.0);
    glColor3f(accentRed * 0.28f, accentGreen * 0.28f, accentBlue * 0.28f);
    glVertex3d(-1.22, 0.02, 0.34);
    glColor3f(accentRed * 0.22f, accentGreen * 0.22f, accentBlue * 0.22f);
    glVertex3d(-1.22, 0.02, -0.34);
    glColor3f(accentRed, accentGreen, accentBlue);
    glVertex3d(1.42, 0.16, 0.0);
    glColor3f(accentRed * 0.22f, accentGreen * 0.22f, accentBlue * 0.22f);
    glVertex3d(-1.22, 0.02, -0.34);
    glColor3f(accentRed * 0.5f, accentGreen * 0.5f, accentBlue * 0.5f);
    glVertex3d(-0.88, 0.08, -0.74);
    glColor3f(accentRed, accentGreen, accentBlue);
    glVertex3d(1.42, 0.16, 0.0);
    glColor3f(accentRed * 0.5f, accentGreen * 0.5f, accentBlue * 0.5f);
    glVertex3d(-0.88, 0.08, -0.74);
    glColor3f(accentRed * 0.8f, accentGreen * 0.8f, accentBlue * 0.8f);
    glVertex3d(0.48, 0.13, -0.72);
    glEnd();
    glColor3f(0.11f, 0.14f, 0.16f);
    glBegin(GL_QUADS);
    glNormal3d(0.0, 0.4, 0.9);
    glVertex3d(1.42, 0.16, 0.0);
    glVertex3d(-0.88, 0.08, 0.74);
    glVertex3d(-1.22, -0.12, 0.42);
    glVertex3d(0.56, -0.18, 0.78);
    glNormal3d(0.0, 0.4, -0.9);
    glVertex3d(1.42, 0.16, 0.0);
    glVertex3d(0.56, -0.18, -0.78);
    glVertex3d(-1.22, -0.12, -0.42);
    glVertex3d(-0.88, 0.08, -0.74);
    glEnd();
    glBegin(GL_TRIANGLES);
    glColor3f(accentRed * 0.5f, accentGreen * 0.5f, accentBlue * 0.5f);
    glVertex3d(1.08, 0.18, 0.0);
    glColor3f(accentRed, accentGreen, accentBlue);
    glVertex3d(0.36, 0.17, 0.58);
    glVertex3d(0.36, 0.17, -0.58);
    glEnd();
    glBegin(GL_TRIANGLE_FAN);
    glNormal3d(0.0, 1.0, 0.0);
    glColor3f(0.76f, 0.84f, 0.86f);
    glVertex3d(0.28, 0.06, 0.0);
    for (int degree = 0; degree <= 360; degree += 20)
    {
        const double angle = degree * kPi / 180.0;
        const float panelShade = static_cast<float>(0.58 + (std::cos(angle) + 1.0) * 0.16);
        glColor3f(0.82f * panelShade, 0.9f * panelShade, 0.92f * panelShade);
        glVertex3d(0.28 + std::cos(angle) * 0.68, 0.06, std::sin(angle) * 0.42);
    }
    glEnd();
    glColor3f(0.025f, 0.09f, 0.13f);
    glBegin(GL_TRIANGLE_FAN);
    glNormal3d(0.0, 1.0, 0.0);
    glVertex3d(0.24, 0.64, 0.0);
    for (int degree = 0; degree <= 360; degree += 20)
    {
        const double angle = degree * kPi / 180.0;
        glVertex3d(0.24 + std::cos(angle) * 0.42, 0.16,
                   std::sin(angle) * 0.3);
    }
    glEnd();
    glColor3f(accentRed, accentGreen, accentBlue);
    glBegin(GL_TRIANGLES);
    glVertex3d(1.62, 0.18, 0.0);
    glVertex3d(0.62, 0.1, 0.62);
    glVertex3d(0.62, 0.1, -0.62);
    glVertex3d(1.62, 0.18, 0.0);
    glVertex3d(0.76, 0.42, 0.34);
    glVertex3d(0.76, 0.42, -0.34);
    glEnd();
    glColor3f(0.8f, 0.88f, 0.9f);
    glBegin(GL_TRIANGLES);
    glVertex3d(1.46, 0.22, 0.0);
    glVertex3d(0.86, 0.16, 0.18);
    glVertex3d(0.86, 0.16, -0.18);
    glEnd();
    glColor3f(0.035f, 0.05f, 0.06f);
    glBegin(GL_TRIANGLES);
    glVertex3d(-0.66, 0.2, 0.0);
    glVertex3d(-0.42, 0.92, 0.0);
    glVertex3d(0.46, 0.32, 0.0);
    glEnd();
    glColor3f(accentRed, accentGreen, accentBlue);
    for (int side = -1; side <= 1; side += 2)
    {
        glBegin(GL_TRIANGLES);
        glVertex3d(0.92, 0.14, side * 0.38);
        glVertex3d(1.28, 0.11, side * 0.86);
        glVertex3d(0.2, 0.08, side * 0.72);
        glEnd();
    }
    glColor3f(0.09f, 0.13f, 0.15f);
    glBegin(GL_QUADS);
    glVertex3d(-0.42, 0.18, -0.26);
    glVertex3d(0.44, 0.18, -0.26);
    glVertex3d(0.26, 0.42, -0.18);
    glVertex3d(-0.24, 0.42, -0.18);
    glVertex3d(-0.42, 0.18, 0.26);
    glVertex3d(-0.24, 0.42, 0.18);
    glVertex3d(0.26, 0.42, 0.18);
    glVertex3d(0.44, 0.18, 0.26);
    glEnd();
    const GLfloat canopySpecular[] = {0.68f, 0.9f, 1.0f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, canopySpecular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 92.0f);
    glColor3f(0.04f, 0.22f, 0.28f);
    for (int latitude = 0; latitude < 5; ++latitude)
    {
        const double lower = -kPi * 0.5 + latitude * kPi / 5.0;
        const double upper = -kPi * 0.5 + (latitude + 1) * kPi / 5.0;
        glBegin(GL_QUAD_STRIP);
        for (int longitude = 0; longitude <= 10; ++longitude)
        {
            const double angle = longitude * 2.0 * kPi / 10.0;
            glVertex3d(0.06 + std::cos(angle) * std::cos(lower) * 0.34,
                       0.52 + std::sin(lower) * 0.23,
                       std::sin(angle) * std::cos(lower) * 0.26);
            glVertex3d(0.06 + std::cos(angle) * std::cos(upper) * 0.34,
                       0.52 + std::sin(upper) * 0.23,
                       std::sin(angle) * std::cos(upper) * 0.26);
        }
        glEnd();
    }
    glColor3f(0.72f, 0.78f, 0.76f);
    glBegin(GL_QUADS);
    glVertex3d(0.43, 0.42, -0.18);
    glVertex3d(0.56, 0.46, -0.14);
    glVertex3d(0.56, 0.46, 0.14);
    glVertex3d(0.43, 0.42, 0.18);
    glEnd();
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, hullSpecular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 38.0f);
    glColor3f(accentRed, accentGreen, accentBlue);
    for (int side = -1; side <= 1; side += 2)
    {
        glBegin(GL_TRIANGLES);
        glVertex3d(-0.62, 0.1, side * 0.68);
        glVertex3d(-1.38, 0.16, side * 1.02);
        glVertex3d(-1.06, 0.68, side * 0.8);
        glEnd();
    }
    for (int side = -1; side <= 1; side += 2)
    {
        const double ductZ = side * 0.8;
        glColor3f(accentRed, accentGreen, accentBlue);
        glBegin(GL_TRIANGLE_FAN);
        glVertex3d(-0.8, 0.34, ductZ);
        for (int degree = 0; degree <= 360; degree += 20)
        {
            const double angle = degree * kPi / 180.0;
            glVertex3d(-0.8, 0.34 + std::sin(angle) * 0.3,
                       ductZ + std::cos(angle) * 0.32);
        }
        glEnd();
        glColor3f(0.03f, 0.04f, 0.05f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex3d(-0.83, 0.34, ductZ);
        for (int degree = 0; degree <= 360; degree += 20)
        {
            const double angle = degree * kPi / 180.0;
            glVertex3d(-0.83, 0.34 + std::sin(angle) * 0.19,
                       ductZ + std::cos(angle) * 0.21);
        }
        glEnd();
        glDisable(GL_LIGHTING);
        glColor3f(1.0f, 0.24f, 0.04f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex3d(-0.86, 0.34, ductZ);
        for (int degree = 0; degree <= 360; degree += 30)
        {
            const double angle = degree * kPi / 180.0;
            glVertex3d(-0.87, 0.34 + std::sin(angle) * 0.1,
                       ductZ + std::cos(angle) * 0.11);
        }
        glEnd();
        glEnable(GL_LIGHTING);
        glColor3f(0.72f, 0.78f, 0.8f);
        glBegin(GL_LINES);
        for (int degree = 0; degree < 360; degree += 45)
        {
            const double angle = degree * kPi / 180.0;
            glVertex3d(-0.85, 0.34, ductZ);
            glVertex3d(-0.85, 0.34 + std::sin(angle) * 0.17,
                       ductZ + std::cos(angle) * 0.18);
        }
        glEnd();
        DrawEngineFlame(pState.mEngineThrust, ductZ, SDL_GetTicks() * 0.001);
    }
    if (pCraftClass == CraftClass::Sprint)
    {
        glColor3f(accentRed, accentGreen, accentBlue);
        glBegin(GL_TRIANGLES);
        glVertex3d(1.48, 0.2, 0.0);
        glVertex3d(0.42, 0.42, 0.0);
        glVertex3d(0.78, 1.04, 0.0);
        glEnd();
    }
    else if (pCraftClass == CraftClass::Control)
    {
        glColor3f(accentRed, accentGreen, accentBlue);
        for (int side = -1; side <= 1; side += 2)
        {
            glBegin(GL_TRIANGLES);
            glVertex3d(0.82, 0.12, side * 0.58);
            glVertex3d(0.18, 0.28, side * 1.32);
            glVertex3d(-0.42, 0.16, side * 0.7);
            glEnd();
        }
    }
    if (pState.mBoosting)
    {
        glColor3f(0.2f, 0.9f, 1.0f);
        glBegin(GL_TRIANGLES);
        glVertex3d(-1.05, 0.0, 0.5);
        glVertex3d(-2.1, 0.0, 0.0);
        glVertex3d(-1.05, 0.0, 0.1);
        glVertex3d(-1.05, 0.0, -0.1);
        glVertex3d(-2.1, 0.0, 0.0);
        glVertex3d(-1.05, 0.0, -0.5);
        glEnd();
    }
    glPopMatrix();
}

int PixelGlyphIndex(char pCharacter)
{
    if (pCharacter >= 'a' && pCharacter <= 'z')
        pCharacter = static_cast<char>(pCharacter - 'a' + 'A');
    if (pCharacter >= 'A' && pCharacter <= 'Z')
        return pCharacter - 'A';
    if (pCharacter >= '0' && pCharacter <= '9')
        return 26 + pCharacter - '0';
    if (pCharacter == '_')
        return 36;
    if (pCharacter == '-')
        return 37;
    return -1;
}

void DrawPixelText(const char* pText, int pLeft, int pTop, int pScale)
{
    static const unsigned char kGlyphs[38][7] = {
        {0x0e,0x11,0x11,0x1f,0x11,0x11,0x11},{0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e},
        {0x0f,0x10,0x10,0x10,0x10,0x10,0x0f},{0x1e,0x11,0x11,0x11,0x11,0x11,0x1e},
        {0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f},{0x1f,0x10,0x10,0x1e,0x10,0x10,0x10},
        {0x0f,0x10,0x10,0x17,0x11,0x11,0x0f},{0x11,0x11,0x11,0x1f,0x11,0x11,0x11},
        {0x1f,0x04,0x04,0x04,0x04,0x04,0x1f},{0x07,0x02,0x02,0x02,0x02,0x12,0x0c},
        {0x11,0x12,0x14,0x18,0x14,0x12,0x11},{0x10,0x10,0x10,0x10,0x10,0x10,0x1f},
        {0x11,0x1b,0x15,0x15,0x11,0x11,0x11},{0x11,0x19,0x15,0x13,0x11,0x11,0x11},
        {0x0e,0x11,0x11,0x11,0x11,0x11,0x0e},{0x1e,0x11,0x11,0x1e,0x10,0x10,0x10},
        {0x0e,0x11,0x11,0x11,0x15,0x12,0x0d},{0x1e,0x11,0x11,0x1e,0x14,0x12,0x11},
        {0x0f,0x10,0x10,0x0e,0x01,0x01,0x1e},{0x1f,0x04,0x04,0x04,0x04,0x04,0x04},
        {0x11,0x11,0x11,0x11,0x11,0x11,0x0e},{0x11,0x11,0x11,0x11,0x11,0x0a,0x04},
        {0x11,0x11,0x11,0x15,0x15,0x15,0x0a},{0x11,0x11,0x0a,0x04,0x0a,0x11,0x11},
        {0x11,0x11,0x0a,0x04,0x04,0x04,0x04},{0x1f,0x01,0x02,0x04,0x08,0x10,0x1f},
        {0x0e,0x11,0x13,0x15,0x19,0x11,0x0e},{0x04,0x0c,0x04,0x04,0x04,0x04,0x0e},
        {0x0e,0x11,0x01,0x02,0x04,0x08,0x1f},{0x1e,0x01,0x01,0x0e,0x01,0x01,0x1e},
        {0x02,0x06,0x0a,0x12,0x1f,0x02,0x02},{0x1f,0x10,0x10,0x1e,0x01,0x01,0x1e},
        {0x0e,0x10,0x10,0x1e,0x11,0x11,0x0e},{0x1f,0x01,0x02,0x04,0x08,0x08,0x08},
        {0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e},{0x0e,0x11,0x11,0x0f,0x01,0x01,0x0e},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x1f},{0x00,0x00,0x00,0x1f,0x00,0x00,0x00}
    };
    int cursorX = pLeft;
    glBegin(GL_QUADS);
    for (const char* character = pText; *character != '\0'; ++character)
    {
        const int glyphIndex = PixelGlyphIndex(*character);
        if (glyphIndex < 0)
        {
            cursorX += pScale * 4;
            continue;
        }
        for (int row = 0; row < 7; ++row)
        {
            for (int column = 0; column < 5; ++column)
            {
                if ((kGlyphs[glyphIndex][row] & (1 << (4 - column))) == 0)
                    continue;
                const int left = cursorX + column * pScale;
                const int top = pTop + row * pScale;
                glVertex2i(left, top);
                glVertex2i(left + pScale, top);
                glVertex2i(left + pScale, top + pScale);
                glVertex2i(left, top + pScale);
            }
        }
        cursorX += pScale * 6;
    }
    glEnd();
}

int PixelTextWidth(const std::string& pText, int pScale)
{
    int width = 0;
    for (char character : pText)
        width += PixelGlyphIndex(character) < 0 ? pScale * 4 : pScale * 6;
    return width;
}

void DrawSetupOverlay(int pWidth, int pHeight)
{
    glColor3f(0.2f, 0.9f, 1.0f);
    DrawPixelText("RACE STARTING", 24, 92, 3);
    glColor3f(0.82f, 0.9f, 0.92f);
    DrawPixelText("A D STEER  S BRAKE  UP JUMP", 24, 120, 2);
    DrawPixelText("SHIFT ACCEL  CTRL FIRE", 24, 138, 2);
}

void DrawResultOverlay(int pWinner, int pPlayerPosition, int pCompetitorCount,
                       bool pChampionship, const char* pChampionshipPoints,
                       double pPlayerElapsedSeconds, const LapTiming& pLapTiming,
                       int pSelection, int pWidth, int pHeight)
{
    const int panelWidth = 420;
    const int panelHeight = pChampionship ? 310 : 268;
    const int left = (pWidth - panelWidth) / 2;
    const int top = (pHeight - panelHeight) / 2 - 10;
    glColor3f(0.02f, 0.05f, 0.08f);
    glBegin(GL_QUADS);
    glVertex2i(left, top);
    glVertex2i(left + panelWidth, top);
    glVertex2i(left + panelWidth, top + panelHeight);
    glVertex2i(left, top + panelHeight);
    glEnd();
    glColor3f(pWinner == 1 ? 0.2f : 0.95f, pWinner == 1 ? 0.9f : 0.24f,
              pWinner == 1 ? 1.0f : 0.18f);
    glBegin(GL_LINE_LOOP);
    glVertex2i(left, top);
    glVertex2i(left + panelWidth, top);
    glVertex2i(left + panelWidth, top + panelHeight);
    glVertex2i(left, top + panelHeight);
    glEnd();
    if (pWinner == 1)
        DrawPixelText("FINISH", left + 138, top + 18, 5);
    else
        DrawPixelText("RIVAL FINISH", left + 78, top + 18, 4);
    char placement[32];
    std::snprintf(placement, sizeof(placement), "PLACE %d OF %d", pPlayerPosition, pCompetitorCount);
    glColor3f(0.82f, 0.9f, 0.92f);
    DrawPixelText(placement, left + 108, top + 72, 3);
    const int elapsedSeconds = static_cast<int>(pPlayerElapsedSeconds);
    char raceTime[32];
    std::snprintf(raceTime, sizeof(raceTime), "TIME %d M %d S", elapsedSeconds / 60, elapsedSeconds % 60);
    DrawPixelText(raceTime, left + 110, top + 102, 3);
    const int lastLapSeconds = static_cast<int>(pLapTiming.mLastSeconds);
    const int bestLapSeconds = static_cast<int>(pLapTiming.mBestSeconds);
    char lastLap[32];
    char bestLap[32];
    std::snprintf(lastLap, sizeof(lastLap), "LAST %d M %d S", lastLapSeconds / 60, lastLapSeconds % 60);
    std::snprintf(bestLap, sizeof(bestLap), "BEST %d M %d S", bestLapSeconds / 60, bestLapSeconds % 60);
    DrawPixelText(lastLap, left + 110, top + 132, 2);
    DrawPixelText(bestLap, left + 110, top + 154, 2);
    if (pChampionship)
    {
        glColor3f(0.2f, 0.9f, 1.0f);
        DrawPixelText("SERIES POINTS", left + 122, top + 180, 3);
        glColor3f(0.82f, 0.9f, 0.92f);
        DrawPixelText(pChampionshipPoints, left + 42, top + 210, 2);
    }
    const int actionsTop = top + (pChampionship ? 248 : 198);
    for (int action = 0; action < 2; ++action)
    {
        const int actionLeft = left + 24 + action * 194;
        const bool selected = action == pSelection;
        glColor3f(selected ? 0.12f : 0.06f, selected ? 0.52f : 0.18f,
                  selected ? 0.62f : 0.24f);
        glBegin(GL_QUADS);
        glVertex2i(actionLeft, actionsTop);
        glVertex2i(actionLeft + 178, actionsTop);
        glVertex2i(actionLeft + 178, actionsTop + 38);
        glVertex2i(actionLeft, actionsTop + 38);
        glEnd();
    }
    glColor3f(pSelection == 0 ? 1.0f : 0.82f, pSelection == 0 ? 0.82f : 0.9f,
              pSelection == 0 ? 0.22f : 0.92f);
    DrawPixelText(pChampionship ? "NEXT EVENT" : "RESTART", left + 42, actionsTop + 12, 2);
    glColor3f(pSelection == 1 ? 1.0f : 0.82f, pSelection == 1 ? 0.82f : 0.9f,
              pSelection == 1 ? 0.22f : 0.92f);
    DrawPixelText("MAIN MENU", left + 242, actionsTop + 12, 2);
}

void DrawPauseOverlay(int pSelection, int pWidth, int pHeight)
{
    const int panelWidth = 340;
    const int panelHeight = 236;
    const int left = (pWidth - panelWidth) / 2;
    const int top = (pHeight - panelHeight) / 2;
    glColor4f(0.01f, 0.025f, 0.04f, 0.9f);
    glBegin(GL_QUADS);
    glVertex2i(left, top);
    glVertex2i(left + panelWidth, top);
    glVertex2i(left + panelWidth, top + panelHeight);
    glVertex2i(left, top + panelHeight);
    glEnd();
    glColor3f(0.2f, 0.9f, 1.0f);
    DrawPixelText("PAUSED", left + 104, top + 24, 4);
    const char* actions[] = {"RESUME", "MAIN MENU", "EXIT OPENHOVER"};
    for (int action = 0; action < 3; ++action)
    {
        const int actionTop = top + 82 + action * 44;
        const bool selected = action == pSelection;
        glColor3f(selected ? 0.12f : 0.03f, selected ? 0.52f : 0.1f,
                  selected ? 0.62f : 0.14f);
        glBegin(GL_QUADS);
        glVertex2i(left + 24, actionTop);
        glVertex2i(left + panelWidth - 24, actionTop);
        glVertex2i(left + panelWidth - 24, actionTop + 34);
        glVertex2i(left + 24, actionTop + 34);
        glEnd();
        glColor3f(selected ? 1.0f : 0.82f, selected ? 0.82f : 0.9f,
                  selected ? 0.22f : 0.92f);
        DrawPixelText(actions[action], left + 66, actionTop + 10, 2);
    }
}

void DrawMenuHovercraft(int pCenterX, int pCenterY)
{
    glColor3f(0.02f, 0.06f, 0.08f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2i(pCenterX, pCenterY);
    for (int degree = 0; degree <= 360; degree += 15)
    {
        const double angle = degree * kPi / 180.0;
        glVertex2d(pCenterX + std::cos(angle) * 155.0, pCenterY + std::sin(angle) * 74.0);
    }
    glEnd();
    glColor3f(0.12f, 0.72f, 0.82f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2i(pCenterX + 18, pCenterY - 12);
    for (int degree = 0; degree <= 360; degree += 15)
    {
        const double angle = degree * kPi / 180.0;
        glVertex2d(pCenterX + 18 + std::cos(angle) * 106.0,
                   pCenterY - 12 + std::sin(angle) * 48.0);
    }
    glEnd();
    glColor3f(0.96f, 0.66f, 0.16f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2i(pCenterX + 30, pCenterY - 26);
    for (int degree = 0; degree <= 360; degree += 15)
    {
        const double angle = degree * kPi / 180.0;
        glVertex2d(pCenterX + 30 + std::cos(angle) * 47.0,
                   pCenterY - 26 + std::sin(angle) * 27.0);
    }
    glEnd();
    glColor3f(0.03f, 0.11f, 0.15f);
    glBegin(GL_TRIANGLES);
    glVertex2i(pCenterX - 150, pCenterY + 6);
    glVertex2i(pCenterX - 205, pCenterY + 42);
    glVertex2i(pCenterX - 126, pCenterY + 36);
    glVertex2i(pCenterX + 136, pCenterY + 10);
    glVertex2i(pCenterX + 202, pCenterY + 44);
    glVertex2i(pCenterX + 112, pCenterY + 39);
    glEnd();
    glColor3f(0.2f, 0.9f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2i(pCenterX - 66, pCenterY + 70);
    glVertex2i(pCenterX + 42, pCenterY + 70);
    glVertex2i(pCenterX + 72, pCenterY + 95);
    glVertex2i(pCenterX - 94, pCenterY + 95);
    glEnd();
}

void DrawMissile(const Missile& pMissile)
{
    if (!pMissile.Active())
        return;
    const HovercraftState& state = pMissile.State();
    glPushMatrix();
    glTranslated(state.mX, state.mHeight, state.mY);
    glRotated(-state.mTravelHeading * 180.0 / kPi, 0.0, 1.0, 0.0);

    glColor3f(0.16f, 0.68f, 0.58f);
    glBegin(GL_QUAD_STRIP);
    for (int degree = 0; degree <= 360; degree += 45)
    {
        const double angle = degree * kPi / 180.0;
        const double vertical = std::sin(angle) * 0.13;
        const double sideways = std::cos(angle) * 0.13;
        glVertex3d(-0.4, vertical, sideways);
        glVertex3d(0.3, vertical, sideways);
    }
    glEnd();

    glColor3f(0.7f, 0.88f, 0.78f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3d(0.62, 0.0, 0.0);
    for (int degree = 0; degree <= 360; degree += 45)
    {
        const double angle = degree * kPi / 180.0;
        glVertex3d(0.3, std::sin(angle) * 0.13, std::cos(angle) * 0.13);
    }
    glEnd();

    glColor3f(0.09f, 0.13f, 0.14f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3d(-0.43, 0.0, 0.0);
    for (int degree = 0; degree <= 360; degree += 45)
    {
        const double angle = degree * kPi / 180.0;
        glVertex3d(-0.4, std::sin(angle) * 0.09, std::cos(angle) * 0.09);
    }
    glEnd();

    glColor3f(0.92f, 0.3f, 0.16f);
    for (int fin = 0; fin < 4; ++fin)
    {
        const double angle = fin * kPi * 0.5;
        const double vertical = std::sin(angle);
        const double sideways = std::cos(angle);
        glBegin(GL_TRIANGLES);
        glVertex3d(-0.28, vertical * 0.11, sideways * 0.11);
        glVertex3d(-0.52, vertical * 0.34, sideways * 0.34);
        glVertex3d(0.08, vertical * 0.12, sideways * 0.12);
        glEnd();
    }

    const double exhaustLength = 0.22 + std::fmod(state.mSpeed * 0.013, 0.16);
    glDisable(GL_LIGHTING);
    glColor3f(1.0f, 0.7f, 0.12f);
    glBegin(GL_TRIANGLES);
    glVertex3d(-0.4, -0.08, 0.0);
    glVertex3d(-0.4, 0.08, 0.0);
    glVertex3d(-0.4 - exhaustLength, 0.0, 0.0);
    glEnd();
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

void DrawLobbyPanel(int pLeft, int pTop, int pWidth, int pHeight)
{
    glColor3f(0.105f, 0.105f, 0.13f);
    glBegin(GL_QUADS);
    glVertex2i(pLeft, pTop);
    glVertex2i(pLeft + pWidth, pTop);
    glVertex2i(pLeft + pWidth, pTop + pHeight);
    glVertex2i(pLeft, pTop + pHeight);
    glEnd();
    glColor3f(0.29f, 0.29f, 0.35f);
    glBegin(GL_LINE_LOOP);
    glVertex2i(pLeft, pTop);
    glVertex2i(pLeft + pWidth, pTop);
    glVertex2i(pLeft + pWidth, pTop + pHeight);
    glVertex2i(pLeft, pTop + pHeight);
    glEnd();
}

void DrawLobbyButton(const char* pLabel, int pLeft, int pTop, int pWidth, bool pPrimary)
{
    glColor3f(pPrimary ? 0.68f : 0.24f, pPrimary ? 0.14f : 0.24f, pPrimary ? 0.21f : 0.31f);
    glBegin(GL_QUADS);
    glVertex2i(pLeft, pTop);
    glVertex2i(pLeft + pWidth, pTop);
    glVertex2i(pLeft + pWidth, pTop + 42);
    glVertex2i(pLeft, pTop + 42);
    glEnd();
    glColor3f(0.92f, 0.88f, 0.92f);
    DrawPixelText(pLabel, pLeft + 18, pTop + 13, 2);
}

void DrawFrontScreen(FrontScreen pScreen, int pSelection, int pCameraDistanceSetting,
                     bool pAudioEnabled, int pTrackIndex, int pLaps, int pRivalCount,
                     RivalDifficulty pRivalDifficulty, RaceMode pRaceMode, bool pWeaponsAllowed,
                     int pWidth, int pHeight)
{
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, pWidth, pHeight, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(0.015f, 0.04f, 0.055f);
    glBegin(GL_QUADS);
    glVertex2i(0, 0);
    glVertex2i(pWidth, 0);
    glVertex2i(pWidth, pHeight);
    glVertex2i(0, pHeight);
    glEnd();
    glColor3f(0.08f, 0.28f, 0.34f);
    glBegin(GL_LINES);
    for (int x = -pWidth; x <= pWidth * 2; x += 72)
    {
        glVertex2i(pWidth / 2, pHeight / 2 + 90);
        glVertex2i(x, pHeight);
    }
    for (int y = pHeight / 2 + 100; y < pHeight; y += 44)
    {
        glVertex2i(0, y);
        glVertex2i(pWidth, y);
    }
    glEnd();

    if (pScreen == FrontScreen::Welcome)
    {
        glColor3f(0.2f, 0.9f, 1.0f);
        DrawPixelText("OPENHOVER", 62, 62, 7);
        glColor3f(1.0f, 0.78f, 0.12f);
        DrawPixelText("ORIGINAL HOVER RACING", 64, 132, 3);
        DrawMenuHovercraft(pWidth / 4, pHeight / 2 + 125);

        const char* options[] = {"PLAY LOCAL GAME", "MULTIPLAYER", "HOW TO PLAY", "SETTINGS", "QUIT"};
        const int panelLeft = pWidth / 2 - 200;
        const int panelTop = 145;
        for (int option = 0; option < 5; ++option)
        {
            const int top = panelTop + option * 72;
            const bool selected = option == pSelection;
            glColor3f(selected ? 0.12f : 0.03f, selected ? 0.52f : 0.1f,
                      selected ? 0.62f : 0.14f);
            glBegin(GL_QUADS);
            glVertex2i(panelLeft, top);
            glVertex2i(panelLeft + 400, top);
            glVertex2i(panelLeft + 400, top + 62);
            glVertex2i(panelLeft, top + 62);
            glEnd();
            glColor3f(selected ? 1.0f : 0.56f, selected ? 0.82f : 0.72f,
                      selected ? 0.22f : 0.76f);
            DrawPixelText(options[option], panelLeft + 68, top + 20, 3);
        }
        glColor3f(0.72f, 0.82f, 0.84f);
        DrawPixelText("UP DOWN TO SELECT", pWidth / 2 - 132, pHeight - 82, 3);
        DrawPixelText("ENTER TO CONFIRM", pWidth / 2 - 120, pHeight - 52, 3);
    }
    else if (pScreen == FrontScreen::Multiplayer)
    {
        glColor3f(0.075f, 0.075f, 0.095f);
        glBegin(GL_QUADS);
        glVertex2i(0, 0);
        glVertex2i(pWidth, 0);
        glVertex2i(pWidth, pHeight);
        glVertex2i(0, pHeight);
        glEnd();
        const int margin = 18;
        const int gap = 12;
        const int top = 16;
        const int leftWidth = pWidth * 29 / 100;
        const int actionWidth = pWidth * 15 / 100;
        const int detailWidth = pWidth - margin * 2 - gap * 2 - leftWidth - actionWidth;
        const int topHeight = pHeight * 42 / 100;
        const int bottomTop = top + topHeight + gap;
        const int bottomHeight = pHeight - bottomTop - margin;
        const int detailLeft = margin + leftWidth + gap;
        const int actionLeft = detailLeft + detailWidth + gap;

        DrawLobbyPanel(margin, top, leftWidth, topHeight);
        DrawLobbyPanel(detailLeft, top, detailWidth, topHeight);
        DrawLobbyPanel(actionLeft, top, actionWidth, topHeight);
        DrawLobbyPanel(margin, bottomTop, leftWidth, bottomHeight);
        DrawLobbyPanel(detailLeft, bottomTop, detailWidth + gap + actionWidth, bottomHeight);

        glColor3f(0.95f, 0.35f, 0.4f);
        DrawPixelText("GAME LIST", margin + 16, top + 18, 2);
        DrawPixelText("GAME DETAILS", detailLeft + 16, top + 18, 2);
        DrawPixelText("USERS LIST (2)", margin + 16, bottomTop + 18, 2);
        DrawPixelText("CHAT", detailLeft + 16, bottomTop + 18, 2);

        glColor3f(0.22f, 0.22f, 0.27f);
        glBegin(GL_LINES);
        glVertex2i(margin + 16, top + 44);
        glVertex2i(margin + leftWidth - 16, top + 44);
        glVertex2i(detailLeft + 16, top + 44);
        glVertex2i(detailLeft + detailWidth - 16, top + 44);
        glVertex2i(margin + 16, bottomTop + 44);
        glVertex2i(margin + leftWidth - 16, bottomTop + 44);
        glVertex2i(detailLeft + 16, bottomTop + 44);
        glVertex2i(pWidth - margin - 16, bottomTop + 44);
        glEnd();

        glColor3f(0.065f, 0.065f, 0.085f);
        glBegin(GL_QUADS);
        glVertex2i(margin + 16, top + 54);
        glVertex2i(margin + leftWidth - 16, top + 54);
        glVertex2i(margin + leftWidth - 16, top + topHeight - 16);
        glVertex2i(margin + 16, top + topHeight - 16);
        glVertex2i(margin + 16, bottomTop + 56);
        glVertex2i(margin + leftWidth - 16, bottomTop + 56);
        glVertex2i(margin + leftWidth - 16, bottomTop + bottomHeight - 16);
        glVertex2i(margin + 16, bottomTop + bottomHeight - 16);
        glEnd();
        if (gLobbyRooms.empty())
        {
            glColor3f(0.62f, 0.62f, 0.68f);
            DrawPixelText("NO OPEN RACES - HOST", margin + 26, top + 70, 2);
            DrawPixelText("ONE TO GET STARTED", margin + 26, top + 94, 2);
        }
        else
        {
            for (int roomIndex = 0; roomIndex < static_cast<int>(gLobbyRooms.size()); ++roomIndex)
            {
                const int rowTop = top + 58 + roomIndex * 38;
                if (rowTop + 30 >= top + topHeight - 16)
                    break;
                const LobbyRoomView& room = gLobbyRooms[roomIndex];
                glColor3f(roomIndex == gLobbySelectedRoom ? 0.18f : 0.09f,
                          roomIndex == gLobbySelectedRoom ? 0.36f : 0.09f,
                          roomIndex == gLobbySelectedRoom ? 0.42f : 0.12f);
                glBegin(GL_QUADS);
                glVertex2i(margin + 20, rowTop);
                glVertex2i(margin + leftWidth - 20, rowTop);
                glVertex2i(margin + leftWidth - 20, rowTop + 30);
                glVertex2i(margin + 20, rowTop + 30);
                glEnd();
                glColor3f(0.9f, 0.9f, 0.94f);
                DrawPixelText(room.mName.c_str(), margin + 28, rowTop + 9, 2);
            }
        }
        for (int playerIndex = 0; playerIndex < static_cast<int>(gLobbyPlayers.size()); ++playerIndex)
        {
            const int playerTop = bottomTop + 74 + playerIndex * 24;
            if (playerTop + 16 >= bottomTop + bottomHeight - 16)
                break;
            glColor3f(playerIndex == 0 ? 0.95f : 0.76f, playerIndex == 0 ? 0.35f : 0.76f,
                      playerIndex == 0 ? 0.4f : 0.82f);
            DrawPixelText(gLobbyPlayers[playerIndex].mDisplayName.c_str(), margin + 26, playerTop, 2);
        }

        const int previewLeft = detailLeft + 18;
        const int previewTop = top + 56;
        const int previewSize = std::min(detailWidth * 34 / 100, topHeight - 78);
        glColor3f(0.06f, 0.06f, 0.08f);
        glBegin(GL_QUADS);
        glVertex2i(previewLeft, previewTop);
        glVertex2i(previewLeft + previewSize, previewTop);
        glVertex2i(previewLeft + previewSize, previewTop + previewSize);
        glVertex2i(previewLeft, previewTop + previewSize);
        glEnd();
        glColor3f(0.5f, 0.5f, 0.58f);
        glBegin(GL_LINE_LOOP);
        glVertex2i(previewLeft, previewTop);
        glVertex2i(previewLeft + previewSize, previewTop);
        glVertex2i(previewLeft + previewSize, previewTop + previewSize);
        glVertex2i(previewLeft, previewTop + previewSize);
        glEnd();
        DrawPixelText("PREVIEW UNAVAILABLE", previewLeft + 10, previewTop + previewSize / 2, 2);
        glColor3f(0.66f, 0.66f, 0.73f);
        if (gLobbySelectedRoom >= 0 && gLobbySelectedRoom < static_cast<int>(gLobbyRooms.size()))
        {
            const LobbyRoomView& room = gLobbyRooms[gLobbySelectedRoom];
            const char* trackNames[] = {"HARBOR LOOP", "GLASS SWITCHBACK", "VELOCITY RING"};
            const int validTrackIndex = room.mTrackIndex >= 0 && room.mTrackIndex < 3 ? room.mTrackIndex : 0;
            const std::string players = std::to_string(room.mPlayerCount) + " OF "
                + std::to_string(room.mPlayerCapacity) + " PLAYERS";
            DrawPixelText(room.mName.c_str(), previewLeft + previewSize + 16, previewTop + 4, 2);
            DrawPixelText(players.c_str(), previewLeft + previewSize + 16, previewTop + 28, 2);
            DrawPixelText(trackNames[validTrackIndex], previewLeft + previewSize + 16, previewTop + 52, 2);
            const std::string settings = std::to_string(room.mLapCount) + " LAPS  "
                + std::to_string(room.mRivalCount) + " RIVALS  "
                + (room.mWeaponsAllowed ? "WEAPONS ON" : "WEAPONS OFF");
            DrawPixelText(settings.c_str(), previewLeft + previewSize + 16, previewTop + 76, 2);
            DrawPixelText(room.mRaceRunning ? "RACE IN PROGRESS" : "WAITING FOR HOST",
                          previewLeft + previewSize + 16, previewTop + 100, 2);
        }
        else
        {
            DrawPixelText("SELECT A RACE IN THE LIST", previewLeft + previewSize + 16, previewTop + 4, 2);
            DrawPixelText("TO SEE ITS TRACK LAPS", previewLeft + previewSize + 16, previewTop + 28, 2);
            DrawPixelText("WEAPONS AND PLAYERS", previewLeft + previewSize + 16, previewTop + 52, 2);
        }

        const bool selectedRoomIsHosted = gLobbySelectedRoom >= 0
            && gLobbySelectedRoom < static_cast<int>(gLobbyRooms.size())
            && gLobbyRooms[gLobbySelectedRoom].mHostId == gLobbyPlayerId;
        const bool selectedRoomIsJoined = gLobbySelectedRoom >= 0
            && gLobbySelectedRoom < static_cast<int>(gLobbyRooms.size())
            && gLobbyRooms[gLobbySelectedRoom].mId == gLobbyJoinedRoomId;
        DrawLobbyButton(selectedRoomIsHosted ? "START RACE" : selectedRoomIsJoined ? "LEAVE ROOM" : "JOIN GAME",
                        actionLeft + 16, top + 16, actionWidth - 32, true);
        DrawLobbyButton("HOST RACE", actionLeft + 16, top + 72, actionWidth - 32, false);
        glColor3f(0.22f, 0.22f, 0.27f);
        glBegin(GL_LINES);
        glVertex2i(actionLeft + 16, top + 132);
        glVertex2i(actionLeft + actionWidth - 16, top + 132);
        glEnd();
        glColor3f(0.95f, 0.35f, 0.4f);
        DrawPixelText(gLobbyStatus.c_str(), actionLeft + 16, top + 158, 2);
        DrawLobbyButton("BACK TO MENU", actionLeft + 16, top + topHeight - 58, actionWidth - 32, false);

        const int chatLeft = detailLeft + 16;
        const int chatTop = bottomTop + 56;
        const int chatWidth = detailWidth + gap + actionWidth - 32;
        const int inputTop = bottomTop + bottomHeight - 46;
        glColor3f(0.08f, 0.08f, 0.1f);
        glBegin(GL_QUADS);
        glVertex2i(chatLeft, chatTop);
        glVertex2i(chatLeft + chatWidth, chatTop);
        glVertex2i(chatLeft + chatWidth, inputTop - 10);
        glVertex2i(chatLeft, inputTop - 10);
        glVertex2i(chatLeft, inputTop);
        glVertex2i(chatLeft + chatWidth - 86, inputTop);
        glVertex2i(chatLeft + chatWidth - 86, inputTop + 32);
        glVertex2i(chatLeft, inputTop + 32);
        glEnd();
        glColor3f(0.3f, 0.3f, 0.36f);
        glBegin(GL_LINE_LOOP);
        glVertex2i(chatLeft, chatTop);
        glVertex2i(chatLeft + chatWidth, chatTop);
        glVertex2i(chatLeft + chatWidth, inputTop - 10);
        glVertex2i(chatLeft, inputTop - 10);
        glEnd();
        const int firstMessage = std::max(0, static_cast<int>(gLobbyChatMessages.size()) - 8);
        glColor3f(0.76f, 0.76f, 0.82f);
        for (int messageIndex = firstMessage; messageIndex < static_cast<int>(gLobbyChatMessages.size()); ++messageIndex)
            DrawPixelText(gLobbyChatMessages[messageIndex].c_str(), chatLeft + 12,
                          chatTop + 12 + (messageIndex - firstMessage) * 22, 2);
        const int inputTextLeft = chatLeft + 12;
        const int inputTextWidth = chatWidth - 98;
        const int maximumVisibleCharacters = std::max(1, inputTextWidth / 12);
        const std::string visibleChatInput = gLobbyChatInput.size()
            > static_cast<std::size_t>(maximumVisibleCharacters)
            ? gLobbyChatInput.substr(gLobbyChatInput.size() - maximumVisibleCharacters) : gLobbyChatInput;
        glColor3f(gLobbyChatInputFocused ? 0.82f : 0.62f,
                  gLobbyChatInputFocused ? 0.9f : 0.62f,
                  gLobbyChatInputFocused ? 0.92f : 0.68f);
        DrawPixelText(visibleChatInput.c_str(), inputTextLeft, inputTop + 9, 2);
        if (gLobbyChatInputFocused)
        {
            glColor3f(0.2f, 0.9f, 1.0f);
            glBegin(GL_LINE_LOOP);
            glVertex2i(chatLeft, inputTop);
            glVertex2i(chatLeft + chatWidth - 86, inputTop);
            glVertex2i(chatLeft + chatWidth - 86, inputTop + 32);
            glVertex2i(chatLeft, inputTop + 32);
            glEnd();
            if ((SDL_GetTicks() / 500) % 2 == 0)
            {
                const int caretX = inputTextLeft + PixelTextWidth(visibleChatInput, 2);
                glBegin(GL_QUADS);
                glVertex2i(caretX, inputTop + 7);
                glVertex2i(caretX + 2, inputTop + 7);
                glVertex2i(caretX + 2, inputTop + 24);
                glVertex2i(caretX, inputTop + 24);
                glEnd();
            }
        }
        DrawLobbyButton("SEND", chatLeft + chatWidth - 76, inputTop - 5, 76, true);
    }
    else if (pScreen == FrontScreen::HostRaceSetup)
    {
        const char* trackNames[] = {"HARBOR LOOP", "GLASS SWITCHBACK", "VELOCITY RING"};
        const char* modeNames[] = {"SINGLE RACE", "TIME TRIAL", "PRACTICE", "CHAMPIONSHIP"};
        const char* labels[] = {"MODE", "TRACK", "LAPS", "MAX PLAYERS", "RIVALS", "WEAPONS", "HOST RACE", "BACK"};
        glColor3f(0.075f, 0.075f, 0.095f);
        glBegin(GL_QUADS);
        glVertex2i(0, 0);
        glVertex2i(pWidth, 0);
        glVertex2i(pWidth, pHeight);
        glVertex2i(0, pHeight);
        glEnd();
        const int panelLeft = pWidth / 2 - 280;
        const int panelTop = 72;
        glColor3f(0.105f, 0.105f, 0.13f);
        glBegin(GL_QUADS);
        glVertex2i(panelLeft, panelTop);
        glVertex2i(panelLeft + 560, panelTop);
        glVertex2i(panelLeft + 560, panelTop + 492);
        glVertex2i(panelLeft, panelTop + 492);
        glEnd();
        glColor3f(0.29f, 0.29f, 0.35f);
        glBegin(GL_LINE_LOOP);
        glVertex2i(panelLeft, panelTop);
        glVertex2i(panelLeft + 560, panelTop);
        glVertex2i(panelLeft + 560, panelTop + 492);
        glVertex2i(panelLeft, panelTop + 492);
        glEnd();
        glColor3f(0.95f, 0.35f, 0.4f);
        DrawPixelText("HOST RACE", panelLeft + 190, panelTop + 24, 5);
        for (int option = 0; option < 8; ++option)
        {
            const int rowTop = panelTop + 82 + option * 46;
            const bool selected = option == pSelection;
            glColor3f(selected ? 0.18f : 0.08f, selected ? 0.38f : 0.08f,
                      selected ? 0.44f : 0.1f);
            glBegin(GL_QUADS);
            glVertex2i(panelLeft + 18, rowTop);
            glVertex2i(panelLeft + 542, rowTop);
            glVertex2i(panelLeft + 542, rowTop + 36);
            glVertex2i(panelLeft + 18, rowTop + 36);
            glEnd();
            glColor3f(selected ? 1.0f : 0.76f, selected ? 0.82f : 0.76f,
                      selected ? 0.22f : 0.82f);
            DrawPixelText(labels[option], panelLeft + 36, rowTop + 10, 2);
            std::string value;
            if (option == 0)
                value = modeNames[gHostRaceMode];
            else if (option == 1)
                value = trackNames[gHostTrackIndex];
            else if (option == 2)
                value = std::to_string(gHostLapCount);
            else if (option == 3)
                value = std::to_string(gHostPlayerCapacity);
            else if (option == 4)
                value = std::to_string(gHostRivalCount);
            else if (option == 5)
                value = gHostWeaponsAllowed ? "ALLOWED" : "OFF";
            else if (option == 6)
                value = "CREATE ROOM";
            else
                value = "RETURN TO LOBBY";
            DrawPixelText(value.c_str(), panelLeft + 300, rowTop + 10, 2);
        }
        glColor3f(0.76f, 0.76f, 0.82f);
        DrawPixelText("UP DOWN SELECT  LEFT RIGHT CHANGE", panelLeft + 76, panelTop + 454, 2);
    }
    else if (pScreen == FrontScreen::HowToPlay)
    {
        glColor3f(0.2f, 0.9f, 1.0f);
        DrawPixelText("HOW TO PLAY", pWidth / 2 - 165, 70, 5);
        glColor3f(0.82f, 0.9f, 0.92f);
        DrawPixelText("SHIFT ACCEL", pWidth / 2 - 98, 190, 3);
        DrawPixelText("A D STEER", pWidth / 2 - 72, 270, 3);
        DrawPixelText("S BRAKE", pWidth / 2 - 60, 310, 3);
        DrawPixelText("UP JUMP", pWidth / 2 - 60, 350, 3);
        DrawPixelText("CTRL FIRE", pWidth / 2 - 78, 390, 3);
        DrawPixelText("FOLLOW THE CYAN FLOW MARKERS", pWidth / 2 - 225, 410, 3);
        glColor3f(1.0f, 0.78f, 0.12f);
        DrawPixelText("ENTER TO RETURN", pWidth / 2 - 120, pHeight - 70, 3);
    }
    else if (pScreen == FrontScreen::LocalSetup)
    {
        const char* trackNames[] = {"HARBOR LOOP", "GLASS SWITCHBACK", "VELOCITY RING"};
        const char* labels[] = {"START RACE", "MODE", "TRACK", "LAPS", "RIVALS", "DIFFICULTY", "WEAPONS", "BACK"};
        glColor3f(0.2f, 0.9f, 1.0f);
        DrawPixelText("LOCAL RACE", pWidth / 2 - 150, 70, 5);
        for (int option = 0; option < 8; ++option)
        {
            const int top = 120 + option * 48;
            const bool selected = option == pSelection;
            glColor3f(selected ? 0.12f : 0.03f, selected ? 0.52f : 0.1f,
                      selected ? 0.62f : 0.14f);
            glBegin(GL_QUADS);
            glVertex2i(pWidth / 2 - 250, top);
            glVertex2i(pWidth / 2 + 250, top);
            glVertex2i(pWidth / 2 + 250, top + 52);
            glVertex2i(pWidth / 2 - 250, top + 52);
            glEnd();
            glColor3f(selected ? 1.0f : 0.72f, selected ? 0.82f : 0.86f,
                      selected ? 0.22f : 0.9f);
            DrawPixelText(labels[option], pWidth / 2 - 218, top + 16, 3);
            if (option == 1)
                DrawPixelText(RaceModeSetupLabel(pRaceMode), pWidth / 2 + 18, top + 19, 2);
            else if (option == 2)
                DrawPixelText(trackNames[pTrackIndex], pWidth / 2 + 10, top + 16, 3);
            else if (option == 3)
            {
                char laps[16];
                std::snprintf(laps, sizeof(laps), "%d", pLaps);
                DrawPixelText(laps, pWidth / 2 + 180, top + 16, 3);
            }
            else if (option == 4)
            {
                char rivals[16];
                std::snprintf(rivals, sizeof(rivals), "%d", pRivalCount);
                DrawPixelText(rivals, pWidth / 2 + 180, top + 16, 3);
            }
            else if (option == 5)
                DrawPixelText(RivalDifficultySetupLabel(pRivalDifficulty), pWidth / 2 + 18, top + 19, 2);
            else if (option == 6)
                DrawPixelText(pWeaponsAllowed ? "ALLOWED" : "OFF", pWidth / 2 + 120, top + 16, 3);
        }
        glColor3f(0.72f, 0.82f, 0.84f);
        DrawPixelText("UP DOWN SELECT  LEFT RIGHT CHANGE", pWidth / 2 - 230, pHeight - 94, 2);
        glColor3f(1.0f, 0.78f, 0.12f);
        DrawPixelText("ENTER CONFIRM  ESC BACK", pWidth / 2 - 168, pHeight - 62, 2);
    }
    else if (pScreen == FrontScreen::DisplayNameSetup)
    {
        const int panelWidth = 520;
        const int panelLeft = (pWidth - panelWidth) / 2;
        const int panelTop = pHeight / 2 - 140;
        glColor3f(0.07f, 0.09f, 0.12f);
        glBegin(GL_QUADS);
        glVertex2i(panelLeft, panelTop);
        glVertex2i(panelLeft + panelWidth, panelTop);
        glVertex2i(panelLeft + panelWidth, panelTop + 280);
        glVertex2i(panelLeft, panelTop + 280);
        glEnd();
        glColor3f(0.2f, 0.9f, 1.0f);
        DrawPixelText("PILOT NAME", panelLeft + 142, panelTop + 38, 4);
        glColor3f(0.7f, 0.78f, 0.82f);
        DrawPixelText("CHOOSE YOUR LOBBY ID", panelLeft + 118, panelTop + 84, 2);
        glColor3f(0.03f, 0.04f, 0.06f);
        glBegin(GL_QUADS);
        glVertex2i(panelLeft + 42, panelTop + 116);
        glVertex2i(panelLeft + panelWidth - 42, panelTop + 116);
        glVertex2i(panelLeft + panelWidth - 42, panelTop + 164);
        glVertex2i(panelLeft + 42, panelTop + 164);
        glEnd();
        glColor3f(1.0f, 0.78f, 0.12f);
        DrawPixelText(gPlayerDisplayName.empty() ? "TYPE A NAME" : gPlayerDisplayName.c_str(),
                      panelLeft + 58, panelTop + 132, 3);
        DrawLobbyButton("SAVE", panelLeft + 172, panelTop + 196, 176, true);
        glColor3f(0.7f, 0.78f, 0.82f);
        DrawPixelText("LETTERS NUMBERS _ -", panelLeft + 150, panelTop + 254, 2);
    }
    else
    {
        glColor3f(0.2f, 0.9f, 1.0f);
        DrawPixelText("SETTINGS", pWidth / 2 - 120, 90, 5);
        glColor3f(pSelection == 0 ? 1.0f : 0.82f, pSelection == 0 ? 0.78f : 0.9f,
                  pSelection == 0 ? 0.12f : 0.92f);
        DrawPixelText("DISPLAY NAME", pWidth / 2 - 105, 160, 3);
        DrawPixelText(gPlayerDisplayName.empty() ? "NOT SET" : gPlayerDisplayName.c_str(),
                      pWidth / 2 - 105, 200, 3);
        glColor3f(pSelection == 1 ? 1.0f : 0.82f, pSelection == 1 ? 0.78f : 0.9f,
                  pSelection == 1 ? 0.12f : 0.92f);
        DrawPixelText("CAMERA DISTANCE", pWidth / 2 - 135, 245, 3);
        const char* cameraOptions[] = {"NEAR", "STANDARD", "FAR"};
        for (int option = 0; option < 3; ++option)
        {
            if (option == pCameraDistanceSetting)
                glColor3f(1.0f, 0.78f, 0.12f);
            else
                glColor3f(0.82f, 0.9f, 0.92f);
            DrawPixelText(cameraOptions[option], pWidth / 2 - 128 + option * 104, 285, 3);
        }
        glColor3f(pSelection == 2 ? 1.0f : 0.82f, pSelection == 2 ? 0.78f : 0.9f,
                  pSelection == 2 ? 0.12f : 0.92f);
        DrawPixelText("AUDIO FEEDBACK", pWidth / 2 - 120, 375, 3);
        DrawPixelText(pAudioEnabled ? "ON" : "OFF", pWidth / 2 - 24, 420, 3);
        glColor3f(pSelection == 3 ? 1.0f : 0.82f, pSelection == 3 ? 0.78f : 0.9f,
                  pSelection == 3 ? 0.12f : 0.92f);
        DrawPixelText("BACK", pWidth / 2 - 42, 505, 3);
        glColor3f(0.82f, 0.9f, 0.92f);
        DrawPixelText("UP DOWN SELECT  LEFT RIGHT CHANGE", pWidth / 2 - 230, pHeight - 94, 2);
        glColor3f(1.0f, 0.78f, 0.12f);
        DrawPixelText("ENTER CONFIRM  ESC BACK", pWidth / 2 - 168, pHeight - 62, 2);
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FOG);
    glEnable(GL_LIGHTING);
}

void DrawHud(const RaceProgress& pPlayerProgress, int pTargetLaps,
             const std::vector<RaceProgress>& pRivalProgresses,
             const HovercraftState& pPlayerState, const std::vector<HovercraftState>& pRivalStates,
             const RaceGate& pActiveGate, const std::vector<RaceGate>& pWaypoints,
             bool pShowRivals, bool pWrongWay, int pWinner,
             int pPlayerPosition, int pCompetitorCount, bool pChampionship, int pStartLights,
             bool pShowSetupOverlay, bool pShowResultOverlay, const char* pChampionshipPoints,
             double pPlayerElapsedSeconds, const LapTiming& pLapTiming, const Missile& pMissile,
             bool pWeaponsAllowed, bool pPauseMenuOpen, int pPauseMenuSelection,
             int pResultSelection, int pWidth, int pHeight)
{
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, pWidth, pHeight, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    const int speed = static_cast<int>(std::fabs(pPlayerState.mSpeed));
    const double gateDeltaX = pActiveGate.mX - pPlayerState.mX;
    const double gateDeltaY = pActiveGate.mY - pPlayerState.mY;
    const int gateDistance = static_cast<int>(std::sqrt(gateDeltaX * gateDeltaX + gateDeltaY * gateDeltaY));
    char speedLabel[24];
    char gateLabel[24];
    std::snprintf(speedLabel, sizeof(speedLabel), "SPEED %d", speed);
    std::snprintf(gateLabel, sizeof(gateLabel), "NEXT %d", gateDistance);
    glColor3f(0.82f, 0.9f, 0.92f);
    DrawPixelText(speedLabel, 300, 27, 2);
    glColor3f(0.2f, 0.9f, 1.0f);
    DrawPixelText(gateLabel, 300, 45, 2);

    const int resourceLeft = 24;
    const int resourceWidth = 122;
    const int resourceHeight = 9;
    const int fuelWidth = static_cast<int>(resourceWidth * pPlayerState.mFuel);
    const int missileWidth = static_cast<int>(resourceWidth * pMissile.RechargeFraction());
    glColor3f(0.72f, 0.82f, 0.84f);
    DrawPixelText("FUEL", resourceLeft, 132, 2);
    DrawPixelText(pWeaponsAllowed ? "MISSILE" : "WEAPONS OFF", resourceLeft, 154, 2);
    glColor3f(0.02f, 0.05f, 0.08f);
    glBegin(GL_QUADS);
    glVertex2i(resourceLeft + 62, 134);
    glVertex2i(resourceLeft + 62 + resourceWidth, 134);
    glVertex2i(resourceLeft + 62 + resourceWidth, 134 + resourceHeight);
    glVertex2i(resourceLeft + 62, 134 + resourceHeight);
    glVertex2i(resourceLeft + 62, 156);
    glVertex2i(resourceLeft + 62 + resourceWidth, 156);
    glVertex2i(resourceLeft + 62 + resourceWidth, 156 + resourceHeight);
    glVertex2i(resourceLeft + 62, 156 + resourceHeight);
    glEnd();
    glColor3f(0.18f, 0.78f, 0.54f);
    glBegin(GL_QUADS);
    glVertex2i(resourceLeft + 62, 134);
    glVertex2i(resourceLeft + 62 + fuelWidth, 134);
    glVertex2i(resourceLeft + 62 + fuelWidth, 134 + resourceHeight);
    glVertex2i(resourceLeft + 62, 134 + resourceHeight);
    glEnd();
    glColor3f(pMissile.Ready() ? 0.2f : 0.92f, pMissile.Ready() ? 0.82f : 0.52f,
              pMissile.Ready() ? 0.96f : 0.14f);
    glBegin(GL_QUADS);
    glVertex2i(resourceLeft + 62, 156);
    glVertex2i(resourceLeft + 62 + (pWeaponsAllowed ? missileWidth : 0), 156);
    glVertex2i(resourceLeft + 62 + (pWeaponsAllowed ? missileWidth : 0), 156 + resourceHeight);
    glVertex2i(resourceLeft + 62, 156 + resourceHeight);
    glEnd();

    const int displayedLap = pPlayerProgress.mCompletedLaps + 1 > pTargetLaps
        ? pTargetLaps : pPlayerProgress.mCompletedLaps + 1;
    const int elapsedSeconds = static_cast<int>(pPlayerProgress.mElapsedSeconds);
    const int currentLapSeconds = static_cast<int>(pLapTiming.mCurrentSeconds);
    const int bestLapSeconds = static_cast<int>(pLapTiming.mBestSeconds);
    const int lastSplitSeconds = static_cast<int>(pLapTiming.mLastSplitSeconds);
    char lapLabel[24];
    char timeLabel[32];
    char currentLapLabel[32];
    char bestLapLabel[32];
    char splitLabel[32];
    char positionLabel[24];
    std::snprintf(lapLabel, sizeof(lapLabel), "LAP %d OF %d", displayedLap, pTargetLaps);
    std::snprintf(timeLabel, sizeof(timeLabel), "TIME %d M %d S", elapsedSeconds / 60, elapsedSeconds % 60);
    std::snprintf(currentLapLabel, sizeof(currentLapLabel), "LAP TIME %d M %d S",
                  currentLapSeconds / 60, currentLapSeconds % 60);
    std::snprintf(bestLapLabel, sizeof(bestLapLabel), "BEST %d M %d S",
                  bestLapSeconds / 60, bestLapSeconds % 60);
    std::snprintf(splitLabel, sizeof(splitLabel), "SPLIT %d M %d S",
                  lastSplitSeconds / 60, lastSplitSeconds % 60);
    std::snprintf(positionLabel, sizeof(positionLabel), "PLACE %d OF %d", pPlayerPosition, pCompetitorCount);
    glColor3f(0.82f, 0.9f, 0.92f);
    DrawPixelText(lapLabel, pWidth / 2 + 36, 27, 2);
    DrawPixelText(timeLabel, pWidth / 2 + 36, 45, 2);
    DrawPixelText(currentLapLabel, pWidth / 2 + 36, 63, 2);
    DrawPixelText(bestLapLabel, pWidth / 2 + 36, 81, 2);
    DrawPixelText(splitLabel, pWidth / 2 + 36, 99, 2);
    glColor3f(1.0f, 0.78f, 0.12f);
    DrawPixelText(positionLabel, pWidth / 2 + 36, 117, 2);

    const double targetHeading = GetGateDirection(pPlayerState, pActiveGate);
    const double headingOffset = targetHeading - pPlayerState.mHeading;
    const double forwardX = std::sin(headingOffset);
    const double forwardY = -std::cos(headingOffset);
    const double sideX = std::cos(headingOffset);
    const double sideY = std::sin(headingOffset);
    const int pointerX = pWidth / 2;
    const int pointerY = pHeight - 86;
    glColor3f(0.02f, 0.05f, 0.08f);
    glBegin(GL_QUADS);
    glVertex2i(pointerX - 18, pointerY - 15);
    glVertex2i(pointerX + 18, pointerY - 15);
    glVertex2i(pointerX + 18, pointerY + 15);
    glVertex2i(pointerX - 18, pointerY + 15);
    glEnd();
    glColor3f(0.2f, 0.9f, 1.0f);
    glBegin(GL_TRIANGLES);
    glVertex2d(pointerX + forwardX * 11.0, pointerY + forwardY * 11.0);
    glVertex2d(pointerX - forwardX * 6.0 + sideX * 6.0, pointerY - forwardY * 6.0 + sideY * 6.0);
    glVertex2d(pointerX - forwardX * 6.0 - sideX * 6.0, pointerY - forwardY * 6.0 - sideY * 6.0);
    glEnd();

    if (pWrongWay)
    {
        glColor3f(0.72f, 0.06f, 0.08f);
        glBegin(GL_QUADS);
        glVertex2i(pointerX - 96, pointerY - 54);
        glVertex2i(pointerX + 96, pointerY - 54);
        glVertex2i(pointerX + 96, pointerY - 32);
        glVertex2i(pointerX - 96, pointerY - 32);
        glEnd();
        glColor3f(1.0f, 0.82f, 0.2f);
        DrawPixelText("WRONG WAY", pointerX - 72, pointerY - 50, 2);
    }

    if (pPlayerState.mSpinOutSeconds > 0.0)
    {
        const int alertWidth = 320;
        const int alertLeft = (pWidth - alertWidth) / 2;
        glColor3f(0.5f, 0.03f, 0.04f);
        glBegin(GL_QUADS);
        glVertex2i(alertLeft, 28);
        glVertex2i(alertLeft + alertWidth, 28);
        glVertex2i(alertLeft + alertWidth, 76);
        glVertex2i(alertLeft, 76);
        glEnd();
        glColor3f(1.0f, 0.74f, 0.12f);
        DrawPixelText("LOSS OF CONTROL", alertLeft + 20, 42, 4);
    }

    if (!pWaypoints.empty())
    {
        double minimumX = pWaypoints.front().mX;
        double maximumX = minimumX;
        double minimumY = pWaypoints.front().mY;
        double maximumY = minimumY;
        for (const RaceGate& waypoint : pWaypoints)
        {
            minimumX = std::fmin(minimumX, waypoint.mX);
            maximumX = std::fmax(maximumX, waypoint.mX);
            minimumY = std::fmin(minimumY, waypoint.mY);
            maximumY = std::fmax(maximumY, waypoint.mY);
        }
        const double rangeX = std::fmax(1.0, maximumX - minimumX);
        const double rangeY = std::fmax(1.0, maximumY - minimumY);
        const int mapSize = 132;
        const int mapLeft = pWidth - mapSize - 24;
        const int mapTop = 24;
        const auto mapX = [&](double x)
        {
            return mapLeft + 8 + static_cast<int>((x - minimumX) * (mapSize - 16) / rangeX);
        };
        const auto mapY = [&](double y)
        {
            return mapTop + mapSize - 8 - static_cast<int>((y - minimumY) * (mapSize - 16) / rangeY);
        };

        glColor3f(0.02f, 0.05f, 0.08f);
        glBegin(GL_QUADS);
        glVertex2i(mapLeft, mapTop);
        glVertex2i(mapLeft + mapSize, mapTop);
        glVertex2i(mapLeft + mapSize, mapTop + mapSize);
        glVertex2i(mapLeft, mapTop + mapSize);
        glEnd();
        glColor3f(0.2f, 0.8f, 0.9f);
        glBegin(GL_LINE_LOOP);
        for (const RaceGate& waypoint : pWaypoints)
            glVertex2i(mapX(waypoint.mX), mapY(waypoint.mY));
        glEnd();
        glColor3f(0.2f, 0.8f, 0.9f);
        glPointSize(4.0f);
        glBegin(GL_POINTS);
        glVertex2i(mapX(pWaypoints.front().mX), mapY(pWaypoints.front().mY));
        glEnd();
        glColor3f(1.0f, 0.78f, 0.12f);
        glPointSize(8.0f);
        glBegin(GL_POINTS);
        glVertex2i(mapX(pPlayerState.mX), mapY(pPlayerState.mY));
        glEnd();
        if (pShowRivals)
        {
            glColor3f(0.95f, 0.22f, 0.18f);
            glBegin(GL_POINTS);
            for (const HovercraftState& rivalState : pRivalStates)
                glVertex2i(mapX(rivalState.mX), mapY(rivalState.mY));
            glEnd();
        }
        glPointSize(1.0f);
    }

    for (int lap = 0; lap < pTargetLaps; ++lap)
    {
        const int left = 24 + lap * 34;
        glColor3f(lap < pPlayerProgress.mCompletedLaps ? 1.0f : 0.22f,
                  lap < pPlayerProgress.mCompletedLaps ? 0.78f : 0.25f,
                  lap < pPlayerProgress.mCompletedLaps ? 0.12f : 0.28f);
        glBegin(GL_QUADS);
        glVertex2i(left, 58);
        glVertex2i(left + 28, 58);
        glVertex2i(left + 28, 70);
        glVertex2i(left, 70);
        glEnd();

        for (int rivalIndex = 0; rivalIndex < static_cast<int>(pRivalProgresses.size()); ++rivalIndex)
        {
            const RaceProgress& rivalProgress = pRivalProgresses[rivalIndex];
            const int top = 76 + rivalIndex * 18;
            glColor3f(lap < rivalProgress.mCompletedLaps ? 0.94f : 0.3f,
                      lap < rivalProgress.mCompletedLaps ? 0.22f : 0.12f,
                      lap < rivalProgress.mCompletedLaps ? 0.18f : 0.14f);
            glBegin(GL_QUADS);
            glVertex2i(left, top);
            glVertex2i(left + 28, top);
            glVertex2i(left + 28, top + 12);
            glVertex2i(left, top + 12);
            glEnd();
        }
    }

    if (pWinner != 0)
    {
        if (pWinner == 1)
            glColor3f(0.12f, 0.8f, 0.18f);
        else if (pWinner == 2)
            glColor3f(0.7f, 0.12f, 0.18f);
        else
            glColor3f(0.92f, 0.68f, 0.12f);
        glBegin(GL_QUADS);
        glVertex2i(pWidth / 2 - 110, 28);
        glVertex2i(pWidth / 2 + 110, 28);
        glVertex2i(pWidth / 2 + 110, 42);
        glVertex2i(pWidth / 2 - 110, 42);
        glEnd();
    }

    if (pShowSetupOverlay)
        DrawSetupOverlay(pWidth, pHeight);
    else if (pWinner != 0 && pShowResultOverlay)
        DrawResultOverlay(pWinner, pPlayerPosition, pCompetitorCount, pChampionship,
                          pChampionshipPoints, pPlayerElapsedSeconds, pLapTiming, pResultSelection,
                          pWidth, pHeight);
    if (pPauseMenuOpen)
        DrawPauseOverlay(pPauseMenuSelection, pWidth, pHeight);

    for (int light = 0; light < 3; ++light)
    {
        glColor3f(light < pStartLights ? 1.0f : 0.18f,
                  light < pStartLights ? (light == 2 ? 0.8f : 0.18f) : 0.12f,
                  light < pStartLights ? 0.12f : 0.14f);
        glBegin(GL_QUADS);
        glVertex2i(pWidth / 2 - 44 + light * 32, pHeight - 48);
        glVertex2i(pWidth / 2 - 20 + light * 32, pHeight - 48);
        glVertex2i(pWidth / 2 - 20 + light * 32, pHeight - 24);
        glVertex2i(pWidth / 2 - 44 + light * 32, pHeight - 24);
        glEnd();
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FOG);
    glEnable(GL_LIGHTING);
}

void DrawOnlineHudPanel(int pLeft, int pTop, int pWidth, int pHeight)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.015f, 0.035f, 0.05f, 0.8f);
    glBegin(GL_QUADS);
    glVertex2i(pLeft, pTop);
    glVertex2i(pLeft + pWidth, pTop);
    glVertex2i(pLeft + pWidth, pTop + pHeight);
    glVertex2i(pLeft, pTop + pHeight);
    glEnd();
    glColor4f(0.18f, 0.72f, 0.82f, 0.7f);
    glBegin(GL_LINE_LOOP);
    glVertex2i(pLeft, pTop);
    glVertex2i(pLeft + pWidth, pTop);
    glVertex2i(pLeft + pWidth, pTop + pHeight);
    glVertex2i(pLeft, pTop + pHeight);
    glEnd();
    glDisable(GL_BLEND);
}

void DrawOnlineHud(const OnlineRacerView& pPlayer, int pRacerCount, int pTargetLaps,
                   int pWidth, int pHeight)
{
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, pWidth, pHeight, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    const int speed = static_cast<int>(std::fabs(pPlayer.mState.mSpeed));
    const int targetLaps = std::max(1, std::min(5, pTargetLaps));
    const int displayedLap = std::min(targetLaps, pPlayer.mProgress.mCompletedLaps + 1);
    const int elapsedSeconds = static_cast<int>(pPlayer.mProgress.mElapsedSeconds);
    const int currentLapSeconds = static_cast<int>(pPlayer.mLapTiming.mCurrentSeconds);
    const int bestLapSeconds = static_cast<int>(pPlayer.mLapTiming.mBestSeconds);
    char speedLabel[24];
    char lapLabel[24];
    char timeLabel[32];
    char currentLapLabel[32];
    char bestLapLabel[32];
    char positionLabel[24];
    std::snprintf(speedLabel, sizeof(speedLabel), "%03d", speed);
    std::snprintf(lapLabel, sizeof(lapLabel), "LAP %d OF %d", displayedLap, targetLaps);
    std::snprintf(timeLabel, sizeof(timeLabel), "TIME %02d %02d", elapsedSeconds / 60, elapsedSeconds % 60);
    std::snprintf(currentLapLabel, sizeof(currentLapLabel), "LIVE %02d %02d",
                  currentLapSeconds / 60, currentLapSeconds % 60);
    std::snprintf(bestLapLabel, sizeof(bestLapLabel), "BEST %02d %02d",
                  bestLapSeconds / 60, bestLapSeconds % 60);
    std::snprintf(positionLabel, sizeof(positionLabel), "PLACE %d OF %d",
                  pPlayer.mPosition, pRacerCount);
    DrawOnlineHudPanel(18, 18, 170, 72);
    DrawOnlineHudPanel(pWidth - 180, 18, 162, 42);
    DrawOnlineHudPanel(18, pHeight - 88, 170, 70);
    DrawOnlineHudPanel(pWidth - 180, pHeight - 70, 162, 52);
    glColor3f(0.2f, 0.9f, 1.0f);
    DrawPixelText("ONLINE", 30, 30, 2);
    glColor3f(0.82f, 0.9f, 0.92f);
    DrawPixelText(lapLabel, 30, 50, 2);
    DrawPixelText(timeLabel, 30, 70, 2);
    DrawPixelText("SPEED", 30, pHeight - 78, 2);
    DrawPixelText(speedLabel, 30, pHeight - 56, 4);
    DrawPixelText(currentLapLabel, pWidth - 168, pHeight - 58, 2);
    DrawPixelText(bestLapLabel, pWidth - 168, pHeight - 36, 2);
    glColor3f(1.0f, 0.78f, 0.12f);
    DrawPixelText(positionLabel, pWidth - 168, 32, 2);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FOG);
    glEnable(GL_LIGHTING);
}
}

int main(int pArgumentCount, char* pArguments[])
{
    (void)pArgumentCount;
    (void)pArguments;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
    {
        std::fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_Window* window = SDL_CreateWindow("OpenHover", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, kWindowWidth, kWindowHeight,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_GLContext context = window == nullptr ? nullptr : SDL_GL_CreateContext(window);
    if (context == nullptr)
    {
        std::fprintf(stderr, "SDL OpenGL window creation failed: %s\n", SDL_GetError());
        if (window != nullptr)
            SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_SetSwapInterval(1);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FOG);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_NORMALIZE);
    const GLfloat ambientLight[] = {0.26f, 0.3f, 0.34f, 1.0f};
    const GLfloat diffuseLight[] = {0.88f, 0.92f, 0.84f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
    const GLfloat fogColor[] = {0.14f, 0.28f, 0.32f, 1.0f};
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 35.0f);
    glFogf(GL_FOG_END, 110.0f);
    const GLuint roadTexture = CreateRoadTexture();
    const GLuint wallTexture = CreateWallTexture();
    AudioFeedback audioFeedback;
    audioFeedback.Initialize();

    SDL_GameController* controller = nullptr;
    for (int joystick = 0; joystick < SDL_NumJoysticks(); ++joystick)
    {
        if (SDL_IsGameController(joystick))
        {
            controller = SDL_GameControllerOpen(joystick);
            break;
        }
    }

    const std::vector<TrackDefinition>& builtInTracks = BuiltInTracks();
    int trackIndex = 0;
    TrackDefinition selectedTrack = builtInTracks[trackIndex];
    CraftClass playerCraftClass = CraftClass::Balanced;
    Hovercraft hovercraft(CraftClassTuning(playerCraftClass));
    Hovercraft replayGhost(CraftClassTuning(playerCraftClass));
    HovercraftState spawn;
    std::vector<RaceGate> checkpoints = selectedTrack.Checkpoints();
    RaceGate finish = selectedTrack.Finish();
    std::vector<RaceGate> courseWaypoints = selectedTrack.mWaypoints;
    Course course(courseWaypoints, selectedTrack.mRoadHalfWidth);
    std::vector<BoostPad> boostPads = selectedTrack.mBoostPads;
    std::vector<HazardZone> hazardZones = selectedTrack.mHazardZones;
    std::vector<Mine> mines = selectedTrack.mMines;
    std::vector<RaisedSection> raisedSections = selectedTrack.mRaisedSections;
    RaceMode raceMode = RaceMode::SingleRace;
    RivalDifficulty rivalDifficulty = RivalDifficulty::Standard;
    int rivalCount = kRivalCount;
    int targetLaps = 3;
    Championship championship(static_cast<int>(builtInTracks.size()));
    Race race(checkpoints, finish, targetLaps);
    std::vector<Race> rivalRaces(kRivalCount, Race(checkpoints, finish, targetLaps));
    LapTimer lapTimer;
    RaceStart raceStart;
    std::vector<RaceGate> rivalRoute(courseWaypoints.begin() + 1, courseWaypoints.end());
    rivalRoute.push_back(finish);
    std::vector<RivalTuning> rivalTunings(kRivalCount);
    rivalTunings[0].mPace = 1.0;
    rivalTunings[0].mSteeringGain = 1.35;
    rivalTunings[1].mPace = 0.88;
    rivalTunings[1].mSteeringGain = 1.85;
    std::vector<RivalController> rivalControllers;
    const auto rebuildRivalControllers = [&]()
    {
        rivalControllers.clear();
        for (int rivalIndex = 0; rivalIndex < kRivalCount; ++rivalIndex)
            rivalControllers.emplace_back(rivalRoute,
                TuneRivalForDifficulty(rivalTunings[rivalIndex], rivalDifficulty));
    };
    rebuildRivalControllers();
    std::vector<Hovercraft> rivals(kRivalCount);
    std::vector<HovercraftState> rivalSpawns(kRivalCount);
    const auto configureStartGrid = [&]()
    {
        const RaceGate& firstCheckpoint = checkpoints.empty() ? finish : checkpoints.front();
        double forwardX = firstCheckpoint.mX - finish.mX;
        double forwardY = firstCheckpoint.mY - finish.mY;
        const double length = std::sqrt(forwardX * forwardX + forwardY * forwardY);
        if (length > 0.0)
        {
            forwardX /= length;
            forwardY /= length;
        }
        else
        {
            forwardX = 1.0;
            forwardY = 0.0;
        }
        spawn = HovercraftState();
        spawn.mX = finish.mX + forwardX * 7.0;
        spawn.mY = finish.mY + forwardY * 7.0;
        spawn.mHeading = std::atan2(forwardY, forwardX);
        spawn.mTravelHeading = spawn.mHeading;
        const double laneX = -forwardY;
        const double laneY = forwardX;
        for (int rivalIndex = 0; rivalIndex < kRivalCount; ++rivalIndex)
        {
            const int gridRow = rivalIndex / 3;
            const int gridColumn = rivalIndex % 3 - 1;
            const double laneOffset = gridColumn * 3.6;
            const double rowOffset = 3.8 + gridRow * 4.2;
            rivalSpawns[rivalIndex] = spawn;
            rivalSpawns[rivalIndex].mX -= forwardX * rowOffset;
            rivalSpawns[rivalIndex].mY -= forwardY * rowOffset;
            rivalSpawns[rivalIndex].mX += laneX * laneOffset;
            rivalSpawns[rivalIndex].mY += laneY * laneOffset;
        }
    };
    configureStartGrid();
    hovercraft.Reset(spawn);
    for (int rivalIndex = 0; rivalIndex < kRivalCount; ++rivalIndex)
        rivals[rivalIndex].Reset(rivalSpawns[rivalIndex]);

    Uint64 previousTick = SDL_GetPerformanceCounter();
    const double tickFrequency = static_cast<double>(SDL_GetPerformanceFrequency());
    FixedStepClock simulationClock;
    int winner = 0;
    bool continueDriving = false;
    bool pauseMenuOpen = false;
    int pauseMenuSelection = 0;
    int resultSelection = 0;
    bool steeringAssistEnabled = false;
    bool brakingAssistEnabled = false;
    bool showControls = false;
    FrontScreen frontScreen = FrontScreen::Welcome;
    int frontSelection = 0;
    int localSetupSelection = 0;
    int settingsSelection = 0;
    int cameraDistanceSetting = 1;
    bool weaponsAllowed = true;
    TcpLobbyClient lobbyClient;
    bool lobbyHelloSent = false;
    double onlineInputSeconds = 0.0;
    std::string lobbyDisplayName;
    char preferencesFile[512] = {};
    char* preferencesDirectory = SDL_GetPrefPath("OpenHover", "OpenHover");
    if (preferencesDirectory != nullptr)
    {
        std::snprintf(preferencesFile, sizeof(preferencesFile), "%ssetup.cfg", preferencesDirectory);
        SDL_free(preferencesDirectory);
    }
    Missile missile;
    bool fireHeld = false;
    double impactSoundCooldown = 0.0;
    InputRecording activeRecording;
    InputRecording ghostRecording;
    std::size_t ghostFrame = 0;
    bool ghostActive = false;
    const auto resetRace = [&](bool pStartCountdown = true)
    {
        if (!activeRecording.Empty())
        {
            ghostRecording = activeRecording;
            replayGhost = Hovercraft(CraftClassTuning(playerCraftClass));
            replayGhost.Reset(spawn);
            ghostFrame = 0;
            ghostActive = true;
        }
        activeRecording.Clear();
        missile.Reset();
        fireHeld = false;
        hovercraft.Reset(spawn);
        race.Reset();
        lapTimer.Reset();
        raceStart.Reset();
        for (int rivalIndex = 0; rivalIndex < kRivalCount; ++rivalIndex)
        {
            rivalRaces[rivalIndex].Reset();
            rivals[rivalIndex].Reset(rivalSpawns[rivalIndex]);
            rivalControllers[rivalIndex].Reset();
        }
        simulationClock.Reset();
        winner = 0;
        continueDriving = false;
        resultSelection = 0;
        if (pStartCountdown)
            raceStart.Begin();
    };
    const auto loadTrack = [&](bool pStartCountdown = true)
    {
        activeRecording.Clear();
        ghostRecording.Clear();
        ghostFrame = 0;
        ghostActive = false;
        selectedTrack = builtInTracks[trackIndex];
        checkpoints = selectedTrack.Checkpoints();
        finish = selectedTrack.Finish();
        courseWaypoints = selectedTrack.mWaypoints;
        course = Course(courseWaypoints, selectedTrack.mRoadHalfWidth);
        boostPads = selectedTrack.mBoostPads;
        hazardZones = selectedTrack.mHazardZones;
        mines = selectedTrack.mMines;
        raisedSections = selectedTrack.mRaisedSections;
        race = Race(checkpoints, finish, targetLaps);
        rivalRaces.assign(kRivalCount, Race(checkpoints, finish, targetLaps));
        rivalRoute.assign(courseWaypoints.begin() + 1, courseWaypoints.end());
        rivalRoute.push_back(finish);
        configureStartGrid();
        rebuildRivalControllers();
        resetRace(pStartCountdown);
    };
    const auto savePreferences = [&]()
    {
        if (preferencesFile[0] == '\0')
            return;
        FILE* preferences = std::fopen(preferencesFile, "w");
        if (preferences == nullptr)
            return;
        std::fprintf(preferences, "%d %d %d %d %d %s\n", trackIndex, targetLaps,
                 weaponsAllowed ? 1 : 0, cameraDistanceSetting, audioFeedback.Enabled() ? 1 : 0,
                 gPlayerDisplayName.c_str());
        std::fclose(preferences);
    };
    if (preferencesFile[0] != '\0')
    {
        FILE* preferences = std::fopen(preferencesFile, "r");
        int savedTrack = 0;
        int savedLaps = 3;
        int savedWeapons = 1;
        int savedCameraDistance = 1;
        int savedAudio = 1;
        char savedDisplayName[25] = {};
        if (preferences != nullptr
            && std::fscanf(preferences, "%d %d %d %d %d", &savedTrack, &savedLaps,
                           &savedWeapons, &savedCameraDistance, &savedAudio) == 5)
        {
            trackIndex = savedTrack >= 0 && savedTrack < static_cast<int>(builtInTracks.size())
                ? savedTrack : 0;
            targetLaps = savedLaps >= 1 && savedLaps <= 5 ? savedLaps : 3;
            weaponsAllowed = savedWeapons != 0;
            cameraDistanceSetting = savedCameraDistance >= 0 && savedCameraDistance <= 2
                ? savedCameraDistance : 1;
            audioFeedback.SetEnabled(savedAudio != 0);
            if (std::fscanf(preferences, "%24s", savedDisplayName) == 1)
                gPlayerDisplayName = savedDisplayName;
        }
        if (preferences != nullptr)
            std::fclose(preferences);
    }
    loadTrack(false);
    const auto changeLocalSetupOption = [&](int pOption, int pDirection)
    {
        if (pOption == 1)
        {
            const int changes = pDirection > 0 ? 1 : 3;
            for (int change = 0; change < changes; ++change)
                raceMode = NextRaceMode(raceMode);
            championship.Reset();
            loadTrack(false);
        }
        else if (pOption == 2)
        {
            trackIndex = (trackIndex + pDirection + static_cast<int>(builtInTracks.size()))
                % static_cast<int>(builtInTracks.size());
            loadTrack(false);
        }
        else if (pOption == 3)
        {
            targetLaps = pDirection > 0 ? (targetLaps == 5 ? 1 : targetLaps + 1)
                                          : (targetLaps == 1 ? 5 : targetLaps - 1);
            loadTrack(false);
        }
        else if (pOption == 4)
        {
            rivalCount = (rivalCount + pDirection + kRivalCount + 1) % (kRivalCount + 1);
            resetRace(false);
        }
        else if (pOption == 5)
        {
            const int changes = pDirection > 0 ? 1 : 2;
            for (int change = 0; change < changes; ++change)
                rivalDifficulty = NextRivalDifficulty(rivalDifficulty);
            rebuildRivalControllers();
            resetRace(false);
        }
        else if (pOption == 6)
            weaponsAllowed = !weaponsAllowed;
        savePreferences();
    };
    const auto startLocalRace = [&]()
    {
        frontScreen = FrontScreen::RaceSetup;
        resetRace();
    };
    const auto returnToMainMenu = [&]()
    {
        pauseMenuOpen = false;
        continueDriving = false;
        frontScreen = FrontScreen::Welcome;
    };
    const auto advanceResult = [&]()
    {
        if (raceMode == RaceMode::Championship)
        {
            if (championship.AdvanceEvent())
            {
                trackIndex = championship.CurrentEvent();
                loadTrack();
            }
            else if (championship.Complete())
            {
                championship.Reset();
                trackIndex = championship.CurrentEvent();
                loadTrack();
            }
            else
                resetRace();
        }
        else
            resetRace();
    };
    const auto leaveLobby = [&]()
    {
        SDL_StopTextInput();
        lobbyClient.Disconnect();
        lobbyHelloSent = false;
        gLobbyPlayers.clear();
        gLobbyRooms.clear();
        gLobbyChatMessages.clear();
        gLobbyChatInput.clear();
        gLobbyChatInputFocused = false;
        gLobbySelectedRoom = -1;
        gLobbyPlayerId = 0;
        gLobbyJoinedRoomId = 0;
        gLobbyJoinPendingRoomId = 0;
        gLobbyStatus = "DISCONNECTED";
        gOnlineRaceRoomId = 0;
        gOnlineRaceTick = 0;
        gOnlineTargetLaps = 0;
        gOnlineRacers.clear();
    };
    const auto connectLobby = [&]()
    {
        leaveLobby();
        const std::string sessionSuffix = "-" + std::to_string(
            static_cast<unsigned long long>(SDL_GetPerformanceCounter() % 1000000));
        lobbyDisplayName = gPlayerDisplayName;
        if (lobbyDisplayName.size() + sessionSuffix.size() > 24)
            lobbyDisplayName.resize(24 - sessionSuffix.size());
        lobbyDisplayName += sessionSuffix;
        SDL_StartTextInput();
        gLobbyChatInputFocused = true;
        gLobbyStatus = lobbyClient.Connect("outiva.com", 9700)
            ? "CONNECTING" : "SERVER UNAVAILABLE";
    };
    const auto openMultiplayer = [&]()
    {
        if (gPlayerDisplayName.empty())
        {
            gDisplayNameSetupConnectsToLobby = true;
            frontScreen = FrontScreen::DisplayNameSetup;
            SDL_StartTextInput();
        }
        else
        {
            frontScreen = FrontScreen::Multiplayer;
            connectLobby();
        }
    };
    const auto updateLobby = [&]()
    {
        if (frontScreen != FrontScreen::Multiplayer && frontScreen != FrontScreen::HostRaceSetup
            && frontScreen != FrontScreen::OnlineRace)
            return;
        lobbyClient.Tick();
        if (lobbyClient.State() == TcpLobbyClientState::Connecting)
            gLobbyStatus = "CONNECTING";
        else if (lobbyClient.State() == TcpLobbyClientState::Failed)
            gLobbyStatus = "SERVER UNAVAILABLE";
        else if (lobbyClient.State() == TcpLobbyClientState::Connected && !lobbyHelloSent)
        {
            lobbyHelloSent = lobbyClient.SendCommand("HELLO " + lobbyDisplayName);
            gLobbyStatus = lobbyHelloSent ? "CONNECTING" : "SERVER UNAVAILABLE";
        }
        for (const std::string& message : lobbyClient.TakeMessages())
        {
            if (message.compare(0, 5, "LOBBY") == 0)
            {
                ParseLobbySnapshot(message);
                gLobbyStatus = "CONNECTED";
            }
            else if (message.compare(0, 8, "WELCOME ") == 0)
                gLobbyPlayerId = std::atoi(message.substr(8).c_str());
            else if (message.compare(0, 5, "CHAT ") == 0)
            {
                const std::size_t textStart = message.find(' ', 5);
                const int senderId = std::atoi(message.substr(5, textStart - 5).c_str());
                const std::string text = textStart == std::string::npos ? "" : message.substr(textStart + 1);
                std::string senderName = senderId == 0 ? "LOBBY" : "PILOT";
                for (const LobbyPlayerView& player : gLobbyPlayers)
                {
                    if (player.mId == senderId)
                    {
                        senderName = player.mDisplayName;
                        break;
                    }
                }
                gLobbyChatMessages.push_back(senderName + " " + text);
                if (gLobbyChatMessages.size() > 32)
                    gLobbyChatMessages.erase(gLobbyChatMessages.begin());
            }
            else if (message.compare(0, 5, "ROOM ") == 0 && gHostCreatePending)
            {
                gHostCreatePending = false;
                gLobbyJoinedRoomId = std::atoi(message.substr(5).c_str());
                gLobbyStatus = "ROOM CREATED";
                frontScreen = FrontScreen::Multiplayer;
            }
            else if (message.compare(0, 5, "RACE ") == 0 && ParseRaceSnapshot(message))
            {
                for (const LobbyRoomView& room : gLobbyRooms)
                {
                    if (room.mId == gOnlineRaceRoomId
                        && room.mTrackIndex >= 0
                        && room.mTrackIndex < static_cast<int>(builtInTracks.size()))
                    {
                        trackIndex = room.mTrackIndex;
                        loadTrack(false);
                        break;
                    }
                }
                SDL_StopTextInput();
                frontScreen = FrontScreen::OnlineRace;
                gLobbyStatus = "RACING";
            }
            else if (message.compare(0, 8, "RACEHUD ") == 0)
                ParseRaceHudSnapshot(message);
            else if (message.compare(0, 11, "RACEFINISH ") == 0
                     && std::atoi(message.substr(11).c_str()) == gOnlineRaceRoomId)
            {
                gOnlineRaceRoomId = 0;
                gOnlineRaceTick = 0;
                gOnlineTargetLaps = 0;
                gOnlineRacers.clear();
                SDL_StartTextInput();
                gLobbyChatInputFocused = true;
                frontScreen = FrontScreen::Multiplayer;
                gLobbyStatus = "RACE COMPLETE";
            }
            else if (message.compare(0, 6, "ERROR ") == 0)
            {
                gHostCreatePending = false;
                if (gLobbyJoinPendingRoomId != 0)
                {
                    gLobbyJoinedRoomId = 0;
                    gLobbyJoinPendingRoomId = 0;
                }
                const std::string error = message.substr(6);
                if (error == "room full")
                    gLobbyStatus = "ROOM FULL";
                else if (error == "race running")
                    gLobbyStatus = "RACE STARTED";
                else if (error == "already in room")
                    gLobbyStatus = "ALREADY JOINED";
                else if (error == "room unavailable")
                    gLobbyStatus = "ROOM UNAVAILABLE";
                else
                    gLobbyStatus = "SERVER ERROR";
            }
        }
    };
    bool running = true;
    while (running)
    {
        updateLobby();
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0)
        {
            if (event.type == SDL_QUIT)
                running = false;
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
            {
                int windowWidth = 0;
                int windowHeight = 0;
                int drawableWidth = 0;
                int drawableHeight = 0;
                SDL_GetWindowSize(window, &windowWidth, &windowHeight);
                SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);
                const int mouseX = event.button.x * drawableWidth / std::max(1, windowWidth);
                const int mouseY = event.button.y * drawableHeight / std::max(1, windowHeight);
                if (frontScreen == FrontScreen::Welcome)
                {
                    const int option = (mouseY - 145) / 72;
                    if (option >= 0 && option < 5
                        && IsPointInRect(mouseX, mouseY, drawableWidth / 2 - 200, 145 + option * 72, 400, 62))
                    {
                        if (option == 0)
                        {
                            frontScreen = FrontScreen::LocalSetup;
                            localSetupSelection = 0;
                            loadTrack(false);
                        }
                        else if (option == 1)
                        {
                            openMultiplayer();
                        }
                        else if (option == 2)
                            frontScreen = FrontScreen::HowToPlay;
                        else if (option == 3)
                        {
                            frontScreen = FrontScreen::Settings;
                            settingsSelection = 0;
                        }
                        else
                            running = false;
                    }
                }
                else if (frontScreen == FrontScreen::Multiplayer)
                {
                    const int margin = 18;
                    const int gap = 12;
                    const int top = 16;
                    const int leftWidth = drawableWidth * 29 / 100;
                    const int actionWidth = drawableWidth * 15 / 100;
                    const int detailWidth = drawableWidth - margin * 2 - gap * 2 - leftWidth - actionWidth;
                    const int topHeight = drawableHeight * 42 / 100;
                    const int actionLeft = margin + leftWidth + gap + detailWidth + gap;
                    const int chatLeft = margin + leftWidth + gap + 16;
                    const int chatWidth = detailWidth + gap + actionWidth - 32;
                    const int bottomTop = top + topHeight + gap;
                    const int bottomHeight = drawableHeight - bottomTop - margin;
                    const int inputTop = bottomTop + bottomHeight - 46;
                    if (IsPointInRect(mouseX, mouseY, actionLeft + 16, top + 16, actionWidth - 32, 42)
                        && gLobbySelectedRoom >= 0 && gLobbySelectedRoom < static_cast<int>(gLobbyRooms.size()))
                    {
                        gLobbyChatInputFocused = false;
                        const LobbyRoomView& room = gLobbyRooms[gLobbySelectedRoom];
                        if (room.mHostId == gLobbyPlayerId)
                            lobbyClient.SendCommand("START " + std::to_string(room.mId));
                        else if (room.mId == gLobbyJoinedRoomId)
                        {
                            if (lobbyClient.SendCommand("LEAVE"))
                            {
                                gLobbyJoinedRoomId = 0;
                                gLobbyJoinPendingRoomId = 0;
                                gLobbyStatus = "LEFT ROOM";
                            }
                        }
                        else if (lobbyClient.SendCommand("JOIN " + std::to_string(room.mId)))
                        {
                            gLobbyJoinedRoomId = room.mId;
                            gLobbyJoinPendingRoomId = room.mId;
                            gLobbyStatus = "JOINING RACE";
                        }
                    }
                    else if (IsPointInRect(mouseX, mouseY, actionLeft + 16, top + 72, actionWidth - 32, 42))
                    {
                        gLobbyChatInputFocused = false;
                        gHostSetupSelection = 0;
                        frontScreen = FrontScreen::HostRaceSetup;
                    }
                    else if (IsPointInRect(mouseX, mouseY, actionLeft + 16, top + topHeight - 58, actionWidth - 32, 42))
                    {
                        leaveLobby();
                        frontScreen = FrontScreen::Welcome;
                    }
                    else if (IsPointInRect(mouseX, mouseY, margin + 16, top + 54, leftWidth - 32,
                                           topHeight - 70) && !gLobbyRooms.empty())
                    {
                        gLobbyChatInputFocused = false;
                        gLobbySelectedRoom = std::min(static_cast<int>(gLobbyRooms.size()) - 1,
                            std::max(0, (mouseY - (top + 58)) / 38));
                    }
                    else if (IsPointInRect(mouseX, mouseY, chatLeft + chatWidth - 76, inputTop - 5, 76, 42)
                             && !gLobbyChatInput.empty())
                    {
                        if (lobbyClient.SendCommand("CHAT " + gLobbyChatInput))
                            gLobbyChatInput.clear();
                        gLobbyChatInputFocused = true;
                    }
                    else if (IsPointInRect(mouseX, mouseY, chatLeft, inputTop, chatWidth, 32))
                    {
                        SDL_StartTextInput();
                        gLobbyChatInputFocused = true;
                    }
                }
                else if (frontScreen == FrontScreen::HostRaceSetup)
                {
                    const int panelLeft = drawableWidth / 2 - 280;
                    const int panelTop = 72;
                    const int option = (mouseY - (panelTop + 82)) / 46;
                    if (option >= 0 && option < 8
                        && IsPointInRect(mouseX, mouseY, panelLeft + 18, panelTop + 82 + option * 46, 524, 36))
                    {
                        gHostSetupSelection = option;
                        if (option == 6)
                        {
                            const std::string command = "CREATE OPEN RACE|" + std::to_string(gHostRaceMode)
                                + "|" + std::to_string(gHostTrackIndex) + "|" + std::to_string(gHostLapCount)
                                + "|" + std::to_string(gHostPlayerCapacity) + "|"
                                + std::to_string(gHostRivalCount) + "|" + (gHostWeaponsAllowed ? "1" : "0");
                            gHostCreatePending = lobbyClient.SendCommand(command);
                            gLobbyStatus = gHostCreatePending ? "CREATING ROOM" : "SERVER UNAVAILABLE";
                        }
                        else if (option == 7)
                            frontScreen = FrontScreen::Multiplayer;
                    }
                }
                else if (frontScreen == FrontScreen::HowToPlay)
                    frontScreen = FrontScreen::Welcome;
                else if (frontScreen == FrontScreen::Settings)
                {
                    if (mouseY >= 130 && mouseY < 220)
                    {
                        gDisplayNameSetupConnectsToLobby = false;
                        frontScreen = FrontScreen::DisplayNameSetup;
                        SDL_StartTextInput();
                    }
                    else if (mouseY >= 260 && mouseY < 320)
                    {
                        cameraDistanceSetting = std::max(0, std::min(2, (mouseX - (drawableWidth / 2 - 150)) / 104));
                        savePreferences();
                    }
                    else if (mouseY >= 370 && mouseY < 450)
                    {
                        audioFeedback.SetEnabled(!audioFeedback.Enabled());
                        savePreferences();
                    }
                    else if (mouseY >= 480 && mouseY < 570)
                        frontScreen = FrontScreen::Welcome;
                }
                else if (frontScreen == FrontScreen::DisplayNameSetup)
                {
                    const int panelLeft = (drawableWidth - 520) / 2;
                    const int panelTop = drawableHeight / 2 - 140;
                    if (IsPointInRect(mouseX, mouseY, panelLeft + 172, panelTop + 196, 176, 42)
                        && !gPlayerDisplayName.empty())
                    {
                        savePreferences();
                        SDL_StopTextInput();
                        if (gDisplayNameSetupConnectsToLobby)
                        {
                            frontScreen = FrontScreen::Multiplayer;
                            connectLobby();
                        }
                        else
                            frontScreen = FrontScreen::Settings;
                    }
                }
                else if (frontScreen == FrontScreen::LocalSetup)
                {
                    const int option = (mouseY - 120) / 48;
                    if (option >= 0 && option < 8
                        && IsPointInRect(mouseX, mouseY, drawableWidth / 2 - 250, 120 + option * 48, 500, 52))
                    {
                        localSetupSelection = option;
                        if (option == 0)
                            startLocalRace();
                        else if (option == 7)
                            frontScreen = FrontScreen::Welcome;
                        else
                            changeLocalSetupOption(option, 1);
                    }
                }
                else if (pauseMenuOpen)
                {
                    const int left = (drawableWidth - 340) / 2;
                    const int top = (drawableHeight - 236) / 2;
                    const int option = (mouseY - (top + 82)) / 44;
                    if (option >= 0 && option < 3
                        && IsPointInRect(mouseX, mouseY, left + 24, top + 82 + option * 44, 292, 34))
                    {
                        pauseMenuSelection = option;
                        if (option == 0)
                            pauseMenuOpen = false;
                        else if (option == 1)
                            returnToMainMenu();
                        else
                            running = false;
                    }
                }
                else if (winner != 0 && !continueDriving)
                {
                    const int panelHeight = raceMode == RaceMode::Championship ? 310 : 268;
                    const int left = (drawableWidth - 420) / 2;
                    const int top = (drawableHeight - panelHeight) / 2 - 10;
                    const int actionsTop = top + (raceMode == RaceMode::Championship ? 248 : 198);
                    if (IsPointInRect(mouseX, mouseY, left + 24, actionsTop, 178, 38))
                    {
                        resultSelection = 0;
                        advanceResult();
                    }
                    else if (IsPointInRect(mouseX, mouseY, left + 218, actionsTop, 178, 38))
                    {
                        resultSelection = 1;
                        returnToMainMenu();
                    }
                }
                continue;
            }
            if (frontScreen != FrontScreen::RaceSetup && event.type == SDL_KEYDOWN)
            {
                if (event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_DOWN
                    || event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_RIGHT)
                    audioFeedback.PlayMenuMove();
                else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)
                    audioFeedback.PlayMenuConfirm();
            }
            if (frontScreen == FrontScreen::Multiplayer && gLobbyChatInputFocused
                && event.type == SDL_TEXTINPUT)
            {
                if (gLobbyChatInput.size() + std::strlen(event.text.text) <= 120)
                    gLobbyChatInput += event.text.text;
                continue;
            }
            if (frontScreen == FrontScreen::DisplayNameSetup && event.type == SDL_TEXTINPUT)
            {
                AppendLobbyNameText(gPlayerDisplayName, event.text.text);
                continue;
            }
            if (frontScreen != FrontScreen::RaceSetup)
            {
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                {
                    if (frontScreen == FrontScreen::Multiplayer || frontScreen == FrontScreen::HostRaceSetup || frontScreen == FrontScreen::OnlineRace || frontScreen == FrontScreen::DisplayNameSetup || frontScreen == FrontScreen::HowToPlay || frontScreen == FrontScreen::Settings
                        || frontScreen == FrontScreen::LocalSetup)
                    {
                        if (frontScreen == FrontScreen::Multiplayer || frontScreen == FrontScreen::HostRaceSetup)
                            leaveLobby();
                        if (frontScreen == FrontScreen::OnlineRace)
                        {
                            lobbyClient.SendCommand("LEAVE");
                            gOnlineRaceRoomId = 0;
                            gOnlineRaceTick = 0;
                            gOnlineTargetLaps = 0;
                            gOnlineRacers.clear();
                            SDL_StartTextInput();
                            frontScreen = FrontScreen::Multiplayer;
                        }
                        else
                            frontScreen = FrontScreen::Welcome;
                    }
                    else
                        running = false;
                }
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::Multiplayer
                         && gLobbyChatInputFocused)
                {
                    if (event.key.keysym.sym == SDLK_BACKSPACE && !gLobbyChatInput.empty())
                        gLobbyChatInput.erase(gLobbyChatInput.size() - 1);
                    else if ((event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)
                             && !gLobbyChatInput.empty())
                    {
                        if (lobbyClient.SendCommand("CHAT " + gLobbyChatInput))
                            gLobbyChatInput.clear();
                    }
                }
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::Multiplayer
                         && !gLobbyChatInputFocused)
                {
                    if (event.key.keysym.sym == SDLK_UP && !gLobbyRooms.empty())
                        gLobbySelectedRoom = (gLobbySelectedRoom + static_cast<int>(gLobbyRooms.size()) - 1)
                            % static_cast<int>(gLobbyRooms.size());
                    else if (event.key.keysym.sym == SDLK_DOWN && !gLobbyRooms.empty())
                        gLobbySelectedRoom = (gLobbySelectedRoom + 1) % static_cast<int>(gLobbyRooms.size());
                }
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::DisplayNameSetup)
                {
                    if (event.key.keysym.sym == SDLK_BACKSPACE && !gPlayerDisplayName.empty())
                        gPlayerDisplayName.erase(gPlayerDisplayName.size() - 1);
                    else if ((event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)
                             && !gPlayerDisplayName.empty())
                    {
                        savePreferences();
                        SDL_StopTextInput();
                        if (gDisplayNameSetupConnectsToLobby)
                        {
                            frontScreen = FrontScreen::Multiplayer;
                            connectLobby();
                        }
                        else
                            frontScreen = FrontScreen::Settings;
                    }
                }
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::HostRaceSetup)
                {
                    if (event.key.keysym.sym == SDLK_UP)
                        gHostSetupSelection = (gHostSetupSelection + 7) % 8;
                    else if (event.key.keysym.sym == SDLK_DOWN)
                        gHostSetupSelection = (gHostSetupSelection + 1) % 8;
                    else if (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_RIGHT)
                    {
                        const int direction = event.key.keysym.sym == SDLK_LEFT ? -1 : 1;
                        if (gHostSetupSelection == 0)
                            gHostRaceMode = (gHostRaceMode + direction + 4) % 4;
                        else if (gHostSetupSelection == 1)
                            gHostTrackIndex = (gHostTrackIndex + direction + 3) % 3;
                        else if (gHostSetupSelection == 2)
                            gHostLapCount = direction > 0 ? (gHostLapCount == 5 ? 1 : gHostLapCount + 1)
                                                               : (gHostLapCount == 1 ? 5 : gHostLapCount - 1);
                        else if (gHostSetupSelection == 3)
                            gHostPlayerCapacity = direction > 0
                                ? (gHostPlayerCapacity == 8 ? 2 : gHostPlayerCapacity + 1)
                                : (gHostPlayerCapacity == 2 ? 8 : gHostPlayerCapacity - 1);
                        else if (gHostSetupSelection == 4)
                            gHostRivalCount = (gHostRivalCount + direction + 8) % 8;
                        else if (gHostSetupSelection == 5)
                            gHostWeaponsAllowed = !gHostWeaponsAllowed;
                    }
                    else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)
                    {
                        if (gHostSetupSelection == 6)
                        {
                            const std::string command = "CREATE OPEN RACE|" + std::to_string(gHostRaceMode)
                                + "|" + std::to_string(gHostTrackIndex) + "|" + std::to_string(gHostLapCount)
                                + "|" + std::to_string(gHostPlayerCapacity) + "|"
                                + std::to_string(gHostRivalCount) + "|" + (gHostWeaponsAllowed ? "1" : "0");
                            gHostCreatePending = lobbyClient.SendCommand(command);
                            gLobbyStatus = gHostCreatePending ? "CREATING ROOM" : "SERVER UNAVAILABLE";
                        }
                        else if (gHostSetupSelection == 7)
                            frontScreen = FrontScreen::Multiplayer;
                    }
                }
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::HowToPlay
                         && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER))
                    frontScreen = FrontScreen::Welcome;
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::Settings
                         && event.key.keysym.sym == SDLK_UP)
                    settingsSelection = (settingsSelection + 3) % 4;
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::Settings
                         && event.key.keysym.sym == SDLK_DOWN)
                    settingsSelection = (settingsSelection + 1) % 4;
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::Settings
                         && (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_RIGHT))
                {
                    const int direction = event.key.keysym.sym == SDLK_LEFT ? -1 : 1;
                    if (settingsSelection == 1)
                        cameraDistanceSetting = (cameraDistanceSetting + direction + 3) % 3;
                    else if (settingsSelection == 2)
                        audioFeedback.SetEnabled(!audioFeedback.Enabled());
                    savePreferences();
                }
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::Settings
                         && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)
                    && settingsSelection == 0)
                {
                    gDisplayNameSetupConnectsToLobby = false;
                    frontScreen = FrontScreen::DisplayNameSetup;
                    SDL_StartTextInput();
                }
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::Settings
                         && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)
                         && settingsSelection == 3)
                    frontScreen = FrontScreen::Welcome;
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::LocalSetup)
                {
                    if (event.key.keysym.sym == SDLK_UP)
                        localSetupSelection = (localSetupSelection + 7) % 8;
                    else if (event.key.keysym.sym == SDLK_DOWN)
                        localSetupSelection = (localSetupSelection + 1) % 8;
                    else if (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_RIGHT)
                    {
                        const int direction = event.key.keysym.sym == SDLK_LEFT ? -1 : 1;
                        changeLocalSetupOption(localSetupSelection, direction);
                    }
                    else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)
                    {
                        if (localSetupSelection == 0)
                            startLocalRace();
                        else if (localSetupSelection == 7)
                            frontScreen = FrontScreen::Welcome;
                    }
                }
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::Welcome)
                {
                    if (event.key.keysym.sym == SDLK_UP)
                        frontSelection = (frontSelection + 4) % 5;
                    else if (event.key.keysym.sym == SDLK_DOWN)
                        frontSelection = (frontSelection + 1) % 5;
                    else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)
                    {
                        if (frontSelection == 0)
                        {
                            frontScreen = FrontScreen::LocalSetup;
                            localSetupSelection = 0;
                            loadTrack(false);
                        }
                        else if (frontSelection == 1)
                        {
                            openMultiplayer();
                        }
                        else if (frontSelection == 2)
                            frontScreen = FrontScreen::HowToPlay;
                        else if (frontSelection == 3)
                        {
                            frontScreen = FrontScreen::Settings;
                            settingsSelection = 0;
                        }
                        else
                            running = false;
                    }
                }
                continue;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
            {
                pauseMenuOpen = !pauseMenuOpen;
                pauseMenuSelection = 0;
                continue;
            }
            if (pauseMenuOpen)
            {
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_UP)
                    pauseMenuSelection = (pauseMenuSelection + 2) % 3;
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_DOWN)
                    pauseMenuSelection = (pauseMenuSelection + 1) % 3;
                else if (event.type == SDL_KEYDOWN
                         && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER))
                {
                    if (pauseMenuSelection == 0)
                        pauseMenuOpen = false;
                    else if (pauseMenuSelection == 1)
                        returnToMainMenu();
                    else
                        running = false;
                }
                continue;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_c && winner != 0)
                continueDriving = true;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_m && winner != 0)
                returnToMainMenu();
            if (event.type == SDL_KEYDOWN && winner != 0 && !continueDriving
                && (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_RIGHT))
            {
                resultSelection = 1 - resultSelection;
                continue;
            }
            if (event.type == SDL_KEYDOWN
                && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)
                && winner != 0 && !continueDriving)
            {
                if (resultSelection == 0)
                    advanceResult();
                else
                    returnToMainMenu();
                continue;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r && winner != 0)
                advanceResult();
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_v)
                steeringAssistEnabled = !steeringAssistEnabled;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_b)
                brakingAssistEnabled = !brakingAssistEnabled;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_x && raceStart.Started()
                && winner == 0)
            {
                const RaceGate& recoveryGate = race.Progress().mNextCheckpoint
                    < static_cast<int>(checkpoints.size())
                    ? checkpoints[race.Progress().mNextCheckpoint] : finish;
                HovercraftState recoveryState = hovercraft.State();
                if (RecoverHovercraftToRoute(recoveryState, course, recoveryGate))
                    hovercraft.Reset(recoveryState);
            }
            if (event.type == SDL_CONTROLLERDEVICEADDED && controller == nullptr
                && SDL_IsGameController(event.cdevice.which))
                controller = SDL_GameControllerOpen(event.cdevice.which);
            if (event.type == SDL_CONTROLLERDEVICEREMOVED && controller != nullptr
                && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller)) == event.cdevice.which)
            {
                SDL_GameControllerClose(controller);
                controller = nullptr;
            }
        }

        const Uint64 currentTick = SDL_GetPerformanceCounter();
        const double frameSeconds = (currentTick - previousTick) / tickFrequency;
        previousTick = currentTick;
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        const bool shiftPressed = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
        const bool downPressed = keys[SDL_SCANCODE_DOWN];
        HovercraftInput input;
        input.mThrottle = (shiftPressed || keys[SDL_SCANCODE_W] ? 1.0 : 0.0)
            - (keys[SDL_SCANCODE_S] || downPressed ? 1.0 : 0.0);
        input.mSteering = (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT] ? 1.0 : 0.0)
            - (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT] ? 1.0 : 0.0);
        input.mJump = keys[SDL_SCANCODE_UP];
        input.mFire = keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL];
        input.mReverseFacing = shiftPressed && downPressed;
        if (controller != nullptr)
        {
            input.mSteering += ControllerAxis(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX));
            input.mThrottle += SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 32767.0;
            input.mThrottle -= SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 32767.0;
            input.mJump = input.mJump || SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B);
            input.mFire = input.mFire || SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_X);
        }
        if (frontScreen == FrontScreen::OnlineRace)
        {
            onlineInputSeconds += std::fmin(frameSeconds, 0.1);
            if (onlineInputSeconds >= 1.0 / 30.0)
            {
                std::ostringstream command;
                command << "INPUT " << input.mThrottle << '|' << input.mSteering << '|'
                        << (input.mJump ? 1 : 0) << '|' << (input.mReverseFacing ? 1 : 0);
                lobbyClient.SendCommand(command.str());
                onlineInputSeconds = 0.0;
            }
        }
        const int simulationSteps = frontScreen == FrontScreen::RaceSetup && !pauseMenuOpen
            ? simulationClock.Consume(frameSeconds) : 0;
        for (int step = 0; step < simulationSteps; ++step)
        {
            const double seconds = simulationClock.StepSeconds();
            impactSoundCooldown = std::fmax(0.0, impactSoundCooldown - seconds);
            raceStart.Update(seconds);
            if (raceStart.Started() && (winner == 0 || continueDriving))
            {
                const RaceGate& simulationGate = race.Progress().mNextCheckpoint
                    < static_cast<int>(checkpoints.size())
                    ? checkpoints[race.Progress().mNextCheckpoint] : finish;
                HovercraftInput playerInput = steeringAssistEnabled
                    ? ApplySteeringAssist(input, hovercraft.State(), simulationGate) : input;
                if (brakingAssistEnabled)
                    playerInput = ApplyBrakingAssist(playerInput, hovercraft.State(), simulationGate);
                hovercraft.Step(playerInput, seconds);
                activeRecording.Record(playerInput);
                if (weaponsAllowed && playerInput.mFire && !fireHeld && missile.Fire(hovercraft.State()))
                    audioFeedback.PlayBoost();
                fireHeld = playerInput.mFire;
                if (ghostActive)
                {
                    HovercraftInput ghostInput;
                    if (ghostRecording.InputAt(ghostFrame, ghostInput))
                    {
                        replayGhost.Step(ghostInput, seconds);
                        BounceOffCourseWall(replayGhost, course);
                        ApplyRaisedSections(replayGhost, raisedSections);
                        ApplyMines(replayGhost, mines);
                        ++ghostFrame;
                    }
                    else
                        ghostActive = false;
                }
                if (RaceModeUsesRivals(raceMode))
                {
                    for (int rivalIndex = 0; rivalIndex < rivalCount; ++rivalIndex)
                    {
                        rivalControllers[rivalIndex].Update(rivals[rivalIndex].State());
                        HovercraftInput rivalInput = rivalControllers[rivalIndex].InputFor(
                            rivals[rivalIndex].State());
                        rivalInput.mJump = ShouldJumpRaisedSection(rivals[rivalIndex].State(),
                                                                   raisedSections);
                        rivals[rivalIndex].Step(rivalInput, seconds);
                    }
                    for (int rivalIndex = 0; rivalIndex < rivalCount; ++rivalIndex)
                    {
                        HovercraftState playerState = hovercraft.State();
                        HovercraftState rivalState = rivals[rivalIndex].State();
                        if (ResolveRacerCollision(playerState, rivalState))
                        {
                            hovercraft.Reset(playerState);
                            rivals[rivalIndex].Reset(rivalState);
                        }
                    }
                    for (int firstRival = 0; firstRival < rivalCount; ++firstRival)
                    {
                        for (int secondRival = firstRival + 1; secondRival < rivalCount; ++secondRival)
                        {
                            HovercraftState firstState = rivals[firstRival].State();
                            HovercraftState secondState = rivals[secondRival].State();
                            if (ResolveRacerCollision(firstState, secondState))
                            {
                                rivals[firstRival].Reset(firstState);
                                rivals[secondRival].Reset(secondState);
                            }
                        }
                    }
                    for (int rivalIndex = 0; rivalIndex < rivalCount; ++rivalIndex)
                    {
                        Hovercraft& rival = rivals[rivalIndex];
                        BounceOffCourseWall(rival, course);
                        ApplyRaisedSections(rival, raisedSections);
                        ApplyMines(rival, mines);
                        ApplyHazardZones(rival, hazardZones, seconds);
                    }
                }
                if (weaponsAllowed)
                {
                    missile.Step(seconds, course);
                    HovercraftState playerState = hovercraft.State();
                    if (missile.ApplyHit(playerState))
                    {
                        hovercraft.Reset(playerState);
                        audioFeedback.PlayImpact();
                    }
                    for (int rivalIndex = 0; rivalIndex < kRivalCount; ++rivalIndex)
                    {
                        HovercraftState rivalState = rivals[rivalIndex].State();
                        if (missile.ApplyHit(rivalState))
                        {
                            rivals[rivalIndex].Reset(rivalState);
                            audioFeedback.PlayImpact();
                        }
                    }
                }
                if (BounceOffCourseWall(hovercraft, course) && impactSoundCooldown <= 0.0)
                {
                    audioFeedback.PlayImpact();
                    impactSoundCooldown = 0.18;
                }
                ApplyRaisedSections(hovercraft, raisedSections);
                if (ApplyMines(hovercraft, mines) && impactSoundCooldown <= 0.0)
                {
                    audioFeedback.PlayImpact();
                    impactSoundCooldown = 0.18;
                }
                ApplyHazardZones(hovercraft, hazardZones, seconds);
            }

            const HovercraftState& simulationState = hovercraft.State();
            if (raceStart.Started() && winner == 0)
            {
                const int previousCheckpoint = race.Progress().mNextCheckpoint;
                race.Update(simulationState.mX, simulationState.mY, seconds);
                if (race.Progress().mNextCheckpoint != previousCheckpoint)
                    audioFeedback.PlayCheckpoint();
                lapTimer.Update(race.Progress());
                if (RaceModeUsesRivals(raceMode))
                {
                    for (int rivalIndex = 0; rivalIndex < rivalCount; ++rivalIndex)
                    {
                        const HovercraftState& rivalState = rivals[rivalIndex].State();
                        rivalRaces[rivalIndex].Update(rivalState.mX, rivalState.mY, seconds);
                    }
                }
            }
        }
        const HovercraftState& state = hovercraft.State();
        if (winner == 0 && RaceModeHasFinish(raceMode))
        {
            if (race.Progress().mFinished)
                winner = 1;
            else if (RaceModeUsesRivals(raceMode))
            {
                for (int rivalIndex = 0; rivalIndex < rivalCount; ++rivalIndex)
                {
                    if (rivalRaces[rivalIndex].Progress().mFinished)
                    {
                        winner = 2;
                        break;
                    }
                }
            }
        }
        std::vector<RaceProgress> raceProgresses;
        raceProgresses.push_back(race.Progress());
        std::vector<HovercraftState> rivalStates;
        for (int rivalIndex = 0; rivalIndex < rivalCount; ++rivalIndex)
        {
            raceProgresses.push_back(rivalRaces[rivalIndex].Progress());
            rivalStates.push_back(rivals[rivalIndex].State());
        }
        const int playerPosition = RaceModeUsesRivals(raceMode)
            ? CalculateRacePosition(raceProgresses, 0) : 1;
        const RaceGate& activeGate = race.Progress().mNextCheckpoint < static_cast<int>(checkpoints.size())
            ? checkpoints[race.Progress().mNextCheckpoint] : finish;
        const bool wrongWay = !race.Progress().mFinished && IsHeadingAwayFromGate(state, activeGate);
        if (raceMode == RaceMode::Championship && winner != 0 && !championship.EventRecorded())
        {
            std::vector<int> eventPositions;
            for (int competitorIndex = 0; competitorIndex < static_cast<int>(raceProgresses.size()); ++competitorIndex)
                eventPositions.push_back(CalculateRacePosition(raceProgresses, competitorIndex));
            championship.RecordResults(eventPositions);
        }
        char title[320];
        char championshipStandings[96];
        char championshipOverlayPoints[96];
        if (championship.CompetitorCount() == 3)
        {
            std::snprintf(championshipStandings, sizeof(championshipStandings),
                          "You %d | R1 %d | R2 %d", championship.CompetitorPoints(0),
                          championship.CompetitorPoints(1), championship.CompetitorPoints(2));
            std::snprintf(championshipOverlayPoints, sizeof(championshipOverlayPoints),
                          "YOU %d R1 %d R2 %d", championship.CompetitorPoints(0),
                          championship.CompetitorPoints(1), championship.CompetitorPoints(2));
        }
        else if (championship.CompetitorCount() == 2)
        {
            std::snprintf(championshipStandings, sizeof(championshipStandings), "You %d | R1 %d",
                          championship.CompetitorPoints(0), championship.CompetitorPoints(1));
            std::snprintf(championshipOverlayPoints, sizeof(championshipOverlayPoints), "YOU %d R1 %d",
                          championship.CompetitorPoints(0), championship.CompetitorPoints(1));
        }
        else
        {
            std::snprintf(championshipStandings, sizeof(championshipStandings), "You %d",
                          championship.PlayerPoints());
            std::snprintf(championshipOverlayPoints, sizeof(championshipOverlayPoints), "YOU %d",
                          championship.PlayerPoints());
        }
        if (frontScreen == FrontScreen::OnlineRace)
            std::snprintf(title, sizeof(title), "OpenHover | Online Race | Server tick %u", gOnlineRaceTick);
        else if (frontScreen != FrontScreen::RaceSetup)
            std::snprintf(title, sizeof(title), "OpenHover | Welcome");
        else if (raceStart.Ready())
        {
            if (showControls)
                std::snprintf(title, sizeof(title), "OpenHover | R start | T track | M mode | C craft | L laps | 0/1/2 rivals | D difficulty | V/B assists | F1 close");
            else if (raceMode == RaceMode::Championship)
                std::snprintf(title, sizeof(title), "OpenHover | Championship %d/%d | %s | %d laps | %d rivals | %s AI | %s craft | R start | F1 controls",
                              championship.CurrentEvent() + 1, championship.EventCount(),
                              championshipStandings, targetLaps, rivalCount,
                              RivalDifficultyName(rivalDifficulty), CraftClassName(playerCraftClass));
            else
                std::snprintf(title, sizeof(title), "OpenHover | %s | %s | %d laps | %d rivals | %s AI | %s craft | Assist S:%s B:%s | R start | F1 controls",
                              selectedTrack.mName.c_str(), RaceModeName(raceMode), targetLaps, rivalCount,
                              RivalDifficultyName(rivalDifficulty), CraftClassName(playerCraftClass),
                              steeringAssistEnabled ? "on" : "off",
                              brakingAssistEnabled ? "on" : "off");
        }
        else if (raceStart.CountdownActive())
            std::snprintf(title, sizeof(title), "OpenHover | Starting | %d lights", raceStart.LightsLit());
        else if (winner == 1)
        {
            if (raceMode == RaceMode::Championship)
                std::snprintf(title, sizeof(title), "OpenHover | You finish P%d/%d: %.2fs | %s | Press R to continue",
                              playerPosition, rivalCount + 1, race.Progress().mElapsedSeconds,
                              championshipStandings);
            else
                std::snprintf(title, sizeof(title), "OpenHover | Finish P%d/%d: %.2fs | Press R to restart",
                              playerPosition, RaceModeUsesRivals(raceMode) ? rivalCount + 1 : 1,
                              race.Progress().mElapsedSeconds);
        }
        else if (winner == 2)
        {
            if (raceMode == RaceMode::Championship)
                std::snprintf(title, sizeof(title), "OpenHover | You finish P%d/%d | %s | Press R to continue",
                              playerPosition, rivalCount + 1, championshipStandings);
            else
                std::snprintf(title, sizeof(title), "OpenHover | Rival finishes first | You P%d/%d | Press R to restart",
                              playerPosition, RaceModeUsesRivals(raceMode) ? rivalCount + 1 : 1);
        }
        else if (raceMode == RaceMode::Practice)
            std::snprintf(title, sizeof(title), "OpenHover | %s | Practice | Speed %.1f",
                          selectedTrack.mName.c_str(), state.mSpeed);
        else
        {
            const LapTiming& lapTiming = lapTimer.Timing();
            if (race.Progress().mNextCheckpoint == static_cast<int>(checkpoints.size()))
                std::snprintf(title, sizeof(title), "OpenHover | P%d/%d | Lap %d/%d | Finish! | %.2fs | Split %.2fs",
                              playerPosition, RaceModeUsesRivals(raceMode) ? rivalCount + 1 : 1,
                              race.Progress().mCompletedLaps + 1, race.TargetLaps(),
                              lapTiming.mCurrentSeconds, lapTiming.mLastSplitSeconds);
            else
                std::snprintf(title, sizeof(title), "OpenHover | P%d/%d | Lap %d/%d | %s %d/%d | %.2fs | Split %.2fs",
                              playerPosition, RaceModeUsesRivals(raceMode) ? rivalCount + 1 : 1,
                              race.Progress().mCompletedLaps + 1, race.TargetLaps(),
                              wrongWay ? "Wrong way, checkpoint" : "Checkpoint",
                              race.Progress().mNextCheckpoint + 1, static_cast<int>(checkpoints.size()),
                              lapTiming.mCurrentSeconds, lapTiming.mLastSplitSeconds);
        }
        SDL_SetWindowTitle(window, title);

        int drawableWidth = 0;
        int drawableHeight = 0;
        SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);
        glViewport(0, 0, drawableWidth, drawableHeight);
        if (frontScreen != FrontScreen::RaceSetup && frontScreen != FrontScreen::OnlineRace)
        {
            DrawFrontScreen(frontScreen,
                            frontScreen == FrontScreen::LocalSetup ? localSetupSelection
                                : (frontScreen == FrontScreen::Settings ? settingsSelection
                                   : (frontScreen == FrontScreen::HostRaceSetup ? gHostSetupSelection : frontSelection)),
                            cameraDistanceSetting, audioFeedback.Enabled(), trackIndex, targetLaps, rivalCount,
                            rivalDifficulty, raceMode, weaponsAllowed,
                            drawableWidth, drawableHeight);
            SDL_GL_SwapWindow(window);
            continue;
        }
        if (frontScreen == FrontScreen::OnlineRace)
        {
            HovercraftState cameraState;
            for (const OnlineRacerView& racer : gOnlineRacers)
            {
                if (racer.mPlayerId == gLobbyPlayerId)
                {
                    cameraState = racer.mState;
                    break;
                }
            }
            const GLfloat atmosphere[] = {selectedTrack.mAtmosphereRed, selectedTrack.mAtmosphereGreen,
                              selectedTrack.mAtmosphereBlue, 1.0f};
            glFogfv(GL_FOG_COLOR, atmosphere);
            glClearColor(selectedTrack.mAtmosphereRed, selectedTrack.mAtmosphereGreen,
                         selectedTrack.mAtmosphereBlue, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            SetPerspective(static_cast<double>(drawableWidth) / drawableHeight, cameraState.mSpeed);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            const double cameraDistances[] = {4.6, 5.8, 7.4};
            SetChaseCamera(cameraState, cameraDistances[cameraDistanceSetting]);
            const GLfloat sunDirection[] = {-0.35f, 0.82f, 0.45f, 0.0f};
            glLightfv(GL_LIGHT0, GL_POSITION, sunDirection);
            DrawCourseGrid(cameraState);
            DrawTrackEnvironment(selectedTrack);
            DrawConnectedTrack(courseWaypoints, selectedTrack.mRoadHalfWidth,
                       selectedTrack.mRoadRed, selectedTrack.mRoadGreen, selectedTrack.mRoadBlue,
                       selectedTrack.mWallRed, selectedTrack.mWallGreen, selectedTrack.mWallBlue, roadTexture,
                       wallTexture);
            DrawFinishZone(courseWaypoints, selectedTrack.mRoadHalfWidth);
            DrawCheckpointGates(courseWaypoints, checkpoints, 0, selectedTrack.mRoadHalfWidth);
            const OnlineRacerView* localRacer = nullptr;
            for (std::size_t racerIndex = 0; racerIndex < gOnlineRacers.size(); ++racerIndex)
            {
                const OnlineRacerView& racer = gOnlineRacers[racerIndex];
                if (racer.mPlayerId == gLobbyPlayerId)
                    localRacer = &racer;
                DrawHovercraft(racer.mState, racer.mPlayerId != gLobbyPlayerId, false,
                               CraftClass::Balanced, static_cast<int>(racerIndex) + 1,
                               static_cast<int>(racerIndex));
            }
            if (localRacer != nullptr)
                DrawOnlineHud(*localRacer, static_cast<int>(gOnlineRacers.size()), gOnlineTargetLaps,
                              drawableWidth, drawableHeight);
            SDL_GL_SwapWindow(window);
            continue;
        }
        const GLfloat atmosphere[] = {selectedTrack.mAtmosphereRed, selectedTrack.mAtmosphereGreen,
                          selectedTrack.mAtmosphereBlue, 1.0f};
        glFogfv(GL_FOG_COLOR, atmosphere);
        glClearColor(selectedTrack.mAtmosphereRed, selectedTrack.mAtmosphereGreen,
                 selectedTrack.mAtmosphereBlue, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        SetPerspective(static_cast<double>(drawableWidth) / drawableHeight, state.mSpeed);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        const double cameraDistances[] = {4.6, 5.8, 7.4};
        SetChaseCamera(state, cameraDistances[cameraDistanceSetting]);
        const GLfloat sunDirection[] = {-0.35f, 0.82f, 0.45f, 0.0f};
        glLightfv(GL_LIGHT0, GL_POSITION, sunDirection);
        DrawCourseGrid(state);
        DrawTrackEnvironment(selectedTrack);
        DrawConnectedTrack(courseWaypoints, selectedTrack.mRoadHalfWidth,
                   selectedTrack.mRoadRed, selectedTrack.mRoadGreen, selectedTrack.mRoadBlue,
                   selectedTrack.mWallRed, selectedTrack.mWallGreen, selectedTrack.mWallBlue, roadTexture,
                   wallTexture);
        DrawFinishZone(courseWaypoints, selectedTrack.mRoadHalfWidth);
        DrawCheckpointGates(courseWaypoints, checkpoints, race.Progress().mNextCheckpoint,
                    selectedTrack.mRoadHalfWidth);
        for (const HazardZone& zone : hazardZones)
            DrawHazardZone(zone);
        for (const RaisedSection& section : raisedSections)
            DrawRaisedSection(section);
        for (const Mine& mine : mines)
            DrawMine(mine);
        DrawMissile(missile);

        if (RaceModeUsesRivals(raceMode))
        {
            for (int rivalIndex = 0; rivalIndex < static_cast<int>(rivalStates.size()); ++rivalIndex)
                DrawHovercraft(rivalStates[rivalIndex], true, false, CraftClass::Balanced, rivalIndex + 2);
        }
        if (ghostActive)
            DrawHovercraft(replayGhost.State(), false, true, playerCraftClass, 1);
        DrawHovercraft(state, false, false, playerCraftClass, 1);
        std::vector<RaceProgress> rivalProgresses(raceProgresses.begin() + 1, raceProgresses.end());
        DrawHud(race.Progress(), race.TargetLaps(), rivalProgresses, state, rivalStates, activeGate,
            courseWaypoints,
            RaceModeUsesRivals(raceMode), wrongWay, winner, playerPosition,
            RaceModeUsesRivals(raceMode) ? rivalCount + 1 : 1,
            raceMode == RaceMode::Championship, raceStart.LightsLit(), raceStart.CountdownActive(),
            !continueDriving,
            championshipOverlayPoints, race.Progress().mElapsedSeconds, lapTimer.Timing(), missile,
            weaponsAllowed, pauseMenuOpen, pauseMenuSelection,
            resultSelection,
            drawableWidth, drawableHeight);
        SDL_GL_SwapWindow(window);
    }

    if (controller != nullptr)
        SDL_GameControllerClose(controller);
    audioFeedback.Shutdown();
    glDeleteTextures(1, &roadTexture);
    glDeleteTextures(1, &wallTexture);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}