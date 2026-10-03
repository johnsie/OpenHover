// SPDX-License-Identifier: MIT OR Apache-2.0
#include "TrkConverter.h"

#include <algorithm>
#include <cmath>

namespace
{
struct Point
{
    double mX = 0.0;
    double mY = 0.0;
};

double Distance(const Point& pA, const Point& pB)
{
    return std::hypot(pA.mX - pB.mX, pA.mY - pB.mY);
}

Point Centroid(const TrkRoom& pRoom)
{
    Point centre;
    for (const std::pair<double, double>& corner : pRoom.mCorners)
    {
        centre.mX += corner.first;
        centre.mY += corner.second;
    }
    centre.mX /= static_cast<double>(pRoom.mCorners.size());
    centre.mY /= static_cast<double>(pRoom.mCorners.size());
    return centre;
}

bool Contains(const TrkRoom& pRoom, const Point& pPoint)
{
    bool inside = false;
    const std::size_t count = pRoom.mCorners.size();
    for (std::size_t index = 0, previous = count - 1; index < count; previous = index++)
    {
        const double xi = pRoom.mCorners[index].first;
        const double yi = pRoom.mCorners[index].second;
        const double xj = pRoom.mCorners[previous].first;
        const double yj = pRoom.mCorners[previous].second;
        if (((yi > pPoint.mY) != (yj > pPoint.mY))
            && (pPoint.mX < (xj - xi) * (pPoint.mY - yi) / (yj - yi) + xi))
            inside = !inside;
    }
    return inside;
}

// The shared stretch of two rooms' edges, if any: collinear within a few centimetres and overlapping
// by at least a metre.
bool SharedStretch(const TrkRoom& pFirst, const TrkRoom& pSecond, Point& pMid, double& pWidth)
{
    bool found = false;
    pWidth = 0.0;
    const std::size_t firstCount = pFirst.mCorners.size();
    const std::size_t secondCount = pSecond.mCorners.size();
    for (std::size_t i = 0; i < firstCount; ++i)
    {
        const Point a = {pFirst.mCorners[i].first, pFirst.mCorners[i].second};
        const Point b = {pFirst.mCorners[(i + 1) % firstCount].first, pFirst.mCorners[(i + 1) % firstCount].second};
        const double length = Distance(a, b);
        if (length < 1.0)
            continue;
        const double directionX = (b.mX - a.mX) / length;
        const double directionY = (b.mY - a.mY) / length;
        for (std::size_t j = 0; j < secondCount; ++j)
        {
            const Point c = {pSecond.mCorners[j].first, pSecond.mCorners[j].second};
            const Point d = {pSecond.mCorners[(j + 1) % secondCount].first,
                             pSecond.mCorners[(j + 1) % secondCount].second};
            const double offsetC = std::fabs((c.mX - a.mX) * directionY - (c.mY - a.mY) * directionX);
            const double offsetD = std::fabs((d.mX - a.mX) * directionY - (d.mY - a.mY) * directionX);
            if (offsetC > 0.25 || offsetD > 0.25)
                continue;
            const double tc = (c.mX - a.mX) * directionX + (c.mY - a.mY) * directionY;
            const double td = (d.mX - a.mX) * directionX + (d.mY - a.mY) * directionY;
            const double from = std::max(0.0, std::min(tc, td));
            const double to = std::min(length, std::max(tc, td));
            if (to - from >= 1.0 && to - from > pWidth)
            {
                pWidth = to - from;
                const double middle = (from + to) * 0.5;
                pMid.mX = a.mX + directionX * middle;
                pMid.mY = a.mY + directionY * middle;
                found = true;
            }
        }
    }
    return found;
}

struct CycleSearch
{
    const std::vector<std::vector<int>>* mNeighbours = nullptr;
    const std::vector<Point>* mCentres = nullptr;
    int mStart = 0;
    long mBudget = 3000000;
    std::vector<int> mPath;
    std::vector<char> mOnPath;
    std::vector<int> mBest;
    double mBestLength = 0.0;
    double mLength = 0.0;

    void Visit(int pRoom)
    {
        if (mBudget-- <= 0)
            return;
        for (int next : (*mNeighbours)[pRoom])
        {
            if (next == mStart && mPath.size() >= 3)
            {
                const double total = mLength + Distance((*mCentres)[pRoom], (*mCentres)[mStart]);
                if (total > mBestLength)
                {
                    mBestLength = total;
                    mBest = mPath;
                }
                continue;
            }
            if (mOnPath[next] || mPath.size() >= 120)
                continue;
            // No sharp reversals: stepping back the way we came is how a search would otherwise
            // zig-zag between side-by-side lanes to make a loop look longer.
            if (mPath.size() >= 2)
            {
                const Point& before = (*mCentres)[mPath[mPath.size() - 2]];
                const Point& here = (*mCentres)[pRoom];
                const Point& there = (*mCentres)[next];
                const double ax = here.mX - before.mX;
                const double ay = here.mY - before.mY;
                const double bx = there.mX - here.mX;
                const double by = there.mY - here.mY;
                const double lengths = std::hypot(ax, ay) * std::hypot(bx, by);
                if (lengths > 0.0 && (ax * bx + ay * by) / lengths < -0.5)
                    continue;
            }
            const double step = Distance((*mCentres)[pRoom], (*mCentres)[next]);
            mOnPath[next] = 1;
            mPath.push_back(next);
            mLength += step;
            Visit(next);
            mLength -= step;
            mPath.pop_back();
            mOnPath[next] = 0;
        }
    }
};

double TurnDegrees(const Point& pBefore, const Point& pHere, const Point& pAfter)
{
    const double ax = pHere.mX - pBefore.mX;
    const double ay = pHere.mY - pBefore.mY;
    const double bx = pAfter.mX - pHere.mX;
    const double by = pAfter.mY - pHere.mY;
    const double lengths = std::hypot(ax, ay) * std::hypot(bx, by);
    if (lengths <= 0.0)
        return 0.0;
    double cosine = (ax * bx + ay * by) / lengths;
    cosine = std::max(-1.0, std::min(1.0, cosine));
    return std::acos(cosine) * 180.0 / 3.14159265358979323846;
}

// Distance between segments (a,b) and (c,d), by checking the four endpoints against the other
// segment (adequate here because it is only used to warn about roads running close together).
double PointToSegment(const Point& pPoint, const Point& pA, const Point& pB)
{
    const double dx = pB.mX - pA.mX;
    const double dy = pB.mY - pA.mY;
    const double lengthSquared = dx * dx + dy * dy;
    double t = lengthSquared > 0.0 ? ((pPoint.mX - pA.mX) * dx + (pPoint.mY - pA.mY) * dy) / lengthSquared : 0.0;
    t = std::max(0.0, std::min(1.0, t));
    return std::hypot(pPoint.mX - (pA.mX + dx * t), pPoint.mY - (pA.mY + dy * t));
}
}

std::string TrkTrackNameFromFile(const std::string& pPath)
{
    std::size_t slash = pPath.find_last_of("/\\");
    std::string stem = slash == std::string::npos ? pPath : pPath.substr(slash + 1);
    const std::size_t dot = stem.find_last_of('.');
    if (dot != std::string::npos)
        stem.resize(dot);
    const std::size_t bracket = stem.find('[');
    if (bracket != std::string::npos)
        stem.resize(bracket);
    std::string name;
    for (char c : stem)
    {
        const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        if (plain)
            name += c;
        else if (!name.empty() && name.back() != ' ')
            name += ' ';
    }
    while (!name.empty() && name.back() == ' ')
        name.pop_back();
    if (name.size() > 24)
    {
        name.resize(24);
        while (!name.empty() && name.back() == ' ')
            name.pop_back();
    }
    return name;
}

TrkConversion ConvertTrk(const TrkTrack& pTrack, const TrkConvertOptions& pOptions)
{
    TrkConversion result;
    const int roomCount = static_cast<int>(pTrack.mRooms.size());
    if (roomCount < 3)
    {
        result.mProblem = "THERE ARE NOT ENOUGH ROOMS TO MAKE A TRACK";
        return result;
    }
    std::vector<Point> centres;
    for (const TrkRoom& room : pTrack.mRooms)
        centres.push_back(Centroid(room));

    // Which rooms touch.
    for (int a = 0; a < roomCount; ++a)
    {
        for (int b = a + 1; b < roomCount; ++b)
        {
            Point middle;
            double width = 0.0;
            if (SharedStretch(pTrack.mRooms[a], pTrack.mRooms[b], middle, width))
            {
                TrkLink link;
                link.mA = a;
                link.mB = b;
                link.mMidX = middle.mX;
                link.mMidY = middle.mY;
                link.mWidth = width;
                result.mLinks.push_back(link);
            }
        }
    }

    // The room the first start position is in (the nearest room if it is in none).
    if (pTrack.mStarts.empty())
    {
        result.mProblem = "THE TRACK HAS NO STARTING POSITIONS";
        return result;
    }
    Point startAverage;
    for (const TrkStart& start : pTrack.mStarts)
    {
        startAverage.mX += start.mX;
        startAverage.mY += start.mY;
    }
    startAverage.mX /= static_cast<double>(pTrack.mStarts.size());
    startAverage.mY /= static_cast<double>(pTrack.mStarts.size());
    // The file says which room each start is in; trust that if it is a room we read, and check it
    // against the position.
    const int statedRoom = pTrack.mStarts.front().mRoom;
    if (statedRoom >= 0 && statedRoom < roomCount)
    {
        result.mStartRoom = statedRoom;
        if (!Contains(pTrack.mRooms[statedRoom], {pTrack.mStarts.front().mX, pTrack.mStarts.front().mY}))
            result.mWarnings.push_back("THE START IS NOT INSIDE THE ROOM THE FILE NAMES");
    }
    for (int room = 0; room < roomCount && result.mStartRoom < 0; ++room)
    {
        if (Contains(pTrack.mRooms[room], {pTrack.mStarts.front().mX, pTrack.mStarts.front().mY})
            || Contains(pTrack.mRooms[room], startAverage))
            result.mStartRoom = room;
    }
    if (result.mStartRoom < 0)
    {
        double best = 1e18;
        for (int room = 0; room < roomCount; ++room)
        {
            const double distance = Distance(centres[room], startAverage);
            if (distance < best)
            {
                best = distance;
                result.mStartRoom = room;
            }
        }
        result.mWarnings.push_back("THE START IS NOT INSIDE ANY ROOM - USING THE NEAREST ONE");
    }

    // A loop through the start room, over the rooms that have not been switched off.
    std::vector<std::vector<int>> neighbours(static_cast<std::size_t>(roomCount));
    for (const TrkLink& link : result.mLinks)
    {
        const bool aOff = pOptions.mExcluded.count(link.mA) != 0 && link.mA != result.mStartRoom;
        const bool bOff = pOptions.mExcluded.count(link.mB) != 0 && link.mB != result.mStartRoom;
        if (aOff || bOff)
            continue;
        neighbours[link.mA].push_back(link.mB);
        neighbours[link.mB].push_back(link.mA);
    }
    CycleSearch search;
    search.mNeighbours = &neighbours;
    search.mCentres = &centres;
    search.mStart = result.mStartRoom;
    search.mOnPath.assign(static_cast<std::size_t>(roomCount), 0);
    search.mPath.push_back(result.mStartRoom);
    search.mOnPath[result.mStartRoom] = 1;
    search.Visit(result.mStartRoom);
    if (search.mBest.empty())
    {
        result.mProblem = "NO LOOP THROUGH THE START ROOM - SWITCH ROOMS BACK ON";
        return result;
    }
    result.mCycle = search.mBest;
    if (pOptions.mReverse)
        std::reverse(result.mCycle.begin() + 1, result.mCycle.end());

    // Follow the loop: each room's middle, and the middle of the doorway to the next room.
    const auto portal = [&](int pFrom, int pTo, Point& pOut) -> bool
    {
        for (const TrkLink& link : result.mLinks)
        {
            if ((link.mA == pFrom && link.mB == pTo) || (link.mA == pTo && link.mB == pFrom))
            {
                pOut = {link.mMidX, link.mMidY};
                return true;
            }
        }
        return false;
    };
    std::vector<Point> raw;
    std::vector<double> rawHeights;
    std::vector<double> widths;
    std::vector<char> mustKeep;
    const double startFloor = pTrack.mRooms[result.mStartRoom].mFloor;
    // Each room's floor as a level, measured from the start room in steps of a quarter metre, so
    // rooms a hair apart are one level.
    const auto level = [&](int pRoom)
    {
        return std::floor((pTrack.mRooms[pRoom].mFloor - startFloor) / 0.25 + 0.5) * 0.25;
    };
    for (std::size_t index = 0; index < result.mCycle.size(); ++index)
    {
        const int room = result.mCycle[index];
        const int next = result.mCycle[(index + 1) % result.mCycle.size()];
        raw.push_back(centres[room]);
        rawHeights.push_back(level(room));
        mustKeep.push_back(0);
        Point door;
        if (portal(room, next, door))
        {
            // The ground steps at the doorway: the stretch that starts there is the next room's.
            raw.push_back(door);
            rawHeights.push_back(level(next));
            const bool steps = level(next) != level(room);
            mustKeep.back() = steps ? 1 : 0;
            mustKeep.push_back(steps ? 1 : 0);
            for (const TrkLink& link : result.mLinks)
            {
                if ((link.mA == room && link.mB == next) || (link.mA == next && link.mB == room))
                    widths.push_back(link.mWidth);
            }
        }
    }

    // Thin the points out: drop ones that are very close to the last kept point or on a straight
    // line, raising the thresholds until the route is a sensible size.
    std::vector<std::size_t> kept;
    double spacing = 8.0;
    double straightDegrees = 6.0;
    for (int attempt = 0; attempt < 20; ++attempt)
    {
        kept.clear();
        for (std::size_t index = 0; index < raw.size(); ++index)
        {
            if (!mustKeep[index] && !kept.empty() && Distance(raw[kept.back()], raw[index]) < spacing)
                continue;
            kept.push_back(index);
        }
        if (kept.size() >= 3 && !mustKeep[kept.back()] && Distance(raw[kept.front()], raw[kept.back()]) < spacing)
            kept.pop_back();
        std::vector<std::size_t> thinned;
        for (std::size_t index = 0; index < kept.size(); ++index)
        {
            const Point& before = raw[kept[(index + kept.size() - 1) % kept.size()]];
            const Point& after = raw[kept[(index + 1) % kept.size()]];
            if (index == 0 || mustKeep[kept[index]] || TurnDegrees(before, raw[kept[index]], after) >= straightDegrees)
                thinned.push_back(kept[index]);
        }
        kept = thinned;
        if (kept.size() <= 200)
            break;
        spacing *= 1.5;
        straightDegrees += 4.0;
    }
    for (const std::size_t index : kept)
    {
        result.mRoute.push_back({raw[index].mX, raw[index].mY});
        result.mHeights.push_back(rawHeights[index]);
    }

    // Ground heights: each stretch of route is as high as the floor it runs over, and the ground
    // steps where the floor does.
    {
        double lowestHeight = 1e18;
        double highestHeight = -1e18;
        for (const double height : result.mHeights)
        {
            lowestHeight = std::min(lowestHeight, height);
            highestHeight = std::max(highestHeight, height);
        }
        if (!pOptions.mHeights || highestHeight - lowestHeight < 0.25)
            result.mHeights.clear();
        else
        {
            for (double& height : result.mHeights)
                height = std::floor(height * 100.0 + 0.5) / 100.0;
            // A step up of more than a jump clears is a wall: a pit (a drop into a room that the road
            // carries on from at the old level) is a trap the player must jump over.
            const std::size_t size = result.mHeights.size();
            int walls = 0;
            for (std::size_t index = 0; index < size; ++index)
                if (result.mHeights[(index + 1) % size] - result.mHeights[index] > kMaximumStepUp)
                    ++walls;
            if (walls > 0)
                result.mWarnings.push_back(std::to_string(walls) + " PITS HAVE WALLS TOO HIGH TO CLIMB OUT OF - TRAPS");
        }
    }

    // Road width: a narrow quarter of the doorways, so the road fits the tightest parts.
    if (!widths.empty())
    {
        std::sort(widths.begin(), widths.end());
        const double narrow = widths[widths.size() / 4];
        result.mHalfWidth = std::max(4.0, std::min(14.0, std::floor(narrow * 0.5)));
    }

    // Warnings about what the conversion cannot carry over.
    double lowest = 1e18;
    double highest = -1e18;
    for (const TrkRoom& room : pTrack.mRooms)
    {
        lowest = std::min(lowest, room.mFloor);
        highest = std::max(highest, room.mFloor);
    }
    if (highest - lowest > 0.5 && !pOptions.mHeights)
        result.mWarnings.push_back("FLOOR HEIGHTS ARE SWITCHED OFF - THE TRACK IS FLAT");
    const int unused = roomCount - static_cast<int>(result.mCycle.size());
    if (unused > 0)
        result.mWarnings.push_back(std::to_string(unused) + " ROOMS ARE NOT ON THE ROUTE");
    // Parts of the route running close alongside each other make the roads overlap. Only stretches
    // well apart along the route count (a sharp corner always brings the road back near itself).
    bool crowded = false;
    const std::size_t routeCount = result.mRoute.size();
    std::vector<double> along(routeCount + 1, 0.0);
    for (std::size_t i = 0; i < routeCount; ++i)
    {
        const EditorPoint& from = result.mRoute[i];
        const EditorPoint& to = result.mRoute[(i + 1) % routeCount];
        along[i + 1] = along[i] + std::hypot(to.mX - from.mX, to.mY - from.mY);
    }
    const double loopLength = along[routeCount];
    for (std::size_t i = 0; i < routeCount && !crowded; ++i)
    {
        const Point a = {result.mRoute[i].mX, result.mRoute[i].mY};
        const Point b = {result.mRoute[(i + 1) % routeCount].mX, result.mRoute[(i + 1) % routeCount].mY};
        for (std::size_t j = i + 2; j < routeCount && !crowded; ++j)
        {
            const double apartAlong = std::min(along[j] - along[i], loopLength - (along[j] - along[i]));
            if (apartAlong < 100.0)
                continue;
            const Point c = {result.mRoute[j].mX, result.mRoute[j].mY};
            if (PointToSegment(c, a, b) < result.mHalfWidth * 1.6)
                crowded = true;
        }
    }
    if (crowded)
        result.mWarnings.push_back("PARTS OF THE ROUTE RUN CLOSE BESIDE EACH OTHER - SWITCH A LANE OFF");

    // The start goes on a route side that is long enough for the starting grid, preferring the one
    // nearest the original start. Each candidate side is tried in turn until one gives a valid
    // track; if the first choice is far from the original start, the player is told it has moved.
    {
        struct Side
        {
            int mIndex;
            double mLength;
            double mToStart;
        };
        std::vector<Side> sides;
        const std::size_t count = result.mRoute.size();
        for (std::size_t index = 0; index < count; ++index)
        {
            const Point a = {result.mRoute[index].mX, result.mRoute[index].mY};
            const Point b = {result.mRoute[(index + 1) % count].mX, result.mRoute[(index + 1) % count].mY};
            sides.push_back({static_cast<int>(index), Distance(a, b),
                             Distance({(a.mX + b.mX) * 0.5, (a.mY + b.mY) * 0.5}, startAverage)});
        }
        std::stable_sort(sides.begin(), sides.end(), [](const Side& pLeft, const Side& pRight)
        {
            const bool leftLong = pLeft.mLength >= 70.0;
            const bool rightLong = pRight.mLength >= 70.0;
            if (leftLong != rightLong)
                return leftLong;
            return leftLong ? pLeft.mToStart < pRight.mToStart : pLeft.mLength > pRight.mLength;
        });
        bool built = false;
        std::string firstProblem;
        for (int attempt = 0; attempt < 2 && !built; ++attempt)
        {
            if (attempt == 1)
            {
                // The sloping ground made every start fail (most likely two crossing roads at
                // different heights), so try again on the flat.
                if (result.mHeights.empty())
                    break;
                result.mHeights.clear();
                result.mWarnings.push_back("THE FLOOR HEIGHTS DID NOT FIT THE ROUTE - THE TRACK IS FLAT");
            }
            for (const Side& side : sides)
            {
                BuilderOptions builder;
                builder.mStartSide = side.mIndex;
                builder.mPads = pOptions.mPads;
                builder.mMines = pOptions.mMines;
                builder.mHazards = pOptions.mHazards;
                builder.mHeights = result.mHeights;
                result.mBuilt = BuildTrackFromPoints(pOptions.mName, "Converted track", result.mRoute,
                                                     result.mHalfWidth, builder);
                if (firstProblem.empty() && !result.mBuilt.mOk)
                    firstProblem = result.mBuilt.mProblem;
                if (result.mBuilt.mOk)
                {
                    if (side.mToStart > 25.0)
                        result.mWarnings.push_back("THE START HAS MOVED ALONG THE ROUTE TO A LONGER STRAIGHT");
                    built = true;
                    break;
                }
            }
        }
        // If no start works, report the reason the preferred start failed, not the last one tried.
        if (!built)
            result.mBuilt.mProblem = firstProblem;
    }
    for (std::size_t index = 0; index < result.mHeights.size(); ++index)
    {
        const double change = result.mHeights[(index + 1) % result.mHeights.size()] - result.mHeights[index];
        result.mBiggestRise = std::max(result.mBiggestRise, change);
        result.mBiggestDrop = std::max(result.mBiggestDrop, -change);
    }
    if (!result.mBuilt.mOk)
        result.mProblem = result.mBuilt.mProblem;
    return result;
}
