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
#include "GhostLibrary.h"
#include "InputRecording.h"
#include "LapTiming.h"
#include "Lobby.h"
#include "Missile.h"
#include "CameraRig.h"
#include "CrashReport.h"
#include "KeyBindings.h"
#include "PadBindings.h"
#include "PadMenuInput.h"
#include "PixelFont.h"
#include "PracticeGuide.h"
#include "Protocol.h"
#include "Race.h"
#include "RaceMode.h"
#include "RacePosition.h"
#include "RaceStart.h"
#include "RacerCollision.h"
#include "RecoveryAssist.h"
#include "ReconnectPolicy.h"
#include "RivalController.h"
#include "RivalNames.h"
#include "RouteTracker.h"
#include "StallDetector.h"
#include "RouteGuidance.h"
#include "SteeringAssist.h"
#include "TrackBuilder.h"
#include "TrackDefinition.h"
#include "TrackFile.h"
#include "TrackHash.h"
#include "TrackLoader.h"
#include "TcpLobbyClient.h"
#include "WallCollision.h"

#include <cmath>
#include <functional>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <sstream>
#include <algorithm>
#include <cctype>
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
    RaceSetup,
    TrackEditor
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
    std::string mCustomHash = "-";
    int mLapCount = 0;
    int mRivalCount = 0;
    bool mWeaponsAllowed = false;
    bool mPrivate = false;
    int mReadyPlayerCount = 0;
};

struct LobbyPlayerView
{
    int mId = 0;
    std::string mDisplayName;
    int mRoomId = 0;
    bool mHost = false;
    bool mReady = false;
};

struct OnlineRacerView
{
    int mPlayerId = 0;
    HovercraftState mState;
    CraftClass mCraftClass = CraftClass::Balanced;
    HovercraftState mPreviousState;
    bool mHasPreviousSnapshot = false;
    RaceProgress mProgress;
    LapTiming mLapTiming;
    int mPosition = 0;
};

struct OnlineMissileView
{
    int mPlayerId = 0;
    HovercraftState mState;
};

std::vector<LobbyPlayerView> gLobbyPlayers;
std::vector<LobbyRoomView> gLobbyRooms;
std::vector<std::string> gLobbyChatMessages;
std::string gLobbyChatInput;
bool gLobbyChatInputFocused = false;
std::vector<std::string> gOnlineChatMessages;
std::string gOnlineChatInput;
bool gOnlineChatInputFocused = false;
LobbyMuteList gLobbyMuteList;
std::string gPlayerDisplayName;
std::string gLobbyStatus = "CONNECTING TO SERVER";
ReconnectPolicy gReconnectPolicy;
std::string gLobbyServerVersion;
int gLobbyServerProtocol = 0;
int gLobbyServerContent = 0;
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
bool gHostPrivateRoom = false;
std::string gHostedRoomCode;
bool gLobbyReady = false;
bool gHostCreatePending = false;
// Hosting a custom track: the track is uploaded first (stage 1: header sent, 2: data sent), then
// the room is created.
int gHostUploadStage = 0;
std::string gHostUploadHex;
std::string gHostPendingCreate;
// A track being downloaded for the room this player joined.
struct TrackDownload
{
    bool mActive = false;
    int mRoomId = 0;
    std::size_t mBytes = 0;
    std::string mHash;
    std::string mHex;
};
TrackDownload gTrackDownload;
// The custom-track hash this player last told the server they have verified, and the hash whose
// download is in flight (to avoid asking twice).
std::string gHaveTrackHashSent;
std::string gTrackRequestedHash;

std::string EncodeHex(const std::string& pBytes)
{
    static const char* const digits = "0123456789abcdef";
    std::string out;
    out.reserve(pBytes.size() * 2);
    for (unsigned char c : pBytes)
    {
        out += digits[c >> 4];
        out += digits[c & 15];
    }
    return out;
}

bool DecodeHex(const std::string& pHex, std::string& pOut)
{
    if (pHex.size() % 2 != 0)
        return false;
    pOut.clear();
    for (std::size_t index = 0; index < pHex.size(); index += 2)
    {
        int value = 0;
        for (int nibble = 0; nibble < 2; ++nibble)
        {
            const char c = pHex[index + nibble];
            int digit = 0;
            if (c >= '0' && c <= '9')
                digit = c - '0';
            else if (c >= 'a' && c <= 'f')
                digit = c - 'a' + 10;
            else
                return false;
            value = value * 16 + digit;
        }
        pOut += static_cast<char>(value);
    }
    return true;
}
bool gDisplayNameSetupConnectsToLobby = false;
int gOnlineRaceRoomId = 0;
unsigned int gOnlineRaceTick = 0;
int gOnlineTargetLaps = 0;
std::vector<OnlineRacerView> gOnlineRacers;
Uint32 gOnlineRaceSnapshotTicks = 0;
std::vector<OnlineMissileView> gOnlineMissiles;
std::vector<bool> gOnlineMineTriggered;
double gOnlineHudElapsedSeconds = 0.0;
Uint32 gOnlineHudSnapshotTicks = 0;
bool gOnlineRecoveryRequested = false;
int gOnlineStartLights = 0;
bool gOnlineCountdownActive = false;
int gOnlineCountdownSeconds = 0;
int gOnlineLastCheckpoint = -1;
int gOnlineLastCompletedLaps = -1;
bool gOnlineCheckpointCue = false;
bool gOnlineHasBoostSnapshot = false;
bool gOnlineBoostActive = false;
bool gOnlineSpinOutActive = false;
bool gOnlineBoostCue = false;
bool gOnlineImpactCue = false;
Uint32 gOnlineCheckpointVisualUntil = 0;
Uint32 gOnlineBoostVisualUntil = 0;
Uint32 gOnlineImpactVisualUntil = 0;
bool gOnlineRaceFinished = false;
int gOnlineChampionshipEvent = 0;
int gOnlineChampionshipEventCount = 0;
int gOnlineChampionshipPoints = 0;

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
        if (fields.size() == 6 && fields[0] == "P")
            gLobbyPlayers.push_back({std::atoi(fields[1].c_str()), fields[2],
                                     std::atoi(fields[3].c_str()),
                                     std::atoi(fields[4].c_str()) != 0,
                                     std::atoi(fields[5].c_str()) != 0});
        else if (fields.size() == 15 && fields[0] == "R")
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
            room.mPrivate = std::atoi(fields[12].c_str()) != 0;
            room.mReadyPlayerCount = std::atoi(fields[13].c_str());
            room.mCustomHash = fields[14];
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
    std::vector<OnlineMissileView> missiles;
    std::vector<bool> mineTriggered;
    for (std::size_t index = 2; index < entries.size(); ++index)
    {
        const std::vector<std::string> fields = SplitLobbyField(entries[index], ',');
        if (fields.size() == 10 && fields[0] == "R")
        {
            OnlineRacerView racer;
            racer.mPlayerId = std::atoi(fields[1].c_str());
            racer.mState.mX = std::strtod(fields[2].c_str(), nullptr);
            racer.mState.mY = std::strtod(fields[3].c_str(), nullptr);
            racer.mState.mHeading = std::strtod(fields[4].c_str(), nullptr);
            racer.mState.mTravelHeading = racer.mState.mHeading;
            racer.mState.mSpeed = std::strtod(fields[5].c_str(), nullptr);
            racer.mState.mHeight = std::strtod(fields[6].c_str(), nullptr);
            racer.mState.mBoosting = std::atoi(fields[7].c_str()) != 0;
            racer.mState.mSpinOutSeconds = std::strtod(fields[8].c_str(), nullptr);
            const int craftClass = std::atoi(fields[9].c_str());
            if (craftClass < static_cast<int>(CraftClass::Balanced)
                || craftClass > static_cast<int>(CraftClass::Control))
            {
                return false;
            }
            racer.mCraftClass = static_cast<CraftClass>(craftClass);
            racer.mState.mPreviousX = racer.mState.mX;
            racer.mState.mPreviousY = racer.mState.mY;
            racer.mState.mHasPreviousPosition = true;
            if (racer.mPlayerId <= 0)
                return false;
            for (const OnlineRacerView& previous : gOnlineRacers)
            {
                if (previous.mPlayerId == racer.mPlayerId)
                {
                    racer.mPreviousState = previous.mState;
                    racer.mHasPreviousSnapshot = true;
                    break;
                }
            }
            if (racer.mPlayerId == gLobbyPlayerId)
            {
                if (gOnlineHasBoostSnapshot)
                {
                    if (racer.mState.mBoosting && !gOnlineBoostActive)
                        gOnlineBoostCue = true;
                    if (racer.mState.mSpinOutSeconds > 0.0 && !gOnlineSpinOutActive)
                        gOnlineImpactCue = true;
                }
                gOnlineHasBoostSnapshot = true;
                gOnlineBoostActive = racer.mState.mBoosting;
                gOnlineSpinOutActive = racer.mState.mSpinOutSeconds > 0.0;
            }
            racers.push_back(racer);
        }
        else if (fields.size() == 7 && fields[0] == "M")
        {
            OnlineMissileView missile;
            missile.mPlayerId = std::atoi(fields[1].c_str());
            missile.mState.mX = std::strtod(fields[2].c_str(), nullptr);
            missile.mState.mY = std::strtod(fields[3].c_str(), nullptr);
            missile.mState.mHeading = std::strtod(fields[4].c_str(), nullptr);
            missile.mState.mTravelHeading = missile.mState.mHeading;
            missile.mState.mSpeed = std::strtod(fields[5].c_str(), nullptr);
            missile.mState.mHeight = std::strtod(fields[6].c_str(), nullptr);
            missiles.push_back(missile);
        }
        else if (fields.size() == 3 && fields[0] == "N")
        {
            const int mineIndex = std::atoi(fields[1].c_str());
            const int triggered = std::atoi(fields[2].c_str());
            if (mineIndex < 0 || (triggered != 0 && triggered != 1))
                return false;
            if (mineTriggered.size() <= static_cast<std::size_t>(mineIndex))
                mineTriggered.resize(static_cast<std::size_t>(mineIndex) + 1, false);
            mineTriggered[mineIndex] = triggered != 0;
        }
        else
            return false;
    }
    gOnlineRaceRoomId = roomId;
    gOnlineRaceTick = static_cast<unsigned int>(tick);
    gOnlineRacers.swap(racers);
    gOnlineMissiles.swap(missiles);
    gOnlineMineTriggered.swap(mineTriggered);
    gOnlineRaceSnapshotTicks = SDL_GetTicks();
    return true;
}

double InterpolateOnlineHeading(double pFrom, double pTo, double pFraction)
{
    double delta = pTo - pFrom;
    while (delta > kPi)
        delta -= 2.0 * kPi;
    while (delta < -kPi)
        delta += 2.0 * kPi;
    return pFrom + delta * pFraction;
}

HovercraftState InterpolatedOnlineState(const OnlineRacerView& pRacer)
{
    if (!pRacer.mHasPreviousSnapshot)
        return pRacer.mState;
    const double fraction = std::min(1.0, (SDL_GetTicks() - gOnlineRaceSnapshotTicks) / 34.0);
    HovercraftState state = pRacer.mPreviousState;
    state.mX += (pRacer.mState.mX - state.mX) * fraction;
    state.mY += (pRacer.mState.mY - state.mY) * fraction;
    state.mHeight += (pRacer.mState.mHeight - state.mHeight) * fraction;
    state.mSpeed += (pRacer.mState.mSpeed - state.mSpeed) * fraction;
    state.mHeading = InterpolateOnlineHeading(state.mHeading, pRacer.mState.mHeading, fraction);
    state.mTravelHeading = state.mHeading;
    state.mBoosting = pRacer.mState.mBoosting;
    state.mSpinOutSeconds = pRacer.mState.mSpinOutSeconds;
    return state;
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
        if (fields.size() == 4 && fields[0] == "S")
        {
            gOnlineStartLights = std::atoi(fields[1].c_str());
            gOnlineCountdownActive = std::atoi(fields[2].c_str()) != 0;
            gOnlineCountdownSeconds = std::atoi(fields[3].c_str());
            continue;
        }
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
            if (playerId == gLobbyPlayerId)
            {
                if (gOnlineLastCheckpoint >= 0
                    && (gOnlineLastCheckpoint != racer.mProgress.mNextCheckpoint
                        || gOnlineLastCompletedLaps != racer.mProgress.mCompletedLaps))
                {
                    gOnlineCheckpointCue = true;
                }
                gOnlineLastCheckpoint = racer.mProgress.mNextCheckpoint;
                gOnlineLastCompletedLaps = racer.mProgress.mCompletedLaps;
                gOnlineHudElapsedSeconds = racer.mProgress.mElapsedSeconds;
                gOnlineHudSnapshotTicks = SDL_GetTicks();
            }
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

bool LocalSetupOptionVisible(RaceMode pRaceMode, int pOption)
{
    if (pOption == 3)
        return RaceModeHasFinish(pRaceMode);
    if (pOption == 4 || pOption == 5)
        return RaceModeUsesRivals(pRaceMode);
    return pOption >= 0 && pOption < 10;
}

// Where a row of the main menu goes. Rows shrink on short windows so the menu clears the title
// above it and the key hints below it.
void WelcomeRowGeometry(int pWidth, int pHeight, int pOption, int& pLeft, int& pTop, int& pRowWidth,
                        int& pRowHeight)
{
    const int firstTop = pHeight >= 700 ? 145 : 190;
    const int step = std::min(64, (pHeight - 100 - firstTop) / 6);
    // Keep clear of the title block on the left when the window is narrow.
    pLeft = std::min(pWidth - 410, std::max(pWidth / 2 - 200, 450));
    pTop = firstTop + pOption * step;
    pRowWidth = 400;
    pRowHeight = step - 8;
}

// Where a row of the local race setup goes: 46 pixels apart when there is room, closer together on
// short windows so the last rows clear the key hints at the bottom. The row box is 40 high at the
// normal spacing and shrinks with it.
void LocalSetupRowGeometry(int pHeight, int pRowCount, int pRow, int& pTop, int& pHeight2)
{
    const int step = std::max(30, std::min(46, (pHeight - 100 - 120) / std::max(1, pRowCount)));
    pTop = 120 + pRow * step;
    pHeight2 = step - 6;
}

// Left edge of each camera distance choice on the Settings screen's camera row.
int SettingsCameraOptionX(int pWidth, int pOption)
{
    static const int offsets[3] = {44, 104, 206};
    return pWidth / 2 + offsets[pOption < 0 ? 0 : (pOption > 2 ? 2 : pOption)];
}

int LocalSetupOptionRow(RaceMode pRaceMode, int pOption)
{
    if (!LocalSetupOptionVisible(pRaceMode, pOption))
        return -1;
    int row = 0;
    for (int option = 0; option < pOption; ++option)
    {
        if (LocalSetupOptionVisible(pRaceMode, option))
            ++row;
    }
    return row;
}

int NextLocalSetupOption(RaceMode pRaceMode, int pOption, int pDirection)
{
    do
        pOption = (pOption + pDirection + 10) % 10;
    while (!LocalSetupOptionVisible(pRaceMode, pOption));
    return pOption;
}

bool IsPointInRect(int pX, int pY, int pLeft, int pTop, int pWidth, int pHeight)
{
    return pX >= pLeft && pX < pLeft + pWidth && pY >= pTop && pY < pTop + pHeight;
}

void AppendLobbyNameText(std::string& pName, const char* pText)
{
    for (const char* character = pText; *character != '\0' && pName.size() < 24; ++character)
    {
        if (IsValidLobbyNameCharacter(*character))
        {
            const char uppercaseCharacter = *character >= 'a' && *character <= 'z'
                ? static_cast<char>(*character - 'a' + 'A') : *character;
            pName += uppercaseCharacter;
        }
    }
}

void AppendOnlineChatKey(std::string& pText, SDL_Keycode pKey, Uint16 pModifiers)
{
    if (pText.size() >= 120)
        return;
    const bool shifted = (pModifiers & KMOD_SHIFT) != 0;
    char character = '\0';
    if (pKey >= SDLK_a && pKey <= SDLK_z)
        character = static_cast<char>(pKey - SDLK_a + (shifted ? 'A' : 'a'));
    else if (pKey >= SDLK_0 && pKey <= SDLK_9)
    {
        static const char shiftedDigits[] = ")!@#$%^&*(";
        character = shifted ? shiftedDigits[pKey - SDLK_0] : static_cast<char>(pKey);
    }
    else if (pKey == SDLK_SPACE)
        character = ' ';
    else
    {
        switch (pKey)
        {
        case SDLK_MINUS: character = shifted ? '_' : '-'; break;
        case SDLK_EQUALS: character = shifted ? '+' : '='; break;
        case SDLK_LEFTBRACKET: character = shifted ? '{' : '['; break;
        case SDLK_RIGHTBRACKET: character = shifted ? '}' : ']'; break;
        case SDLK_BACKSLASH: character = shifted ? '|' : '\\'; break;
        case SDLK_SEMICOLON: character = shifted ? ':' : ';'; break;
        case SDLK_QUOTE: character = shifted ? '"' : '\''; break;
        case SDLK_COMMA: character = shifted ? '<' : ','; break;
        case SDLK_PERIOD: character = shifted ? '>' : '.'; break;
        case SDLK_SLASH: character = shifted ? '?' : '/'; break;
        case SDLK_BACKQUOTE: character = shifted ? '~' : '`'; break;
        default: break;
        }
    }
    if (character != '\0')
        pText += character;
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

void ApplyRaisedSections(Hovercraft& pHovercraft, const std::vector<RaisedSection>& pSections,
                         const GroundProfile& pGround)
{
    const auto raisedLevel = [](const HovercraftState& pState)
    {
        return pState.mSurfaceHeight > pState.mGroundHeight ? pState.mSurfaceHeight - pState.mGroundHeight : 0.0;
    };
    const double previousSurfaceHeight = raisedLevel(pHovercraft.State());
    pHovercraft.ApplyGround(pGround);
    HovercraftState state = pHovercraft.State();
    const double previousHeight = state.mHeight;
    const double previousVerticalSpeed = state.mVerticalSpeed;
    for (const RaisedSection& section : pSections)
    {
        if (LandOnRaisedSection(state, section))
        {
            if (raisedLevel(state) != previousSurfaceHeight || state.mHeight != previousHeight
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
    if (raisedLevel(state) != previousSurfaceHeight)
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

void SetPerspective(double pAspect, double pSpeed, double pZoomScale = 1.0)
{
    const double nearPlane = 0.2;
    const double farPlane = 400.0;
    const double speedFraction = std::fmin(1.0, std::fabs(pSpeed) / 55.0);
    const double fieldOfView = 54.0 + speedFraction * 7.0 * pZoomScale;
    const double top = nearPlane * std::tan(fieldOfView * kPi / 360.0);
    glFrustum(-top * pAspect, top * pAspect, -top, top, nearPlane, farPlane);
}

void SetChaseCamera(const HovercraftState& pState, double pDistance, double pHeading,
                    double pRise, double pGround)
{
    const double forwardX = std::cos(pHeading);
    const double forwardZ = std::sin(pHeading);
    const double eyeX = pState.mX - forwardX * pDistance;
    const double eyeY = 2.3 + pDistance * 0.09 + pRise + pGround;
    const double eyeZ = pState.mY - forwardZ * pDistance;
    const double targetX = pState.mX + forwardX * pDistance * 0.8;
    const double targetY = 1.55 + pRise * 0.8 + pGround;
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

// The ground the track is drawn on; null draws everything on the flat.
const GroundProfile* gDrawGround = nullptr;

// A vertex lifted by the height of the ground beneath it.
void GroundVertex(double pX, double pY, double pZ)
{
    glVertex3d(pX, pY + (gDrawGround != nullptr ? gDrawGround->HeightAt(pX, pZ) : 0.0), pZ);
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
    GroundVertex(pStartX + sideX, roadHeight, pStartZ + sideZ);
    GroundVertex(pStartX - sideX, roadHeight, pStartZ - sideZ);
    GroundVertex(pEndX - sideX, roadHeight, pEndZ - sideZ);
    GroundVertex(pEndX + sideX, roadHeight, pEndZ + sideZ);
    glEnd();

    glColor3f(0.84f, 0.9f, 0.91f);
    glBegin(GL_QUADS);
    GroundVertex(pStartX + sideX, 0.0, pStartZ + sideZ);
    GroundVertex(pStartX + sideX, wallHeight, pStartZ + sideZ);
    GroundVertex(pEndX + sideX, wallHeight, pEndZ + sideZ);
    GroundVertex(pEndX + sideX, 0.0, pEndZ + sideZ);
    GroundVertex(pStartX - sideX, wallHeight, pStartZ - sideZ);
    GroundVertex(pStartX - sideX, 0.0, pStartZ - sideZ);
    GroundVertex(pEndX - sideX, 0.0, pEndZ - sideZ);
    GroundVertex(pEndX - sideX, wallHeight, pEndZ - sideZ);
    glEnd();

    glColor3f(0.08f, 0.74f, 0.8f);
    glBegin(GL_LINES);
    GroundVertex(pStartX + sideX, wallHeight, pStartZ + sideZ);
    GroundVertex(pEndX + sideX, wallHeight, pEndZ + sideZ);
    GroundVertex(pStartX - sideX, wallHeight, pStartZ - sideZ);
    GroundVertex(pEndX - sideX, wallHeight, pEndZ - sideZ);
    for (double offset = 0.0; offset <= length; offset += 4.0)
    {
        const double postX = pStartX + directionX / length * std::fmin(offset, length);
        const double postZ = pStartZ + directionZ / length * std::fmin(offset, length);
        GroundVertex(postX + sideX, roadHeight, postZ + sideZ);
        GroundVertex(postX + sideX, wallHeight, postZ + sideZ);
        GroundVertex(postX - sideX, roadHeight, postZ - sideZ);
        GroundVertex(postX - sideX, wallHeight, postZ - sideZ);
    }
    glEnd();

    glColor3f(0.96f, 0.48f, 0.14f);
    glBegin(GL_QUADS);
    GroundVertex(pStartX + sideX * 0.82, roadHeight + 0.012, pStartZ + sideZ * 0.82);
    GroundVertex(pEndX + sideX * 0.82, roadHeight + 0.012, pEndZ + sideZ * 0.82);
    GroundVertex(pEndX + sideX, roadHeight + 0.012, pEndZ + sideZ);
    GroundVertex(pStartX + sideX, roadHeight + 0.012, pStartZ + sideZ);
    GroundVertex(pStartX - sideX, roadHeight + 0.012, pStartZ - sideZ);
    GroundVertex(pEndX - sideX, roadHeight + 0.012, pEndZ - sideZ);
    GroundVertex(pEndX - sideX * 0.82, roadHeight + 0.012, pEndZ - sideZ * 0.82);
    GroundVertex(pStartX - sideX * 0.82, roadHeight + 0.012, pStartZ - sideZ * 0.82);
    glEnd();

    glColor3f(0.45f, 0.54f, 0.57f);
    glBegin(GL_LINES);
    for (double offset = 0.0; offset <= length; offset += 2.0)
    {
        const double markerOffset = std::fmin(offset, length);
        const double centerX = pStartX + directionX / length * markerOffset;
        const double centerZ = pStartZ + directionZ / length * markerOffset;
        GroundVertex(centerX + sideX * 0.8, roadHeight + 0.016, centerZ + sideZ * 0.8);
        GroundVertex(centerX - sideX * 0.8, roadHeight + 0.016, centerZ - sideZ * 0.8);
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
        GroundVertex(centerX - forwardX * 0.8 + sideX * 1.01, 0.62,
                   centerZ - forwardZ * 0.8 + sideZ * 1.01);
        GroundVertex(centerX + forwardX * 0.8 + sideX * 1.01, 0.62,
                   centerZ + forwardZ * 0.8 + sideZ * 1.01);
        GroundVertex(centerX + forwardX * 0.8 + sideX * 1.01, 1.2,
                   centerZ + forwardZ * 0.8 + sideZ * 1.01);
        GroundVertex(centerX - forwardX * 0.8 + sideX * 1.01, 1.2,
                   centerZ - forwardZ * 0.8 + sideZ * 1.01);
    }
    glEnd();
}

CameraMotion gCameraMotion = CameraMotion::Standard;

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
    // The ground is level along each stretch of route and steps at the waypoints: every edge is
    // drawn at the height of the stretch before it and then, if the ground steps there, again at the
    // height of the stretch after it, which makes the vertical face of the step.
    const int edgeCount = static_cast<int>(edges.size());
    const bool stepped = gDrawGround != nullptr && !gDrawGround->Flat();
    const auto segmentHeight = [&](int pIndex)
    {
        const int wrapped = ((pIndex % edgeCount) + edgeCount) % edgeCount;
        return stepped ? gDrawGround->SegmentHeight(static_cast<std::size_t>(wrapped)) : 0.0;
    };
    const auto atEdge = [&](int pIndex, bool pBefore, bool pAfter, const std::function<void(double)>& pDraw)
    {
        const double before = segmentHeight(pIndex - 1);
        const double after = segmentHeight(pIndex);
        if (pBefore && (before != after || !pAfter))
            pDraw(before);
        if (pAfter)
            pDraw(after);
    };
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
        atEdge(index, index > 0, index < edgeCount, [&](double pHeight)
        {
            glTexCoord2d(textureDistance / 12.0, 0.0);
            glVertex3d(edge.mLeftX, roadHeight + pHeight, edge.mLeftZ);
            glTexCoord2d(textureDistance / 12.0, 3.0);
            glVertex3d(edge.mRightX, roadHeight + pHeight, edge.mRightZ);
        });
    }
    glEnd();
    if (segmentHeight(-1) != segmentHeight(0))
    {
        // The step where the loop closes.
        const double low = segmentHeight(-1);
        const double high = segmentHeight(0);
        glBegin(GL_QUADS);
        glTexCoord2d(0.0, 0.0);
        glVertex3d(edges[0].mLeftX, roadHeight + low, edges[0].mLeftZ);
        glTexCoord2d(0.0, 3.0);
        glVertex3d(edges[0].mRightX, roadHeight + low, edges[0].mRightZ);
        glTexCoord2d(1.0, 3.0);
        glVertex3d(edges[0].mRightX, roadHeight + high, edges[0].mRightZ);
        glTexCoord2d(1.0, 0.0);
        glVertex3d(edges[0].mLeftX, roadHeight + high, edges[0].mLeftZ);
        glEnd();
    }
    glDisable(GL_TEXTURE_2D);

    glColor3f(0.12f, 0.62f, 0.68f);
    glBegin(GL_LINE_LOOP);
    for (int index = 0; index < edgeCount; ++index)
    {
        const EdgePoint& edge = edges[index];
        atEdge(index, true, true, [&](double pHeight)
        {
            glVertex3d(edge.mLeftX * 0.9 + edge.mRightX * 0.1, roadHeight + 0.014 + pHeight,
                       edge.mLeftZ * 0.9 + edge.mRightZ * 0.1);
        });
    }
    glEnd();
    glBegin(GL_LINE_LOOP);
    for (int index = 0; index < edgeCount; ++index)
    {
        const EdgePoint& edge = edges[index];
        atEdge(index, true, true, [&](double pHeight)
        {
            glVertex3d(edge.mLeftX * 0.1 + edge.mRightX * 0.9, roadHeight + 0.014 + pHeight,
                       edge.mLeftZ * 0.1 + edge.mRightZ * 0.9);
        });
    }
    glEnd();
    glColor3f(0.48f, 0.56f, 0.59f);
    glBegin(GL_LINES);
    for (int index = 0; index < static_cast<int>(edges.size()); ++index)
    {
        const EdgePoint& start = edges[index];
        const EdgePoint& end = edges[(index + 1) % edges.size()];
        const double segHeight = segmentHeight(index);
        for (int column = 1; column < 3; ++column)
        {
            const double across = column / 3.0;
            glVertex3d(start.mLeftX + (start.mRightX - start.mLeftX) * across,
                       roadHeight + 0.012 + segHeight,
                       start.mLeftZ + (start.mRightZ - start.mLeftZ) * across);
            glVertex3d(end.mLeftX + (end.mRightX - end.mLeftX) * across,
                       roadHeight + 0.012 + segHeight,
                       end.mLeftZ + (end.mRightZ - end.mLeftZ) * across);
        }
        const double length = std::sqrt((end.mLeftX - start.mLeftX) * (end.mLeftX - start.mLeftX)
            + (end.mLeftZ - start.mLeftZ) * (end.mLeftZ - start.mLeftZ));
        for (double distance = 2.5; distance < length; distance += 2.5)
        {
            const double progress = distance / length;
            glVertex3d(start.mLeftX + (end.mLeftX - start.mLeftX) * progress, roadHeight + 0.012 + segHeight,
                       start.mLeftZ + (end.mLeftZ - start.mLeftZ) * progress);
            glVertex3d(start.mRightX + (end.mRightX - start.mRightX) * progress, roadHeight + 0.012 + segHeight,
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
        atEdge(index, index > 0, index < edgeCount, [&](double pHeight)
        {
            glTexCoord2d(wallTextureDistance / 5.0, 0.0);
            glVertex3d(edge.mLeftX, roadHeight + pHeight, edge.mLeftZ);
            glTexCoord2d(wallTextureDistance / 5.0, 1.0);
            glVertex3d(edge.mLeftX, wallHeight + pHeight, edge.mLeftZ);
        });
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
        atEdge(index, index > 0, index < edgeCount, [&](double pHeight)
        {
            glTexCoord2d(wallTextureDistance / 5.0, 1.0);
            glVertex3d(edge.mRightX, wallHeight + pHeight, edge.mRightZ);
            glTexCoord2d(wallTextureDistance / 5.0, 0.0);
            glVertex3d(edge.mRightX, roadHeight + pHeight, edge.mRightZ);
        });
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);

    glColor3f(0.08f, 0.74f, 0.8f);
    glBegin(GL_LINE_LOOP);
    for (int index = 0; index < edgeCount; ++index)
        atEdge(index, true, true, [&](double pHeight)
        {
            glVertex3d(edges[index].mLeftX, wallHeight + pHeight, edges[index].mLeftZ);
        });
    glEnd();
    glBegin(GL_LINE_LOOP);
    for (int index = 0; index < edgeCount; ++index)
        atEdge(index, true, true, [&](double pHeight)
        {
            glVertex3d(edges[index].mRightX, wallHeight + pHeight, edges[index].mRightZ);
        });
    glEnd();

    // Large bright chevrons on both walls point in the driving direction, so a player who has lost
    // their bearings can always tell which way the route goes. They sit just in front of the wall.
    const GLboolean lightingWasOn = glIsEnabled(GL_LIGHTING);
    if (lightingWasOn)
        glDisable(GL_LIGHTING);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-2.0f, -2.0f);
    // Flash between two bright colours two or three times a second. Reduced camera motion keeps
    // them steady for players who are sensitive to flicker.
    const bool flashAlternate = gCameraMotion != CameraMotion::Reduced && (SDL_GetTicks() / 250) % 2 == 1;
    if (flashAlternate)
        glColor3f(1.0f, 0.1f, 0.85f);
    else
        glColor3f(1.0f, 0.86f, 0.1f);
    glBegin(GL_QUADS);
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
            if (segmentLength < 8.0)
                continue;
            const double forwardX = segmentX / segmentLength;
            const double forwardZ = segmentZ / segmentLength;
            const double acrossX = start.mLeftX - start.mRightX;
            const double acrossZ = start.mLeftZ - start.mRightZ;
            const double acrossLength = std::sqrt(acrossX * acrossX + acrossZ * acrossZ);
            // Toward the road centre, away from the wall face.
            const double inwardX = (side == 0 ? -acrossX : acrossX) / acrossLength * 0.06;
            const double inwardZ = (side == 0 ? -acrossZ : acrossZ) / acrossLength * 0.06;
            const auto point = [&](double pCentreX, double pCentreZ, double pAlong, double pHeight)
            {
                glVertex3d(pCentreX + forwardX * pAlong + inwardX, pHeight + segmentHeight(index),
                           pCentreZ + forwardZ * pAlong + inwardZ);
            };
            for (double distance = 6.0; distance < segmentLength - 4.0; distance += 14.0)
            {
                const double centreX = startX + forwardX * distance;
                const double centreZ = startZ + forwardZ * distance;
                point(centreX, centreZ, -1.4, 4.9);
                point(centreX, centreZ, -0.3, 4.9);
                point(centreX, centreZ, 1.8, 3.0);
                point(centreX, centreZ, 0.7, 3.0);
                point(centreX, centreZ, 0.7, 3.0);
                point(centreX, centreZ, 1.8, 3.0);
                point(centreX, centreZ, -0.3, 1.1);
                point(centreX, centreZ, -1.4, 1.1);
            }
        }
    }
    glEnd();
    glDisable(GL_POLYGON_OFFSET_FILL);
    if (lightingWasOn)
        glEnable(GL_LIGHTING);
}

void DrawCityBuilding(double pX, double pZ, double pWidth, double pDepth, double pHeight,
                      float pRed, float pGreen, float pBlue)
{
    const double halfWidth = pWidth * 0.5;
    const double halfDepth = pDepth * 0.5;
    glColor3f(pRed, pGreen, pBlue);
    glBegin(GL_QUADS);
    GroundVertex(pX - halfWidth, 0.0, pZ - halfDepth);
    GroundVertex(pX + halfWidth, 0.0, pZ - halfDepth);
    GroundVertex(pX + halfWidth, pHeight, pZ - halfDepth);
    GroundVertex(pX - halfWidth, pHeight, pZ - halfDepth);
    GroundVertex(pX + halfWidth, 0.0, pZ - halfDepth);
    GroundVertex(pX + halfWidth, 0.0, pZ + halfDepth);
    GroundVertex(pX + halfWidth, pHeight, pZ + halfDepth);
    GroundVertex(pX + halfWidth, pHeight, pZ - halfDepth);
    GroundVertex(pX + halfWidth, pHeight, pZ + halfDepth);
    GroundVertex(pX - halfWidth, pHeight, pZ + halfDepth);
    GroundVertex(pX - halfWidth, pHeight, pZ - halfDepth);
    GroundVertex(pX + halfWidth, pHeight, pZ - halfDepth);
    GroundVertex(pX - halfWidth, pHeight, pZ - halfDepth);
    GroundVertex(pX + halfWidth, pHeight, pZ - halfDepth);
    GroundVertex(pX + halfWidth, pHeight, pZ + halfDepth);
    GroundVertex(pX - halfWidth, pHeight, pZ + halfDepth);
    glEnd();

    glColor3f(0.72f, 0.86f, 0.74f);
    glBegin(GL_LINES);
    for (double height = 4.0; height < pHeight - 2.0; height += 5.0)
    {
        GroundVertex(pX - halfWidth - 0.02, height, pZ - halfDepth);
        GroundVertex(pX + halfWidth + 0.02, height, pZ - halfDepth);
    }
    glEnd();
}

void DrawMountain(double pX, double pZ, double pRadius, double pHeight,
                  float pRed, float pGreen, float pBlue)
{
    glColor3f(pRed, pGreen, pBlue);
    glBegin(GL_TRIANGLES);
    GroundVertex(pX - pRadius, 0.0, pZ - pRadius);
    GroundVertex(pX + pRadius, 0.0, pZ - pRadius);
    GroundVertex(pX, pHeight, pZ);
    GroundVertex(pX + pRadius, 0.0, pZ - pRadius);
    GroundVertex(pX + pRadius, 0.0, pZ + pRadius);
    GroundVertex(pX, pHeight, pZ);
    GroundVertex(pX + pRadius, 0.0, pZ + pRadius);
    GroundVertex(pX - pRadius, 0.0, pZ + pRadius);
    GroundVertex(pX, pHeight, pZ);
    GroundVertex(pX - pRadius, 0.0, pZ + pRadius);
    GroundVertex(pX - pRadius, 0.0, pZ - pRadius);
    GroundVertex(pX, pHeight, pZ);
    glEnd();
}

void DrawIndustrialBeacon(double pX, double pZ, double pHeight)
{
    glColor3f(0.3f, 0.36f, 0.38f);
    glBegin(GL_QUADS);
    GroundVertex(pX - 0.9, 0.0, pZ - 0.9);
    GroundVertex(pX + 0.9, 0.0, pZ - 0.9);
    GroundVertex(pX + 0.32, pHeight, pZ - 0.32);
    GroundVertex(pX - 0.32, pHeight, pZ - 0.32);
    GroundVertex(pX + 0.9, 0.0, pZ + 0.9);
    GroundVertex(pX - 0.9, 0.0, pZ + 0.9);
    GroundVertex(pX - 0.32, pHeight, pZ + 0.32);
    GroundVertex(pX + 0.32, pHeight, pZ + 0.32);
    glEnd();
    glColor3f(1.0f, 0.68f, 0.12f);
    glBegin(GL_TRIANGLES);
    GroundVertex(pX, pHeight + 2.2, pZ);
    GroundVertex(pX - 0.65, pHeight, pZ);
    GroundVertex(pX + 0.65, pHeight, pZ);
    glEnd();
}

// Distance from a point to the closed route centreline.
double DistanceToRoute(const std::vector<RaceGate>& pWaypoints, double pX, double pY)
{
    double best = 1e18;
    for (std::size_t index = 0; index < pWaypoints.size(); ++index)
    {
        const RaceGate& start = pWaypoints[index];
        const RaceGate& end = pWaypoints[(index + 1) % pWaypoints.size()];
        const double segmentX = end.mX - start.mX;
        const double segmentY = end.mY - start.mY;
        const double lengthSquared = segmentX * segmentX + segmentY * segmentY;
        double fraction = lengthSquared > 0.0
            ? ((pX - start.mX) * segmentX + (pY - start.mY) * segmentY) / lengthSquared : 0.0;
        fraction = std::fmax(0.0, std::fmin(1.0, fraction));
        best = std::fmin(best, std::hypot(pX - (start.mX + segmentX * fraction),
                                          pY - (start.mY + segmentY * fraction)));
    }
    return best;
}

void DrawTrackEnvironment(const TrackDefinition& pTrack)
{
    if (pTrack.mWaypoints.empty())
        return;
    double minimumX = pTrack.mWaypoints.front().mX;
    double maximumX = minimumX;
    double minimumY = pTrack.mWaypoints.front().mY;
    double maximumY = minimumY;
    for (const RaceGate& waypoint : pTrack.mWaypoints)
    {
        minimumX = std::fmin(minimumX, waypoint.mX);
        maximumX = std::fmax(maximumX, waypoint.mX);
        minimumY = std::fmin(minimumY, waypoint.mY);
        maximumY = std::fmax(maximumY, waypoint.mY);
    }
    const double centreX = (minimumX + maximumX) * 0.5;
    const double centreY = (minimumY + maximumY) * 0.5;
    const double halfX = std::fmax(1.0, (maximumX - minimumX) * 0.5);
    const double halfY = std::fmax(1.0, (maximumY - minimumY) * 0.5);
    // Scenery grows with the course so it stays a landmark rather than a speck.
    const double propScale = std::fmax(1.0, std::fmin(2.5, std::fmax(halfX, halfY) / 150.0));
    // Places a prop relative to the course bounds, then pushes it outward from the course centre
    // until it is clear of the road, so scenery never stands on or beside the route.
    const auto place = [&](double pFractionX, double pFractionY, double pFootprintRadius,
                           double& pX, double& pY)
    {
        pX = centreX + halfX * pFractionX;
        pY = centreY + halfY * pFractionY;
        double directionX = pX - centreX;
        double directionY = pY - centreY;
        const double directionLength = std::fmax(1.0, std::hypot(directionX, directionY));
        directionX /= directionLength;
        directionY /= directionLength;
        const double required = pFootprintRadius + pTrack.mRoadHalfWidth + 30.0;
        for (int attempt = 0; attempt < 200 && DistanceToRoute(pTrack.mWaypoints, pX, pY) < required;
             ++attempt)
        {
            pX += directionX * 10.0;
            pY += directionY * 10.0;
        }
    };
    double x = 0.0;
    double y = 0.0;
    if (pTrack.mId == "harbor-loop")
    {
        const double sizes[4][3] = {{24.0, 22.0, 42.0}, {16.0, 18.0, 27.0},
                                    {20.0, 24.0, 35.0}, {28.0, 20.0, 48.0}};
        const double fractions[4][2] = {{-1.3, 0.55}, {-1.2, 0.95}, {1.3, 0.5}, {1.35, 0.9}};
        const float colours[4][3] = {{0.22f, 0.3f, 0.33f}, {0.28f, 0.36f, 0.38f},
                                     {0.18f, 0.28f, 0.32f}, {0.24f, 0.34f, 0.36f}};
        for (int index = 0; index < 4; ++index)
        {
            const double width = sizes[index][0] * propScale;
            const double depth = sizes[index][1] * propScale;
            place(fractions[index][0], fractions[index][1], std::hypot(width, depth) * 0.5, x, y);
            DrawCityBuilding(x, y, width, depth, sizes[index][2] * propScale, colours[index][0],
                             colours[index][1], colours[index][2]);
        }
    }
    else if (pTrack.mId == "glass-switchback")
    {
        const double sizes[3][2] = {{45.0, 48.0}, {58.0, 62.0}, {68.0, 70.0}};
        const double fractions[3][2] = {{-1.4, 0.6}, {1.4, 0.8}, {0.0, 1.5}};
        const float colours[3][3] = {{0.22f, 0.31f, 0.28f}, {0.3f, 0.38f, 0.32f},
                                     {0.18f, 0.27f, 0.26f}};
        for (int index = 0; index < 3; ++index)
        {
            const double radius = sizes[index][0] * propScale;
            place(fractions[index][0], fractions[index][1], radius * 1.5, x, y);
            DrawMountain(x, y, radius, sizes[index][1] * propScale, colours[index][0],
                         colours[index][1], colours[index][2]);
        }
    }
    else
    {
        const double heights[4] = {32.0, 46.0, 38.0, 42.0};
        const double fractions[4][2] = {{-1.3, -0.5}, {1.35, -0.4}, {1.25, 0.8}, {-1.25, 0.6}};
        for (int index = 0; index < 4; ++index)
        {
            place(fractions[index][0], fractions[index][1], 4.0 * propScale, x, y);
            DrawIndustrialBeacon(x, y, heights[index] * propScale);
        }
    }
}

void DrawFinishZone(const RaceGate& pFinish, const std::vector<RaceGate>& pWaypoints,
                    double pTrackHalfWidth)
{
    if (pWaypoints.size() < 2 || pTrackHalfWidth <= 0.0)
        return;
    const RaceGate& turn = pWaypoints.front();
    double forwardX = turn.mX - pFinish.mX;
    double forwardZ = turn.mY - pFinish.mY;
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
            GroundVertex(pFinish.mX + forwardX * start + sideX * left, 0.4,
                       pFinish.mY + forwardZ * start + sideZ * left);
            GroundVertex(pFinish.mX + forwardX * end + sideX * left, 0.4,
                       pFinish.mY + forwardZ * end + sideZ * left);
            GroundVertex(pFinish.mX + forwardX * end + sideX * right, 0.4,
                       pFinish.mY + forwardZ * end + sideZ * right);
            GroundVertex(pFinish.mX + forwardX * start + sideX * right, 0.4,
                       pFinish.mY + forwardZ * start + sideZ * right);
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
            GroundVertex(pFinish.mX + sideX * left - forwardX * 0.04, bottom,
                       pFinish.mY + sideZ * left - forwardZ * 0.04);
            GroundVertex(pFinish.mX + sideX * right - forwardX * 0.04, bottom,
                       pFinish.mY + sideZ * right - forwardZ * 0.04);
            GroundVertex(pFinish.mX + sideX * right - forwardX * 0.04, top,
                       pFinish.mY + sideZ * right - forwardZ * 0.04);
            GroundVertex(pFinish.mX + sideX * left - forwardX * 0.04, top,
                       pFinish.mY + sideZ * left - forwardZ * 0.04);
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
    GroundVertex(pGate.mX - forwardX * 0.28 - sideX * gateHalfWidth, 0.4,
               pGate.mY - forwardZ * 0.28 - sideZ * gateHalfWidth);
    GroundVertex(pGate.mX + forwardX * 0.28 - sideX * gateHalfWidth, 0.4,
               pGate.mY + forwardZ * 0.28 - sideZ * gateHalfWidth);
    GroundVertex(pGate.mX + forwardX * 0.28 + sideX * gateHalfWidth, 0.4,
               pGate.mY + forwardZ * 0.28 + sideZ * gateHalfWidth);
    GroundVertex(pGate.mX - forwardX * 0.28 + sideX * gateHalfWidth, 0.4,
               pGate.mY - forwardZ * 0.28 + sideZ * gateHalfWidth);
    glEnd();
    for (int side = -1; side <= 1; side += 2)
    {
        const double postX = pGate.mX + sideX * gateHalfWidth * side;
        const double postZ = pGate.mY + sideZ * gateHalfWidth * side;
        glBegin(GL_QUADS);
        GroundVertex(postX - forwardX * postHalfWidth, 0.36, postZ - forwardZ * postHalfWidth);
        GroundVertex(postX + forwardX * postHalfWidth, 0.36, postZ + forwardZ * postHalfWidth);
        GroundVertex(postX + forwardX * postHalfWidth, postHeight, postZ + forwardZ * postHalfWidth);
        GroundVertex(postX - forwardX * postHalfWidth, postHeight, postZ - forwardZ * postHalfWidth);
        glEnd();
    }
    glBegin(GL_QUADS);
    GroundVertex(pGate.mX - sideX * gateHalfWidth, postHeight - 0.32,
               pGate.mY - sideZ * gateHalfWidth);
    GroundVertex(pGate.mX + sideX * gateHalfWidth, postHeight - 0.32,
               pGate.mY + sideZ * gateHalfWidth);
    GroundVertex(pGate.mX + sideX * gateHalfWidth, postHeight,
               pGate.mY + sideZ * gateHalfWidth);
    GroundVertex(pGate.mX - sideX * gateHalfWidth, postHeight,
               pGate.mY - sideZ * gateHalfWidth);
    glEnd();
    glLineWidth(2.5f);
    glBegin(GL_LINE_LOOP);
    GroundVertex(pGate.mX - sideX * gateHalfWidth, 0.38, pGate.mY - sideZ * gateHalfWidth);
    GroundVertex(pGate.mX - sideX * gateHalfWidth, postHeight, pGate.mY - sideZ * gateHalfWidth);
    GroundVertex(pGate.mX + sideX * gateHalfWidth, postHeight, pGate.mY + sideZ * gateHalfWidth);
    GroundVertex(pGate.mX + sideX * gateHalfWidth, 0.38, pGate.mY + sideZ * gateHalfWidth);
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

double BoostPadHeading(const BoostPad& pPad, const std::vector<RaceGate>& pWaypoints)
{
    double closestDistanceSquared = -1.0;
    double heading = 0.0;
    for (std::size_t index = 0; index < pWaypoints.size(); ++index)
    {
        const RaceGate& start = pWaypoints[index];
        const RaceGate& end = pWaypoints[(index + 1) % pWaypoints.size()];
        const double directionX = end.mX - start.mX;
        const double directionY = end.mY - start.mY;
        const double lengthSquared = directionX * directionX + directionY * directionY;
        if (lengthSquared <= 0.0)
            continue;
        double progress = ((pPad.mX - start.mX) * directionX + (pPad.mY - start.mY) * directionY)
            / lengthSquared;
        progress = std::fmax(0.0, std::fmin(1.0, progress));
        const double pointX = start.mX + directionX * progress;
        const double pointY = start.mY + directionY * progress;
        const double deltaX = pPad.mX - pointX;
        const double deltaY = pPad.mY - pointY;
        const double distanceSquared = deltaX * deltaX + deltaY * deltaY;
        if (closestDistanceSquared < 0.0 || distanceSquared < closestDistanceSquared)
        {
            closestDistanceSquared = distanceSquared;
            heading = std::atan2(directionY, directionX);
        }
    }
    return heading;
}

void DrawBoostPad(const BoostPad& pPad, const std::vector<RaceGate>& pWaypoints)
{
    const double heading = BoostPadHeading(pPad, pWaypoints);
    const double forwardX = std::cos(heading);
    const double forwardY = std::sin(heading);
    const double sideX = -forwardY;
    const double sideY = forwardX;
    glColor3f(0.02f, 0.18f, 0.28f);
    glBegin(GL_QUADS);
    glNormal3d(0.0, 1.0, 0.0);
    GroundVertex(pPad.mX - pPad.mRadius, 0.38, pPad.mY - pPad.mRadius);
    GroundVertex(pPad.mX + pPad.mRadius, 0.38, pPad.mY - pPad.mRadius);
    GroundVertex(pPad.mX + pPad.mRadius, 0.38, pPad.mY + pPad.mRadius);
    GroundVertex(pPad.mX - pPad.mRadius, 0.38, pPad.mY + pPad.mRadius);
    glEnd();
    glDisable(GL_LIGHTING);
    glColor3f(0.2f, 0.95f, 1.0f);
    for (int arrowIndex = -1; arrowIndex <= 1; ++arrowIndex)
    {
        const double center = arrowIndex * pPad.mRadius * 0.52;
        const double tail = center - pPad.mRadius * 0.28;
        const double tip = center + pPad.mRadius * 0.34;
        const double halfWidth = pPad.mRadius * 0.26;
        glBegin(GL_TRIANGLES);
        GroundVertex(pPad.mX + forwardX * tip, 0.405, pPad.mY + forwardY * tip);
        GroundVertex(pPad.mX + forwardX * tail + sideX * halfWidth, 0.405,
                   pPad.mY + forwardY * tail + sideY * halfWidth);
        GroundVertex(pPad.mX + forwardX * tail - sideX * halfWidth, 0.405,
                   pPad.mY + forwardY * tail - sideY * halfWidth);
        glEnd();
    }
    glEnable(GL_LIGHTING);
}

void DrawMine(const Mine& pMine)
{
    if (pMine.mTriggered)
        return;
    glColor3f(0.1f, 0.08f, 0.06f);
    glBegin(GL_TRIANGLE_FAN);
    GroundVertex(pMine.mX, 0.62, pMine.mY);
    for (int degree = 0; degree <= 360; degree += 30)
    {
        const double angle = degree * kPi / 180.0;
        GroundVertex(pMine.mX + std::cos(angle) * pMine.mRadius,
                   0.45, pMine.mY + std::sin(angle) * pMine.mRadius);
    }
    glEnd();
    glColor3f(0.95f, 0.18f, 0.06f);
    glBegin(GL_TRIANGLES);
    for (int spike = 0; spike < 8; ++spike)
    {
        const double angle = spike * kPi * 0.25;
        GroundVertex(pMine.mX, 0.95, pMine.mY);
        GroundVertex(pMine.mX + std::cos(angle - 0.2) * pMine.mRadius * 1.25,
                   0.48, pMine.mY + std::sin(angle - 0.2) * pMine.mRadius * 1.25);
        GroundVertex(pMine.mX + std::cos(angle + 0.2) * pMine.mRadius * 1.25,
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
    GroundVertex(pZone.mX - pZone.mRadius, waterHeight, pZone.mY - pZone.mRadius);
    GroundVertex(pZone.mX + pZone.mRadius, waterHeight, pZone.mY - pZone.mRadius);
    GroundVertex(pZone.mX + pZone.mRadius, waterHeight, pZone.mY + pZone.mRadius);
    GroundVertex(pZone.mX - pZone.mRadius, waterHeight, pZone.mY + pZone.mRadius);
    glEnd();
    glColor3f(0.12f, 0.7f, 0.82f);
    glBegin(GL_LINE_LOOP);
    GroundVertex(pZone.mX - pZone.mRadius, waterHeight + 0.01, pZone.mY - pZone.mRadius);
    GroundVertex(pZone.mX + pZone.mRadius, waterHeight + 0.01, pZone.mY - pZone.mRadius);
    GroundVertex(pZone.mX + pZone.mRadius, waterHeight + 0.01, pZone.mY + pZone.mRadius);
    GroundVertex(pZone.mX - pZone.mRadius, waterHeight + 0.01, pZone.mY + pZone.mRadius);
    glEnd();
    glBegin(GL_LINES);
    for (double offset = -pZone.mRadius + 0.5; offset < pZone.mRadius; offset += 1.0)
    {
        GroundVertex(pZone.mX + offset - 0.24, waterHeight + 0.012, pZone.mY);
        GroundVertex(pZone.mX + offset + 0.24, waterHeight + 0.012, pZone.mY);
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
    GroundVertex(-pSection.mHalfLength, deckHeight, -pSection.mHalfWidth);
    GroundVertex(pSection.mHalfLength, deckHeight, -pSection.mHalfWidth);
    GroundVertex(pSection.mHalfLength, deckHeight, pSection.mHalfWidth);
    GroundVertex(-pSection.mHalfLength, deckHeight, pSection.mHalfWidth);
    glColor3f(0.82f, 0.58f, 0.06f);
    GroundVertex(-pSection.mHalfLength, baseHeight, -pSection.mHalfWidth);
    GroundVertex(pSection.mHalfLength, baseHeight, -pSection.mHalfWidth);
    GroundVertex(pSection.mHalfLength, deckHeight, -pSection.mHalfWidth);
    GroundVertex(-pSection.mHalfLength, deckHeight, -pSection.mHalfWidth);
    GroundVertex(pSection.mHalfLength, baseHeight, pSection.mHalfWidth);
    GroundVertex(-pSection.mHalfLength, baseHeight, pSection.mHalfWidth);
    GroundVertex(-pSection.mHalfLength, deckHeight, pSection.mHalfWidth);
    GroundVertex(pSection.mHalfLength, deckHeight, pSection.mHalfWidth);
    glColor3f(0.72f, 0.48f, 0.04f);
    GroundVertex(-pSection.mHalfLength, baseHeight, pSection.mHalfWidth);
    GroundVertex(-pSection.mHalfLength, baseHeight, -pSection.mHalfWidth);
    GroundVertex(-pSection.mHalfLength, deckHeight, -pSection.mHalfWidth);
    GroundVertex(-pSection.mHalfLength, deckHeight, pSection.mHalfWidth);
    GroundVertex(pSection.mHalfLength, baseHeight, -pSection.mHalfWidth);
    GroundVertex(pSection.mHalfLength, baseHeight, pSection.mHalfWidth);
    GroundVertex(pSection.mHalfLength, deckHeight, pSection.mHalfWidth);
    GroundVertex(pSection.mHalfLength, deckHeight, -pSection.mHalfWidth);
    glColor3f(0.52f, 0.34f, 0.025f);
    glNormal3d(0.0, -1.0, 0.0);
    GroundVertex(-pSection.mHalfLength, baseHeight, pSection.mHalfWidth);
    GroundVertex(pSection.mHalfLength, baseHeight, pSection.mHalfWidth);
    GroundVertex(pSection.mHalfLength, baseHeight, -pSection.mHalfWidth);
    GroundVertex(-pSection.mHalfLength, baseHeight, -pSection.mHalfWidth);
    glEnd();
    glColor3f(0.62f, 0.42f, 0.03f);
    for (int end = -1; end <= 1; end += 2)
    {
        for (int side = -1; side <= 1; side += 2)
        {
            const double x = end * (pSection.mHalfLength - 0.28);
            const double z = side * (pSection.mHalfWidth - 0.28);
            glBegin(GL_QUADS);
            GroundVertex(x - 0.18, baseHeight, z - 0.18);
            GroundVertex(x + 0.18, baseHeight, z - 0.18);
            GroundVertex(x + 0.18, deckHeight, z - 0.18);
            GroundVertex(x - 0.18, deckHeight, z - 0.18);
            GroundVertex(x + 0.18, baseHeight, z + 0.18);
            GroundVertex(x - 0.18, baseHeight, z + 0.18);
            GroundVertex(x - 0.18, deckHeight, z + 0.18);
            GroundVertex(x + 0.18, deckHeight, z + 0.18);
            glEnd();
        }
    }
    glColor3f(0.74f, 0.84f, 0.88f);
    glBegin(GL_LINES);
    for (double x = -pSection.mHalfLength + 0.35; x < pSection.mHalfLength; x += 0.7)
    {
        GroundVertex(x, deckHeight + 0.01, -pSection.mHalfWidth);
        GroundVertex(x, deckHeight + 0.01, pSection.mHalfWidth);
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
    const double groundHeight = gDrawGround != nullptr ? gDrawGround->HeightAt(pState.mX, pState.mY) : 0.0;
    const double hoverOffset = std::fmax(0.0, pState.mHeight - groundHeight - 1.2);
    const double shadowScale = std::fmax(0.42, 1.0 - hoverOffset * 0.5);
    const double craftScale = pRival || pGhost ? 1.0 : 1.12;
    const float shadowAlpha = static_cast<float>(std::fmax(0.1, 0.38 - hoverOffset * 0.14));
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glColor4f(0.01f, 0.02f, 0.025f, shadowAlpha);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3d(pState.mX, 0.382 + groundHeight, pState.mY);
    for (int degree = 0; degree <= 360; degree += 15)
    {
        const double angle = degree * kPi / 180.0;
        glVertex3d(pState.mX + std::cos(angle) * 1.75 * craftScale * shadowScale, 0.382 + groundHeight,
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
    const double classLengthScale = pCraftClass == CraftClass::Sprint ? 1.2
        : (pCraftClass == CraftClass::Control ? 0.86 : 1.0);
    const double classHeightScale = pCraftClass == CraftClass::Sprint ? 0.84
        : (pCraftClass == CraftClass::Control ? 1.14 : 1.0);
    const double classWidthScale = pCraftClass == CraftClass::Sprint ? 0.78
        : (pCraftClass == CraftClass::Control ? 1.3 : 1.0);
    const double lengthScale = (pRival || pGhost ? craftScale : craftScale * 0.94) * classLengthScale;
    const double heightScale = (pRival || pGhost ? craftScale : craftScale * 1.28) * classHeightScale;
    const double widthScale = (pRival || pGhost ? craftScale : craftScale * 1.08) * classWidthScale;
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


void DrawSetupOverlay(int pWidth, int pHeight)
{
    glColor3f(0.2f, 0.9f, 1.0f);
    DrawPixelText("RACE STARTING", 24, 92, 3);
    glColor3f(0.82f, 0.9f, 0.92f);
    DrawPixelText("A D STEER  S BRAKE  UP JUMP", 24, 120, 2);
    DrawPixelText("SHIFT ACCEL  CTRL FIRE", 24, 138, 2);
}

std::string gRaceWinnerName;
// A note on the main menu, for example that the game closed unexpectedly last time.
std::string gStartupNotice;
// Championship results panel: standings by name, and what comes next.
std::vector<std::string> gChampionshipLines;
// Built-in tracks first, then any valid custom tracks from the player's tracks folder. Online
// rooms only ever use the built-in tracks at the front of this list.
std::vector<TrackDefinition> gLocalTracks;

std::string LocalTrackName(int pIndex)
{
    if (pIndex < 0 || pIndex >= static_cast<int>(gLocalTracks.size()))
        return std::string();
    std::string name = gLocalTracks[pIndex].mName;
    for (char& c : name)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (name.size() > 22)
        name.resize(22);
    return name;
}

// Hash of each entry in gLocalTracks, in the same order.
std::vector<std::string> gLocalTrackHashes;
// Where tracks downloaded from race rooms are kept on this computer.
std::string gDownloadedTracksDirectory;

// Index into gLocalTracks of the track a room is running, or -1 when this player does not have
// it yet. Built-in rooms carry an index; custom rooms carry the SHA-256 hash of the track, and the
// player's copy must match that hash exactly.
int LocalIndexForRoomTrack(int pTrackIndex, const std::string& pCustomHash)
{
    if (pCustomHash.empty() || pCustomHash == "-")
        return pTrackIndex >= 0 && pTrackIndex < 3 ? pTrackIndex : -1;
    for (int index = 0; index < static_cast<int>(gLocalTrackHashes.size()); ++index)
    {
        if (gLocalTrackHashes[index] == pCustomHash)
            return index;
    }
    return -1;
}

// Whether a local track can be hosted online: built-in, or plain-text names within the server's
// size and range limits.
bool TrackEligibleForOnline(int pIndex)
{
    if (pIndex < 0 || pIndex >= static_cast<int>(gLocalTracks.size()))
        return false;
    if (pIndex < 3)
        return true;
    const TrackDefinition& track = gLocalTracks[pIndex];
    return IsOnlineSafeTrack(track) && CheckOnlineTrackLimits(track).empty()
        && static_cast<int>(SerializeTrack(track).size()) <= kMaximumTrackUploadBytes;
}

// Next track a host can pick in a direction, skipping tracks that cannot be hosted online.
int NextHostTrack(int pCurrent, int pDirection)
{
    const int count = static_cast<int>(gLocalTracks.size());
    for (int step = 1; step <= count; ++step)
    {
        const int candidate = ((pCurrent + pDirection * step) % count + count) % count;
        if (TrackEligibleForOnline(candidate))
            return candidate;
    }
    return pCurrent;
}

std::string HostTrackName(int pIndex)
{
    return LocalTrackName(pIndex);
}
bool gNewGhostBest = false;
// Which ghost races alongside the player: 0 off, 1 best completed run, 2 last completed run.
int gGhostMode = 1;
// Larger race readouts (lap, time, speed) for players who find the default text small.
bool gHudTextLarge = false;
// The last completed run (without recovery) for the track and craft class it was driven on.
struct LastRun
{
    std::string mTrackId;
    int mCraftClass = -1;
    double mSeconds = 0.0;
    InputRecording mRecording;
};
LastRun gLastRun;
// Which kind of ghost this race loaded ("BEST" or "LAST"), for the results panel.
std::string gGhostSourceLabel = "BEST";
// A short on-screen note naming the ghost after the player changes it.
std::string gGhostToast;
Uint32 gGhostToastUntil = 0;
// How far the player is ahead of (positive) or behind (negative) the ghost along the road, in
// metres, while a ghost is racing alongside.
bool gGhostGapShown = false;
double gGhostGapMeters = 0.0;
double gGhostSecondsBeforeRace = 0.0; // best ghost time when this race began, 0 if none
GhostLibrary gGhostLibrary;
// Name pool index for each local rival slot, redrawn at every race start so no two rivals share
// a name.
std::vector<int> gRivalNameSlots = PickRivalNames(7, 1);

CraftClass LocalRivalCraftClass(int pRivalIndex)
{
    if (pRivalIndex >= 0 && pRivalIndex < static_cast<int>(gRivalNameSlots.size()))
        return RivalCraftClass(gRivalNameSlots[pRivalIndex]);
    return CraftClass::Balanced;
}

std::string LocalRivalName(int pRivalIndex)
{
    if (pRivalIndex >= 0 && pRivalIndex < static_cast<int>(gRivalNameSlots.size()))
        return RivalName(gRivalNameSlots[pRivalIndex]);
    return RivalName(pRivalIndex);
}

void DrawResultOverlay(int pWinner, int pPlayerPosition, int pCompetitorCount,
                       const std::vector<int>& pCompetitorPositions,
                       bool pChampionship, bool pChampionshipComplete, bool pChampionshipFinalEvent,
                       const char* pChampionshipPoints,
                       double pPlayerElapsedSeconds, const LapTiming& pLapTiming,
                       CraftClass pCraftClass, int pSelection, int pWidth, int pHeight)
{
    const int panelWidth = 620;
    const int panelHeight = pChampionship ? 408 : 352;
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
    if (pChampionshipComplete)
        DrawPixelText("SERIES COMPLETE", left + 42, top + 18, 4);
    else if (pWinner == 1)
        DrawPixelText("FINISH", left + 138, top + 18, 5);
    else
    {
        const std::string heading = gRaceWinnerName.empty() ? std::string("RIVAL FINISH")
                                                            : gRaceWinnerName + " WINS";
        const int headingScale = heading.size() > 14 ? 3 : 4;
        DrawPixelText(heading.c_str(), left + 210 - PixelTextWidth(heading.c_str(), headingScale) / 2,
                      top + 18, headingScale);
    }
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
    char improvement[40];
    if (pLapTiming.mLastImprovementSeconds > 0.0)
        std::snprintf(improvement, sizeof(improvement), "LAP IMPROVED BY %.2f S",
                      pLapTiming.mLastImprovementSeconds);
    else
        std::snprintf(improvement, sizeof(improvement), "NO NEW LAP BEST");
    char craft[40];
    std::snprintf(craft, sizeof(craft), "CRAFT %s", CraftClassName(pCraftClass));
    glColor3f(pLapTiming.mLastImprovementSeconds > 0.0 ? 0.18f : 0.72f,
              pLapTiming.mLastImprovementSeconds > 0.0 ? 0.96f : 0.82f,
              pLapTiming.mLastImprovementSeconds > 0.0 ? 0.4f : 0.84f);
    DrawPixelText(improvement, left + 110, top + 178, 2);
    glColor3f(0.82f, 0.9f, 0.92f);
    DrawPixelText(craft, left + 110, top + 200, 2);
    if (!pChampionship && (gNewGhostBest || gGhostSecondsBeforeRace > 0.0))
    {
        if (gNewGhostBest)
        {
            glColor3f(0.18f, 0.96f, 0.4f);
            DrawPixelText("NEW GHOST SAVED", left + 110, top + 224, 2);
        }
        if (gGhostSecondsBeforeRace > 0.0)
        {
            const int ghostSeconds = static_cast<int>(gGhostSecondsBeforeRace + 0.5);
            char ghostLine[40];
            const std::string ghostLabel = gNewGhostBest ? std::string("GHOST WAS")
                                                         : "GHOST " + gGhostSourceLabel;
            std::snprintf(ghostLine, sizeof(ghostLine), "%s %d M %d S", ghostLabel.c_str(),
                          ghostSeconds / 60, ghostSeconds % 60);
            glColor3f(0.82f, 0.9f, 0.92f);
            DrawPixelText(ghostLine, left + 110, top + 248, 2);
        }
    }
    glColor3f(0.2f, 0.9f, 1.0f);
    DrawPixelText("FINISHING ORDER", left + 412, top + 28, 2);
    glColor3f(0.18f, 0.36f, 0.4f);
    glBegin(GL_LINES);
    glVertex2i(left + 398, top + 18);
    glVertex2i(left + 398, top + 280);
    glEnd();
    const int visibleCompetitors = std::min(8, static_cast<int>(pCompetitorPositions.size()));
    for (int place = 1; place <= visibleCompetitors; ++place)
    {
        int competitor = -1;
        for (int index = 0; index < static_cast<int>(pCompetitorPositions.size()); ++index)
        {
            if (pCompetitorPositions[index] == place)
            {
                competitor = index;
                break;
            }
        }
        if (competitor < 0)
            continue;
        char orderEntry[48];
        if (competitor == 0)
            std::snprintf(orderEntry, sizeof(orderEntry), "%d  YOU", place);
        else
            std::snprintf(orderEntry, sizeof(orderEntry), "%d  %s", place,
                          LocalRivalName(competitor - 1).c_str());
        glColor3f(competitor == 0 ? 1.0f : 0.82f, competitor == 0 ? 0.78f : 0.9f,
                  competitor == 0 ? 0.12f : 0.92f);
        DrawPixelText(orderEntry, left + 414, top + 66 + (place - 1) * 27, 2);
    }
    if (pChampionship)
    {
        glColor3f(0.2f, 0.9f, 1.0f);
        DrawPixelText("SERIES UPDATE", left + 116, top + 224, 3);
        glColor3f(0.82f, 0.9f, 0.92f);
        for (std::size_t line = 0; line < gChampionshipLines.size(); ++line)
        {
            // The last line says what happens next, so it is picked out in yellow.
            if (line + 1 == gChampionshipLines.size() && line > 0)
                glColor3f(1.0f, 0.86f, 0.1f);
            DrawPixelText(gChampionshipLines[line].c_str(), left + 42, top + 254 + static_cast<int>(line) * 24, 2);
        }
        (void)pChampionshipPoints;
    }
    const int actionsTop = top + (pChampionship ? 352 : 292);
    for (int action = 0; action < 3; ++action)
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
    const char* primaryAction = pChampionshipComplete ? "NEW SERIES"
        : pChampionshipFinalEvent ? "FINAL RESULTS" : pChampionship ? "NEXT EVENT" : "RESTART";
    DrawPixelText(primaryAction, left + 42, actionsTop + 12, 2);
    glColor3f(pSelection == 1 ? 1.0f : 0.82f, pSelection == 1 ? 0.82f : 0.9f,
              pSelection == 1 ? 0.22f : 0.92f);
    DrawPixelText("SETUP", left + 270, actionsTop + 12, 2);
    glColor3f(pSelection == 2 ? 1.0f : 0.82f, pSelection == 2 ? 0.82f : 0.9f,
              pSelection == 2 ? 0.22f : 0.92f);
    DrawPixelText("MAIN MENU", left + 436, actionsTop + 12, 2);
}

KeyBindings gKeyBindings;
PadBindings gPadBindings;
constexpr int kKeyRemapRows = static_cast<int>(BindAction::Count);
constexpr int kRemapRows = kKeyRemapRows + static_cast<int>(PadAction::Count);
CameraRig gCameraRig;
bool gRemapOpen = false;
bool gRemapCapturing = false;
int gRemapSelection = 0;

void DrawPauseOverlay(int pSelection, bool pControllerConnected, bool pSteeringAssist,
                      bool pBrakingAssist, int pCameraDistanceSetting, int pMenuVolume,
                      int pRaceVolume,
                      int pWidth, int pHeight)
{
    const int panelWidth = 760;
    const int panelHeight = 536;
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
    DrawPixelText("PAUSED", left + 104, top + 22, 4);
    const char* cameraDistances[] = {"NEAR", "MEDIUM", "FAR"};
    const char* actions[] = {"RESUME", "STEERING ASSIST", "BRAKING ASSIST",
                             "CAMERA DISTANCE", "CAMERA MOTION", "HUD TEXT", "MENU VOLUME", "RACE VOLUME",
                             "KEY BINDINGS", "MAIN MENU", "EXIT OPENHOVER"};
    for (int action = 0; action < 11; ++action)
    {
        const int actionTop = top + 70 + action * 40;
        const bool selected = action == pSelection;
        glColor3f(selected ? 0.12f : 0.03f, selected ? 0.52f : 0.1f,
                  selected ? 0.62f : 0.14f);
        glBegin(GL_QUADS);
        glVertex2i(left + 24, actionTop);
        glVertex2i(left + 350, actionTop);
        glVertex2i(left + 350, actionTop + 34);
        glVertex2i(left + 24, actionTop + 34);
        glEnd();
        glColor3f(selected ? 1.0f : 0.82f, selected ? 0.82f : 0.9f,
                  selected ? 0.22f : 0.92f);
        DrawPixelText(actions[action], left + 42, actionTop + 10, 2);
        if (action == 1)
            DrawPixelText(pSteeringAssist ? "ON" : "OFF", left + 292, actionTop + 10, 2);
        else if (action == 2)
            DrawPixelText(pBrakingAssist ? "ON" : "OFF", left + 292, actionTop + 10, 2);
        else if (action == 3)
            DrawPixelText(cameraDistances[pCameraDistanceSetting], left + 262, actionTop + 10, 2);
        else if (action == 4)
            DrawPixelText(CameraMotionName(gCameraMotion), left + 238, actionTop + 10, 2);
        else if (action == 5)
            DrawPixelText(gHudTextLarge ? "LARGE" : "NORMAL", left + 262, actionTop + 10, 2);
        else if (action == 6 || action == 7)
        {
            char volume[8];
            std::snprintf(volume, sizeof(volume), "%d", action == 6 ? pMenuVolume : pRaceVolume);
            DrawPixelText(volume, left + 292, actionTop + 10, 2);
        }
    }
    glColor3f(0.2f, 0.9f, 1.0f);
    DrawPixelText(gRemapOpen ? "KEY BINDINGS" : "CONTROLS", left + 486, top + 24, 3);
    glColor3f(0.82f, 0.9f, 0.92f);
    if (gRemapOpen)
    {
        for (int i = 0; i < kRemapRows; ++i)
        {
            const bool selected = i == gRemapSelection;
            const int rowTop = top + 74 + i * 34;
            if (selected)
            {
                glColor3f(0.12f, 0.52f, 0.62f);
                glBegin(GL_QUADS);
                glVertex2i(left + 390, rowTop - 6);
                glVertex2i(left + 736, rowTop - 6);
                glVertex2i(left + 736, rowTop + 24);
                glVertex2i(left + 390, rowTop + 24);
                glEnd();
            }
            glColor3f(selected ? 1.0f : 0.82f, selected ? 0.82f : 0.9f, selected ? 0.22f : 0.92f);
            const bool padRow = i >= kKeyRemapRows;
            DrawPixelText(padRow ? PadBindings::ActionName(static_cast<PadAction>(i - kKeyRemapRows))
                                 : KeyBindings::ActionName(static_cast<BindAction>(i)),
                          left + 398, rowTop, 2);
            std::string keyName = selected && gRemapCapturing
                ? std::string(padRow ? "PRESS BTN" : "PRESS KEY")
                : padRow
                    ? std::string(PadBindings::ButtonName(
                          gPadBindings.Button(static_cast<PadAction>(i - kKeyRemapRows))))
                    : std::string(SDL_GetScancodeName(
                          static_cast<SDL_Scancode>(gKeyBindings.Key(static_cast<BindAction>(i)))));
            for (char& c : keyName)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            if (keyName.size() > 11)
                keyName.resize(11);
            DrawPixelText(keyName.c_str(), left + 590, rowTop, 2);
        }
        glColor3f(0.82f, 0.9f, 0.92f);
        DrawPixelText("ENTER CHANGE", left + 398, top + 390, 2);
        DrawPixelText("BACKSPACE DEFAULTS", left + 398, top + 414, 2);
        DrawPixelText("ARROWS AND SHIFT FIXED", left + 398, top + 438, 2);
    }
    else if (pControllerConnected)
    {
        DrawPixelText("LEFT STICK  STEER", left + 398, top + 80, 2);
        DrawPixelText("TRIGGERS    DRIVE", left + 398, top + 112, 2);
        const auto padLine = [&](PadAction pAction, const char* pLabel, int pRow)
        {
            std::string line = PadBindings::ButtonName(gPadBindings.Button(pAction));
            if (line.size() < 12)
                line.append(12 - line.size(), ' ');
            line += pLabel;
            DrawPixelText(line.c_str(), left + 398, top + 144 + pRow * 32, 2);
        };
        padLine(PadAction::Jump, "JUMP", 0);
        padLine(PadAction::Fire, "FIRE", 1);
        padLine(PadAction::Recover, "RECOVER", 2);
    }
    else
    {
        const auto keyLine = [&](BindAction pAction, const char* pExtra, const char* pLabel, int pRow)
        {
            std::string name = SDL_GetScancodeName(
                static_cast<SDL_Scancode>(gKeyBindings.Key(pAction)));
            for (char& c : name)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            std::string line = name + pExtra;
            if (line.size() < 12)
                line.append(12 - line.size(), ' ');
            line += pLabel;
            DrawPixelText(line.c_str(), left + 398, top + 80 + pRow * 32, 2);
        };
        keyLine(BindAction::SteerLeft, " ARROWS", "STEER L", 0);
        keyLine(BindAction::SteerRight, " ARROWS", "STEER R", 1);
        keyLine(BindAction::Accelerate, " SHIFT", "ACCEL", 2);
        keyLine(BindAction::Brake, " DOWN", "BRAKE", 3);
        DrawPixelText("UP          JUMP", left + 398, top + 80 + 4 * 32, 2);
        keyLine(BindAction::Fire, "", "FIRE", 5);
        keyLine(BindAction::Recover, "", "RECOVER", 6);
    }
    glColor3f(1.0f, 0.78f, 0.12f);
    DrawPixelText(gRemapOpen ? "ESC BACK" : "ESC RESUME", left + 492, top + 486, 2);
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

void DrawMissile(const HovercraftState& pState)
{
    const HovercraftState& state = pState;
    glPushMatrix();
    glTranslated(state.mX, state.mHeight, state.mY);
    glRotated(-state.mTravelHeading * 180.0 / kPi, 0.0, 1.0, 0.0);

    glColor3f(0.06f, 0.22f, 0.94f);
    glBegin(GL_QUAD_STRIP);
    for (int degree = 0; degree <= 360; degree += 60)
    {
        const double angle = degree * kPi / 180.0;
        const double vertical = std::sin(angle) * 0.17;
        const double sideways = std::cos(angle) * 0.17;
        glVertex3d(-0.46, vertical, sideways);
        glVertex3d(0.3, vertical, sideways);
    }
    glEnd();

    glColor3f(0.08f, 0.9f, 1.0f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3d(0.78, 0.0, 0.0);
    for (int degree = 0; degree <= 360; degree += 60)
    {
        const double angle = degree * kPi / 180.0;
        glVertex3d(0.3, std::sin(angle) * 0.17, std::cos(angle) * 0.17);
    }
    glEnd();

    glColor3f(1.0f, 0.84f, 0.05f);
    glBegin(GL_QUAD_STRIP);
    for (int degree = 0; degree <= 360; degree += 60)
    {
        const double angle = degree * kPi / 180.0;
        const double vertical = std::sin(angle) * 0.18;
        const double sideways = std::cos(angle) * 0.18;
        glVertex3d(-0.18, vertical, sideways);
        glVertex3d(-0.02, vertical, sideways);
    }
    glEnd();

    glColor3f(1.0f, 0.04f, 0.48f);
    for (int fin = 0; fin < 4; ++fin)
    {
        const double angle = fin * kPi * 0.5;
        const double vertical = std::sin(angle);
        const double sideways = std::cos(angle);
        glBegin(GL_TRIANGLES);
        glVertex3d(-0.18, vertical * 0.14, sideways * 0.14);
        glVertex3d(-0.56, vertical * 0.42, sideways * 0.42);
        glVertex3d(0.14, vertical * 0.15, sideways * 0.15);
        glEnd();
    }

    glColor3f(0.98f, 0.98f, 1.0f);
    glBegin(GL_QUADS);
    glVertex3d(0.02, 0.175, -0.11);
    glVertex3d(0.27, 0.175, -0.09);
    glVertex3d(0.34, 0.175, 0.09);
    glVertex3d(0.02, 0.175, 0.11);
    glEnd();

    const double exhaustLength = 0.28 + std::fmod(state.mSpeed * 0.018, 0.22);
    glDisable(GL_LIGHTING);
    glColor3f(1.0f, 0.16f, 0.02f);
    glBegin(GL_TRIANGLES);
    glVertex3d(-0.46, -0.14, 0.0);
    glVertex3d(-0.46, 0.14, 0.0);
    glVertex3d(-0.46 - exhaustLength, 0.0, 0.0);
    glEnd();
    glColor3f(1.0f, 0.92f, 0.1f);
    glBegin(GL_TRIANGLES);
    glVertex3d(-0.47, -0.07, 0.0);
    glVertex3d(-0.47, 0.07, 0.0);
    glVertex3d(-0.47 - exhaustLength * 0.72, 0.0, 0.0);
    glEnd();
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

void DrawMissile(const Missile& pMissile)
{
    if (pMissile.Active())
        DrawMissile(pMissile.State());
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

void DrawTrackMinimap(int pTrackIndex, int pLeft, int pTop, int pSize)
{
    const std::vector<TrackDefinition>& tracks = gLocalTracks;
    if (pTrackIndex < 0 || pTrackIndex >= static_cast<int>(tracks.size())
        || tracks[pTrackIndex].mWaypoints.size() < 2)
    {
        return;
    }

    const std::vector<RaceGate>& waypoints = tracks[pTrackIndex].mWaypoints;
    double minimumX = waypoints.front().mX;
    double maximumX = minimumX;
    double minimumY = waypoints.front().mY;
    double maximumY = minimumY;
    for (const RaceGate& waypoint : waypoints)
    {
        minimumX = std::min(minimumX, waypoint.mX);
        maximumX = std::max(maximumX, waypoint.mX);
        minimumY = std::min(minimumY, waypoint.mY);
        maximumY = std::max(maximumY, waypoint.mY);
    }
    const double span = std::max(1.0, std::max(maximumX - minimumX, maximumY - minimumY));
    const double scale = (pSize - 28.0) / span;
    const double offsetX = pLeft + (pSize - (maximumX - minimumX) * scale) * 0.5;
    const double offsetY = pTop + (pSize - (maximumY - minimumY) * scale) * 0.5;
    const auto mapX = [minimumX, offsetX, scale](double pX)
    {
        return static_cast<int>(offsetX + (pX - minimumX) * scale);
    };
    const auto mapY = [minimumY, offsetY, scale](double pY)
    {
        return static_cast<int>(offsetY + (pY - minimumY) * scale);
    };

    glLineWidth(6.0f);
    glColor3f(0.16f, 0.48f, 0.56f);
    glBegin(GL_LINE_LOOP);
    for (const RaceGate& waypoint : waypoints)
        glVertex2i(mapX(waypoint.mX), mapY(waypoint.mY));
    glEnd();
    glLineWidth(2.0f);
    glColor3f(0.7f, 0.94f, 0.96f);
    glBegin(GL_LINE_LOOP);
    for (const RaceGate& waypoint : waypoints)
        glVertex2i(mapX(waypoint.mX), mapY(waypoint.mY));
    glEnd();
    glLineWidth(1.0f);

    const int startX = mapX(waypoints.front().mX);
    const int startY = mapY(waypoints.front().mY);
    glColor3f(1.0f, 0.78f, 0.12f);
    glBegin(GL_QUADS);
    glVertex2i(startX - 4, startY - 4);
    glVertex2i(startX + 4, startY - 4);
    glVertex2i(startX + 4, startY + 4);
    glVertex2i(startX - 4, startY + 4);
    glEnd();
}

// ---- Track editor -------------------------------------------------------------------------
// A top-down canvas where the player places corner points; the builder turns them into a valid
// track (start straight, checkpoints, bridges, pads) and the screen shows the result live.
struct TrackEditorState
{
    std::vector<EditorPoint> mPoints;
    double mHalfWidth = 8.0;
    int mDragIndex = -1;
    bool mNaming = false;
    std::string mName = "My Track";
    std::string mStatus;
    BuiltTrack mBuilt;
    // Index in gLocalTracks of the track this editor last saved, so saving again after more edits
    // replaces it instead of being refused as a duplicate name.
    int mSavedIndex = -1;
    bool mMines = false;   // scatter mines along long straights
    bool mHazards = false; // scatter slowing hazard zones along long straights
};
TrackEditorState gEditor;
constexpr double kEditorWorldHalf = 600.0;
constexpr double kEditorGrid = 10.0;
constexpr std::size_t kEditorMaximumPoints = 64;

struct EditorLayout
{
    int mCanvasLeft = 24;
    int mCanvasTop = 70;
    int mCanvasSize = 500;
    int mPanelLeft = 540;
    int mPanelWidth = 240;
};

EditorLayout ComputeEditorLayout(int pWidth, int pHeight)
{
    EditorLayout layout;
    layout.mCanvasSize = std::max(300, std::min(pHeight - 170, pWidth - 330));
    layout.mPanelLeft = layout.mCanvasLeft + layout.mCanvasSize + 20;
    layout.mPanelWidth = std::max(200, pWidth - layout.mPanelLeft - 24);
    return layout;
}

int EditorScreenX(const EditorLayout& pLayout, double pWorldX)
{
    return pLayout.mCanvasLeft + static_cast<int>((pWorldX + kEditorWorldHalf) / (2.0 * kEditorWorldHalf)
                                                  * pLayout.mCanvasSize);
}

int EditorScreenY(const EditorLayout& pLayout, double pWorldY)
{
    return pLayout.mCanvasTop + pLayout.mCanvasSize
        - static_cast<int>((pWorldY + kEditorWorldHalf) / (2.0 * kEditorWorldHalf) * pLayout.mCanvasSize);
}

double EditorWorldX(const EditorLayout& pLayout, int pScreenX)
{
    return (pScreenX - pLayout.mCanvasLeft) * 2.0 * kEditorWorldHalf / pLayout.mCanvasSize - kEditorWorldHalf;
}

double EditorWorldY(const EditorLayout& pLayout, int pScreenY)
{
    return (pLayout.mCanvasTop + pLayout.mCanvasSize - pScreenY) * 2.0 * kEditorWorldHalf
        / pLayout.mCanvasSize - kEditorWorldHalf;
}

double EditorSnap(double pValue)
{
    const double snapped = std::floor(pValue / kEditorGrid + 0.5) * kEditorGrid;
    return std::max(-kEditorWorldHalf + 20.0, std::min(kEditorWorldHalf - 20.0, snapped));
}

void RefreshEditorBuild()
{
    BuilderOptions options;
    options.mMines = gEditor.mMines;
    options.mHazards = gEditor.mHazards;
    gEditor.mBuilt = BuildTrackFromPoints(gEditor.mName, gPlayerDisplayName, gEditor.mPoints,
                                          gEditor.mHalfWidth, options);
}

// Buttons down the right-hand panel, top to bottom.
enum EditorButton
{
    kEditorName,
    kEditorNarrower,
    kEditorWider,
    kEditorMines,
    kEditorHazards,
    kEditorUndo,
    kEditorClear,
    kEditorSave,
    kEditorDrive,
    kEditorBack,
    kEditorButtonCount
};

void EditorButtonRect(const EditorLayout& pLayout, int pButton, int& pLeft, int& pTop, int& pWidth, int& pHeight)
{
    pLeft = pLayout.mPanelLeft;
    pWidth = pLayout.mPanelWidth;
    pHeight = 38;
    pTop = pLayout.mCanvasTop + 4 + pButton * 46;
    // Rows: name, narrower/wider, mines/hazards, then one button per row.
    if (pButton == kEditorNarrower || pButton == kEditorWider || pButton == kEditorMines
        || pButton == kEditorHazards)
    {
        pWidth = (pLayout.mPanelWidth - 8) / 2;
        const bool right = pButton == kEditorWider || pButton == kEditorHazards;
        if (right)
            pLeft += pWidth + 8;
        pTop = pLayout.mCanvasTop + 4 + (pButton <= kEditorWider ? 1 : 2) * 46;
    }
    else if (pButton > kEditorHazards)
        pTop = pLayout.mCanvasTop + 4 + (pButton - 2) * 46;
}

void DrawTrackEditor(int pWidth, int pHeight)
{
    const EditorLayout layout = ComputeEditorLayout(pWidth, pHeight);
    glColor3f(0.075f, 0.075f, 0.095f);
    glBegin(GL_QUADS);
    glVertex2i(0, 0);
    glVertex2i(pWidth, 0);
    glVertex2i(pWidth, pHeight);
    glVertex2i(0, pHeight);
    glEnd();
    glColor3f(0.2f, 0.9f, 1.0f);
    DrawPixelText("TRACK EDITOR", 24, 24, 3);
    glColor3f(0.72f, 0.78f, 0.82f);
    DrawPixelText("CLICK ADD POINT  DRAG MOVE  RIGHT CLICK DELETE", 270, 30, 2);

    // Canvas and grid.
    glColor3f(0.03f, 0.05f, 0.07f);
    glBegin(GL_QUADS);
    glVertex2i(layout.mCanvasLeft, layout.mCanvasTop);
    glVertex2i(layout.mCanvasLeft + layout.mCanvasSize, layout.mCanvasTop);
    glVertex2i(layout.mCanvasLeft + layout.mCanvasSize, layout.mCanvasTop + layout.mCanvasSize);
    glVertex2i(layout.mCanvasLeft, layout.mCanvasTop + layout.mCanvasSize);
    glEnd();
    glColor3f(0.1f, 0.16f, 0.2f);
    glBegin(GL_LINES);
    for (int line = -500; line <= 500; line += 100)
    {
        glVertex2i(EditorScreenX(layout, line), layout.mCanvasTop);
        glVertex2i(EditorScreenX(layout, line), layout.mCanvasTop + layout.mCanvasSize);
        glVertex2i(layout.mCanvasLeft, EditorScreenY(layout, line));
        glVertex2i(layout.mCanvasLeft + layout.mCanvasSize, EditorScreenY(layout, line));
    }
    glEnd();

    // The player's own loop.
    const std::vector<EditorPoint>& points = gEditor.mPoints;
    glColor3f(0.3f, 0.5f, 0.56f);
    glBegin(GL_LINE_STRIP);
    for (const EditorPoint& point : points)
        glVertex2i(EditorScreenX(layout, point.mX), EditorScreenY(layout, point.mY));
    glEnd();
    if (points.size() >= 3)
    {
        glColor3f(0.16f, 0.26f, 0.3f);
        glBegin(GL_LINES);
        glVertex2i(EditorScreenX(layout, points.back().mX), EditorScreenY(layout, points.back().mY));
        glVertex2i(EditorScreenX(layout, points.front().mX), EditorScreenY(layout, points.front().mY));
        glEnd();
    }

    // What the builder made of it: the real route, checkpoints, bridges and pads.
    if (gEditor.mBuilt.mOk)
    {
        const TrackDefinition& track = gEditor.mBuilt.mTrack;
        glColor3f(0.2f, 0.9f, 1.0f);
        glBegin(GL_LINE_LOOP);
        for (const RaceGate& gate : track.mWaypoints)
            glVertex2i(EditorScreenX(layout, gate.mX), EditorScreenY(layout, gate.mY));
        glEnd();
        const auto marker = [&](double pX, double pY, int pHalf)
        {
            const int x = EditorScreenX(layout, pX);
            const int y = EditorScreenY(layout, pY);
            glBegin(GL_QUADS);
            glVertex2i(x - pHalf, y - pHalf);
            glVertex2i(x + pHalf, y - pHalf);
            glVertex2i(x + pHalf, y + pHalf);
            glVertex2i(x - pHalf, y + pHalf);
            glEnd();
        };
        glColor3f(1.0f, 0.86f, 0.1f);
        for (const RaceGate& gate : track.mCheckpoints)
            marker(gate.mX, gate.mY, 5);
        glColor3f(0.4f, 0.8f, 1.0f);
        for (const BoostPad& pad : track.mBoostPads)
            marker(pad.mX, pad.mY, 3);
        glColor3f(1.0f, 0.5f, 0.15f);
        for (const RaisedSection& section : track.mRaisedSections)
            marker(section.mX, section.mY, 6);
        glColor3f(1.0f, 0.25f, 0.25f);
        for (const Mine& mine : track.mMines)
            marker(mine.mX, mine.mY, 4);
        glColor3f(0.75f, 0.35f, 1.0f);
        for (const HazardZone& zone : track.mHazardZones)
            marker(zone.mX, zone.mY, 5);
        glColor3f(0.2f, 1.0f, 0.4f);
        marker(track.mWaypoints.front().mX, track.mWaypoints.front().mY, 7);
    }
    for (std::size_t index = 0; index < points.size(); ++index)
    {
        const bool dragged = static_cast<int>(index) == gEditor.mDragIndex;
        glColor3f(dragged ? 1.0f : (index == 0 ? 0.2f : 0.95f), dragged ? 0.86f : (index == 0 ? 1.0f : 0.95f),
                  dragged ? 0.1f : (index == 0 ? 0.4f : 0.95f));
        const int x = EditorScreenX(layout, points[index].mX);
        const int y = EditorScreenY(layout, points[index].mY);
        glBegin(GL_QUADS);
        glVertex2i(x - 4, y - 4);
        glVertex2i(x + 4, y - 4);
        glVertex2i(x + 4, y + 4);
        glVertex2i(x - 4, y + 4);
        glEnd();
    }

    // Status line under the canvas.
    char statusLine[96];
    if (gEditor.mBuilt.mOk)
    {
        double length = 0.0;
        const std::vector<RaceGate>& route = gEditor.mBuilt.mTrack.mWaypoints;
        for (std::size_t index = 0; index < route.size(); ++index)
        {
            const RaceGate& next = route[(index + 1) % route.size()];
            length += std::hypot(next.mX - route[index].mX, next.mY - route[index].mY);
        }
        // Cars cruise at roughly 30 m/s, so this is a rough guide to lap time.
        std::snprintf(statusLine, sizeof(statusLine), "TRACK OK  %d POINTS  ABOUT %d M  %d S A LAP",
                      static_cast<int>(points.size()), static_cast<int>(length),
                      static_cast<int>(length / 30.0 + 0.5));
        glColor3f(0.18f, 0.96f, 0.4f);
    }
    else
    {
        std::snprintf(statusLine, sizeof(statusLine), "%s", gEditor.mBuilt.mProblem.c_str());
        glColor3f(1.0f, 0.4f, 0.3f);
    }
    DrawPixelText(statusLine, layout.mCanvasLeft, layout.mCanvasTop + layout.mCanvasSize + 12, 2);
    if (!gEditor.mStatus.empty())
    {
        glColor3f(1.0f, 0.86f, 0.1f);
        DrawPixelText(gEditor.mStatus.c_str(), layout.mCanvasLeft, layout.mCanvasTop + layout.mCanvasSize + 40, 2);
    }
    glColor3f(0.55f, 0.62f, 0.66f);
    DrawPixelText("GREEN START  YELLOW GATE  ORANGE BRIDGE  BLUE PAD  RED MINE  PURPLE HAZARD", layout.mCanvasLeft,
                  layout.mCanvasTop + layout.mCanvasSize + 68, 2);

    // Buttons.
    for (int button = 0; button < kEditorButtonCount; ++button)
    {
        int left = 0;
        int top = 0;
        int width = 0;
        int height = 0;
        EditorButtonRect(layout, button, left, top, width, height);
        const bool active = (button == kEditorName && gEditor.mNaming)
            || (button == kEditorMines && gEditor.mMines) || (button == kEditorHazards && gEditor.mHazards);
        glColor3f(active ? 0.12f : 0.14f, active ? 0.52f : 0.2f, active ? 0.62f : 0.28f);
        glBegin(GL_QUADS);
        glVertex2i(left, top);
        glVertex2i(left + width, top);
        glVertex2i(left + width, top + height);
        glVertex2i(left, top + height);
        glEnd();
        std::string label;
        switch (button)
        {
        case kEditorName:
            label = "NAME " + gEditor.mName + (active ? "_" : "");
            break;
        case kEditorNarrower:
            label = "NARROWER";
            break;
        case kEditorWider:
            label = "WIDER";
            break;
        case kEditorMines:
            label = gEditor.mMines ? "MINES ON" : "MINES OFF";
            break;
        case kEditorHazards:
            label = gEditor.mHazards ? "HAZARDS ON" : "HAZARDS OFF";
            break;
        case kEditorUndo:
            label = "UNDO POINT";
            break;
        case kEditorClear:
            label = "CLEAR";
            break;
        case kEditorSave:
            label = "SAVE TRACK";
            break;
        case kEditorDrive:
            label = "SAVE AND DRIVE";
            break;
        default:
            label = "BACK";
            break;
        }
        for (char& c : label)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        const bool saveButton = button == kEditorSave || button == kEditorDrive;
        glColor3f(saveButton && gEditor.mBuilt.mOk ? 0.4f : 0.9f,
                  saveButton && gEditor.mBuilt.mOk ? 1.0f : 0.92f,
                  saveButton && gEditor.mBuilt.mOk ? 0.5f : 0.95f);
        const int scale = (label.size() * 12 > static_cast<std::size_t>(width - 12)) ? 1 : 2;
        DrawPixelText(label.c_str(), left + 10, top + (scale == 2 ? 12 : 15), scale);
    }
    char widthLine[48];
    std::snprintf(widthLine, sizeof(widthLine), "ROAD WIDTH %d", static_cast<int>(gEditor.mHalfWidth * 2.0));
    glColor3f(0.72f, 0.78f, 0.82f);
    DrawPixelText(widthLine, layout.mPanelLeft, layout.mCanvasTop + 4 + (kEditorButtonCount - 1) * 46 + 10, 2);
}

void DrawFrontScreen(FrontScreen pScreen, int pSelection, int pCameraDistanceSetting,
                     bool pAudioEnabled, int pMenuVolume, int pRaceVolume,
                     int pTrackIndex, int pLaps, int pRivalCount,
                     RivalDifficulty pRivalDifficulty, RaceMode pRaceMode, bool pWeaponsAllowed,
                     CraftClass pCraftClass, bool pSteeringAssist, bool pBrakingAssist,
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

        const char* options[] = {"PLAY LOCAL GAME", "MULTIPLAYER", "HOW TO PLAY", "SETTINGS", "TRACK EDITOR", "QUIT"};
        for (int option = 0; option < 6; ++option)
        {
            int panelLeft = 0;
            int top = 0;
            int rowWidth = 0;
            int rowHeight = 0;
            WelcomeRowGeometry(pWidth, pHeight, option, panelLeft, top, rowWidth, rowHeight);
            const bool selected = option == pSelection;
            glColor3f(selected ? 0.12f : 0.03f, selected ? 0.52f : 0.1f,
                      selected ? 0.62f : 0.14f);
            glBegin(GL_QUADS);
            glVertex2i(panelLeft, top);
            glVertex2i(panelLeft + rowWidth, top);
            glVertex2i(panelLeft + rowWidth, top + rowHeight);
            glVertex2i(panelLeft, top + rowHeight);
            glEnd();
            glColor3f(selected ? 1.0f : 0.56f, selected ? 0.82f : 0.72f,
                      selected ? 0.22f : 0.76f);
            DrawPixelText(options[option], panelLeft + 68, top + (rowHeight - 21) / 2, 3);
        }
        glColor3f(0.72f, 0.82f, 0.84f);
        if (!gStartupNotice.empty())
        {
            glColor3f(1.0f, 0.6f, 0.25f);
            DrawPixelText(gStartupNotice.c_str(), 24, pHeight - 24, 1);
            glColor3f(0.72f, 0.82f, 0.84f);
        }
        DrawPixelText("UP DOWN TO SELECT", pWidth / 2 - 132, pHeight - 82, 3);
        DrawPixelText("ENTER TO CONFIRM", pWidth / 2 - 120, pHeight - 52, 3);
    }
    else if (pScreen == FrontScreen::TrackEditor)
    {
        DrawTrackEditor(pWidth, pHeight);
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
        // The action column needs room for its buttons and status text even on narrow windows.
        const int actionWidth = std::max(pWidth * 15 / 100, 210);
        const int detailWidth = pWidth - margin * 2 - gap * 2 - leftWidth - actionWidth;
        // Tall enough for the action column: two buttons, status text, and the Back button.
        const int topHeight = std::max(pHeight * 42 / 100, 290);
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
        const std::string userCount = "USERS LIST (" + std::to_string(gLobbyPlayers.size()) + ")";
        DrawPixelText(userCount.c_str(), margin + 16, bottomTop + 18, 2);
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
            const LobbyPlayerView& player = gLobbyPlayers[playerIndex];
            glColor3f(player.mHost ? 0.95f : 0.76f, player.mHost ? 0.72f : 0.76f,
                      player.mHost ? 0.2f : 0.82f);
            const bool muted = gLobbyMuteList.IsMuted(
                static_cast<LobbyPlayerId>(player.mId));
            std::string presence;
            if (player.mRoomId != 0)
                presence = player.mHost ? " HOST" : player.mReady ? " READY" : " NOT READY";
            else
                presence = " LOBBY";
            const std::string playerLabel = player.mDisplayName + presence
                + (muted ? " MUTED" : "");
            DrawPixelText(playerLabel.c_str(), margin + 26, playerTop, 2);
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
        glColor3f(0.66f, 0.66f, 0.73f);
        if (gLobbySelectedRoom >= 0 && gLobbySelectedRoom < static_cast<int>(gLobbyRooms.size()))
        {
            const LobbyRoomView& room = gLobbyRooms[gLobbySelectedRoom];
            // Preview the room's track from this player's own copy. A custom track this player
            // does not have yet downloads automatically when they join the room.
            const int previewTrack = LocalIndexForRoomTrack(room.mTrackIndex, room.mCustomHash);
            const std::string previewTrackName = room.mName;
            if (previewTrack >= 0)
                DrawTrackMinimap(previewTrack, previewLeft, previewTop, previewSize);
            const std::string players = std::to_string(room.mPlayerCount) + " OF "
                + std::to_string(room.mPlayerCapacity) + " PLAYERS";
            DrawPixelText(room.mName.c_str(), previewLeft + previewSize + 16, previewTop + 4, 2);
            DrawPixelText(players.c_str(), previewLeft + previewSize + 16, previewTop + 28, 2);
            DrawPixelText(previewTrackName.c_str(), previewLeft + previewSize + 16, previewTop + 52, 2);
            if (previewTrack < 0)
            {
                glColor3f(1.0f, 0.3f, 0.2f);
                DrawPixelText("CUSTOM TRACK - DOWNLOADS WHEN YOU JOIN", previewLeft, previewTop + previewSize + 8, 2);
                glColor3f(0.66f, 0.66f, 0.73f);
            }
            const std::string settings = std::to_string(room.mLapCount) + " LAPS  "
                + std::to_string(room.mRivalCount) + " RIVALS  "
                + (room.mWeaponsAllowed ? "WEAPONS ON" : "WEAPONS OFF");
            DrawPixelText(settings.c_str(), previewLeft + previewSize + 16, previewTop + 76, 2);
            DrawPixelText(room.mRaceRunning ? "RACE IN PROGRESS" : "WAITING FOR HOST",
                          previewLeft + previewSize + 16, previewTop + 100, 2);
            const std::string ready = std::to_string(room.mReadyPlayerCount) + " OF "
                + std::to_string(room.mPlayerCount) + " READY";
            DrawPixelText(ready.c_str(), previewLeft + previewSize + 16, previewTop + 124, 2);
            if (room.mPrivate)
            {
                const std::string access = room.mHostId == gLobbyPlayerId && !gHostedRoomCode.empty()
                    ? "PRIVATE CODE " + gHostedRoomCode : "PRIVATE ROOM";
                DrawPixelText(access.c_str(), previewLeft + previewSize + 16, previewTop + 148, 2);
            }
        }
        else
        {
            DrawPixelText("SELECT A RACE TO SEE", previewLeft + previewSize + 16, previewTop + 4, 2);
            DrawPixelText("ITS TRACK AND LAPS", previewLeft + previewSize + 16, previewTop + 28, 2);
            DrawPixelText("WEAPONS AND PLAYERS", previewLeft + previewSize + 16, previewTop + 52, 2);
        }

        const bool selectedRoomIsHosted = gLobbySelectedRoom >= 0
            && gLobbySelectedRoom < static_cast<int>(gLobbyRooms.size())
            && gLobbyRooms[gLobbySelectedRoom].mHostId == gLobbyPlayerId;
        const bool selectedRoomIsJoined = gLobbySelectedRoom >= 0
            && gLobbySelectedRoom < static_cast<int>(gLobbyRooms.size())
            && gLobbyRooms[gLobbySelectedRoom].mId == gLobbyJoinedRoomId;
        DrawLobbyButton(selectedRoomIsHosted ? "START RACE"
                            : selectedRoomIsJoined ? (gLobbyReady ? "CANCEL READY" : "READY")
                                                   : "JOIN GAME",
                        actionLeft + 16, top + 16, actionWidth - 32, true);
        if (!selectedRoomIsHosted)
            DrawLobbyButton(selectedRoomIsJoined ? "LEAVE ROOM" : "HOST RACE",
                            actionLeft + 16, top + 72, actionWidth - 32, false);
        glColor3f(0.22f, 0.22f, 0.27f);
        glBegin(GL_LINES);
        glVertex2i(actionLeft + 16, top + 132);
        glVertex2i(actionLeft + actionWidth - 16, top + 132);
        glEnd();
        glColor3f(0.62f, 0.62f, 0.68f);
        DrawPixelText("ENTER OR A ACTION", actionLeft + 16, top + 138, 1);
        glColor3f(0.95f, 0.35f, 0.4f);
        DrawPixelText(gLobbyStatus.c_str(), actionLeft + 16, top + 158, 2);
        if (!gLobbyServerVersion.empty())
        {
            glColor3f(0.62f, 0.72f, 0.76f);
            const std::string server = "SERVER " + gLobbyServerVersion;
            const std::string compatibility = "PROTOCOL " + std::to_string(gLobbyServerProtocol)
                + "  CONTENT " + std::to_string(gLobbyServerContent);
            DrawPixelText(server.c_str(), actionLeft + 16, top + 190, 1);
            DrawPixelText(compatibility.c_str(), actionLeft + 16, top + 208, 1);
        }
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
        const char* labels[] = {"MODE", "TRACK", "LAPS", "MAX PLAYERS", "RIVALS", "WEAPONS",
                                "ROOM ACCESS", "HOST RACE", "BACK"};
        glColor3f(0.075f, 0.075f, 0.095f);
        glBegin(GL_QUADS);
        glVertex2i(0, 0);
        glVertex2i(pWidth, 0);
        glVertex2i(pWidth, pHeight);
        glVertex2i(0, pHeight);
        glEnd();
        const int panelLeft = pWidth / 2 - 280;
        // 72 down from the top, or higher on a short window so the whole panel stays on screen.
        const int panelTop = std::max(8, std::min(72, pHeight - 538 - 8));
        glColor3f(0.105f, 0.105f, 0.13f);
        glBegin(GL_QUADS);
        glVertex2i(panelLeft, panelTop);
        glVertex2i(panelLeft + 560, panelTop);
        glVertex2i(panelLeft + 560, panelTop + 538);
        glVertex2i(panelLeft, panelTop + 538);
        glEnd();
        glColor3f(0.29f, 0.29f, 0.35f);
        glBegin(GL_LINE_LOOP);
        glVertex2i(panelLeft, panelTop);
        glVertex2i(panelLeft + 560, panelTop);
        glVertex2i(panelLeft + 560, panelTop + 538);
        glVertex2i(panelLeft, panelTop + 538);
        glEnd();
        glColor3f(0.95f, 0.35f, 0.4f);
        DrawPixelText("HOST RACE", panelLeft + 190, panelTop + 24, 5);
        for (int option = 0; option < 9; ++option)
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
                value = HostTrackName(gHostTrackIndex);
            else if (option == 2)
                value = std::to_string(gHostLapCount);
            else if (option == 3)
                value = std::to_string(gHostPlayerCapacity);
            else if (option == 4)
                value = std::to_string(gHostRivalCount);
            else if (option == 5)
                value = gHostWeaponsAllowed ? "ALLOWED" : "OFF";
            else if (option == 6)
                value = gHostPrivateRoom ? "PRIVATE CODE" : "PUBLIC LISTED";
            else if (option == 7)
                value = "CREATE ROOM";
            else
                value = "RETURN TO LOBBY";
            DrawPixelText(value.c_str(), panelLeft + 300, rowTop + 10, 2);
        }
        glColor3f(0.76f, 0.76f, 0.82f);
        DrawPixelText("UP DOWN SELECT  LEFT RIGHT CHANGE", panelLeft + 76, panelTop + 500, 2);
    }
    else if (pScreen == FrontScreen::HowToPlay)
    {
        glColor3f(0.2f, 0.9f, 1.0f);
        DrawPixelText("HOW TO PLAY", pWidth / 2 - 165, 70, 5);
        glColor3f(0.82f, 0.9f, 0.92f);
        const auto keyName = [](BindAction pAction)
        {
            std::string name = SDL_GetScancodeName(static_cast<SDL_Scancode>(gKeyBindings.Key(pAction)));
            for (char& c : name)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return name;
        };
        const auto padName = [](PadAction pAction)
        {
            return std::string(PadBindings::ButtonName(gPadBindings.Button(pAction)));
        };
        const std::string lines[] = {
            keyName(BindAction::Accelerate) + " OR SHIFT  ACCELERATE",
            keyName(BindAction::SteerLeft) + " " + keyName(BindAction::SteerRight)
                + " OR ARROWS  STEER",
            keyName(BindAction::Brake) + " OR DOWN  BRAKE",
            "UP  JUMP",
            keyName(BindAction::Fire) + "  FIRE MISSILE",
            keyName(BindAction::Recover) + "  RECOVER TO ROAD",
            "PAD  STICK STEER  TRIGGERS DRIVE",
            "PAD  " + padName(PadAction::Jump) + " JUMP  " + padName(PadAction::Fire) + " FIRE  "
                + padName(PadAction::Recover) + " RECOVER",
            "ALT  GHOST: BEST RUN, LAST RUN, OR OFF",
            "ESC OR START  PAUSE AND KEY BINDINGS"};
        // Tighten the lines on short windows so the guidance clears "ENTER TO RETURN" below it.
        const int lineStep = std::max(24, std::min(34, (pHeight - 150 - 130) / 10));
        int lineTop = 150;
        for (const std::string& line : lines)
        {
            DrawPixelText(line.c_str(), pWidth / 2 - PixelTextWidth(line.c_str(), 2) / 2, lineTop, 2);
            lineTop += lineStep;
        }
        glColor3f(1.0f, 0.86f, 0.1f);
        const char* guidance = "FOLLOW THE FLASHING WALL ARROWS";
        DrawPixelText(guidance, pWidth / 2 - PixelTextWidth(guidance, 3) / 2, lineTop + 12, 3);
        glColor3f(1.0f, 0.78f, 0.12f);
        DrawPixelText("ENTER TO RETURN", pWidth / 2 - 120, pHeight - 70, 3);
    }
    else if (pScreen == FrontScreen::LocalSetup)
    {
        const char* labels[] = {"START RACE", "MODE", "TRACK", "LAPS", "RIVALS", "DIFFICULTY",
                                "CRAFT", "ASSISTS", "WEAPONS", "BACK"};
        glColor3f(0.2f, 0.9f, 1.0f);
        DrawPixelText("LOCAL RACE", pWidth / 2 - 150, 70, 5);
        for (int option = 0; option < 10; ++option)
        {
            const int row = LocalSetupOptionRow(pRaceMode, option);
            if (row < 0)
                continue;
            int boxTop = 0;
            int boxHeight = 0;
            LocalSetupRowGeometry(pHeight, LocalSetupOptionRow(pRaceMode, 9) + 1, row, boxTop, boxHeight);
            // Text is laid out for a 40-pixel row, low in the box; move it up by however much the
            // row has been shortened.
            const int top = boxTop + (boxHeight - 40);
            const bool selected = option == pSelection;
            glColor3f(selected ? 0.12f : 0.03f, selected ? 0.52f : 0.1f,
                      selected ? 0.62f : 0.14f);
            glBegin(GL_QUADS);
            glVertex2i(pWidth / 2 - 250, boxTop);
            glVertex2i(pWidth / 2 + 250, boxTop);
            glVertex2i(pWidth / 2 + 250, boxTop + boxHeight);
            glVertex2i(pWidth / 2 - 250, boxTop + boxHeight);
            glEnd();
            glColor3f(selected ? 1.0f : 0.72f, selected ? 0.82f : 0.86f,
                      selected ? 0.22f : 0.9f);
            DrawPixelText(labels[option], pWidth / 2 - 218, top + 16, 3);
            if (option == 1)
                DrawPixelText(RaceModeSetupLabel(pRaceMode), pWidth / 2 + 18, top + 19, 2);
            else if (option == 2)
                {
                const std::string trackLabel = LocalTrackName(pTrackIndex);
                const bool longName = trackLabel.size() > 14;
                DrawPixelText(trackLabel.c_str(), pWidth / 2 + 10, top + (longName ? 19 : 16),
                              longName ? 2 : 3);
            }
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
            {
                DrawPixelText(CraftClassName(pCraftClass), pWidth / 2 + 96, top + 10, 3);
                DrawPixelText(CraftClassDescription(pCraftClass), pWidth / 2 + 18, top + 29, 1);
            }
            else if (option == 7)
            {
                const char* assists = pSteeringAssist && pBrakingAssist ? "STEER AND BRAKE"
                    : pSteeringAssist ? "STEERING" : pBrakingAssist ? "BRAKING" : "OFF";
                DrawPixelText(assists, pWidth / 2 + 54, top + 13, 2);
            }
            else if (option == 8)
                DrawPixelText(pWeaponsAllowed ? "ALLOWED" : "OFF", pWidth / 2 + 120, top + 10, 3);
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
        DrawPixelText("SETTINGS", pWidth / 2 - 120, 60, 5);
        const char* settingLabels[] = {"DISPLAY NAME", "CAMERA DISTANCE", "HUD TEXT", "AUDIO FEEDBACK",
                                       "MENU VOLUME", "RACE VOLUME", "BACK"};
        for (int row = 0; row < 7; ++row)
        {
            int boxTop = 0;
            int boxHeight = 0;
            LocalSetupRowGeometry(pHeight, 7, row, boxTop, boxHeight);
            const int textTop = boxTop + (boxHeight - 40);
            const bool selected = row == pSelection;
            glColor3f(selected ? 0.12f : 0.03f, selected ? 0.52f : 0.1f, selected ? 0.62f : 0.14f);
            glBegin(GL_QUADS);
            glVertex2i(pWidth / 2 - 250, boxTop);
            glVertex2i(pWidth / 2 + 250, boxTop);
            glVertex2i(pWidth / 2 + 250, boxTop + boxHeight);
            glVertex2i(pWidth / 2 - 250, boxTop + boxHeight);
            glEnd();
            glColor3f(selected ? 1.0f : 0.72f, selected ? 0.82f : 0.86f, selected ? 0.22f : 0.9f);
            DrawPixelText(settingLabels[row], pWidth / 2 - 238, textTop + 16, 3);
            if (row == 0)
            {
                std::string name = gPlayerDisplayName.empty() ? std::string("NOT SET") : gPlayerDisplayName;
                if (name.size() > 12)
                    name.resize(12);
                DrawPixelText(name.c_str(), pWidth / 2 + 34, textTop + 19, 2);
            }
            else if (row == 1)
            {
                const char* cameraOptions[] = {"NEAR", "STANDARD", "FAR"};
                for (int option = 0; option < 3; ++option)
                {
                    if (option == pCameraDistanceSetting)
                        glColor3f(1.0f, 0.78f, 0.12f);
                    else
                        glColor3f(0.62f, 0.72f, 0.76f);
                    DrawPixelText(cameraOptions[option], SettingsCameraOptionX(pWidth, option), textTop + 19, 2);
                }
            }
            else if (row == 2)
                DrawPixelText(gHudTextLarge ? "LARGE" : "NORMAL", pWidth / 2 + 80, textTop + 16, 3);
            else if (row == 3)
                DrawPixelText(pAudioEnabled ? "ON" : "OFF", pWidth / 2 + 120, textTop + 16, 3);
            else if (row == 4 || row == 5)
            {
                char volumeLabel[16];
                std::snprintf(volumeLabel, sizeof(volumeLabel), "%d", row == 4 ? pMenuVolume : pRaceVolume);
                DrawPixelText(volumeLabel, pWidth / 2 + 120, textTop + 16, 3);
            }
        }
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

void DrawCourseMap(const std::vector<RaceGate>& pWaypoints, const HovercraftState& pPlayerState,
                   const std::vector<HovercraftState>& pRivalStates, bool pShowRivals,
                   const RaceGate& pActiveGate, int pWidth)
{
    if (pWaypoints.empty())
        return;

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
    // One scale for both axes keeps the course shape true; the shorter axis is centred.
    const double mapScale = (mapSize - 16) / std::fmax(rangeX, rangeY);
    const double insetX = ((mapSize - 16) - rangeX * mapScale) * 0.5;
    const double insetY = ((mapSize - 16) - rangeY * mapScale) * 0.5;
    const auto mapX = [&](double x)
    {
        return mapLeft + 8 + static_cast<int>(insetX + (x - minimumX) * mapScale);
    };
    const auto mapY = [&](double y)
    {
        return mapTop + mapSize - 8 - static_cast<int>(insetY + (y - minimumY) * mapScale);
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
    glPointSize(4.0f);
    glBegin(GL_POINTS);
    glVertex2i(mapX(pWaypoints.front().mX), mapY(pWaypoints.front().mY));
    glEnd();
    glColor3f(0.18f, 0.96f, 0.4f);
    glPointSize(7.0f);
    glBegin(GL_POINTS);
    glVertex2i(mapX(pActiveGate.mX), mapY(pActiveGate.mY));
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

void DrawRouteCue(const HovercraftState& pPlayerState, const RaceGate& pActiveGate,
                  int pWidth, int pHeight)
{
    const double gateDeltaX = pActiveGate.mX - pPlayerState.mX;
    const double gateDeltaY = pActiveGate.mY - pPlayerState.mY;
    const int gateDistance = static_cast<int>(std::sqrt(gateDeltaX * gateDeltaX
                                                        + gateDeltaY * gateDeltaY));
    char gateLabel[24];
    std::snprintf(gateLabel, sizeof(gateLabel), "NEXT %d", gateDistance);
    glColor3f(0.2f, 0.9f, 1.0f);
    DrawPixelText(gateLabel, 300, 27 + (gHudTextLarge ? 27 : 18), gHudTextLarge ? 3 : 2);

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
    glVertex2d(pointerX - forwardX * 6.0 + sideX * 6.0,
               pointerY - forwardY * 6.0 + sideY * 6.0);
    glVertex2d(pointerX - forwardX * 6.0 - sideX * 6.0,
               pointerY - forwardY * 6.0 - sideY * 6.0);
    glEnd();
}

void DrawRaceCueBanners(bool pCheckpoint, bool pBoost, bool pImpact, int pWidth, int pHeight);

void DrawHud(const RaceProgress& pPlayerProgress, int pTargetLaps,
             const std::vector<RaceProgress>& pRivalProgresses,
             const HovercraftState& pPlayerState, const std::vector<HovercraftState>& pRivalStates,
             const RaceGate& pActiveGate, const std::vector<RaceGate>& pWaypoints,
             bool pShowRivals, bool pWrongWay, bool pCheckpointCue, bool pBoostCue,
             bool pImpactCue, int pWinner,
             int pPlayerPosition, int pCompetitorCount,
             const std::vector<int>& pCompetitorPositions, bool pChampionship,
             bool pChampionshipComplete, bool pChampionshipFinalEvent, int pStartLights,
             bool pShowSetupOverlay, bool pShowResultOverlay, const char* pChampionshipPoints,
             double pPlayerElapsedSeconds, const LapTiming& pLapTiming, CraftClass pPlayerCraftClass,
             const Missile& pMissile,
             bool pWeaponsAllowed, bool pPauseMenuOpen, int pPauseMenuSelection,
             bool pControllerConnected, bool pSteeringAssist, bool pBrakingAssist,
             int pCameraDistanceSetting, int pMenuVolume, int pRaceVolume,
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
    char speedLabel[24];
    std::snprintf(speedLabel, sizeof(speedLabel), "SPEED %d", speed);
    glColor3f(0.82f, 0.9f, 0.92f);
    const int hudScale = gHudTextLarge ? 3 : 2;
    const int hudStep = gHudTextLarge ? 27 : 18;
    DrawPixelText(speedLabel, 300, 27, hudScale);
    DrawRouteCue(pPlayerState, pActiveGate, pWidth, pHeight);
    if (SDL_GetTicks() < gGhostToastUntil && !gGhostToast.empty())
    {
        glColor3f(0.6f, 0.86f, 1.0f);
        DrawPixelText(gGhostToast.c_str(), 300, 27 + 2 * (gHudTextLarge ? 27 : 18) + 13, 2);
    }
    else if (gGhostGapShown)
    {
        char gapLabel[40];
        const int metres = static_cast<int>(std::fabs(gGhostGapMeters) + 0.5);
        std::snprintf(gapLabel, sizeof(gapLabel), "%d M %s GHOST", metres,
                      gGhostGapMeters >= 0.0 ? "AHEAD OF" : "BEHIND");
        if (gGhostGapMeters >= 0.0)
            glColor3f(0.3f, 1.0f, 0.5f);
        else
            glColor3f(1.0f, 0.6f, 0.25f);
        DrawPixelText(gapLabel, 300, 27 + 2 * (gHudTextLarge ? 27 : 18) + 13, 2);
    }

    const int resourceLeft = 24;
    const int resourceWidth = 122;
    const int resourceHeight = 9;
    // Sit below the lap-progress pips of the player and every rival.
    const int resourceTop = 76 + static_cast<int>(pRivalProgresses.size()) * 18 + 8;
    const int fuelWidth = static_cast<int>(resourceWidth * pPlayerState.mFuel);
    const int missileWidth = static_cast<int>(resourceWidth * pMissile.RechargeFraction());
    glColor3f(0.72f, 0.82f, 0.84f);
    DrawPixelText("FUEL", resourceLeft, resourceTop, 2);
    DrawPixelText(pWeaponsAllowed ? "MISSILE" : "WEAPONS OFF", resourceLeft, resourceTop + 22, 2);
    glColor3f(0.02f, 0.05f, 0.08f);
    glBegin(GL_QUADS);
    glVertex2i(resourceLeft + 92, resourceTop + 2);
    glVertex2i(resourceLeft + 92 + resourceWidth, resourceTop + 2);
    glVertex2i(resourceLeft + 92 + resourceWidth, resourceTop + 2 + resourceHeight);
    glVertex2i(resourceLeft + 92, resourceTop + 2 + resourceHeight);
    glVertex2i(resourceLeft + 92, resourceTop + 24);
    glVertex2i(resourceLeft + 92 + resourceWidth, resourceTop + 24);
    glVertex2i(resourceLeft + 92 + resourceWidth, resourceTop + 24 + resourceHeight);
    glVertex2i(resourceLeft + 92, resourceTop + 24 + resourceHeight);
    glEnd();
    glColor3f(0.18f, 0.78f, 0.54f);
    glBegin(GL_QUADS);
    glVertex2i(resourceLeft + 92, resourceTop + 2);
    glVertex2i(resourceLeft + 92 + fuelWidth, resourceTop + 2);
    glVertex2i(resourceLeft + 92 + fuelWidth, resourceTop + 2 + resourceHeight);
    glVertex2i(resourceLeft + 92, resourceTop + 2 + resourceHeight);
    glEnd();
    glColor3f(pMissile.Ready() ? 0.2f : 0.92f, pMissile.Ready() ? 0.82f : 0.52f,
              pMissile.Ready() ? 0.96f : 0.14f);
    glBegin(GL_QUADS);
    glVertex2i(resourceLeft + 92, resourceTop + 24);
    glVertex2i(resourceLeft + 92 + (pWeaponsAllowed ? missileWidth : 0), resourceTop + 24);
    glVertex2i(resourceLeft + 92 + (pWeaponsAllowed ? missileWidth : 0), resourceTop + 24 + resourceHeight);
    glVertex2i(resourceLeft + 92, resourceTop + 24 + resourceHeight);
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
    DrawPixelText(lapLabel, pWidth / 2 + 36, 27, hudScale);
    DrawPixelText(timeLabel, pWidth / 2 + 36, 27 + hudStep, hudScale);
    DrawPixelText(currentLapLabel, pWidth / 2 + 36, 27 + 2 * hudStep, hudScale);
    DrawPixelText(bestLapLabel, pWidth / 2 + 36, 27 + 3 * hudStep, hudScale);
    DrawPixelText(splitLabel, pWidth / 2 + 36, 27 + 4 * hudStep, hudScale);
    glColor3f(1.0f, 0.78f, 0.12f);
    DrawPixelText(positionLabel, pWidth / 2 + 36, 27 + 5 * hudStep, hudScale);

    const int pointerX = pWidth / 2;
    const int pointerY = pHeight - 86;
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

    if (pPlayerState.mInPit && pPlayerState.mSpeed < 6.0 && !pWrongWay)
    {
        // Stopped by a step the craft had not jumped: nothing puts it back by itself.
        std::string recoverKey = SDL_GetScancodeName(static_cast<SDL_Scancode>(gKeyBindings.Key(BindAction::Recover)));
        for (char& c : recoverKey)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        const std::string hint = "STUCK  PRESS " + recoverKey + " TO GO BACK";
        const int hintWidth = PixelTextWidth(hint.c_str(), 2) + 28;
        glColor3f(0.72f, 0.06f, 0.08f);
        glBegin(GL_QUADS);
        glVertex2i(pointerX - hintWidth / 2, pointerY - 54);
        glVertex2i(pointerX + hintWidth / 2, pointerY - 54);
        glVertex2i(pointerX + hintWidth / 2, pointerY - 32);
        glVertex2i(pointerX - hintWidth / 2, pointerY - 32);
        glEnd();
        glColor3f(1.0f, 0.82f, 0.2f);
        DrawPixelText(hint.c_str(), pointerX - hintWidth / 2 + 14, pointerY - 50, 2);
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

    DrawCourseMap(pWaypoints, pPlayerState, pRivalStates, pShowRivals, pActiveGate, pWidth);
    DrawRaceCueBanners(pCheckpointCue, pBoostCue, pImpactCue, pWidth, pHeight);

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
        DrawResultOverlay(pWinner, pPlayerPosition, pCompetitorCount, pCompetitorPositions,
                          pChampionship,
                          pChampionshipComplete, pChampionshipFinalEvent,
                          pChampionshipPoints, pPlayerElapsedSeconds, pLapTiming, pPlayerCraftClass,
                          pResultSelection,
                          pWidth, pHeight);
    if (pPauseMenuOpen)
        DrawPauseOverlay(pPauseMenuSelection, pControllerConnected, pSteeringAssist,
                         pBrakingAssist, pCameraDistanceSetting, pMenuVolume, pRaceVolume,
                         pWidth, pHeight);

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

void DrawRaceCueBanners(bool pCheckpoint, bool pBoost, bool pImpact, int pWidth, int pHeight)
{
    const char* labels[3];
    int labelCount = 0;
    if (pImpact)
        labels[labelCount++] = "IMPACT";
    if (pCheckpoint)
        labels[labelCount++] = "CHECKPOINT CLEARED";
    if (pBoost)
        labels[labelCount++] = "BOOST ACTIVE";
    for (int index = 0; index < labelCount; ++index)
    {
        const int bannerWidth = 260;
        const int bannerLeft = (pWidth - bannerWidth) / 2;
        const int bannerTop = pHeight / 2 - 150 + index * 38;
        glColor3f(labels[index][0] == 'I' ? 0.62f : 0.02f,
                  labels[index][0] == 'I' ? 0.08f : 0.28f,
                  labels[index][0] == 'I' ? 0.06f : 0.34f);
        glBegin(GL_QUADS);
        glVertex2i(bannerLeft, bannerTop);
        glVertex2i(bannerLeft + bannerWidth, bannerTop);
        glVertex2i(bannerLeft + bannerWidth, bannerTop + 30);
        glVertex2i(bannerLeft, bannerTop + 30);
        glEnd();
        glColor3f(1.0f, 0.82f, 0.2f);
        DrawPixelText(labels[index],
                      pWidth / 2 - PixelTextWidth(labels[index], 2) / 2,
                      bannerTop + 8, 2);
    }
}

void DrawPracticeGuide(const PracticeGuide& pGuide, bool pControllerConnected,
                       int pWidth, int pHeight)
{
    const char* instruction = "PRACTICE COMPLETE - KEEP DRIVING";
    switch (pGuide.Step())
    {
    case PracticeStep::Accelerate:
        instruction = pControllerConnected ? "HOLD RIGHT TRIGGER TO ACCELERATE"
                                           : "HOLD SHIFT OR W TO ACCELERATE";
        break;
    case PracticeStep::Steer:
        instruction = pControllerConnected ? "USE LEFT STICK TO STEER" : "USE A D OR ARROWS TO STEER";
        break;
    case PracticeStep::Checkpoint:
        instruction = "FOLLOW CYAN CUES THROUGH A CHECKPOINT";
        break;
    case PracticeStep::Boost:
        instruction = "DRIVE OVER A CYAN BOOST PAD";
        break;
    case PracticeStep::Jump:
        instruction = pControllerConnected ? "PRESS B TO JUMP" : "PRESS UP TO JUMP";
        break;
    case PracticeStep::Recover:
        instruction = pControllerConnected ? "LEAVE THE ROAD THEN PRESS Y TO RECOVER"
                                           : "LEAVE THE ROAD THEN PRESS X TO RECOVER";
        break;
    case PracticeStep::Fire:
        instruction = pControllerConnected ? "PRESS X TO FIRE A MISSILE" : "PRESS CTRL TO FIRE A MISSILE";
        break;
    case PracticeStep::Complete:
        break;
    }

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
    const int panelWidth = std::min(520, pWidth - 40);
    const int panelLeft = (pWidth - panelWidth) / 2;
    const int panelTop = 152;
    glColor3f(0.02f, 0.05f, 0.08f);
    glBegin(GL_QUADS);
    glVertex2i(panelLeft, panelTop);
    glVertex2i(panelLeft + panelWidth, panelTop);
    glVertex2i(panelLeft + panelWidth, panelTop + 58);
    glVertex2i(panelLeft, panelTop + 58);
    glEnd();
    glColor3f(pGuide.Step() == PracticeStep::Complete ? 0.18f : 0.2f,
              pGuide.Step() == PracticeStep::Complete ? 0.96f : 0.9f,
              pGuide.Step() == PracticeStep::Complete ? 0.4f : 1.0f);
    DrawPixelText(instruction,
                  pWidth / 2 - PixelTextWidth(instruction, 2) / 2, panelTop + 12, 2);
    char progress[24];
    std::snprintf(progress, sizeof(progress), "LESSON %d OF %d",
                  std::min(pGuide.CompletedStepCount() + 1, pGuide.TotalStepCount()),
                  pGuide.TotalStepCount());
    glColor3f(0.72f, 0.82f, 0.84f);
    DrawPixelText(progress, pWidth / 2 - PixelTextWidth(progress, 2) / 2, panelTop + 36, 2);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FOG);
    glEnable(GL_LIGHTING);
}

void DrawOnlineHud(const OnlineRacerView& pPlayer, int pRacerCount, int pTargetLaps,
                   const RaceGate& pActiveGate, const std::vector<RaceGate>& pWaypoints,
                   const std::vector<HovercraftState>& pRivalStates, bool pWrongWay,
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
    double displayedElapsedSeconds = pPlayer.mProgress.mElapsedSeconds;
    if (!pPlayer.mProgress.mFinished && gOnlineHudSnapshotTicks != 0)
    {
        const double sinceSnapshot = std::min(0.25,
            (SDL_GetTicks() - gOnlineHudSnapshotTicks) / 1000.0);
        displayedElapsedSeconds = std::max(displayedElapsedSeconds,
                                           gOnlineHudElapsedSeconds + sinceSnapshot);
    }
    const int elapsedSeconds = static_cast<int>(displayedElapsedSeconds);
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
    {
        // Backing panel for the top-left readouts, sized for the text size in use.
        const bool cup = gOnlineChampionshipEventCount > 0;
        const int panelWidth = gHudTextLarge ? (cup ? 340 : 212) : 170;
        const int panelHeight = gHudTextLarge ? (cup ? 4 : 3) * 28 + 12 : (cup ? 92 : 72);
        DrawOnlineHudPanel(18, 18, panelWidth, panelHeight);
    }
    DrawOnlineHudPanel(18, pHeight - 88, 170, 70);
    // Large HUD text widens the bottom-right panel for the live and best lap times.
    const int onlineScale = gHudTextLarge ? 3 : 2;
    const int onlineStep = gHudTextLarge ? 28 : 20;
    const int lapPanelWidth = gHudTextLarge ? 214 : 162;
    const int lapPanelHeight = gHudTextLarge ? 74 : 52;
    DrawOnlineHudPanel(pWidth - lapPanelWidth - 18, pHeight - lapPanelHeight - 18, lapPanelWidth,
                       lapPanelHeight);
    glColor3f(0.2f, 0.9f, 1.0f);
    DrawPixelText("ONLINE", 30, 30, onlineScale);
    glColor3f(0.82f, 0.9f, 0.92f);
    DrawPixelText(lapLabel, 30, 30 + onlineStep, onlineScale);
    DrawPixelText(timeLabel, 30, 30 + 2 * onlineStep, onlineScale);
    if (gOnlineChampionshipEventCount > 0)
    {
        char championshipLabel[32];
        std::snprintf(championshipLabel, sizeof(championshipLabel), "CUP %d OF %d  %d PTS",
                      gOnlineChampionshipEvent, gOnlineChampionshipEventCount,
                      gOnlineChampionshipPoints);
        DrawPixelText(championshipLabel, 30, 30 + 3 * onlineStep, onlineScale);
    }
    DrawPixelText("SPEED", 30, pHeight - 78, 2);
    DrawPixelText(speedLabel, 30, pHeight - 56, 4);
    DrawPixelText(currentLapLabel, pWidth - lapPanelWidth - 6, pHeight - lapPanelHeight - 6, onlineScale);
    DrawPixelText(bestLapLabel, pWidth - lapPanelWidth - 6,
                  pHeight - lapPanelHeight - 6 + (gHudTextLarge ? 28 : 22), onlineScale);
    DrawRouteCue(pPlayer.mState, pActiveGate, pWidth, pHeight);
    DrawCourseMap(pWaypoints, pPlayer.mState, pRivalStates, true, pActiveGate, pWidth);
    const Uint32 cueTicks = SDL_GetTicks();
    DrawRaceCueBanners(cueTicks < gOnlineCheckpointVisualUntil,
                       cueTicks < gOnlineBoostVisualUntil,
                       cueTicks < gOnlineImpactVisualUntil, pWidth, pHeight);
    if (gOnlineCountdownActive)
    {
        const int lensRadius = std::max(14, std::min(30, pWidth / 16));
        const int signalWidth = lensRadius * 8 + 28;
        const int signalHeight = lensRadius * 2 + 24;
        const int signalLeft = pWidth / 2 - signalWidth / 2;
        const int signalTop = std::max(12, pHeight / 2 - signalHeight - 52);
        const int lensY = signalTop + signalHeight / 2;
        const char* countdownText = "START IN 6";
        char countdownBuffer[20];
        std::snprintf(countdownBuffer, sizeof(countdownBuffer), "START IN %d",
                      std::max(0, gOnlineCountdownSeconds));
        countdownText = countdownBuffer;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.01f, 0.015f, 0.02f, 0.9f);
        glBegin(GL_QUADS);
        glVertex2i(signalLeft, signalTop);
        glVertex2i(signalLeft + signalWidth, signalTop);
        glVertex2i(signalLeft + signalWidth, signalTop + signalHeight);
        glVertex2i(signalLeft, signalTop + signalHeight);
        glEnd();
        glColor4f(0.52f, 0.62f, 0.65f, 0.9f);
        glBegin(GL_LINE_LOOP);
        glVertex2i(signalLeft, signalTop);
        glVertex2i(signalLeft + signalWidth, signalTop);
        glVertex2i(signalLeft + signalWidth, signalTop + signalHeight);
        glVertex2i(signalLeft, signalTop + signalHeight);
        glEnd();
        glDisable(GL_BLEND);

        for (int light = 0; light < 3; ++light)
        {
            const bool lit = light < gOnlineStartLights;
            const int lensX = signalLeft + lensRadius * 2 + 14 + light * lensRadius * 2;
            const float red = light == 0 ? 1.0f : (light == 1 ? 1.0f : 0.18f);
            const float green = light == 0 ? 0.12f : (light == 1 ? 0.64f : 1.0f);
            const float blue = light == 0 ? 0.1f : (light == 1 ? 0.08f : 0.15f);
            glColor3f(lit ? red : red * 0.18f,
                      lit ? green : green * 0.18f,
                      lit ? blue : blue * 0.18f);
            glBegin(GL_POLYGON);
            for (int segment = 0; segment < 24; ++segment)
            {
                const double angle = segment * 2.0 * kPi / 24.0;
                glVertex2i(lensX + static_cast<int>(std::cos(angle) * lensRadius),
                           lensY + static_cast<int>(std::sin(angle) * lensRadius));
            }
            glEnd();
            glColor3f(0.7f, 0.76f, 0.76f);
            glBegin(GL_LINE_LOOP);
            for (int segment = 0; segment < 24; ++segment)
            {
                const double angle = segment * 2.0 * kPi / 24.0;
                glVertex2i(lensX + static_cast<int>(std::cos(angle) * lensRadius),
                           lensY + static_cast<int>(std::sin(angle) * lensRadius));
            }
            glEnd();
        }
        glColor3f(1.0f, 0.78f, 0.12f);
        DrawPixelText(countdownText, pWidth / 2 - PixelTextWidth(countdownText, 4) / 2,
                      signalTop + signalHeight + 16, 4);
        char craftLabel[32];
        std::snprintf(craftLabel, sizeof(craftLabel), "CRAFT %s", CraftClassName(pPlayer.mCraftClass));
        glColor3f(0.82f, 0.9f, 0.92f);
        DrawPixelText(craftLabel, pWidth / 2 - PixelTextWidth(craftLabel, 2) / 2,
                      signalTop + signalHeight + 48, 2);
        glColor3f(0.2f, 0.9f, 1.0f);
        DrawPixelText("LEFT RIGHT CHANGE CRAFT",
                      pWidth / 2 - PixelTextWidth("LEFT RIGHT CHANGE CRAFT", 2) / 2,
                      signalTop + signalHeight + 68, 2);
    }
    if (gOnlineRaceFinished)
    {
        const int resultWidth = 360;
        const int resultHeight = 166;
        const int resultLeft = pWidth / 2 - resultWidth / 2;
        const int resultTop = pHeight / 2 - resultHeight / 2;
        char resultPlaceLabel[24];
        char resultTimeLabel[32];
        char resultLastLapLabel[32];
        char resultBestLapLabel[32];
        std::snprintf(resultPlaceLabel, sizeof(resultPlaceLabel), "PLACE %d OF %d",
                      pPlayer.mPosition, pRacerCount);
        std::snprintf(resultTimeLabel, sizeof(resultTimeLabel), "TIME %02d %02d",
                      elapsedSeconds / 60, elapsedSeconds % 60);
        std::snprintf(resultLastLapLabel, sizeof(resultLastLapLabel), "LAST %02d %02d",
                      static_cast<int>(pPlayer.mLapTiming.mLastSeconds) / 60,
                      static_cast<int>(pPlayer.mLapTiming.mLastSeconds) % 60);
        std::snprintf(resultBestLapLabel, sizeof(resultBestLapLabel), "BEST %02d %02d",
                      static_cast<int>(pPlayer.mLapTiming.mBestSeconds) / 60,
                      static_cast<int>(pPlayer.mLapTiming.mBestSeconds) % 60);
        DrawOnlineHudPanel(resultLeft, resultTop, resultWidth, resultHeight);
        glColor3f(1.0f, 0.78f, 0.12f);
        DrawPixelText("RACE COMPLETE", pWidth / 2 - PixelTextWidth("RACE COMPLETE", 3) / 2,
                      resultTop + 16, 3);
        glColor3f(0.82f, 0.9f, 0.92f);
        DrawPixelText(resultPlaceLabel, resultLeft + 28, resultTop + 56, 2);
        DrawPixelText(resultTimeLabel, resultLeft + 28, resultTop + 78, 2);
        DrawPixelText(resultLastLapLabel, resultLeft + 28, resultTop + 100, 2);
        DrawPixelText(resultBestLapLabel, resultLeft + 190, resultTop + 100, 2);
        glColor3f(0.2f, 0.9f, 1.0f);
        DrawPixelText("ENTER FOR LOBBY",
                      pWidth / 2 - PixelTextWidth("ENTER FOR LOBBY", 2) / 2,
                      resultTop + 134, 2);
    }
    glColor3f(1.0f, 0.78f, 0.12f);
    DrawPixelText(positionLabel, pWidth - 168, 170, 2);
    if (pWrongWay)
    {
        glColor3f(1.0f, 0.2f, 0.12f);
        DrawPixelText("WRONG WAY", pWidth / 2 - 54, 26, 3);
    }

    const int leaderboardCount = std::min(8, static_cast<int>(gOnlineRacers.size()));
    if (leaderboardCount > 0)
    {
        // Below the course map and the place readout, which share the top-right corner.
        const int leaderboardTop = 196;
        DrawOnlineHudPanel(pWidth - 216, leaderboardTop, 198, 22 + leaderboardCount * 17);
        glColor3f(0.2f, 0.9f, 1.0f);
        DrawPixelText("LEADERS", pWidth - 204, leaderboardTop + 7, 2);
        for (int place = 1; place <= leaderboardCount; ++place)
        {
            const OnlineRacerView* racer = nullptr;
            for (const OnlineRacerView& candidate : gOnlineRacers)
            {
                if (candidate.mPosition == place)
                {
                    racer = &candidate;
                    break;
                }
            }
            if (racer == nullptr)
                continue;
            std::string name = racer->mPlayerId >= 1000000
                ? RivalName(static_cast<int>(racer->mPlayerId - 1000000)) : "PILOT";
            for (const LobbyPlayerView& player : gLobbyPlayers)
            {
                if (player.mId == racer->mPlayerId)
                {
                    name = player.mDisplayName;
                    break;
                }
            }
            char entry[48];
            std::snprintf(entry, sizeof(entry), "%d %s", place, name.c_str());
            glColor3f(racer->mPlayerId == gLobbyPlayerId ? 1.0f : 0.82f,
                      racer->mPlayerId == gLobbyPlayerId ? 0.78f : 0.9f,
                      racer->mPlayerId == gLobbyPlayerId ? 0.12f : 0.92f);
            DrawPixelText(entry, pWidth - 204, leaderboardTop + 26 + (place - 1) * 17, 2);
        }
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FOG);
    glEnable(GL_LIGHTING);
}

void DrawOnlineChat(int pWidth, int pHeight)
{
    if (gOnlineChatMessages.empty() && !gOnlineChatInputFocused)
        return;
    const int chatWidth = std::min(400, pWidth - 48);
    const int chatHeight = gOnlineChatInputFocused ? 126 : 98;
    const int chatLeft = (pWidth - chatWidth) / 2;
    const int chatTop = pHeight - chatHeight - 18;
    DrawOnlineHudPanel(chatLeft, chatTop, chatWidth, chatHeight);
    const int firstMessage = std::max(0, static_cast<int>(gOnlineChatMessages.size()) - 4);
    glColor3f(0.82f, 0.9f, 0.92f);
    for (int messageIndex = firstMessage; messageIndex < static_cast<int>(gOnlineChatMessages.size()); ++messageIndex)
        DrawPixelText(gOnlineChatMessages[messageIndex].c_str(), chatLeft + 12,
                      chatTop + 12 + (messageIndex - firstMessage) * 18, 2);
    if (!gOnlineChatInputFocused)
        return;

    const int inputTop = chatTop + chatHeight - 30;
    const int maximumVisibleCharacters = std::max(1, (chatWidth - 32) / 12);
    const std::string visibleInput = gOnlineChatInput.size()
        > static_cast<std::size_t>(maximumVisibleCharacters)
        ? gOnlineChatInput.substr(gOnlineChatInput.size() - maximumVisibleCharacters) : gOnlineChatInput;
    glColor3f(0.2f, 0.9f, 1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2i(chatLeft + 8, inputTop - 4);
    glVertex2i(chatLeft + chatWidth - 8, inputTop - 4);
    glVertex2i(chatLeft + chatWidth - 8, inputTop + 20);
    glVertex2i(chatLeft + 8, inputTop + 20);
    glEnd();
    glColor3f(0.9f, 0.96f, 0.98f);
    DrawPixelText(visibleInput.c_str(), chatLeft + 14, inputTop + 2, 2);
    if ((SDL_GetTicks() / 500) % 2 == 0)
    {
        const int caretX = chatLeft + 14 + PixelTextWidth(visibleInput, 2);
        glBegin(GL_QUADS);
        glVertex2i(caretX, inputTop);
        glVertex2i(caretX + 2, inputTop);
        glVertex2i(caretX + 2, inputTop + 17);
        glVertex2i(caretX, inputTop + 17);
        glEnd();
    }
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
    // OPENHOVER_WINDOW=WIDTHxHEIGHT starts the game in a window of that size, for checking how the
    // screens lay out on small displays.
    int windowWidth = kWindowWidth;
    int windowHeight = kWindowHeight;
    if (const char* sizeOverride = std::getenv("OPENHOVER_WINDOW"))
    {
        int requestedWidth = 0;
        int requestedHeight = 0;
        if (std::sscanf(sizeOverride, "%dx%d", &requestedWidth, &requestedHeight) == 2
            && requestedWidth >= 640 && requestedWidth <= 7680 && requestedHeight >= 480
            && requestedHeight <= 4320)
        {
            windowWidth = requestedWidth;
            windowHeight = requestedHeight;
        }
    }
    SDL_Window* window = SDL_CreateWindow("OpenHover", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight,
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

    gLocalTracks = BuiltInTracks();
    if (char* trackPrefix = SDL_GetPrefPath("OpenHover", "OpenHover"))
    {
        // If the game crashes it leaves crash.log here; on the next start the report is kept as
        // last-crash.log so the player can send it with a bug report.
        const std::string crashLog = std::string(trackPrefix) + "crash.log";
        const std::string previousCrash = TakePreviousCrashReport(crashLog);
        if (!previousCrash.empty())
        {
            const std::string keptLog = std::string(trackPrefix) + "last-crash.log";
            if (FILE* kept = std::fopen(keptLog.c_str(), "wb"))
            {
                std::fwrite(previousCrash.data(), 1, previousCrash.size(), kept);
                std::fclose(kept);
            }
            std::fprintf(stderr, "OpenHover closed unexpectedly last time. Report saved to %s\n%s",
                         keptLog.c_str(), previousCrash.c_str());
            gStartupNotice = "OPENHOVER CLOSED UNEXPECTEDLY LAST TIME - REPORT SAVED AS LAST-CRASH.LOG";
        }
        InstallCrashReport(crashLog, OPENHOVER_VERSION);
        std::vector<std::string> trackMessages;
        const std::vector<TrackDefinition> customTracks = LoadCustomTracks(
            std::string(trackPrefix) + "tracks", gLocalTracks, trackMessages);
        gLocalTracks.insert(gLocalTracks.end(), customTracks.begin(), customTracks.end());
        // Tracks downloaded from race rooms in earlier sessions. Two people may publish tracks
        // with the same name, so only exact duplicates are dropped here.
        gDownloadedTracksDirectory = std::string(trackPrefix) + "tracks/downloaded";
        const std::vector<TrackDefinition> downloadedTracks = LoadCustomTracks(
            gDownloadedTracksDirectory, gLocalTracks, trackMessages, true);
        gLocalTracks.insert(gLocalTracks.end(), downloadedTracks.begin(), downloadedTracks.end());
        for (const std::string& message : trackMessages)
            std::fprintf(stderr, "OpenHover custom track %s\n", message.c_str());
        SDL_free(trackPrefix);
    }
    gLocalTrackHashes.clear();
    for (const TrackDefinition& track : gLocalTracks)
        gLocalTrackHashes.push_back(TrackHash(track));
    const std::vector<TrackDefinition>& builtInTracks = gLocalTracks;
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
    GroundProfile ground = selectedTrack.Ground();
    gDrawGround = &ground;
    RaceMode raceMode = RaceMode::SingleRace;
    RivalDifficulty rivalDifficulty = RivalDifficulty::Standard;
    int rivalCount = kRivalCount;
    int targetLaps = 3;
    Championship championship(static_cast<int>(BuiltInTracks().size()));
    Race race(checkpoints, finish, targetLaps);
    std::vector<Race> rivalRaces(kRivalCount, Race(checkpoints, finish, targetLaps));
    LapTimer lapTimer;
    RaceStart raceStart;
    std::vector<RaceGate> rivalRoute(courseWaypoints);
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
        const RaceGate& turn = courseWaypoints.empty() ? finish : courseWaypoints.front();
        double forwardX = turn.mX - finish.mX;
        double forwardY = turn.mY - finish.mY;
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
    FrontScreen frontScreen = FrontScreen::Welcome;
    int frontSelection = 0;
    int localSetupSelection = 0;
    int settingsSelection = 0;
    int cameraDistanceSetting = 1;
    bool weaponsAllowed = true;
    PracticeGuide practiceGuide;
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
    std::vector<Missile> rivalMissiles(kRivalCount);
    std::vector<StallDetector> rivalStalls(kRivalCount);
    std::vector<RouteTracker> rivalRoutes(kRivalCount);
    bool fireHeld = false;
    double impactSoundCooldown = 0.0;
    Uint32 checkpointVisualUntil = 0;
    Uint32 boostVisualUntil = 0;
    Uint32 impactVisualUntil = 0;
    InputRecording activeRecording;
    bool recoveredThisRun = false;
    RouteTracker playerRoute;
    RouteTracker ghostRoute;
    bool ghostSubmitted = false;
    InputRecording ghostRecording;
    std::size_t ghostFrame = 0;
    bool ghostActive = false;
    const auto resetRace = [&](bool pStartCountdown = true)
    {
        // The ghost is the best completed run for this track and craft class, or (if the player
        // chose it) their last completed run. Best is also loaded while the ghost is off, so
        // switching it on mid-race works.
        const int craftClassValue = static_cast<int>(playerCraftClass);
        const InputRecording* ghostSource = nullptr;
        double ghostSourceSeconds = 0.0;
        if (gGhostMode == 2 && gLastRun.mCraftClass == craftClassValue && gLastRun.mTrackId == selectedTrack.mId)
        {
            ghostSource = &gLastRun.mRecording;
            ghostSourceSeconds = gLastRun.mSeconds;
            gGhostSourceLabel = "LAST";
        }
        else
        {
            ghostSource = gGhostLibrary.Find(selectedTrack.mId, craftClassValue);
            ghostSourceSeconds = gGhostLibrary.BestSeconds(selectedTrack.mId, craftClassValue);
            gGhostSourceLabel = "BEST";
        }
        ghostActive = ghostSource != nullptr;
        gGhostSecondsBeforeRace = ghostSourceSeconds;
        if (ghostSource != nullptr)
        {
            ghostRecording = *ghostSource;
            replayGhost = Hovercraft(CraftClassTuning(playerCraftClass));
            replayGhost.Reset(spawn);
            ghostFrame = 0;
        }
        activeRecording.Clear();
        playerRoute.Reset();
        ghostRoute.Reset();
        gGhostGapShown = false;
        recoveredThisRun = false;
        ghostSubmitted = false;
        gNewGhostBest = false;
        missile.Reset();
        for (Missile& rivalMissile : rivalMissiles)
            rivalMissile.Reset();
        for (StallDetector& rivalStall : rivalStalls)
            rivalStall.Reset();
        for (RouteTracker& rivalRoute : rivalRoutes)
            rivalRoute.Reset();
        fireHeld = false;
        checkpointVisualUntil = 0;
        boostVisualUntil = 0;
        impactVisualUntil = 0;
        gRivalNameSlots = PickRivalNames(kRivalCount, static_cast<unsigned int>(
            SDL_GetPerformanceCounter() ^ (SDL_GetTicks() * 2654435761u)));
        // Each rival drives the craft class that goes with its name.
        for (int rivalIndex = 0; rivalIndex < kRivalCount; ++rivalIndex)
            rivals[rivalIndex].SetTuning(CraftClassTuning(LocalRivalCraftClass(rivalIndex)));
        hovercraft.Reset(spawn);
        gCameraRig.Reset(spawn.mHeading);
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
        gRaceWinnerName.clear();
        continueDriving = false;
        resultSelection = 0;
        practiceGuide.Reset(weaponsAllowed);
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
        ground = selectedTrack.Ground();
        race = Race(checkpoints, finish, targetLaps);
        rivalRaces.assign(kRivalCount, Race(checkpoints, finish, targetLaps));
        rivalRoute = courseWaypoints;
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
        const char* savedName = gPlayerDisplayName.empty() ? "-" : gPlayerDisplayName.c_str();
        std::fprintf(preferences, "%d %d %d %d %d %s %d %d %d %d %d\n", trackIndex, targetLaps,
                 weaponsAllowed ? 1 : 0, cameraDistanceSetting, audioFeedback.Enabled() ? 1 : 0,
                 savedName, steeringAssistEnabled ? 1 : 0,
                 brakingAssistEnabled ? 1 : 0, static_cast<int>(playerCraftClass),
                 static_cast<int>(audioFeedback.MenuVolume() * 100.0 + 0.5),
                 static_cast<int>(audioFeedback.RaceVolume() * 100.0 + 0.5));
        std::fprintf(preferences, "%d %d %d\n", static_cast<int>(gCameraMotion), gGhostMode,
                     gHudTextLarge ? 1 : 0);
        std::fclose(preferences);
    };
    char bindingsFile[512] = {};
    if (preferencesFile[0] != '\0')
    {
        std::snprintf(bindingsFile, sizeof(bindingsFile), "%s", preferencesFile);
        char* extension = std::strrchr(bindingsFile, '.');
        if (extension != nullptr)
            std::snprintf(extension, sizeof(bindingsFile) - (extension - bindingsFile), ".keys");
        if (FILE* saved = std::fopen(bindingsFile, "r"))
        {
            char text[128] = {};
            if (std::fgets(text, sizeof(text), saved) != nullptr)
                gKeyBindings.Parse(text);
            if (std::fgets(text, sizeof(text), saved) != nullptr)
                gPadBindings.Parse(text);
            std::fclose(saved);
        }
    }
    const int ghostVersionTag = kOpenHoverContentVersion * 100 + kGhostPhysicsVersion;
    char ghostsFile[512] = {};
    if (preferencesFile[0] != '\0')
    {
        std::snprintf(ghostsFile, sizeof(ghostsFile), "%s", preferencesFile);
        char* extension = std::strrchr(ghostsFile, '.');
        if (extension != nullptr)
            std::snprintf(extension, sizeof(ghostsFile) - (extension - ghostsFile), ".ghosts");
        if (FILE* saved = std::fopen(ghostsFile, "rb"))
        {
            std::string text;
            char chunk[4096];
            std::size_t read = 0;
            while ((read = std::fread(chunk, 1, sizeof(chunk), saved)) > 0 && text.size() < (32u << 20))
                text.append(chunk, read);
            std::fclose(saved);
            gGhostLibrary.Parse(text, ghostVersionTag);
        }
    }
    const auto saveGhosts = [&]()
    {
        if (ghostsFile[0] == '\0')
            return;
        if (FILE* saved = std::fopen(ghostsFile, "wb"))
        {
            const std::string text = gGhostLibrary.Serialize(ghostVersionTag);
            std::fwrite(text.data(), 1, text.size(), saved);
            std::fclose(saved);
        }
    };
    const auto saveBindings = [&]()
    {
        if (bindingsFile[0] == '\0')
            return;
        if (FILE* saved = std::fopen(bindingsFile, "w"))
        {
            std::fprintf(saved, "%s\n%s\n", gKeyBindings.Serialize().c_str(),
                         gPadBindings.Serialize().c_str());
            std::fclose(saved);
        }
    };
    if (preferencesFile[0] != '\0')
    {
        FILE* preferences = std::fopen(preferencesFile, "r");
        int savedTrack = 0;
        int savedLaps = 3;
        int savedWeapons = 1;
        int savedCameraDistance = 1;
        int savedAudio = 1;
        int savedSteeringAssist = 0;
        int savedBrakingAssist = 0;
        int savedCraftClass = 0;
        int savedMenuVolume = 100;
        int savedRaceVolume = 100;
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
            {
                gPlayerDisplayName = std::strcmp(savedDisplayName, "-") == 0 ? "" : savedDisplayName;
                const int optionalSettings = std::fscanf(preferences, "%d %d %d %d %d",
                    &savedSteeringAssist, &savedBrakingAssist, &savedCraftClass,
                    &savedMenuVolume, &savedRaceVolume);
                if (optionalSettings >= 2)
                {
                    steeringAssistEnabled = savedSteeringAssist != 0;
                    brakingAssistEnabled = savedBrakingAssist != 0;
                }
                if (optionalSettings >= 3 && savedCraftClass >= 0 && savedCraftClass <= 2)
                {
                    playerCraftClass = static_cast<CraftClass>(savedCraftClass);
                    hovercraft = Hovercraft(CraftClassTuning(playerCraftClass));
                    replayGhost = Hovercraft(CraftClassTuning(playerCraftClass));
                }
                if (optionalSettings >= 4)
                {
                    audioFeedback.SetMenuVolume(savedMenuVolume / 100.0);
                    audioFeedback.SetRaceVolume((optionalSettings >= 5 ? savedRaceVolume
                                                                       : savedMenuVolume) / 100.0);
                }
                int savedCameraMotion = 0;
                if (optionalSettings >= 5 && std::fscanf(preferences, "%d", &savedCameraMotion) == 1
                    && savedCameraMotion >= 0 && savedCameraMotion <= 2)
                    gCameraMotion = static_cast<CameraMotion>(savedCameraMotion);
                int savedGhostVisible = 1;
                if (std::fscanf(preferences, "%d", &savedGhostVisible) == 1)
                    gGhostMode = savedGhostVisible >= 0 && savedGhostVisible <= 2 ? savedGhostVisible : 1;
                int savedHudLarge = 0;
                if (std::fscanf(preferences, "%d", &savedHudLarge) == 1)
                    gHudTextLarge = savedHudLarge != 0;
            }
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
        {
            const int changes = pDirection > 0 ? 1 : 2;
            for (int change = 0; change < changes; ++change)
                playerCraftClass = NextCraftClass(playerCraftClass);
            hovercraft = Hovercraft(CraftClassTuning(playerCraftClass));
            replayGhost = Hovercraft(CraftClassTuning(playerCraftClass));
            resetRace(false);
        }
        else if (pOption == 7)
        {
            int assists = (steeringAssistEnabled ? 1 : 0) | (brakingAssistEnabled ? 2 : 0);
            assists = (assists + pDirection + 4) % 4;
            steeringAssistEnabled = (assists & 1) != 0;
            brakingAssistEnabled = (assists & 2) != 0;
        }
        else if (pOption == 8)
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
    const auto returnToSetup = [&]()
    {
        pauseMenuOpen = false;
        continueDriving = false;
        frontScreen = FrontScreen::LocalSetup;
    };
    const auto advanceResult = [&]()
    {
        if (raceMode == RaceMode::Championship)
        {
            if (championship.Complete())
            {
                championship.Reset();
                trackIndex = championship.CurrentEvent();
                loadTrack();
            }
            else if (championship.AdvanceEvent())
            {
                trackIndex = championship.CurrentEvent();
                loadTrack();
            }
            else if (!championship.Complete())
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
        gOnlineChatMessages.clear();
        gOnlineChatInput.clear();
        gOnlineChatInputFocused = false;
        gLobbyMuteList.Clear();
        gHostedRoomCode.clear();
        gLobbyReady = false;
        gLobbySelectedRoom = -1;
        gLobbyPlayerId = 0;
        gLobbyJoinedRoomId = 0;
        gLobbyJoinPendingRoomId = 0;
        gLobbyStatus = "DISCONNECTED";
        gHaveTrackHashSent.clear();
        gTrackRequestedHash.clear();
        gHostUploadStage = 0;
        gTrackDownload = TrackDownload();
        gLobbyServerVersion.clear();
        gLobbyServerProtocol = 0;
        gLobbyServerContent = 0;
        gOnlineRaceRoomId = 0;
        gOnlineRaceTick = 0;
        gOnlineTargetLaps = 0;
        gOnlineRacers.clear();
        gOnlineHudElapsedSeconds = 0.0;
        gOnlineHudSnapshotTicks = 0;
        gOnlineHasBoostSnapshot = false;
        gOnlineBoostActive = false;
        gOnlineSpinOutActive = false;
        gOnlineBoostCue = false;
        gOnlineImpactCue = false;
        gOnlineCheckpointVisualUntil = 0;
        gOnlineBoostVisualUntil = 0;
        gOnlineImpactVisualUntil = 0;
        gOnlineStartLights = 0;
        gOnlineCountdownActive = false;
        gOnlineCountdownSeconds = 0;
        gOnlineChampionshipEvent = 0;
        gOnlineChampionshipEventCount = 0;
        gOnlineChampionshipPoints = 0;
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
        // OPENHOVER_SERVER=host:port points the client at another server (a private or test one).
        std::string serverHost = "outiva.com";
        int serverPort = 9700;
        if (const char* override = std::getenv("OPENHOVER_SERVER"))
        {
            const std::string text = override;
            const std::size_t colon = text.rfind(':');
            if (colon != std::string::npos && colon > 0 && colon + 1 < text.size())
            {
                serverHost = text.substr(0, colon);
                serverPort = std::atoi(text.c_str() + colon + 1);
            }
            else if (!text.empty())
                serverHost = text;
        }
        gLobbyStatus = lobbyClient.Connect(serverHost, serverPort)
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
            gReconnectPolicy.Reset();
            connectLobby();
        }
    };
    const auto submitChat = [&](std::string& pInput)
    {
        const bool muteCommand = pInput.compare(0, 6, "/MUTE ") == 0;
        const bool unmuteCommand = pInput.compare(0, 8, "/UNMUTE ") == 0;
        const bool reportCommand = pInput.compare(0, 8, "/REPORT ") == 0;
        const bool joinCommand = pInput.compare(0, 6, "/JOIN ") == 0;
        if (pInput == "/READY")
        {
            if (gLobbyJoinedRoomId == 0)
                gLobbyStatus = "JOIN A ROOM FIRST";
            else
            {
                bool localPlayerIsHost = false;
                for (const LobbyRoomView& room : gLobbyRooms)
                    localPlayerIsHost = localPlayerIsHost
                        || (room.mId == gLobbyJoinedRoomId && room.mHostId == gLobbyPlayerId);
                if (localPlayerIsHost)
                    gLobbyStatus = "HOST IS ALWAYS READY";
                else
                {
                    gLobbyReady = !gLobbyReady;
                    if (lobbyClient.SendCommand(std::string("READY ") + (gLobbyReady ? "1" : "0")))
                        gLobbyStatus = gLobbyReady ? "READY TO RACE" : "NOT READY";
                }
            }
            pInput.clear();
            return;
        }
        if (joinCommand)
        {
            const std::string code = pInput.substr(6);
            if (code.size() == 6 && lobbyClient.SendCommand("JOINCODE " + code))
                gLobbyStatus = "JOINING PRIVATE ROOM";
            else
                gLobbyStatus = "USE /JOIN ROOMCODE";
            pInput.clear();
            return;
        }
        if (!muteCommand && !unmuteCommand && !reportCommand)
        {
            if (lobbyClient.SendCommand("CHAT " + pInput))
                pInput.clear();
            return;
        }

        const std::size_t nameStart = muteCommand ? 6 : 8;
        const std::size_t reasonStart = reportCommand ? pInput.find(' ', nameStart) : std::string::npos;
        const std::string displayName = pInput.substr(nameStart,
            reasonStart == std::string::npos ? std::string::npos : reasonStart - nameStart);
        const LobbyPlayerView* target = nullptr;
        for (const LobbyPlayerView& player : gLobbyPlayers)
        {
            if (player.mDisplayName == displayName)
            {
                target = &player;
                break;
            }
        }
        if (target == nullptr)
            gLobbyStatus = "PLAYER NOT FOUND";
        else if (target->mId == gLobbyPlayerId)
            gLobbyStatus = reportCommand ? "YOU CANNOT REPORT YOURSELF" : "YOU CANNOT MUTE YOURSELF";
        else if (reportCommand && (reasonStart == std::string::npos
                                  || !IsValidLobbyReportReason(pInput.substr(reasonStart + 1))))
            gLobbyStatus = "USE /REPORT NAME REASON";
        else if (reportCommand)
        {
            lobbyClient.SendCommand("REPORT " + std::to_string(target->mId) + "|"
                                    + pInput.substr(reasonStart + 1));
            gLobbyStatus = "SENDING REPORT";
        }
        else
        {
            if (muteCommand)
                gLobbyMuteList.Mute(static_cast<LobbyPlayerId>(target->mId));
            else
                gLobbyMuteList.Unmute(static_cast<LobbyPlayerId>(target->mId));
            gLobbyStatus = std::string(muteCommand ? "MUTED " : "UNMUTED ") + target->mDisplayName;
        }
        pInput.clear();
    };
    const auto activateLobbyPrimary = [&]()
    {
        if (gLobbySelectedRoom < 0 || gLobbySelectedRoom >= static_cast<int>(gLobbyRooms.size()))
            return;
        gLobbyChatInputFocused = false;
        const LobbyRoomView& room = gLobbyRooms[gLobbySelectedRoom];
        if (room.mHostId == gLobbyPlayerId)
            lobbyClient.SendCommand("START " + std::to_string(room.mId));
        else if (room.mId == gLobbyJoinedRoomId)
        {
            const bool nextReady = !gLobbyReady;
            if (nextReady && room.mCustomHash != "-" && gHaveTrackHashSent != room.mCustomHash)
            {
                gLobbyStatus = "TRACK STILL DOWNLOADING";
                return;
            }
            if (lobbyClient.SendCommand(std::string("READY ") + (nextReady ? "1" : "0")))
            {
                gLobbyReady = nextReady;
                gLobbyStatus = gLobbyReady ? "READY TO RACE" : "NOT READY";
            }
        }
        else if (lobbyClient.SendCommand("JOIN " + std::to_string(room.mId)))
        {
            gLobbyJoinedRoomId = room.mId;
            gLobbyJoinPendingRoomId = room.mId;
            gLobbyReady = false;
            gLobbyStatus = "JOINING RACE";
        }
    };
    const auto activateLobbySecondary = [&]()
    {
        const LobbyRoomView* room = gLobbySelectedRoom >= 0
            && gLobbySelectedRoom < static_cast<int>(gLobbyRooms.size())
            ? &gLobbyRooms[gLobbySelectedRoom] : nullptr;
        if (room != nullptr && room->mId == gLobbyJoinedRoomId
            && room->mHostId != gLobbyPlayerId)
        {
            if (lobbyClient.SendCommand("LEAVE"))
            {
                gLobbyJoinedRoomId = 0;
                gLobbyJoinPendingRoomId = 0;
                gLobbyReady = false;
                gLobbyStatus = "LEFT ROOM";
            }
        }
        else if (room == nullptr || room->mHostId != gLobbyPlayerId)
        {
            gLobbyChatInputFocused = false;
            gHostSetupSelection = 0;
            frontScreen = FrontScreen::HostRaceSetup;
        }
    };
    // Writes the editor's track to the tracks folder and adds it to the local race list. Saving again
    // after more edits replaces the track this editor saved before. Returns true on success and
    // leaves the reason in gEditor.mStatus otherwise.
    const auto saveEditorTrack = [&]() -> bool
    {
        if (!gEditor.mBuilt.mOk)
        {
            gEditor.mStatus = "FIX THE PROBLEM ABOVE FIRST";
            return false;
        }
        const TrackDefinition& track = gEditor.mBuilt.mTrack;
        int replaceIndex = -1;
        for (int index = 0; index < static_cast<int>(gLocalTracks.size()); ++index)
        {
            if (gLocalTracks[index].mId == track.mId || gLocalTracks[index].mName == track.mName)
            {
                if (index == gEditor.mSavedIndex && gLocalTracks[index].mId == track.mId)
                    replaceIndex = index;
                else
                {
                    gEditor.mStatus = "A TRACK WITH THAT NAME EXISTS - CHANGE THE NAME";
                    return false;
                }
            }
        }
        char* savePrefix = SDL_GetPrefPath("OpenHover", "OpenHover");
        if (savePrefix == nullptr)
        {
            gEditor.mStatus = "CANNOT FIND A PLACE TO SAVE";
            return false;
        }
        const std::string directory = std::string(savePrefix) + "tracks";
        SDL_free(savePrefix);
        const std::string path = directory + "/" + track.mId + ".ohtrack";
        const std::string text = SerializeTrack(track);
        FILE* file = EnsureDirectory(directory) ? std::fopen(path.c_str(), "wb") : nullptr;
        const bool written = file != nullptr && std::fwrite(text.data(), 1, text.size(), file) == text.size();
        if (file != nullptr)
            std::fclose(file);
        if (!written)
        {
            gEditor.mStatus = "COULD NOT SAVE THE FILE";
            return false;
        }
        if (replaceIndex >= 0)
        {
            gLocalTracks[replaceIndex] = track;
            gLocalTrackHashes[replaceIndex] = TrackHash(track);
            gEditor.mSavedIndex = replaceIndex;
        }
        else
        {
            gLocalTracks.push_back(track);
            gLocalTrackHashes.push_back(TrackHash(track));
            gEditor.mSavedIndex = static_cast<int>(gLocalTracks.size()) - 1;
        }
        gEditor.mStatus = "SAVED - RACE IT FROM PLAY LOCAL GAME";
        return true;
    };
    // Starts hosting a room. A built-in track is just named in the request; a custom track is
    // uploaded first and the room is created once the server has accepted it.
    const auto requestCreateRoom = [&](const std::string& pRoomName)
    {
        const bool customTrack = gHostTrackIndex >= 3;
        const std::string command = "CREATE " + (customTrack ? std::string("CUSTOM") : pRoomName) + "|"
            + std::to_string(gHostRaceMode) + "|"
            + std::to_string(customTrack ? kCustomTrackIndex : gHostTrackIndex) + "|"
            + std::to_string(gHostLapCount) + "|" + std::to_string(gHostPlayerCapacity) + "|"
            + std::to_string(gHostRivalCount) + "|" + (gHostWeaponsAllowed ? "1" : "0") + "|"
            + (gHostPrivateRoom ? "1" : "0");
        if (!customTrack)
        {
            gHostCreatePending = lobbyClient.SendCommand(command);
            gLobbyStatus = gHostCreatePending ? "CREATING ROOM" : "SERVER UNAVAILABLE";
            return;
        }
        if (!TrackEligibleForOnline(gHostTrackIndex))
        {
            gLobbyStatus = "THIS TRACK CANNOT BE HOSTED ONLINE";
            return;
        }
        const std::string text = SerializeTrack(gLocalTracks[gHostTrackIndex]);
        gHostUploadHex = EncodeHex(text);
        gHostPendingCreate = command;
        if (lobbyClient.SendCommand("TRACKUP " + std::to_string(text.size()) + "|"
                                    + gLocalTrackHashes[gHostTrackIndex]))
        {
            gHostUploadStage = 1;
            gLobbyStatus = "UPLOADING TRACK";
        }
        else
            gLobbyStatus = "SERVER UNAVAILABLE";
    };
    // Makes sure this player has, and has told the server they have, the custom track of the
    // room they joined: use a matching copy they already own, or download it from the room.
    const auto syncRoomTrack = [&]()
    {
        if (gLobbyJoinedRoomId == 0)
        {
            gTrackRequestedHash.clear();
            return;
        }
        for (const LobbyRoomView& room : gLobbyRooms)
        {
            if (room.mId != gLobbyJoinedRoomId || room.mCustomHash == "-" || room.mCustomHash.empty())
                continue;
            if (gHaveTrackHashSent == room.mCustomHash)
                return;
            if (LocalIndexForRoomTrack(kCustomTrackIndex, room.mCustomHash) >= 0)
            {
                if (lobbyClient.SendCommand("HAVE " + room.mCustomHash))
                    gHaveTrackHashSent = room.mCustomHash;
            }
            else if (gTrackRequestedHash != room.mCustomHash && !gTrackDownload.mActive
                     && lobbyClient.SendCommand("TRACKGET " + std::to_string(room.mId)))
            {
                gTrackRequestedHash = room.mCustomHash;
                gLobbyStatus = "DOWNLOADING TRACK";
            }
            return;
        }
    };
    const auto updateLobby = [&]()
    {
        if (frontScreen != FrontScreen::Multiplayer && frontScreen != FrontScreen::HostRaceSetup
            && frontScreen != FrontScreen::OnlineRace)
            return;
        if (gReconnectPolicy.TakeDueAttempt(SDL_GetTicks() / 1000.0))
            connectLobby();
        else if (gReconnectPolicy.Pending())
        {
            gLobbyStatus = "CONNECTION LOST - RECONNECTING IN "
                + std::to_string(static_cast<int>(
                      gReconnectPolicy.SecondsUntilNext(SDL_GetTicks() / 1000.0) + 0.99))
                + "S (" + std::to_string(gReconnectPolicy.Attempt()) + "/"
                + std::to_string(ReconnectPolicy::kMaxAttempts) + ")";
        }
        lobbyClient.Tick();
        syncRoomTrack();
        if (lobbyClient.State() == TcpLobbyClientState::Connecting)
            gLobbyStatus = "CONNECTING";
        else if (lobbyClient.State() == TcpLobbyClientState::Failed)
        {
            const bool raceConnectionLost = frontScreen == FrontScreen::OnlineRace;
            lobbyClient.Disconnect();
            lobbyHelloSent = false;
            gLobbyPlayers.clear();
            gLobbyRooms.clear();
            gLobbySelectedRoom = -1;
            gLobbyPlayerId = 0;
            gLobbyJoinedRoomId = 0;
            gLobbyJoinPendingRoomId = 0;
            gHaveTrackHashSent.clear();
            gTrackRequestedHash.clear();
            gHostUploadStage = 0;
            gTrackDownload = TrackDownload();
            gLobbyServerVersion.clear();
            gLobbyServerProtocol = 0;
            gLobbyServerContent = 0;
            gOnlineRaceRoomId = 0;
            gOnlineRaceTick = 0;
            gOnlineTargetLaps = 0;
            gOnlineRacers.clear();
            gOnlineMissiles.clear();
            gOnlineMineTriggered.clear();
            gOnlineRaceFinished = false;
            gOnlineChatInput.clear();
            gOnlineChatInputFocused = false;
            gLobbyChatInputFocused = false;
            if (raceConnectionLost)
                frontScreen = FrontScreen::Multiplayer;
            if (gReconnectPolicy.OnLost(SDL_GetTicks() / 1000.0))
            {
                if (raceConnectionLost)
                    frontScreen = FrontScreen::Multiplayer;
                gLobbyStatus = "CONNECTION LOST - RECONNECTING";
            }
            else
                gLobbyStatus = raceConnectionLost ? "CONNECTION LOST - BACK THEN RETRY"
                                                  : "SERVER UNAVAILABLE - BACK THEN RETRY";
        }
        else if (lobbyClient.State() == TcpLobbyClientState::Connected && !lobbyHelloSent)
        {
            lobbyHelloSent = lobbyClient.SendCommand("HELLO "
                + std::to_string(kOpenHoverProtocolVersion) + "|"
                + std::to_string(kOpenHoverContentVersion) + "|" + lobbyDisplayName);
            gLobbyStatus = lobbyHelloSent ? "CONNECTING" : "SERVER UNAVAILABLE";
        }
        for (const std::string& message : lobbyClient.TakeMessages())
        {
            if (message.compare(0, 10, "OPENHOVER ") == 0)
            {
                std::istringstream greeting(message.substr(10));
                int protocol = 0;
                int content = 0;
                std::string version;
                if (greeting >> protocol >> content >> version)
                {
                    gLobbyServerProtocol = protocol;
                    gLobbyServerContent = content;
                    gLobbyServerVersion = version;
                }
            }
            else if (message.compare(0, 5, "LOBBY") == 0)
            {
                ParseLobbySnapshot(message);
                gReconnectPolicy.OnConnected();
                gLobbyStatus = "CONNECTED";
            }
            else if (message.compare(0, 8, "WELCOME ") == 0)
            {
                gLobbyPlayerId = std::atoi(message.substr(8).c_str());
            }
            else if (message.compare(0, 11, "TRACKUP OK ") == 0 && gHostUploadStage == 2)
            {
                // The server accepted the track: now create the room that carries it.
                gHostUploadStage = 0;
                gHostCreatePending = lobbyClient.SendCommand(gHostPendingCreate);
                gLobbyStatus = gHostCreatePending ? "CREATING ROOM" : "SERVER UNAVAILABLE";
            }
            else if (message == "TRACKUP READY" && gHostUploadStage == 1)
            {
                for (std::size_t offset = 0; offset < gHostUploadHex.size(); offset += kTrackChunkHexCharacters)
                    lobbyClient.SendCommand("TRACKDATA " + gHostUploadHex.substr(offset, kTrackChunkHexCharacters));
                gHostUploadStage = 2;
            }
            else if (message.compare(0, 8, "TRACKDL ") == 0)
            {
                const std::vector<std::string> fields = SplitLobbyField(message.substr(8), '|');
                gTrackDownload = TrackDownload();
                if (fields.size() == 3 && std::atoi(fields[1].c_str()) > 0
                    && std::atoi(fields[1].c_str()) <= kMaximumTrackUploadBytes && fields[2].size() == 64)
                {
                    gTrackDownload.mActive = true;
                    gTrackDownload.mRoomId = std::atoi(fields[0].c_str());
                    gTrackDownload.mBytes = static_cast<std::size_t>(std::atoi(fields[1].c_str()));
                    gTrackDownload.mHash = fields[2];
                }
            }
            else if (message.compare(0, 11, "TRACKCHUNK ") == 0 && gTrackDownload.mActive)
            {
                const std::size_t bar = message.find('|', 11);
                const std::string hex = bar == std::string::npos ? std::string() : message.substr(bar + 1);
                if (gTrackDownload.mHex.size() + hex.size() <= gTrackDownload.mBytes * 2)
                    gTrackDownload.mHex += hex;
                else
                    gTrackDownload = TrackDownload();
            }
            else if (message.compare(0, 9, "TRACKEND ") == 0 && gTrackDownload.mActive)
            {
                const TrackDownload download = gTrackDownload;
                gTrackDownload = TrackDownload();
                std::string text;
                std::string problem;
                if (download.mHex.size() != download.mBytes * 2 || !DecodeHex(download.mHex, text))
                    problem = "incomplete download";
                TrackDefinition track;
                if (problem.empty())
                    problem = ParseTrack(text, track);
                if (problem.empty())
                    problem = track.Validate();
                if (problem.empty())
                    problem = CheckOnlineTrackLimits(track);
                if (problem.empty() && (SerializeTrack(track) != text || TrackHash(track) != download.mHash))
                    problem = "the file does not match the room's hash";
                bool wantedByRoom = false;
                for (const LobbyRoomView& room : gLobbyRooms)
                    wantedByRoom = wantedByRoom || (room.mId == download.mRoomId && room.mCustomHash == download.mHash);
                if (problem.empty() && !wantedByRoom)
                    problem = "the room no longer uses this track";
                if (!problem.empty())
                    gLobbyStatus = "TRACK DOWNLOAD FAILED: " + problem;
                else
                {
                    if (LocalIndexForRoomTrack(kCustomTrackIndex, download.mHash) < 0)
                    {
                        gLocalTracks.push_back(track);
                        gLocalTrackHashes.push_back(download.mHash);
                    }
                    const bool saved = SaveDownloadedTrack(gDownloadedTracksDirectory, download.mHash, text);
                    gHaveTrackHashSent = download.mHash;
                    lobbyClient.SendCommand("HAVE " + download.mHash);
                    gLobbyStatus = saved ? "TRACK DOWNLOADED" : "TRACK READY - NOT SAVED";
                }
            }
            else if (message.compare(0, 10, "REPORTACK ") == 0)
                gLobbyStatus = "REPORT RECEIVED - THANK YOU";
            else if (message.compare(0, 7, "JOINED ") == 0)
            {
                gLobbyJoinedRoomId = std::atoi(message.substr(7).c_str());
                gLobbyReady = false;
                gLobbyStatus = "JOINED PRIVATE ROOM";
            }
            else if (message.compare(0, 5, "CHAT ") == 0)
            {
                const std::size_t textStart = message.find(' ', 5);
                const int senderId = std::atoi(message.substr(5, textStart - 5).c_str());
                const std::string text = textStart == std::string::npos ? "" : message.substr(textStart + 1);
                if (senderId != 0 && gLobbyMuteList.IsMuted(static_cast<LobbyPlayerId>(senderId)))
                    continue;
                std::string senderName = senderId == 0 ? "LOBBY" : "PILOT";
                for (const LobbyPlayerView& player : gLobbyPlayers)
                {
                    if (player.mId == senderId)
                    {
                        senderName = player.mDisplayName;
                        break;
                    }
                }
                std::vector<std::string>& chatMessages = frontScreen == FrontScreen::OnlineRace
                    ? gOnlineChatMessages : gLobbyChatMessages;
                chatMessages.push_back(senderName + " " + text);
                if (chatMessages.size() > 32)
                    chatMessages.erase(chatMessages.begin());
            }
            else if (message.compare(0, 5, "ROOM ") == 0 && gHostCreatePending)
            {
                gHostCreatePending = false;
                gLobbyJoinedRoomId = std::atoi(message.substr(5).c_str());
                const std::size_t codeSeparator = message.find('|', 5);
                gHostedRoomCode = codeSeparator == std::string::npos
                    ? "" : message.substr(codeSeparator + 1);
                gLobbyStatus = gHostedRoomCode.empty() ? "ROOM CREATED"
                    : "PRIVATE ROOM CREATED";
                frontScreen = FrontScreen::Multiplayer;
            }
            else if (message.compare(0, 5, "RACE ") == 0 && ParseRaceSnapshot(message))
            {
                if (gOnlineBoostCue)
                {
                    audioFeedback.PlayBoost();
                    gOnlineBoostVisualUntil = SDL_GetTicks() + 650;
                    gOnlineBoostCue = false;
                }
                if (gOnlineImpactCue)
                {
                    audioFeedback.PlayImpact();
                    gOnlineImpactVisualUntil = SDL_GetTicks() + 650;
                    gOnlineImpactCue = false;
                }
                if (frontScreen != FrontScreen::OnlineRace)
                {
                    gOnlineHudElapsedSeconds = 0.0;
                    gOnlineHudSnapshotTicks = 0;
                    gOnlineLastCheckpoint = -1;
                    gOnlineLastCompletedLaps = -1;
                    gOnlineCheckpointCue = false;
                    gOnlineHasBoostSnapshot = false;
                    gOnlineBoostActive = false;
                    gOnlineSpinOutActive = false;
                    gOnlineBoostCue = false;
                    gOnlineImpactCue = false;
                    gOnlineRaceFinished = false;
                    for (const LobbyRoomView& room : gLobbyRooms)
                    {
                        const int localTrack = LocalIndexForRoomTrack(room.mTrackIndex, room.mCustomHash);
                        if (room.mId == gOnlineRaceRoomId && localTrack >= 0)
                        {
                            trackIndex = localTrack;
                            loadTrack(false);
                            break;
                        }
                    }
                    gOnlineChatMessages.clear();
                    gOnlineChatInput.clear();
                    gOnlineChatInputFocused = true;
                    SDL_StartTextInput();
                    frontScreen = FrontScreen::OnlineRace;
                    gLobbyStatus = "RACING";
                }
            }
            else if (message.compare(0, 8, "RACEHUD ") == 0)
            {
                if (ParseRaceHudSnapshot(message) && gOnlineCheckpointCue)
                {
                    audioFeedback.PlayCheckpoint();
                    gOnlineCheckpointVisualUntil = SDL_GetTicks() + 850;
                    gOnlineCheckpointCue = false;
                }
            }
            else if (message.compare(0, 10, "RACEEVENT ") == 0)
            {
                const std::vector<std::string> fields = SplitLobbyField(message.substr(10), '|');
                const int eventRoomId = fields.empty() ? 0 : std::atoi(fields[0].c_str());
                if (fields.size() >= 4 && eventRoomId > 0
                    && (gOnlineRaceRoomId == 0 || eventRoomId == gOnlineRaceRoomId))
                {
                    const int eventTrack = std::atoi(fields[1].c_str());
                    gOnlineRaceRoomId = eventRoomId;
                    gOnlineChampionshipEvent = std::atoi(fields[2].c_str());
                    gOnlineChampionshipEventCount = std::atoi(fields[3].c_str());
                    gOnlineChampionshipPoints = 0;
                    for (std::size_t fieldIndex = 4; fieldIndex < fields.size(); ++fieldIndex)
                    {
                        const std::vector<std::string> points = SplitLobbyField(fields[fieldIndex], ',');
                        if (points.size() == 3 && points[0] == "P"
                            && std::atoi(points[1].c_str()) == gLobbyPlayerId)
                        {
                            gOnlineChampionshipPoints = std::atoi(points[2].c_str());
                            break;
                        }
                    }
                    if (eventTrack >= 0 && eventTrack < 3)
                    {
                        trackIndex = eventTrack;
                        loadTrack(false);
                        gOnlineRacers.clear();
                        gOnlineMissiles.clear();
                        gOnlineMineTriggered.clear();
                        gOnlineHudElapsedSeconds = 0.0;
                        gOnlineHudSnapshotTicks = 0;
                        gOnlineLastCheckpoint = -1;
                        gOnlineLastCompletedLaps = -1;
                        gOnlineCheckpointCue = false;
                        gOnlineHasBoostSnapshot = false;
                        gOnlineBoostActive = false;
                        gOnlineSpinOutActive = false;
                        gOnlineBoostCue = false;
                        gOnlineImpactCue = false;
                    }
                }
            }
            else if (message.compare(0, 10, "RACEABORT ") == 0)
            {
                const std::size_t reasonSeparator = message.find('|', 10);
                const int roomId = std::atoi(message.substr(10, reasonSeparator - 10).c_str());
                if (roomId == gOnlineRaceRoomId)
                {
                    const std::string reason = reasonSeparator == std::string::npos
                        ? "CONNECTION LOST" : message.substr(reasonSeparator + 1);
                    gOnlineRaceRoomId = 0;
                    gOnlineRaceTick = 0;
                    gOnlineTargetLaps = 0;
                    gOnlineRacers.clear();
                    gOnlineMissiles.clear();
                    gOnlineMineTriggered.clear();
                    gOnlineRaceFinished = false;
                    gOnlineChatInput.clear();
                    gOnlineChatInputFocused = false;
                    gLobbyChatInputFocused = true;
                    SDL_StartTextInput();
                    frontScreen = FrontScreen::Multiplayer;
                    gLobbyStatus = "RACE ENDED - " + reason;
                }
            }
            else if (message.compare(0, 11, "RACEFINISH ") == 0
                     && std::atoi(message.substr(11).c_str()) == gOnlineRaceRoomId)
            {
                gOnlineRaceFinished = true;
                gLobbyStatus = "RACE COMPLETE";
            }
            else if (message.compare(0, 6, "ERROR ") == 0)
            {
                gHostCreatePending = false;
                gHostUploadStage = 0;
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
                else if (error.find("INCOMPATIBLE PROTOCOL") == 0)
                    gLobbyStatus = "UPDATE GAME - PROTOCOL MISMATCH";
                else if (error.find("INCOMPATIBLE CONTENT") == 0)
                    gLobbyStatus = "UPDATE GAME - TRACK CONTENT MISMATCH";
                else if (error.find("INCOMPATIBLE CLIENT") == 0)
                    gLobbyStatus = "UPDATE GAME TO JOIN";
                else if (error == "invalid name")
                    gLobbyStatus = "NAME USES INVALID CHARACTERS";
                else if (error == "chat rate limit")
                    gLobbyStatus = "CHAT SLOW MODE - TRY AGAIN";
                else if (error == "report rate limit")
                    gLobbyStatus = "REPORT LIMIT REACHED - TRY LATER";
                else if (error == "invalid report")
                    gLobbyStatus = "REPORT COULD NOT BE SENT";
                else if (error == "server full")
                    gLobbyStatus = "SERVER FULL - TRY LATER";
                else if (error == "players not ready")
                    gLobbyStatus = "WAITING FOR ALL PLAYERS TO READY";
                else if (error.compare(0, 15, "TRACK REJECTED ") == 0)
                {
                    std::string reason = error.substr(15);
                    for (char& c : reason)
                        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    gLobbyStatus = "TRACK REJECTED: " + reason;
                }
                else if (error == "TRACK NOT VERIFIED YET")
                    gLobbyStatus = "TRACK STILL DOWNLOADING";
                else if (error == "players still need the track")
                    gLobbyStatus = "WAITING FOR PLAYERS TO GET THE TRACK";
                else if (error == "CHAMPIONSHIP NEEDS BUILT-IN TRACKS")
                    gLobbyStatus = "CHAMPIONSHIP NEEDS A BUILT-IN TRACK";
                else if (error.find("track downloads") != std::string::npos)
                    gLobbyStatus = "TOO MANY TRACK DOWNLOADS - WAIT A MINUTE";
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
            if (event.type == SDL_CONTROLLERBUTTONDOWN && !gRemapCapturing)
            {
                const bool raceScreen = frontScreen == FrontScreen::RaceSetup;
                const bool resultsOpen = raceScreen && winner != 0 && !continueDriving;
                PadMenuContext padContext = PadMenuContext::Menu;
                if (frontScreen == FrontScreen::OnlineRace || (raceScreen && !pauseMenuOpen && !resultsOpen))
                    padContext = PadMenuContext::Driving;
                else if (frontScreen == FrontScreen::Multiplayer)
                    padContext = PadMenuContext::LobbyList;
                const int padKey = PadMenuKey(event.cbutton.button, padContext);
                if (padKey != 0)
                {
                    event.type = SDL_KEYDOWN;
                    event.key.state = SDL_PRESSED;
                    event.key.repeat = 0;
                    event.key.keysym.sym = padKey;
                    event.key.keysym.scancode = SDL_GetScancodeFromKey(padKey);
                    event.key.keysym.mod = KMOD_NONE;
                }
            }
            if (frontScreen == FrontScreen::TrackEditor)
            {
                int windowWidth = 0;
                int windowHeight = 0;
                int drawableWidth = 0;
                int drawableHeight = 0;
                SDL_GetWindowSize(window, &windowWidth, &windowHeight);
                SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);
                const EditorLayout layout = ComputeEditorLayout(drawableWidth, drawableHeight);
                const auto toDrawableX = [&](int pX) { return pX * drawableWidth / std::max(1, windowWidth); };
                const auto toDrawableY = [&](int pY) { return pY * drawableHeight / std::max(1, windowHeight); };
                const auto nearestPoint = [&](int pMouseX, int pMouseY)
                {
                    int best = -1;
                    double bestDistance = 12.0;
                    for (std::size_t index = 0; index < gEditor.mPoints.size(); ++index)
                    {
                        const double distance = std::hypot(
                            EditorScreenX(layout, gEditor.mPoints[index].mX) - pMouseX,
                            EditorScreenY(layout, gEditor.mPoints[index].mY) - pMouseY);
                        if (distance < bestDistance)
                        {
                            bestDistance = distance;
                            best = static_cast<int>(index);
                        }
                    }
                    return best;
                };
                const auto inCanvas = [&](int pMouseX, int pMouseY)
                {
                    return IsPointInRect(pMouseX, pMouseY, layout.mCanvasLeft, layout.mCanvasTop,
                                         layout.mCanvasSize, layout.mCanvasSize);
                };
                if (event.type == SDL_TEXTINPUT)
                {
                    if (gEditor.mNaming)
                    {
                        for (const char* c = event.text.text; *c != '\0'; ++c)
                        {
                            const bool allowed = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z')
                                || (*c >= '0' && *c <= '9') || *c == ' ' || *c == '.' || *c == '_'
                                || *c == '-';
                            if (allowed && gEditor.mName.size() < 24)
                                gEditor.mName += *c;
                        }
                        RefreshEditorBuild();
                    }
                    continue;
                }
                if (event.type == SDL_KEYDOWN)
                {
                    const SDL_Keycode key = event.key.keysym.sym;
                    if (gEditor.mNaming)
                    {
                        if (key == SDLK_BACKSPACE && !gEditor.mName.empty())
                            gEditor.mName.pop_back();
                        else if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_ESCAPE)
                        {
                            gEditor.mNaming = false;
                            SDL_StopTextInput();
                        }
                        RefreshEditorBuild();
                    }
                    else if (key == SDLK_ESCAPE)
                        frontScreen = FrontScreen::Welcome;
                    else if (key == SDLK_BACKSPACE && !gEditor.mPoints.empty())
                    {
                        gEditor.mPoints.pop_back();
                        gEditor.mStatus.clear();
                        RefreshEditorBuild();
                    }
                    continue;
                }
                if (event.type == SDL_MOUSEMOTION && gEditor.mDragIndex >= 0
                    && gEditor.mDragIndex < static_cast<int>(gEditor.mPoints.size()))
                {
                    const int mouseX = toDrawableX(event.motion.x);
                    const int mouseY = toDrawableY(event.motion.y);
                    gEditor.mPoints[gEditor.mDragIndex].mX = EditorSnap(EditorWorldX(layout, mouseX));
                    gEditor.mPoints[gEditor.mDragIndex].mY = EditorSnap(EditorWorldY(layout, mouseY));
                    RefreshEditorBuild();
                    continue;
                }
                if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT)
                {
                    gEditor.mDragIndex = -1;
                    continue;
                }
                if (event.type == SDL_MOUSEBUTTONDOWN)
                {
                    const int mouseX = toDrawableX(event.button.x);
                    const int mouseY = toDrawableY(event.button.y);
                    if (event.button.button == SDL_BUTTON_RIGHT)
                    {
                        const int hit = inCanvas(mouseX, mouseY) ? nearestPoint(mouseX, mouseY) : -1;
                        if (hit >= 0)
                        {
                            gEditor.mPoints.erase(gEditor.mPoints.begin() + hit);
                            gEditor.mStatus.clear();
                            RefreshEditorBuild();
                        }
                        continue;
                    }
                    if (event.button.button != SDL_BUTTON_LEFT)
                        continue;
                    int clicked = -1;
                    for (int button = 0; button < kEditorButtonCount; ++button)
                    {
                        int left = 0;
                        int top = 0;
                        int width = 0;
                        int height = 0;
                        EditorButtonRect(layout, button, left, top, width, height);
                        if (IsPointInRect(mouseX, mouseY, left, top, width, height))
                            clicked = button;
                    }
                    if (clicked == kEditorName)
                    {
                        gEditor.mNaming = !gEditor.mNaming;
                        if (gEditor.mNaming)
                            SDL_StartTextInput();
                        else
                            SDL_StopTextInput();
                    }
                    else if (clicked == kEditorNarrower || clicked == kEditorWider)
                    {
                        gEditor.mHalfWidth = std::max(4.0, std::min(14.0,
                            gEditor.mHalfWidth + (clicked == kEditorWider ? 1.0 : -1.0)));
                        gEditor.mStatus.clear();
                        RefreshEditorBuild();
                    }
                    else if (clicked == kEditorMines || clicked == kEditorHazards)
                    {
                        if (clicked == kEditorMines)
                            gEditor.mMines = !gEditor.mMines;
                        else
                            gEditor.mHazards = !gEditor.mHazards;
                        gEditor.mStatus.clear();
                        RefreshEditorBuild();
                    }
                    else if (clicked == kEditorUndo)
                    {
                        if (!gEditor.mPoints.empty())
                            gEditor.mPoints.pop_back();
                        gEditor.mStatus.clear();
                        RefreshEditorBuild();
                    }
                    else if (clicked == kEditorClear)
                    {
                        gEditor.mPoints.clear();
                        gEditor.mStatus.clear();
                        RefreshEditorBuild();
                    }
                    else if (clicked == kEditorBack)
                    {
                        if (gEditor.mNaming)
                            SDL_StopTextInput();
                        gEditor.mNaming = false;
                        frontScreen = FrontScreen::Welcome;
                    }
                    else if (clicked == kEditorSave || clicked == kEditorDrive)
                    {
                        if (saveEditorTrack() && clicked == kEditorDrive)
                        {
                            trackIndex = gEditor.mSavedIndex;
                            localSetupSelection = 0;
                            loadTrack(false);
                            startLocalRace();
                        }
                    }
                    else if (inCanvas(mouseX, mouseY))
                    {
                        const int hit = nearestPoint(mouseX, mouseY);
                        if (hit >= 0)
                            gEditor.mDragIndex = hit;
                        else if (gEditor.mPoints.size() < kEditorMaximumPoints)
                        {
                            EditorPoint point;
                            point.mX = EditorSnap(EditorWorldX(layout, mouseX));
                            point.mY = EditorSnap(EditorWorldY(layout, mouseY));
                            gEditor.mPoints.push_back(point);
                            gEditor.mStatus.clear();
                            RefreshEditorBuild();
                        }
                    }
                    continue;
                }
            }
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
                    int option = -1;
                    for (int candidate = 0; candidate < 6; ++candidate)
                    {
                        int rowLeft = 0;
                        int rowTop = 0;
                        int rowWidth = 0;
                        int rowHeight = 0;
                        WelcomeRowGeometry(drawableWidth, drawableHeight, candidate, rowLeft, rowTop, rowWidth, rowHeight);
                        if (IsPointInRect(mouseX, mouseY, rowLeft, rowTop, rowWidth, rowHeight))
                            option = candidate;
                    }
                    if (option >= 0)
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
                        else if (option == 4)
                        {
                            frontScreen = FrontScreen::TrackEditor;
                            gEditor.mStatus.clear();
                            RefreshEditorBuild();
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
                    const int actionWidth = std::max(drawableWidth * 15 / 100, 210);
                    const int detailWidth = drawableWidth - margin * 2 - gap * 2 - leftWidth - actionWidth;
                    const int topHeight = std::max(drawableHeight * 42 / 100, 290);
                    const int actionLeft = margin + leftWidth + gap + detailWidth + gap;
                    const int chatLeft = margin + leftWidth + gap + 16;
                    const int chatWidth = detailWidth + gap + actionWidth - 32;
                    const int bottomTop = top + topHeight + gap;
                    const int bottomHeight = drawableHeight - bottomTop - margin;
                    const int inputTop = bottomTop + bottomHeight - 46;
                    if (IsPointInRect(mouseX, mouseY, actionLeft + 16, top + 16, actionWidth - 32, 42)
                        && gLobbySelectedRoom >= 0 && gLobbySelectedRoom < static_cast<int>(gLobbyRooms.size()))
                        activateLobbyPrimary();
                    else if (IsPointInRect(mouseX, mouseY, actionLeft + 16, top + 72, actionWidth - 32, 42)
                            && !(gLobbySelectedRoom >= 0
                                && gLobbySelectedRoom < static_cast<int>(gLobbyRooms.size())
                                && gLobbyRooms[gLobbySelectedRoom].mHostId == gLobbyPlayerId))
                        activateLobbySecondary();
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
                        submitChat(gLobbyChatInput);
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
                    const int panelTop = std::max(8, std::min(72, drawableHeight - 538 - 8));
                    const int option = (mouseY - (panelTop + 82)) / 46;
                    if (option >= 0 && option < 9
                        && IsPointInRect(mouseX, mouseY, panelLeft + 18, panelTop + 82 + option * 46, 524, 36))
                    {
                        gHostSetupSelection = option;
                        if (option == 6)
                            gHostPrivateRoom = !gHostPrivateRoom;
                        else if (option == 7)
                        {
                            requestCreateRoom(HostTrackName(gHostTrackIndex));
                        }
                        else if (option == 8)
                            frontScreen = FrontScreen::Multiplayer;
                    }
                }
                else if (frontScreen == FrontScreen::HowToPlay)
                    frontScreen = FrontScreen::Welcome;
                else if (frontScreen == FrontScreen::Settings)
                {
                    int clickedRow = -1;
                    for (int row = 0; row < 7; ++row)
                    {
                        int rowTop = 0;
                        int rowHeight = 0;
                        LocalSetupRowGeometry(drawableHeight, 7, row, rowTop, rowHeight);
                        if (IsPointInRect(mouseX, mouseY, drawableWidth / 2 - 250, rowTop, 500, rowHeight))
                            clickedRow = row;
                    }
                    settingsSelection = clickedRow >= 0 ? clickedRow : settingsSelection;
                    if (clickedRow == 0)
                    {
                        gDisplayNameSetupConnectsToLobby = false;
                        frontScreen = FrontScreen::DisplayNameSetup;
                        SDL_StartTextInput();
                    }
                    else if (clickedRow == 1)
                    {
                        // Pick the choice nearest the click.
                        int best = 0;
                        for (int option = 1; option < 3; ++option)
                        {
                            if (mouseX >= SettingsCameraOptionX(drawableWidth, option) - 8)
                                best = option;
                        }
                        cameraDistanceSetting = best;
                        savePreferences();
                    }
                    else if (clickedRow == 2)
                    {
                        gHudTextLarge = !gHudTextLarge;
                        savePreferences();
                    }
                    else if (clickedRow == 3)
                    {
                        audioFeedback.SetEnabled(!audioFeedback.Enabled());
                        savePreferences();
                    }
                    else if (clickedRow == 4)
                    {
                        const int volume = (static_cast<int>(audioFeedback.MenuVolume() * 100.0 + 0.5) + 25)
                            % 125;
                        audioFeedback.SetMenuVolume(volume / 100.0);
                        savePreferences();
                    }
                    else if (clickedRow == 5)
                    {
                        const int volume = (static_cast<int>(audioFeedback.RaceVolume() * 100.0 + 0.5) + 25)
                            % 125;
                        audioFeedback.SetRaceVolume(volume / 100.0);
                        savePreferences();
                    }
                    else if (clickedRow == 6)
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
                    int clickedOption = -1;
                    for (int option = 0; option < 10; ++option)
                    {
                        const int row = LocalSetupOptionRow(raceMode, option);
                        int rowTop = 0;
                        int rowHeight = 0;
                        LocalSetupRowGeometry(drawableHeight, LocalSetupOptionRow(raceMode, 9) + 1, row,
                                              rowTop, rowHeight);
                        if (row >= 0 && IsPointInRect(mouseX, mouseY, drawableWidth / 2 - 250,
                                                     rowTop, 500, rowHeight))
                        {
                            clickedOption = option;
                            break;
                        }
                    }
                    if (clickedOption >= 0)
                    {
                        localSetupSelection = clickedOption;
                        if (clickedOption == 0)
                            startLocalRace();
                        else if (clickedOption == 9)
                            frontScreen = FrontScreen::Welcome;
                        else
                            changeLocalSetupOption(clickedOption, 1);
                    }
                }
                else if (pauseMenuOpen)
                {
                    const int left = (drawableWidth - 760) / 2;
                    const int top = (drawableHeight - 536) / 2;
                    const int option = (mouseY - (top + 70)) / 40;
                    if (option >= 0 && option < 11
                        && IsPointInRect(mouseX, mouseY, left + 24, top + 70 + option * 40, 326, 34))
                    {
                        pauseMenuSelection = option;
                        if (option == 0)
                            pauseMenuOpen = false;
                        else if (option == 1)
                        {
                            steeringAssistEnabled = !steeringAssistEnabled;
                            savePreferences();
                        }
                        else if (option == 2)
                        {
                            brakingAssistEnabled = !brakingAssistEnabled;
                            savePreferences();
                        }
                        else if (option == 3)
                        {
                            cameraDistanceSetting = (cameraDistanceSetting + 1) % 3;
                            savePreferences();
                        }
                        else if (option == 4)
                        {
                            gCameraMotion = NextCameraMotion(gCameraMotion, 1);
                            savePreferences();
                        }
                        else if (option == 5)
                        {
                            gHudTextLarge = !gHudTextLarge;
                            savePreferences();
                        }
                        else if (option == 6)
                        {
                            audioFeedback.SetMenuVolume(audioFeedback.MenuVolume() + 0.1);
                            savePreferences();
                        }
                        else if (option == 7)
                        {
                            audioFeedback.SetRaceVolume(audioFeedback.RaceVolume() + 0.1);
                            savePreferences();
                        }
                        else if (option == 8)
                        {
                            gRemapOpen = true;
                            gRemapCapturing = false;
                        }
                        else if (option == 9)
                            returnToMainMenu();
                        else if (option == 10)
                            running = false;
                    }
                }
                else if (winner != 0 && !continueDriving)
                {
                    const int panelHeight = raceMode == RaceMode::Championship ? 408 : 352;
                    const int left = (drawableWidth - 620) / 2;
                    const int top = (drawableHeight - panelHeight) / 2 - 10;
                    const int actionsTop = top + (raceMode == RaceMode::Championship ? 352 : 292);
                    if (IsPointInRect(mouseX, mouseY, left + 24, actionsTop, 178, 38))
                    {
                        resultSelection = 0;
                        advanceResult();
                    }
                    else if (IsPointInRect(mouseX, mouseY, left + 218, actionsTop, 178, 38))
                    {
                        resultSelection = 1;
                        returnToSetup();
                    }
                    else if (IsPointInRect(mouseX, mouseY, left + 412, actionsTop, 178, 38))
                    {
                        resultSelection = 2;
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
                            gOnlineHasBoostSnapshot = false;
                            gOnlineBoostActive = false;
                            gOnlineSpinOutActive = false;
                            gOnlineBoostCue = false;
                            gOnlineImpactCue = false;
                            gOnlineChatMessages.clear();
                            gOnlineChatInput.clear();
                            gOnlineChatInputFocused = false;
                            SDL_StartTextInput();
                            frontScreen = FrontScreen::Multiplayer;
                        }
                        else
                            frontScreen = FrontScreen::Welcome;
                    }
                    else
                        running = false;
                }
                else if (event.type == SDL_CONTROLLERBUTTONDOWN
                         && frontScreen == FrontScreen::Multiplayer)
                {
                    gLobbyChatInputFocused = false;
                    if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP && !gLobbyRooms.empty())
                        gLobbySelectedRoom = (gLobbySelectedRoom + static_cast<int>(gLobbyRooms.size()) - 1)
                            % static_cast<int>(gLobbyRooms.size());
                    else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN
                             && !gLobbyRooms.empty())
                        gLobbySelectedRoom = (gLobbySelectedRoom + 1)
                            % static_cast<int>(gLobbyRooms.size());
                    else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A)
                        activateLobbyPrimary();
                    else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_X)
                        activateLobbySecondary();
                }
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::Multiplayer
                         && gLobbyChatInputFocused)
                {
                    if (event.key.keysym.sym == SDLK_TAB)
                        gLobbyChatInputFocused = false;
                    else if (event.key.keysym.sym == SDLK_BACKSPACE && !gLobbyChatInput.empty())
                        gLobbyChatInput.erase(gLobbyChatInput.size() - 1);
                    else if ((event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)
                             && !gLobbyChatInput.empty())
                    {
                        submitChat(gLobbyChatInput);
                    }
                }
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::Multiplayer
                         && !gLobbyChatInputFocused)
                {
                    if (event.key.keysym.sym == SDLK_TAB)
                    {
                        gLobbyChatInputFocused = true;
                        SDL_StartTextInput();
                    }
                    else if (event.key.keysym.sym == SDLK_UP && !gLobbyRooms.empty())
                        gLobbySelectedRoom = (gLobbySelectedRoom + static_cast<int>(gLobbyRooms.size()) - 1)
                            % static_cast<int>(gLobbyRooms.size());
                    else if (event.key.keysym.sym == SDLK_DOWN && !gLobbyRooms.empty())
                        gLobbySelectedRoom = (gLobbySelectedRoom + 1) % static_cast<int>(gLobbyRooms.size());
                    else if (event.key.keysym.sym == SDLK_RETURN
                             || event.key.keysym.sym == SDLK_KP_ENTER)
                        activateLobbyPrimary();
                    else if (event.key.keysym.sym == SDLK_x)
                        activateLobbySecondary();
                }
                else if (event.type == SDL_CONTROLLERBUTTONDOWN
                         && event.cbutton.button == gPadBindings.Button(PadAction::Recover)
                         && frontScreen == FrontScreen::OnlineRace && !gOnlineRaceFinished)
                    gOnlineRecoveryRequested = true;
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::OnlineRace)
                {
                    if (gOnlineRaceFinished
                        && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER))
                    {
                        gOnlineRaceRoomId = 0;
                        gOnlineRaceTick = 0;
                        gOnlineTargetLaps = 0;
                        gOnlineRacers.clear();
                        gOnlineRaceFinished = false;
                        gOnlineChatInput.clear();
                        gOnlineChatInputFocused = false;
                        SDL_StartTextInput();
                        gLobbyChatInputFocused = true;
                        frontScreen = FrontScreen::Multiplayer;
                    }
                    else if (event.key.keysym.sym == SDLK_F2)
                        gOnlineRecoveryRequested = true;
                    else if (event.key.keysym.sym == SDLK_F3)
                        steeringAssistEnabled = !steeringAssistEnabled;
                    else if (event.key.keysym.sym == SDLK_F4)
                        brakingAssistEnabled = !brakingAssistEnabled;
                    else if (gOnlineCountdownActive && !gOnlineRaceFinished
                             && (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_RIGHT))
                    {
                        const int changes = event.key.keysym.sym == SDLK_LEFT ? 2 : 1;
                        for (int change = 0; change < changes; ++change)
                            playerCraftClass = NextCraftClass(playerCraftClass);
                    }
                    else if (gOnlineChatInputFocused && event.key.keysym.sym == SDLK_BACKSPACE
                        && !gOnlineChatInput.empty())
                        gOnlineChatInput.erase(gOnlineChatInput.size() - 1);
                    else if (gOnlineChatInputFocused
                             && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER))
                    {
                        if (!gOnlineChatInput.empty())
                            submitChat(gOnlineChatInput);
                    }
                    else if (gOnlineChatInputFocused)
                        AppendOnlineChatKey(gOnlineChatInput, event.key.keysym.sym, event.key.keysym.mod);
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
                        gHostSetupSelection = (gHostSetupSelection + 8) % 9;
                    else if (event.key.keysym.sym == SDLK_DOWN)
                        gHostSetupSelection = (gHostSetupSelection + 1) % 9;
                    else if (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_RIGHT)
                    {
                        const int direction = event.key.keysym.sym == SDLK_LEFT ? -1 : 1;
                        if (gHostSetupSelection == 0)
                            gHostRaceMode = (gHostRaceMode + direction + 4) % 4;
                        else if (gHostSetupSelection == 1)
                            gHostTrackIndex = NextHostTrack(gHostTrackIndex, direction);
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
                        else if (gHostSetupSelection == 6)
                            gHostPrivateRoom = !gHostPrivateRoom;
                    }
                    else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)
                    {
                        if (gHostSetupSelection == 7)
                        {
                            requestCreateRoom("OPEN RACE");
                        }
                        else if (gHostSetupSelection == 8)
                            frontScreen = FrontScreen::Multiplayer;
                    }
                }
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::HowToPlay
                         && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER))
                    frontScreen = FrontScreen::Welcome;
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::Settings
                         && event.key.keysym.sym == SDLK_UP)
                    settingsSelection = (settingsSelection + 6) % 7;
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::Settings
                         && event.key.keysym.sym == SDLK_DOWN)
                    settingsSelection = (settingsSelection + 1) % 7;
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::Settings
                         && (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_RIGHT))
                {
                    const int direction = event.key.keysym.sym == SDLK_LEFT ? -1 : 1;
                    if (settingsSelection == 1)
                        cameraDistanceSetting = (cameraDistanceSetting + direction + 3) % 3;
                    else if (settingsSelection == 2)
                        gHudTextLarge = !gHudTextLarge;
                    else if (settingsSelection == 3)
                        audioFeedback.SetEnabled(!audioFeedback.Enabled());
                    else if (settingsSelection == 4)
                    {
                        const double change = direction * 0.1;
                        audioFeedback.SetMenuVolume(audioFeedback.MenuVolume() + change);
                    }
                    else if (settingsSelection == 5)
                    {
                        const double change = direction * 0.1;
                        audioFeedback.SetRaceVolume(audioFeedback.RaceVolume() + change);
                    }
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
                         && settingsSelection == 6)
                    frontScreen = FrontScreen::Welcome;
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::LocalSetup)
                {
                    if (event.key.keysym.sym == SDLK_UP)
                        localSetupSelection = NextLocalSetupOption(raceMode, localSetupSelection, -1);
                    else if (event.key.keysym.sym == SDLK_DOWN)
                        localSetupSelection = NextLocalSetupOption(raceMode, localSetupSelection, 1);
                    else if (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_RIGHT)
                    {
                        const int direction = event.key.keysym.sym == SDLK_LEFT ? -1 : 1;
                        changeLocalSetupOption(localSetupSelection, direction);
                    }
                    else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)
                    {
                        if (localSetupSelection == 0)
                            startLocalRace();
                        else if (localSetupSelection == 9)
                            frontScreen = FrontScreen::Welcome;
                    }
                }
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::Welcome)
                {
                    if (event.key.keysym.sym == SDLK_UP)
                        frontSelection = (frontSelection + 5) % 6;
                    else if (event.key.keysym.sym == SDLK_DOWN)
                        frontSelection = (frontSelection + 1) % 6;
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
                        else if (frontSelection == 4)
                        {
                            frontScreen = FrontScreen::TrackEditor;
                            gEditor.mStatus.clear();
                            RefreshEditorBuild();
                        }
                        else
                            running = false;
                    }
                }
                continue;
            }
            if (pauseMenuOpen && gRemapOpen
                && (event.type == SDL_KEYDOWN
                    || (event.type == SDL_CONTROLLERBUTTONDOWN && gRemapCapturing
                        && gRemapSelection >= kKeyRemapRows)))
            {
                const bool padEvent = event.type == SDL_CONTROLLERBUTTONDOWN;
                const SDL_Keycode key = padEvent ? SDLK_UNKNOWN : event.key.keysym.sym;
                const int count = kRemapRows;
                if (gRemapCapturing)
                {
                    if (padEvent || key != SDLK_ESCAPE)
                    {
                        const bool padRow = gRemapSelection >= kKeyRemapRows;
                        bool accepted = false;
                        if (padEvent && padRow)
                            accepted = gPadBindings.Rebind(
                                static_cast<PadAction>(gRemapSelection - kKeyRemapRows),
                                event.cbutton.button);
                        else if (!padEvent && !padRow)
                            accepted = gKeyBindings.Rebind(static_cast<BindAction>(gRemapSelection),
                                                           event.key.keysym.scancode);
                        if (accepted)
                            saveBindings();
                        else
                            audioFeedback.PlayImpact();
                    }
                    gRemapCapturing = false;
                }
                else if (key == SDLK_ESCAPE)
                    gRemapOpen = false;
                else if (key == SDLK_UP)
                    gRemapSelection = (gRemapSelection + count - 1) % count;
                else if (key == SDLK_DOWN)
                    gRemapSelection = (gRemapSelection + 1) % count;
                else if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
                    gRemapCapturing = true;
                else if (key == SDLK_BACKSPACE)
                {
                    gKeyBindings.ResetDefaults();
                    gPadBindings.ResetDefaults();
                    saveBindings();
                }
                continue;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
            {
                gRemapOpen = false;
                gRemapCapturing = false;
                pauseMenuOpen = !pauseMenuOpen;
                pauseMenuSelection = 0;
                continue;
            }
            if (pauseMenuOpen)
            {
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_UP)
                    pauseMenuSelection = (pauseMenuSelection + 10) % 11;
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_DOWN)
                    pauseMenuSelection = (pauseMenuSelection + 1) % 11;
                else if (event.type == SDL_KEYDOWN
                         && (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_RIGHT))
                {
                    if (pauseMenuSelection == 1)
                    {
                        steeringAssistEnabled = !steeringAssistEnabled;
                        savePreferences();
                    }
                    else if (pauseMenuSelection == 2)
                    {
                        brakingAssistEnabled = !brakingAssistEnabled;
                        savePreferences();
                    }
                    else if (pauseMenuSelection == 3)
                    {
                        const int direction = event.key.keysym.sym == SDLK_LEFT ? -1 : 1;
                        cameraDistanceSetting = (cameraDistanceSetting + direction + 3) % 3;
                        savePreferences();
                    }
                    else if (pauseMenuSelection == 4)
                    {
                        gCameraMotion = NextCameraMotion(
                            gCameraMotion, event.key.keysym.sym == SDLK_LEFT ? -1 : 1);
                        savePreferences();
                    }
                    else if (pauseMenuSelection == 5)
                    {
                        gHudTextLarge = !gHudTextLarge;
                        savePreferences();
                    }
                    else if (pauseMenuSelection == 6)
                    {
                        const int direction = event.key.keysym.sym == SDLK_LEFT ? -1 : 1;
                        audioFeedback.SetMenuVolume(audioFeedback.MenuVolume() + direction * 0.1);
                        savePreferences();
                    }
                    else if (pauseMenuSelection == 7)
                    {
                        const int direction = event.key.keysym.sym == SDLK_LEFT ? -1 : 1;
                        audioFeedback.SetRaceVolume(audioFeedback.RaceVolume() + direction * 0.1);
                        savePreferences();
                    }
                }
                else if (event.type == SDL_KEYDOWN
                         && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER))
                {
                    if (pauseMenuSelection == 0)
                        pauseMenuOpen = false;
                    else if (pauseMenuSelection == 1)
                    {
                        steeringAssistEnabled = !steeringAssistEnabled;
                        savePreferences();
                    }
                    else if (pauseMenuSelection == 2)
                    {
                        brakingAssistEnabled = !brakingAssistEnabled;
                        savePreferences();
                    }
                    else if (pauseMenuSelection == 3)
                    {
                        cameraDistanceSetting = (cameraDistanceSetting + 1) % 3;
                        savePreferences();
                    }
                    else if (pauseMenuSelection == 4)
                    {
                        gCameraMotion = NextCameraMotion(gCameraMotion, 1);
                        savePreferences();
                    }
                    else if (pauseMenuSelection == 5)
                    {
                        gHudTextLarge = !gHudTextLarge;
                        savePreferences();
                    }
                    else if (pauseMenuSelection == 6)
                    {
                        audioFeedback.SetMenuVolume(audioFeedback.MenuVolume() + 0.1);
                        savePreferences();
                    }
                    else if (pauseMenuSelection == 7)
                    {
                        audioFeedback.SetRaceVolume(audioFeedback.RaceVolume() + 0.1);
                        savePreferences();
                    }
                    else if (pauseMenuSelection == 8)
                    {
                        gRemapOpen = true;
                        gRemapCapturing = false;
                    }
                    else if (pauseMenuSelection == 9)
                        returnToMainMenu();
                    else if (pauseMenuSelection == 10)
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
                resultSelection = (resultSelection + (event.key.keysym.sym == SDLK_LEFT ? 2 : 1)) % 3;
                continue;
            }
            if (event.type == SDL_KEYDOWN
                && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)
                && winner != 0 && !continueDriving)
            {
                if (resultSelection == 0)
                    advanceResult();
                else if (resultSelection == 1)
                    returnToSetup();
                else
                    returnToMainMenu();
                continue;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r && winner != 0)
                advanceResult();
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0
                && (event.key.keysym.sym == SDLK_LALT || event.key.keysym.sym == SDLK_RALT)
                && frontScreen == FrontScreen::RaceSetup)
            {
                gGhostMode = gGhostMode == 1 ? 2 : (gGhostMode == 2 ? 0 : 1);
                gGhostToast = gGhostMode == 0 ? "GHOST OFF"
                    : (gGhostMode == 1 ? "GHOST: BEST RUN" : "GHOST: LAST RUN");
                if (gGhostMode != 0 && raceStart.Started() && winner == 0 && ghostActive
                    && gGhostSourceLabel != (gGhostMode == 1 ? "BEST" : "LAST"))
                    gGhostToast += " FROM NEXT RACE";
                gGhostToastUntil = SDL_GetTicks() + 2200;
                savePreferences();
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_v)
            {
                steeringAssistEnabled = !steeringAssistEnabled;
                savePreferences();
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_b)
            {
                brakingAssistEnabled = !brakingAssistEnabled;
                savePreferences();
            }
            if (event.type == SDL_KEYDOWN && !pauseMenuOpen
                && event.key.keysym.scancode == gKeyBindings.Key(BindAction::Recover) && raceStart.Started()
                && winner == 0)
            {
                const RaceGate& recoveryGate = race.Progress().mNextCheckpoint
                    < static_cast<int>(checkpoints.size())
                    ? checkpoints[race.Progress().mNextCheckpoint] : finish;
                HovercraftState recoveryState = hovercraft.State();
                if (RecoverHovercraftToRoute(recoveryState, course, recoveryGate, false, &ground))
                {
                    hovercraft.Reset(recoveryState);
                    recoveredThisRun = true;
                    if (raceMode == RaceMode::Practice)
                        practiceGuide.ObserveRecovery();
                }
            }
            if (event.type == SDL_CONTROLLERBUTTONDOWN
                && event.cbutton.button == gPadBindings.Button(PadAction::Recover) && raceStart.Started()
                && winner == 0)
            {
                const RaceGate& recoveryGate = race.Progress().mNextCheckpoint
                    < static_cast<int>(checkpoints.size())
                    ? checkpoints[race.Progress().mNextCheckpoint] : finish;
                HovercraftState recoveryState = hovercraft.State();
                if (RecoverHovercraftToRoute(recoveryState, course, recoveryGate, false, &ground))
                {
                    hovercraft.Reset(recoveryState);
                    recoveredThisRun = true;
                    if (raceMode == RaceMode::Practice)
                        practiceGuide.ObserveRecovery();
                }
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
        const bool onlineRace = frontScreen == FrontScreen::OnlineRace;
        const bool shiftPressed = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
        const bool downPressed = keys[SDL_SCANCODE_DOWN];
        HovercraftInput input;
        input.mThrottle = (shiftPressed
                           || (!onlineRace && keys[gKeyBindings.Key(BindAction::Accelerate)]) ? 1.0 : 0.0)
            - ((!onlineRace && keys[gKeyBindings.Key(BindAction::Brake)]) || downPressed ? 1.0 : 0.0);
        input.mSteering = ((!onlineRace && keys[gKeyBindings.Key(BindAction::SteerRight)])
                           || keys[SDL_SCANCODE_RIGHT] ? 1.0 : 0.0)
            - ((!onlineRace && keys[gKeyBindings.Key(BindAction::SteerLeft)])
               || keys[SDL_SCANCODE_LEFT] ? 1.0 : 0.0);
        input.mJump = keys[SDL_SCANCODE_UP];
        input.mFire = keys[gKeyBindings.Key(BindAction::Fire)] || keys[SDL_SCANCODE_RCTRL];
        input.mReverseFacing = shiftPressed && downPressed;
        if (controller != nullptr)
        {
            input.mSteering += ControllerAxis(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX));
            input.mThrottle += SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 32767.0;
            input.mThrottle -= SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 32767.0;
            input.mJump = input.mJump || SDL_GameControllerGetButton(
                controller, static_cast<SDL_GameControllerButton>(gPadBindings.Button(PadAction::Jump)));
            input.mFire = input.mFire || SDL_GameControllerGetButton(
                controller, static_cast<SDL_GameControllerButton>(gPadBindings.Button(PadAction::Fire)));
        }
        if (frontScreen == FrontScreen::OnlineRace && !gOnlineRaceFinished)
        {
            onlineInputSeconds += std::fmin(frameSeconds, 0.1);
            if (onlineInputSeconds >= 1.0 / 30.0)
            {
                std::ostringstream command;
                command << "INPUT " << input.mThrottle << '|' << input.mSteering << '|'
                    << (input.mJump ? 1 : 0) << '|' << (input.mReverseFacing ? 1 : 0)
                    << '|' << (input.mFire ? 1 : 0) << '|'
                        << (gOnlineRecoveryRequested ? 1 : 0) << '|'
                        << (steeringAssistEnabled ? 1 : 0) << '|'
                        << (brakingAssistEnabled ? 1 : 0) << '|'
                        << static_cast<int>(playerCraftClass);
                lobbyClient.SendCommand(command.str());
                gOnlineRecoveryRequested = false;
                onlineInputSeconds = 0.0;
            }
        }
        const int simulationSteps = frontScreen == FrontScreen::RaceSetup && !pauseMenuOpen
            ? simulationClock.Consume(frameSeconds) : 0;
        for (int step = 0; step < simulationSteps; ++step)
        {
            const double seconds = simulationClock.StepSeconds();
            impactSoundCooldown = std::fmax(0.0, impactSoundCooldown - seconds);
            if (!raceStart.Started())
            {
                // On the grid before the start: sit on the ground at the grid, however high it is.
                const auto settle = [&ground](Hovercraft& pCraft)
                {
                    pCraft.ApplyGround(ground);
                };
                settle(hovercraft);
                settle(replayGhost);
                for (Hovercraft& rival : rivals)
                    settle(rival);
            }
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
                if (raceMode == RaceMode::Practice)
                    practiceGuide.ObserveInput(playerInput.mThrottle, playerInput.mSteering,
                                               playerInput.mJump);
                hovercraft.Step(playerInput, seconds);
                activeRecording.Record(playerInput);
                if (weaponsAllowed && playerInput.mFire && !fireHeld && missile.Fire(hovercraft.State()))
                {
                    audioFeedback.PlayBoost();
                    if (raceMode == RaceMode::Practice)
                        practiceGuide.ObserveFire();
                }
                fireHeld = playerInput.mFire;
                if (ghostActive)
                {
                    HovercraftInput ghostInput;
                    if (ghostRecording.InputAt(ghostFrame, ghostInput))
                    {
                        replayGhost.Step(ghostInput, seconds);
                        BounceOffCourseWall(replayGhost, course);
                        ApplyBoostPads(replayGhost, boostPads);
                        ApplyRaisedSections(replayGhost, raisedSections, ground);
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
                        std::vector<HovercraftState> others;
                        others.push_back(hovercraft.State());
                        for (int otherIndex = 0; otherIndex < rivalCount; ++otherIndex)
                        {
                            if (otherIndex != rivalIndex)
                                others.push_back(rivals[otherIndex].State());
                        }
                        HovercraftInput rivalInput = rivalControllers[rivalIndex].InputFor(
                            rivals[rivalIndex].State(), others);
                        rivalInput.mJump = ShouldJumpRaisedSection(rivals[rivalIndex].State(),
                                                                   raisedSections)
                            || ShouldJumpGroundStep(rivals[rivalIndex].State(), ground);
                        rivals[rivalIndex].Step(rivalInput, seconds);
                        if (weaponsAllowed && rivalInput.mFire)
                            rivalMissiles[rivalIndex].Fire(rivals[rivalIndex].State());
                        // A rival stuck against a wall puts itself back on the road, like a player
                        // pressing recover.
                        const bool stuckInPlace = rivalStalls[rivalIndex].Update(
                            rivals[rivalIndex].State().mX, rivals[rivalIndex].State().mY, seconds);
                        const bool goingNowhere = rivalStalls[rivalIndex].UpdateProgress(
                            rivalRoutes[rivalIndex].Update(course, rivals[rivalIndex].State().mX,
                                                           rivals[rivalIndex].State().mY), seconds);
                        if (stuckInPlace || goingNowhere)
                        {
                            HovercraftState recovered = rivals[rivalIndex].State();
                            if (RecoverHovercraftToRoute(recovered, course, finish, true, &ground))
                            {
                                rivals[rivalIndex].Reset(recovered);
                                rivalControllers[rivalIndex].Retarget(recovered);
                            }
                        }
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
                        ApplyBoostPads(rival, boostPads);
                        ApplyRaisedSections(rival, raisedSections, ground);
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
                        impactVisualUntil = SDL_GetTicks() + 650;
                    }
                    for (int rivalIndex = 0; rivalIndex < kRivalCount; ++rivalIndex)
                    {
                        HovercraftState rivalState = rivals[rivalIndex].State();
                        if (missile.ApplyHit(rivalState))
                        {
                            rivals[rivalIndex].Reset(rivalState);
                            audioFeedback.PlayImpact();
                            impactVisualUntil = SDL_GetTicks() + 650;
                        }
                    }
                    // Rival missiles can hit the player and the other rivals.
                    for (int shooter = 0; shooter < kRivalCount; ++shooter)
                    {
                        rivalMissiles[shooter].Step(seconds, course);
                        HovercraftState hitPlayer = hovercraft.State();
                        if (rivalMissiles[shooter].ApplyHit(hitPlayer))
                        {
                            hovercraft.Reset(hitPlayer);
                            audioFeedback.PlayImpact();
                            impactVisualUntil = SDL_GetTicks() + 650;
                        }
                        for (int target = 0; target < kRivalCount; ++target)
                        {
                            if (target == shooter)
                                continue;
                            HovercraftState hitRival = rivals[target].State();
                            if (rivalMissiles[shooter].ApplyHit(hitRival))
                                rivals[target].Reset(hitRival);
                        }
                    }
                }
                if (BounceOffCourseWall(hovercraft, course) && impactSoundCooldown <= 0.0)
                {
                    audioFeedback.PlayImpact();
                    impactVisualUntil = SDL_GetTicks() + 650;
                    impactSoundCooldown = 0.18;
                }
                if (ApplyBoostPads(hovercraft, boostPads))
                {
                    audioFeedback.PlayBoost();
                    boostVisualUntil = SDL_GetTicks() + 650;
                    if (raceMode == RaceMode::Practice)
                        practiceGuide.ObserveBoost();
                }
                ApplyRaisedSections(hovercraft, raisedSections, ground);
                if (ApplyMines(hovercraft, mines) && impactSoundCooldown <= 0.0)
                {
                    audioFeedback.PlayImpact();
                    impactVisualUntil = SDL_GetTicks() + 650;
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
                {
                    audioFeedback.PlayCheckpoint();
                    checkpointVisualUntil = SDL_GetTicks() + 850;
                    if (raceMode == RaceMode::Practice)
                        practiceGuide.ObserveCheckpoint();
                }
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
        // Gap to the ghost along the road, for the HUD.
        if (ghostActive && gGhostMode != 0 && raceStart.Started() && winner == 0)
        {
            const double playerDistance = playerRoute.Update(course, state.mX, state.mY);
            const double ghostDistance = ghostRoute.Update(course, replayGhost.State().mX, replayGhost.State().mY);
            gGhostGapMeters = playerDistance - ghostDistance;
            gGhostGapShown = true;
        }
        else
            gGhostGapShown = false;
        // A run that used recovery cannot be replayed from inputs alone, so it never becomes a ghost.
        if (race.Progress().mFinished && !ghostSubmitted)
        {
            ghostSubmitted = true;
            if (!recoveredThisRun)
            {
                gLastRun.mTrackId = selectedTrack.mId;
                gLastRun.mCraftClass = static_cast<int>(playerCraftClass);
                gLastRun.mSeconds = race.Progress().mElapsedSeconds;
                gLastRun.mRecording = activeRecording;
            }
            if (!recoveredThisRun
                && gGhostLibrary.Submit(selectedTrack.mId, static_cast<int>(playerCraftClass),
                                        race.Progress().mElapsedSeconds, activeRecording))
            {
                gNewGhostBest = true;
                saveGhosts();
            }
        }
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
                        gRaceWinnerName = LocalRivalName(rivalIndex);
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
        std::vector<int> competitorPositions;
        if (RaceModeUsesRivals(raceMode))
        {
            for (int competitorIndex = 0;
                 competitorIndex < static_cast<int>(raceProgresses.size()); ++competitorIndex)
                competitorPositions.push_back(CalculateRacePosition(raceProgresses, competitorIndex));
        }
        else
            competitorPositions.push_back(1);
        const int playerPosition = competitorPositions.front();
        const RaceGate& activeGate = race.Progress().mNextCheckpoint < static_cast<int>(checkpoints.size())
            ? checkpoints[race.Progress().mNextCheckpoint] : finish;
        const bool wrongWay = !race.Progress().mFinished && IsHeadingAwayFromGate(state, activeGate);
        if (raceMode == RaceMode::Championship && winner != 0 && !championship.EventRecorded())
        {
            championship.RecordResults(competitorPositions);
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
        if (raceMode == RaceMode::Championship && championship.CompetitorCount() > 0)
        {
            std::snprintf(championshipOverlayPoints, sizeof(championshipOverlayPoints),
                          "EVENT %d OF %d  YOU %d +%d  PLACE %d OF %d",
                          championship.CurrentEvent() + 1, championship.EventCount(),
                          championship.PlayerPoints(), championship.LastPointsAwarded(0),
                          championship.StandingForCompetitor(0), championship.CompetitorCount());
        }
        gChampionshipLines.clear();
        if (raceMode == RaceMode::Championship && championship.CompetitorCount() > 0)
        {
            // Line 1: where we are in the series.
            gChampionshipLines.push_back("EVENT " + std::to_string(championship.CurrentEvent() + 1) + " OF "
                                         + std::to_string(championship.EventCount()));
            // Line 2: the top three, by points, using rivals' names (first word, at most six letters).
            std::vector<int> order;
            for (int competitor = 0; competitor < championship.CompetitorCount(); ++competitor)
                order.push_back(competitor);
            std::sort(order.begin(), order.end(), [&](int pLeft, int pRight)
            {
                return championship.StandingForCompetitor(pLeft) < championship.StandingForCompetitor(pRight);
            });
            std::string leaders;
            for (std::size_t rank = 0; rank < order.size() && rank < 3; ++rank)
            {
                const int competitor = order[rank];
                std::string name = competitor == 0 ? "YOU" : LocalRivalName(competitor - 1);
                name = name.substr(0, name.find(' '));
                for (char& c : name)
                    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                if (name.size() > 6)
                    name.resize(6);
                if (!leaders.empty())
                    leaders += "  ";
                leaders += name + " " + std::to_string(championship.CompetitorPoints(competitor));
            }
            gChampionshipLines.push_back(leaders);
            // Line 3: how the player is doing, and what this event added.
            const int standing = championship.StandingForCompetitor(0);
            const char* suffix = (standing % 100 >= 11 && standing % 100 <= 13) ? "TH"
                : standing % 10 == 1 ? "ST" : standing % 10 == 2 ? "ND" : standing % 10 == 3 ? "RD" : "TH";
            // (The pixel font has no colon, plus or brackets, so the wording avoids them.)
            gChampionshipLines.push_back("YOU " + std::to_string(standing) + suffix + "  "
                                         + std::to_string(championship.PlayerPoints()) + " POINTS  ADDED "
                                         + std::to_string(championship.LastPointsAwarded(0)));
            // Line 4: what comes next.
            if (championship.Complete())
                gChampionshipLines.push_back("SERIES COMPLETE");
            else if (championship.CurrentEvent() + 1 >= championship.EventCount())
                gChampionshipLines.push_back("NEXT  FINAL RESULTS");
            else
                gChampionshipLines.push_back("NEXT  " + LocalTrackName(championship.CurrentEvent() + 1));
        }
        if (frontScreen == FrontScreen::OnlineRace)
            std::snprintf(title, sizeof(title), "OpenHover | Online Race | Server tick %u", gOnlineRaceTick);
        else if (frontScreen != FrontScreen::RaceSetup)
            std::snprintf(title, sizeof(title), "OpenHover | Welcome");
        else if (raceStart.Ready())
        {
            if (raceMode == RaceMode::Championship)
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
                std::snprintf(title, sizeof(title), "OpenHover | %s finishes first | You P%d/%d | Press R to restart",
                              gRaceWinnerName.empty() ? "Rival" : gRaceWinnerName.c_str(), playerPosition, RaceModeUsesRivals(raceMode) ? rivalCount + 1 : 1);
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
                            cameraDistanceSetting, audioFeedback.Enabled(),
                            static_cast<int>(audioFeedback.MenuVolume() * 100.0 + 0.5),
                            static_cast<int>(audioFeedback.RaceVolume() * 100.0 + 0.5),
                            trackIndex, targetLaps, rivalCount,
                            rivalDifficulty, raceMode, weaponsAllowed,
                            playerCraftClass, steeringAssistEnabled, brakingAssistEnabled,
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
                    cameraState = InterpolatedOnlineState(racer);
                    break;
                }
            }
            cameraState.mGroundHeight = ground.HeightAt(cameraState.mX, cameraState.mY);
            const GLfloat atmosphere[] = {selectedTrack.mAtmosphereRed, selectedTrack.mAtmosphereGreen,
                              selectedTrack.mAtmosphereBlue, 1.0f};
            glFogfv(GL_FOG_COLOR, atmosphere);
            glClearColor(selectedTrack.mAtmosphereRed, selectedTrack.mAtmosphereGreen,
                         selectedTrack.mAtmosphereBlue, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            SetPerspective(static_cast<double>(drawableWidth) / drawableHeight, cameraState.mSpeed,
                           CameraRig::SpeedZoomScale(gCameraMotion));
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            const double cameraDistances[] = {4.6, 5.8, 7.4};
            const double cameraGround = gCameraRig.UpdateGround(cameraState.mGroundHeight, frameSeconds, gCameraMotion);
            SetChaseCamera(cameraState, cameraDistances[cameraDistanceSetting],
                           gCameraRig.Update(cameraState.mHeading, frameSeconds, gCameraMotion),
                           gCameraRig.UpdateRise(std::fmax(0.0, cameraState.mHeight - cameraGround - 1.2), frameSeconds, gCameraMotion),
                           cameraGround);
            const GLfloat sunDirection[] = {-0.35f, 0.82f, 0.45f, 0.0f};
            glLightfv(GL_LIGHT0, GL_POSITION, sunDirection);
            DrawCourseGrid(cameraState);
            DrawTrackEnvironment(selectedTrack);
            DrawConnectedTrack(courseWaypoints, selectedTrack.mRoadHalfWidth,
                       selectedTrack.mRoadRed, selectedTrack.mRoadGreen, selectedTrack.mRoadBlue,
                       selectedTrack.mWallRed, selectedTrack.mWallGreen, selectedTrack.mWallBlue, roadTexture,
                       wallTexture);
            DrawFinishZone(finish, courseWaypoints, selectedTrack.mRoadHalfWidth);
            int onlineActiveCheckpoint = 0;
            for (const OnlineRacerView& racer : gOnlineRacers)
            {
                if (racer.mPlayerId == gLobbyPlayerId)
                {
                    onlineActiveCheckpoint = racer.mProgress.mNextCheckpoint;
                    break;
                }
            }
            DrawCheckpointGates(courseWaypoints, checkpoints, onlineActiveCheckpoint,
                                selectedTrack.mRoadHalfWidth);
            for (const BoostPad& pad : boostPads)
                DrawBoostPad(pad, courseWaypoints);
            for (const HazardZone& zone : hazardZones)
                DrawHazardZone(zone);
            for (const RaisedSection& section : raisedSections)
                DrawRaisedSection(section);
            for (std::size_t mineIndex = 0; mineIndex < mines.size(); ++mineIndex)
            {
                if (mineIndex >= gOnlineMineTriggered.size() || !gOnlineMineTriggered[mineIndex])
                    DrawMine(mines[mineIndex]);
            }
            for (const OnlineMissileView& missile : gOnlineMissiles)
                DrawMissile(missile.mState);
            const OnlineRacerView* localRacer = nullptr;
            for (std::size_t racerIndex = 0; racerIndex < gOnlineRacers.size(); ++racerIndex)
            {
                const OnlineRacerView& racer = gOnlineRacers[racerIndex];
                if (racer.mPlayerId == gLobbyPlayerId)
                    localRacer = &racer;
                DrawHovercraft(InterpolatedOnlineState(racer), racer.mPlayerId != gLobbyPlayerId, false,
                               racer.mCraftClass, static_cast<int>(racerIndex) + 1,
                               static_cast<int>(racerIndex));
            }
            if (localRacer != nullptr)
            {
                const RaceGate& onlineActiveGate = localRacer->mProgress.mNextCheckpoint
                    < static_cast<int>(checkpoints.size())
                    ? checkpoints[localRacer->mProgress.mNextCheckpoint] : selectedTrack.Finish();
                std::vector<HovercraftState> onlineRivalStates;
                for (const OnlineRacerView& racer : gOnlineRacers)
                {
                    if (racer.mPlayerId != gLobbyPlayerId)
                        onlineRivalStates.push_back(InterpolatedOnlineState(racer));
                }
                const bool onlineWrongWay = !localRacer->mProgress.mFinished
                    && IsHeadingAwayFromGate(localRacer->mState, onlineActiveGate);
                DrawOnlineHud(*localRacer, static_cast<int>(gOnlineRacers.size()), gOnlineTargetLaps,
                              onlineActiveGate, courseWaypoints, onlineRivalStates, onlineWrongWay,
                              drawableWidth, drawableHeight);
            }
            DrawOnlineChat(drawableWidth, drawableHeight);
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
        SetPerspective(static_cast<double>(drawableWidth) / drawableHeight, state.mSpeed,
                       CameraRig::SpeedZoomScale(gCameraMotion));
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        const double cameraDistances[] = {4.6, 5.8, 7.4};
        const double cameraGround = gCameraRig.UpdateGround(state.mGroundHeight, frameSeconds, gCameraMotion);
        SetChaseCamera(state, cameraDistances[cameraDistanceSetting],
                       gCameraRig.Update(state.mHeading, frameSeconds, gCameraMotion),
                           gCameraRig.UpdateRise(std::fmax(0.0, state.mHeight - cameraGround - 1.2), frameSeconds, gCameraMotion),
                           cameraGround);
        const GLfloat sunDirection[] = {-0.35f, 0.82f, 0.45f, 0.0f};
        glLightfv(GL_LIGHT0, GL_POSITION, sunDirection);
        DrawCourseGrid(state);
        DrawTrackEnvironment(selectedTrack);
        DrawConnectedTrack(courseWaypoints, selectedTrack.mRoadHalfWidth,
                   selectedTrack.mRoadRed, selectedTrack.mRoadGreen, selectedTrack.mRoadBlue,
                   selectedTrack.mWallRed, selectedTrack.mWallGreen, selectedTrack.mWallBlue, roadTexture,
                   wallTexture);
        DrawFinishZone(finish, courseWaypoints, selectedTrack.mRoadHalfWidth);
        DrawCheckpointGates(courseWaypoints, checkpoints, race.Progress().mNextCheckpoint,
                    selectedTrack.mRoadHalfWidth);
        for (const BoostPad& pad : boostPads)
            DrawBoostPad(pad, courseWaypoints);
        for (const HazardZone& zone : hazardZones)
            DrawHazardZone(zone);
        for (const RaisedSection& section : raisedSections)
            DrawRaisedSection(section);
        for (const Mine& mine : mines)
            DrawMine(mine);
        DrawMissile(missile);
        for (const Missile& rivalMissile : rivalMissiles)
            DrawMissile(rivalMissile);

        if (RaceModeUsesRivals(raceMode))
        {
            for (int rivalIndex = 0; rivalIndex < static_cast<int>(rivalStates.size()); ++rivalIndex)
                DrawHovercraft(rivalStates[rivalIndex], true, false, LocalRivalCraftClass(rivalIndex), rivalIndex + 2);
        }
        if (ghostActive && gGhostMode != 0)
            DrawHovercraft(replayGhost.State(), false, true, playerCraftClass, 1);
        DrawHovercraft(state, false, false, playerCraftClass, 1);
        std::vector<RaceProgress> rivalProgresses(raceProgresses.begin() + 1, raceProgresses.end());
        DrawHud(race.Progress(), race.TargetLaps(), rivalProgresses, state, rivalStates, activeGate,
            courseWaypoints,
            RaceModeUsesRivals(raceMode), wrongWay,
            SDL_GetTicks() < checkpointVisualUntil, SDL_GetTicks() < boostVisualUntil,
            SDL_GetTicks() < impactVisualUntil, winner, playerPosition,
            RaceModeUsesRivals(raceMode) ? rivalCount + 1 : 1,
            competitorPositions,
            raceMode == RaceMode::Championship, championship.Complete(),
            championship.CurrentEvent() + 1 >= championship.EventCount(),
            raceStart.LightsLit(), raceStart.CountdownActive(),
            !continueDriving,
            championshipOverlayPoints, race.Progress().mElapsedSeconds, lapTimer.Timing(),
            playerCraftClass, missile,
            weaponsAllowed, pauseMenuOpen, pauseMenuSelection,
            controller != nullptr, steeringAssistEnabled, brakingAssistEnabled,
            cameraDistanceSetting,
            static_cast<int>(audioFeedback.MenuVolume() * 100.0 + 0.5),
            static_cast<int>(audioFeedback.RaceVolume() * 100.0 + 0.5),
            resultSelection,
            drawableWidth, drawableHeight);
        if (raceMode == RaceMode::Practice && !pauseMenuOpen)
            DrawPracticeGuide(practiceGuide, controller != nullptr, drawableWidth, drawableHeight);
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
