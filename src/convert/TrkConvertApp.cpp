// SPDX-License-Identifier: MIT OR Apache-2.0
// OpenHoverTrkConvert: converts a room-based track file from another hover racing game into an
// OpenHover track, showing the layout it read and the track it will write. Only convert tracks you
// have the rights to.
//
//   OpenHoverTrkConvert [file]            open the window (or drop a file onto it)
//   OpenHoverTrkConvert file --out out.ohtrack [--name NAME] [--exclude N ...] [--reverse]
//                                         convert without a window and write the track
#include <SDL.h>
#include <SDL_opengl.h>

#include "PixelFont.h"
#include "TrackFile.h"
#include "TrackHash.h"
#include "TrackLoader.h"
#include "TrkConverter.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
std::string ReadFileBytes(const std::string& pPath, bool& pOk)
{
    std::ifstream file(pPath, std::ios::binary);
    pOk = static_cast<bool>(file);
    std::ostringstream bytes;
    if (file)
        bytes << file.rdbuf();
    return bytes.str();
}

std::string Upper(std::string pText)
{
    for (char& c : pText)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return pText;
}

struct AppState
{
    std::string mPath;
    TrkTrack mTrack;
    bool mLoaded = false;
    int mHover = -1;
    int mPressed = -1;
    std::string mLoadProblem;
    TrkConvertOptions mOptions;
    TrkConversion mConversion;
    std::string mMessage;
    bool mNaming = false;
};

void Reconvert(AppState& pState)
{
    if (pState.mLoaded)
        pState.mConversion = ConvertTrk(pState.mTrack, pState.mOptions);
}

void LoadFile(AppState& pState, const std::string& pPath)
{
    bool readable = false;
    const std::string bytes = ReadFileBytes(pPath, readable);
    pState.mPath = pPath;
    pState.mLoaded = false;
    pState.mMessage.clear();
    pState.mConversion = TrkConversion();
    if (!readable)
    {
        pState.mLoadProblem = "CANNOT READ THAT FILE";
        return;
    }
    TrkTrack track;
    const std::string problem = ReadTrk(bytes, track);
    if (!problem.empty())
    {
        pState.mLoadProblem = Upper(problem);
        return;
    }
    pState.mTrack = track;
    pState.mLoaded = true;
    pState.mLoadProblem.clear();
    pState.mOptions = TrkConvertOptions();
    pState.mOptions.mName = TrkTrackNameFromFile(pPath);
    Reconvert(pState);
}

// Writes the track into the game's own tracks folder so it appears in the track list.
std::string SaveToGame(const AppState& pState)
{
    if (!pState.mConversion.mBuilt.mOk)
        return "THERE IS NO VALID TRACK TO SAVE YET";
    char* prefix = SDL_GetPrefPath("OpenHover", "OpenHover");
    if (prefix == nullptr)
        return "CANNOT FIND THE GAME'S TRACKS FOLDER";
    const std::string directory = std::string(prefix) + "tracks";
    SDL_free(prefix);
    if (!EnsureDirectory(directory))
        return "CANNOT CREATE THE TRACKS FOLDER";
    const TrackDefinition& track = pState.mConversion.mBuilt.mTrack;
    const std::string path = directory + "/" + track.mId + ".ohtrack";
    std::ofstream file(path, std::ios::binary);
    file << SerializeTrack(track);
    file.close();
    // The full path, so it is clear where the track went.
    return file ? "SAVED " + Upper(path) : "COULD NOT WRITE THE FILE";
}

// ---- drawing ----

struct View
{
    double mMinX = 0.0, mMaxX = 1.0, mMinY = 0.0, mMaxY = 1.0;
    int mLeft = 0, mTop = 0, mSize = 100;
    double mScale = 1.0;
    int ScreenX(double pX) const { return mLeft + static_cast<int>((pX - mMinX) * mScale); }
    int ScreenY(double pY) const { return mTop + mSize - static_cast<int>((pY - mMinY) * mScale); }
    double WorldX(int pX) const { return (pX - mLeft) / mScale + mMinX; }
    double WorldY(int pY) const { return (mTop + mSize - pY) / mScale + mMinY; }
};

View FitView(const AppState& pState, int pWidth, int pHeight)
{
    View view;
    view.mLeft = 16;
    view.mTop = 16;
    view.mSize = std::max(200, std::min(pHeight - 32, pWidth - 360));
    bool first = true;
    const auto grow = [&](double pX, double pY)
    {
        if (first)
        {
            view.mMinX = view.mMaxX = pX;
            view.mMinY = view.mMaxY = pY;
            first = false;
            return;
        }
        view.mMinX = std::min(view.mMinX, pX);
        view.mMaxX = std::max(view.mMaxX, pX);
        view.mMinY = std::min(view.mMinY, pY);
        view.mMaxY = std::max(view.mMaxY, pY);
    };
    for (const TrkRoom& room : pState.mTrack.mRooms)
        for (const std::pair<double, double>& corner : room.mCorners)
            grow(corner.first, corner.second);
    const double span = std::max(1.0, std::max(view.mMaxX - view.mMinX, view.mMaxY - view.mMinY));
    view.mScale = (view.mSize - 20.0) / span;
    // Centre the drawing in the square.
    const double usedX = (view.mMaxX - view.mMinX) * view.mScale;
    const double usedY = (view.mMaxY - view.mMinY) * view.mScale;
    view.mMinX -= (view.mSize - 20.0 - usedX) * 0.5 / view.mScale + 10.0 / view.mScale;
    view.mMinY -= (view.mSize - 20.0 - usedY) * 0.5 / view.mScale + 10.0 / view.mScale;
    return view;
}

// The buttons down the bottom of the side panel.
enum Button
{
    kSave,
    kFlip,
    kAllRooms,
    kHeights,
    kPads,
    kMines,
    kHazards,
    kRename,
    kButtonCount
};

void ButtonRect(const View& pView, int pWidth, int pHeight, int pButton, int& pLeft, int& pTop,
                int& pButtonWidth, int& pButtonHeight)
{
    pLeft = pView.mLeft + pView.mSize + 24;
    pButtonWidth = std::max(180, pWidth - pLeft - 16);
    pButtonHeight = 28;
    pTop = pHeight - 16 - (kButtonCount - pButton) * 34 + 6;
}

const char* ButtonLabel(const AppState& pState, int pButton)
{
    switch (pButton)
    {
    case kSave: return "SAVE TRACK TO THE GAME";
    case kFlip: return "DRIVE THE OTHER WAY";
    case kAllRooms: return "SWITCH ALL ROOMS ON";
    case kHeights: return pState.mOptions.mHeights ? "GROUND HEIGHTS  ON" : "GROUND HEIGHTS  OFF";
    case kPads: return pState.mOptions.mPads ? "BOOST PADS  ON" : "BOOST PADS  OFF";
    case kMines: return pState.mOptions.mMines ? "MINES  ON" : "MINES  OFF";
    case kHazards: return pState.mOptions.mHazards ? "HAZARD ZONES  ON" : "HAZARD ZONES  OFF";
    default: return "CHANGE THE NAME";
    }
}

void Square(int pX, int pY, int pHalf)
{
    glBegin(GL_QUADS);
    glVertex2i(pX - pHalf, pY - pHalf);
    glVertex2i(pX + pHalf, pY - pHalf);
    glVertex2i(pX + pHalf, pY + pHalf);
    glVertex2i(pX - pHalf, pY + pHalf);
    glEnd();
}

void DrawScene(const AppState& pState, const View& pView, int pWidth, int pHeight)
{
    glClearColor(0.045f, 0.05f, 0.065f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, pWidth, pHeight, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);

    // Canvas.
    glColor3f(0.02f, 0.03f, 0.04f);
    glBegin(GL_QUADS);
    glVertex2i(pView.mLeft, pView.mTop);
    glVertex2i(pView.mLeft + pView.mSize, pView.mTop);
    glVertex2i(pView.mLeft + pView.mSize, pView.mTop + pView.mSize);
    glVertex2i(pView.mLeft, pView.mTop + pView.mSize);
    glEnd();

    const int panelLeft = pView.mLeft + pView.mSize + 24;
    int line = 20;
    const auto text = [&](const std::string& pText, float pR, float pG, float pB, int pScale = 2)
    {
        glColor3f(pR, pG, pB);
        DrawPixelText(pText.c_str(), panelLeft, line, pScale);
        line += pScale * 9 + 6;
    };
    glColor3f(0.2f, 0.9f, 1.0f);
    DrawPixelText("TRACK CONVERTER", panelLeft, line, 3);
    line += 40;

    if (!pState.mLoaded)
    {
        text(pState.mLoadProblem.empty() ? "DROP A TRACK FILE ONTO THIS WINDOW" : pState.mLoadProblem,
             pState.mLoadProblem.empty() ? 0.9f : 1.0f, pState.mLoadProblem.empty() ? 0.9f : 0.4f,
             pState.mLoadProblem.empty() ? 0.9f : 0.3f);
        text("ONLY CONVERT TRACKS YOU HAVE", 0.7f, 0.75f, 0.8f);
        text("THE RIGHTS TO", 0.7f, 0.75f, 0.8f);
        return;
    }

    const TrkConversion& conversion = pState.mConversion;
    // Rooms.
    double lowest = 1e18;
    double highest = -1e18;
    for (const TrkRoom& room : pState.mTrack.mRooms)
    {
        lowest = std::min(lowest, room.mFloor);
        highest = std::max(highest, room.mFloor);
    }
    std::vector<char> onRoute(pState.mTrack.mRooms.size(), 0);
    for (int room : conversion.mCycle)
        onRoute[static_cast<std::size_t>(room)] = 1;
    for (std::size_t index = 0; index < pState.mTrack.mRooms.size(); ++index)
    {
        const TrkRoom& room = pState.mTrack.mRooms[index];
        const bool excluded = pState.mOptions.mExcluded.count(static_cast<int>(index)) != 0;
        const double t = highest > lowest ? (room.mFloor - lowest) / (highest - lowest) : 0.0;
        float r = static_cast<float>(0.16 + 0.5 * t);
        float g = static_cast<float>(0.32 - 0.12 * t);
        float b = static_cast<float>(0.42 - 0.2 * t);
        if (excluded)
        {
            r = g = b = 0.1f;
        }
        else if (!onRoute[index])
        {
            r *= 0.55f;
            g *= 0.55f;
            b *= 0.55f;
        }
        glColor3f(r, g, b);
        glBegin(GL_POLYGON);
        for (const std::pair<double, double>& corner : room.mCorners)
            glVertex2i(pView.ScreenX(corner.first), pView.ScreenY(corner.second));
        glEnd();
        glColor3f(0.5f, 0.55f, 0.6f);
        glBegin(GL_LINE_LOOP);
        for (const std::pair<double, double>& corner : room.mCorners)
            glVertex2i(pView.ScreenX(corner.first), pView.ScreenY(corner.second));
        glEnd();
    }
    // Room numbers.
    for (std::size_t index = 0; index < pState.mTrack.mRooms.size(); ++index)
    {
        double cx = 0.0;
        double cy = 0.0;
        for (const std::pair<double, double>& corner : pState.mTrack.mRooms[index].mCorners)
        {
            cx += corner.first;
            cy += corner.second;
        }
        cx /= static_cast<double>(pState.mTrack.mRooms[index].mCorners.size());
        cy /= static_cast<double>(pState.mTrack.mRooms[index].mCorners.size());
        const std::string label = std::to_string(index);
        glColor3f(0.85f, 0.88f, 0.9f);
        DrawPixelText(label.c_str(), pView.ScreenX(cx) - PixelTextWidth(label, 1) / 2, pView.ScreenY(cy) - 3, 1);
    }
    // The road the new track will have: a translucent band of the real width along every leg, so
    // roads that overlap or run too close together are easy to see.
    if (conversion.mBuilt.mOk)
    {
        const std::vector<RaceGate>& route = conversion.mBuilt.mTrack.mWaypoints;
        const double halfWidth = conversion.mBuilt.mTrack.mRoadHalfWidth;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.1f, 0.85f, 0.95f, 0.22f);
        glBegin(GL_QUADS);
        for (std::size_t index = 0; index < route.size(); ++index)
        {
            const RaceGate& a = route[index];
            const RaceGate& b = route[(index + 1) % route.size()];
            const double length = std::hypot(b.mX - a.mX, b.mY - a.mY);
            if (length <= 0.0)
                continue;
            const double nx = -(b.mY - a.mY) / length * halfWidth;
            const double ny = (b.mX - a.mX) / length * halfWidth;
            glVertex2i(pView.ScreenX(a.mX + nx), pView.ScreenY(a.mY + ny));
            glVertex2i(pView.ScreenX(a.mX - nx), pView.ScreenY(a.mY - ny));
            glVertex2i(pView.ScreenX(b.mX - nx), pView.ScreenY(b.mY - ny));
            glVertex2i(pView.ScreenX(b.mX + nx), pView.ScreenY(b.mY + ny));
        }
        glEnd();
        glDisable(GL_BLEND);
    }
    // The loop through the rooms.
    glColor3f(0.2f, 0.9f, 1.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    for (const EditorPoint& point : conversion.mRoute)
        glVertex2i(pView.ScreenX(point.mX), pView.ScreenY(point.mY));
    glEnd();
    glLineWidth(1.0f);
    for (const EditorPoint& point : conversion.mRoute)
    {
        glColor3f(0.95f, 0.95f, 0.95f);
        Square(pView.ScreenX(point.mX), pView.ScreenY(point.mY), 2);
    }
    // Start positions.
    glColor3f(0.2f, 1.0f, 0.4f);
    for (const TrkStart& start : pState.mTrack.mStarts)
        Square(pView.ScreenX(start.mX), pView.ScreenY(start.mY), 2);
    // What the built track adds.
    if (conversion.mBuilt.mOk)
    {
        const TrackDefinition& track = conversion.mBuilt.mTrack;
        glColor3f(1.0f, 0.86f, 0.1f);
        for (const RaceGate& gate : track.mCheckpoints)
            Square(pView.ScreenX(gate.mX), pView.ScreenY(gate.mY), 5);
        glColor3f(0.4f, 0.8f, 1.0f);
        for (const BoostPad& pad : track.mBoostPads)
            Square(pView.ScreenX(pad.mX), pView.ScreenY(pad.mY), 3);
        glColor3f(1.0f, 0.15f, 0.15f);
        for (const Mine& mine : track.mMines)
            Square(pView.ScreenX(mine.mX), pView.ScreenY(mine.mY), 3);
        glColor3f(0.7f, 0.3f, 1.0f);
        for (const HazardZone& zone : track.mHazardZones)
            Square(pView.ScreenX(zone.mX), pView.ScreenY(zone.mY), 4);
        glColor3f(1.0f, 0.5f, 0.15f);
        for (const RaisedSection& section : track.mRaisedSections)
            Square(pView.ScreenX(section.mX), pView.ScreenY(section.mY), 6);
        glColor3f(0.2f, 1.0f, 0.4f);
        Square(pView.ScreenX(track.mWaypoints.front().mX), pView.ScreenY(track.mWaypoints.front().mY), 7);
        // An arrow from the start along the direction of travel.
        const RaceGate& from = track.mWaypoints[0];
        const RaceGate& to = track.mWaypoints[1 % track.mWaypoints.size()];
        const double length = std::hypot(to.mX - from.mX, to.mY - from.mY);
        if (length > 0.0)
        {
            const double dx = (to.mX - from.mX) / length;
            const double dy = (to.mY - from.mY) / length;
            const int tipX = pView.ScreenX(from.mX + dx * 24.0);
            const int tipY = pView.ScreenY(from.mY + dy * 24.0);
            const int sideX = static_cast<int>(-dy * 8.0);
            const int sideY = static_cast<int>(dx * 8.0);
            const int baseX = pView.ScreenX(from.mX + dx * 12.0);
            const int baseY = pView.ScreenY(from.mY + dy * 12.0);
            glBegin(GL_TRIANGLES);
            glVertex2i(tipX, tipY);
            glVertex2i(baseX + sideX, baseY - sideY);
            glVertex2i(baseX - sideX, baseY + sideY);
            glEnd();
        }
    }

    // Side panel.
    text(Upper(pState.mOptions.mName.empty() ? std::string("NO NAME") : pState.mOptions.mName)
             + (pState.mNaming ? "_" : ""),
         1.0f, 0.86f, 0.1f, 3);
    text(std::to_string(pState.mTrack.mRooms.size()) + " ROOMS  " + std::to_string(conversion.mLinks.size())
             + " LINKS", 0.82f, 0.9f, 0.92f);
    text(std::to_string(conversion.mCycle.size()) + " ROOMS ON THE ROUTE  "
             + std::to_string(conversion.mRoute.size()) + " POINTS", 0.82f, 0.9f, 0.92f);
    text("ROAD WIDTH " + std::to_string(static_cast<int>(conversion.mHalfWidth * 2.0)) + " M", 0.82f, 0.9f, 0.92f);
    if (!conversion.mHeights.empty())
    {
        text("GROUND  BIGGEST DROP " + std::to_string(static_cast<int>(conversion.mBiggestDrop + 0.5))
                 + " M  STEP UP " + std::to_string(static_cast<int>(conversion.mBiggestRise + 0.5)) + " M",
             0.82f, 0.9f, 0.92f);
    }
    line += 6;
    if (conversion.mBuilt.mOk)
    {
        double length = 0.0;
        const std::vector<RaceGate>& route = conversion.mBuilt.mTrack.mWaypoints;
        for (std::size_t index = 0; index < route.size(); ++index)
            length += std::hypot(route[(index + 1) % route.size()].mX - route[index].mX,
                                 route[(index + 1) % route.size()].mY - route[index].mY);
        text("TRACK OK  " + std::to_string(static_cast<int>(length)) + " M  "
                 + std::to_string(static_cast<int>(length / 30.0 + 0.5)) + " S A LAP", 0.18f, 0.96f, 0.4f);
    }
    else
        text(conversion.mProblem.empty() ? std::string("NO TRACK YET") : conversion.mProblem, 1.0f, 0.4f, 0.3f);
    for (const std::string& warning : conversion.mWarnings)
        text(warning, 1.0f, 0.65f, 0.25f, 1);
    line += 10;
    text("CLICK A ROOM  SWITCH IT ON OR OFF", 0.62f, 0.7f, 0.76f, 1);
    text("KEYS  S SAVE  F FLIP  R ROOMS  N NAME", 0.62f, 0.7f, 0.76f, 1);
    text("      G GROUND  P PADS  M MINES  H HAZARDS", 0.62f, 0.7f, 0.76f, 1);
    text("DROP ANOTHER FILE TO OPEN IT", 0.62f, 0.7f, 0.76f, 1);
    line += 6;
    text("LIGHT BLUE LOOP  THE ROUTE", 0.2f, 0.9f, 1.0f, 1);
    text("GREEN  START AND DIRECTION  YELLOW  GATE", 0.2f, 1.0f, 0.4f, 1);
    text("GREEN DOTS  THE ORIGINAL START", 0.2f, 1.0f, 0.4f, 1);
    text("ORANGE  BRIDGE  BLUE  PAD", 1.0f, 0.5f, 0.15f, 1);
    text("RED  MINE  PURPLE  HAZARD ZONE", 1.0f, 0.3f, 0.5f, 1);
    // Buttons.
    for (int button = 0; button < kButtonCount; ++button)
    {
        int left = 0;
        int top = 0;
        int width = 0;
        int height = 0;
        ButtonRect(pView, pWidth, pHeight, button, left, top, width, height);
        const bool saveReady = button != kSave || conversion.mBuilt.mOk;
        const bool active = button == kRename && pState.mNaming;
        const bool hot = saveReady && pState.mHover == button;
        const bool down = hot && pState.mPressed == button;
        const bool lit = active || hot;
        if (down)
            glColor3f(0.2f, 0.7f, 0.82f);
        else if (lit)
            glColor3f(0.12f, 0.52f, 0.62f);
        else
            glColor3f(button == kSave && saveReady ? 0.1f : 0.06f,
                      button == kSave && saveReady ? 0.34f : 0.18f,
                      button == kSave && saveReady ? 0.2f : 0.24f);
        glBegin(GL_QUADS);
        glVertex2i(left, top);
        glVertex2i(left + width, top);
        glVertex2i(left + width, top + height);
        glVertex2i(left, top + height);
        glEnd();
        if (lit)
            glColor3f(1.0f, 0.82f, 0.22f);
        else
            glColor3f(saveReady ? 0.82f : 0.45f, saveReady ? 0.9f : 0.5f, saveReady ? 0.92f : 0.55f);
        DrawPixelText(ButtonLabel(pState, button), left + 14 + (down ? 1 : 0), top + 7 + (down ? 1 : 0), 2);
    }
    if (!pState.mMessage.empty())
    {
        line += 8;
        // Long messages (a saved path) wrap to the width of the panel.
        const std::size_t perLine = static_cast<std::size_t>(std::max(20, (pWidth - panelLeft - 16) / 6));
        for (std::size_t from = 0; from < pState.mMessage.size(); from += perLine)
            text(pState.mMessage.substr(from, perLine), 1.0f, 0.86f, 0.1f, 1);
    }
}

int RunHeadless(const std::string& pInput, const std::string& pOutput, const TrkConvertOptions& pOptions)
{
    AppState state;
    LoadFile(state, pInput);
    if (!state.mLoaded)
    {
        std::cerr << pInput << ": " << state.mLoadProblem << "\n";
        return 1;
    }
    state.mOptions = pOptions;
    if (state.mOptions.mName.empty())
        state.mOptions.mName = TrkTrackNameFromFile(pInput);
    Reconvert(state);
    for (const std::string& warning : state.mConversion.mWarnings)
        std::cerr << "warning: " << warning << "\n";
    if (!state.mConversion.mBuilt.mOk)
    {
        std::cerr << pInput << ": " << state.mConversion.mProblem << "\n";
        return 1;
    }
    std::ofstream file(pOutput, std::ios::binary);
    file << SerializeTrack(state.mConversion.mBuilt.mTrack);
    file.close();
    if (!file)
    {
        std::cerr << "could not write " << pOutput << "\n";
        return 1;
    }
    std::cout << pOutput << ": " << state.mConversion.mCycle.size() << " rooms on the route, "
              << state.mConversion.mRoute.size() << " points\n";
    return 0;
}
}

int main(int pArgumentCount, char* pArguments[])
{
    std::string input;
    std::string output;
    TrkConvertOptions options;
    for (int index = 1; index < pArgumentCount; ++index)
    {
        const std::string argument = pArguments[index];
        if (argument == "--out" && index + 1 < pArgumentCount)
            output = pArguments[++index];
        else if (argument == "--name" && index + 1 < pArgumentCount)
            options.mName = pArguments[++index];
        else if (argument == "--exclude" && index + 1 < pArgumentCount)
            options.mExcluded.insert(std::atoi(pArguments[++index]));
        else if (argument == "--reverse")
            options.mReverse = true;
        else if (argument.compare(0, 2, "--") != 0 && input.empty())
            input = argument;
        else
        {
            std::cerr << "usage: OpenHoverTrkConvert [file] [--out FILE.ohtrack] [--name NAME] "
                         "[--exclude ROOM]... [--reverse]\n";
            return 2;
        }
    }
    if (!output.empty())
    {
        if (input.empty())
        {
            std::cerr << "--out needs an input file\n";
            return 2;
        }
        return RunHeadless(input, output, options);
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cerr << "could not start SDL: " << SDL_GetError() << "\n";
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_Window* window = SDL_CreateWindow("OpenHover Track Converter", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, 1280, 720,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_GLContext context = window == nullptr ? nullptr : SDL_GL_CreateContext(window);
    if (context == nullptr)
    {
        std::cerr << "could not create a window: " << SDL_GetError() << "\n";
        return 1;
    }
    SDL_GL_SetSwapInterval(1);

    AppState state;
    if (!input.empty())
    {
        LoadFile(state, input);
        if (state.mLoaded)
        {
            const std::string chosen = options.mName;
            state.mOptions = options;
            if (state.mOptions.mName.empty())
                state.mOptions.mName = chosen.empty() ? TrkTrackNameFromFile(input) : chosen;
            Reconvert(state);
        }
    }

    bool running = true;
    while (running)
    {
        int width = 0;
        int height = 0;
        SDL_GL_GetDrawableSize(window, &width, &height);
        int windowWidth = 0;
        int windowHeight = 0;
        SDL_GetWindowSize(window, &windowWidth, &windowHeight);
        const View view = FitView(state, width, height);
        const auto buttonAt = [&](int pMouseX, int pMouseY) {
            for (int button = 0; button < kButtonCount; ++button)
            {
                int bx = 0;
                int by = 0;
                int bw = 0;
                int bh = 0;
                ButtonRect(view, width, height, button, bx, by, bw, bh);
                if (pMouseX >= bx && pMouseX < bx + bw && pMouseY >= by && pMouseY < by + bh)
                    return button;
            }
            return -1;
        };
        {
            int hoverX = 0;
            int hoverY = 0;
            SDL_GetMouseState(&hoverX, &hoverY);
            state.mHover = state.mLoaded ? buttonAt(hoverX * width / std::max(1, windowWidth),
                                                    hoverY * height / std::max(1, windowHeight))
                                         : -1;
        }
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0)
        {
            if (event.type == SDL_QUIT)
                running = false;
            else if (event.type == SDL_DROPFILE)
            {
                LoadFile(state, event.drop.file);
                SDL_free(event.drop.file);
            }
            else if (event.type == SDL_TEXTINPUT && state.mNaming)
            {
                for (const char* c = event.text.text; *c != '\0'; ++c)
                {
                    const bool allowed = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z')
                        || (*c >= '0' && *c <= '9') || *c == ' ' || *c == '.' || *c == '_' || *c == '-';
                    if (allowed && state.mOptions.mName.size() < 24)
                        state.mOptions.mName += *c;
                }
                Reconvert(state);
            }
            else if (event.type == SDL_KEYDOWN)
            {
                const SDL_Keycode key = event.key.keysym.sym;
                if (state.mNaming)
                {
                    if (key == SDLK_BACKSPACE && !state.mOptions.mName.empty())
                        state.mOptions.mName.pop_back();
                    else if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_ESCAPE)
                    {
                        state.mNaming = false;
                        SDL_StopTextInput();
                    }
                    Reconvert(state);
                }
                else if (key == SDLK_ESCAPE)
                    running = false;
                else if (state.mLoaded && key == SDLK_f)
                {
                    state.mOptions.mReverse = !state.mOptions.mReverse;
                    Reconvert(state);
                }
                else if (state.mLoaded && key == SDLK_r)
                {
                    state.mOptions.mExcluded.clear();
                    Reconvert(state);
                }
                else if (state.mLoaded && key == SDLK_n)
                {
                    state.mNaming = true;
                    SDL_StartTextInput();
                }
                else if (state.mLoaded && key == SDLK_g)
                {
                    state.mOptions.mHeights = !state.mOptions.mHeights;
                    Reconvert(state);
                }
                else if (state.mLoaded && key == SDLK_p)
                {
                    state.mOptions.mPads = !state.mOptions.mPads;
                    Reconvert(state);
                }
                else if (state.mLoaded && key == SDLK_m)
                {
                    state.mOptions.mMines = !state.mOptions.mMines;
                    Reconvert(state);
                }
                else if (state.mLoaded && key == SDLK_h)
                {
                    state.mOptions.mHazards = !state.mOptions.mHazards;
                    Reconvert(state);
                }
                else if (state.mLoaded && key == SDLK_s)
                    state.mMessage = SaveToGame(state);
            }
            else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT)
                state.mPressed = -1;
            else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && state.mLoaded)
            {
                const int mouseX = event.button.x * width / std::max(1, windowWidth);
                const int mouseY = event.button.y * height / std::max(1, windowHeight);
                const int pressed = buttonAt(mouseX, mouseY);
                state.mPressed = pressed;
                if (pressed == kSave)
                    state.mMessage = SaveToGame(state);
                else if (pressed == kFlip)
                {
                    state.mOptions.mReverse = !state.mOptions.mReverse;
                    Reconvert(state);
                }
                else if (pressed == kAllRooms)
                {
                    state.mOptions.mExcluded.clear();
                    Reconvert(state);
                }
                else if (pressed == kHeights || pressed == kPads || pressed == kMines || pressed == kHazards)
                {
                    bool& flag = pressed == kHeights ? state.mOptions.mHeights : pressed == kPads ? state.mOptions.mPads
                        : pressed == kMines ? state.mOptions.mMines : state.mOptions.mHazards;
                    flag = !flag;
                    Reconvert(state);
                }
                else if (pressed == kRename)
                {
                    state.mNaming = !state.mNaming;
                    if (state.mNaming)
                        SDL_StartTextInput();
                    else
                        SDL_StopTextInput();
                }
                if (pressed >= 0)
                    continue;
                const double worldX = view.WorldX(mouseX);
                const double worldY = view.WorldY(mouseY);
                for (std::size_t room = 0; room < state.mTrack.mRooms.size(); ++room)
                {
                    TrkRoom probe = state.mTrack.mRooms[room];
                    // Point-in-polygon by the crossing-number test.
                    bool inside = false;
                    const std::size_t count = probe.mCorners.size();
                    for (std::size_t i = 0, j = count - 1; i < count; j = i++)
                    {
                        const double xi = probe.mCorners[i].first, yi = probe.mCorners[i].second;
                        const double xj = probe.mCorners[j].first, yj = probe.mCorners[j].second;
                        if (((yi > worldY) != (yj > worldY)) && (worldX < (xj - xi) * (worldY - yi) / (yj - yi) + xi))
                            inside = !inside;
                    }
                    if (inside)
                    {
                        const int index = static_cast<int>(room);
                        if (!state.mOptions.mExcluded.erase(index))
                            state.mOptions.mExcluded.insert(index);
                        Reconvert(state);
                        state.mMessage.clear();
                        break;
                    }
                }
            }
        }
        DrawScene(state, view, width, height);
        SDL_GL_SwapWindow(window);
    }
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
