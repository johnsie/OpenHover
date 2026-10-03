// SPDX-License-Identifier: MIT OR Apache-2.0
#include "TrkReader.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

// What was observed in the one sample this reader is based on (all integers little-endian 32-bit,
// distances in thousandths of a metre):
//
//  * The file is not compressed. It opens with a text banner and a description, then a few
//    numbers.
//  * Starting positions: a count N followed by N records of 22 bytes: a 1-based index, the number of
//    the room the start is in (rooms are numbered in the order they appear in the file, from 0),
//    then x, y, z as signed integers and two more bytes.
//  * Rooms: a run of convex polygons. Each is the corner count, the floor height, the ceiling
//    height, the bounding box (min x, min y, max x, max y), then for each corner its x, y and the
//    length of the edge to the next corner. Between rooms there is other data (neighbour lists and
//    wall texture numbers) that this reader skips: neighbours are found again from shared edges.
//  * The second half of the file is a large block of repeated byte values, which is ignored.
//
// Rooms are found by scanning for that pattern and accepting a candidate only if the bounding box
// matches the corners and every stored edge length matches the real distance, which is very
// unlikely to happen by chance.

namespace
{
constexpr std::size_t kMaximumFileBytes = 8u << 20;
constexpr int kMaximumRooms = 400;
constexpr int kMinimumCorners = 3;
constexpr int kMaximumCorners = 16;
constexpr std::size_t kStartRecordBytes = 22;

bool ReadInt(const std::string& pBytes, std::size_t pOffset, std::int32_t& pOut)
{
    if (pOffset + 4 > pBytes.size())
        return false;
    std::uint32_t value = 0;
    for (int index = 0; index < 4; ++index)
        value |= static_cast<std::uint32_t>(static_cast<unsigned char>(pBytes[pOffset + index])) << (8 * index);
    pOut = static_cast<std::int32_t>(value);
    return true;
}

// Offset of the first long stretch of one repeated non-zero byte after pFrom, or the file size.
std::size_t FindBulkStart(const std::string& pBytes, std::size_t pFrom)
{
    std::size_t index = pFrom;
    while (index + 64 < pBytes.size())
    {
        const char value = pBytes[index];
        std::size_t end = index;
        while (end < pBytes.size() && pBytes[end] == value)
            ++end;
        if (end - index >= 64 && value != 0)
            return index;
        index = end > index ? end : index + 1;
    }
    return pBytes.size();
}

// Finds the table of starting positions; returns the offset just past it, or 0 if there is none.
std::size_t ReadStarts(const std::string& pBytes, std::vector<TrkStart>& pStarts)
{
    const std::size_t limit = std::min<std::size_t>(pBytes.size(), 0x1000);
    for (std::size_t offset = 0x20; offset + 8 < limit; ++offset)
    {
        std::int32_t count = 0;
        if (!ReadInt(pBytes, offset, count) || count < 1 || count > 64)
            continue;
        std::vector<TrkStart> starts;
        std::size_t position = offset + 4;
        bool good = true;
        for (int index = 0; index < count && good; ++index)
        {
            std::int32_t id = 0;
            std::int32_t room = 0;
            std::int32_t x = 0;
            std::int32_t y = 0;
            std::int32_t z = 0;
            good = ReadInt(pBytes, position, id) && ReadInt(pBytes, position + 4, room)
                && ReadInt(pBytes, position + 8, x) && ReadInt(pBytes, position + 12, y)
                && ReadInt(pBytes, position + 16, z) && id == index + 1 && room >= 0 && room < 10000
                && std::abs(x) < 10000000 && std::abs(y) < 10000000;
            if (!good)
                break;
            TrkStart start;
            start.mIndex = id;
            start.mRoom = room;
            start.mX = x / 1000.0;
            start.mY = y / 1000.0;
            start.mZ = z / 1000.0;
            starts.push_back(start);
            position += kStartRecordBytes;
        }
        if (good)
        {
            pStarts = starts;
            return position;
        }
    }
    return 0;
}
}

std::string ReadTrk(const std::string& pBytes, TrkTrack& pOut)
{
    if (pBytes.size() < 0x200)
        return "the file is too small to be a track";
    if (pBytes.size() > kMaximumFileBytes)
        return "the file is too large";

    TrkTrack track;
    const std::size_t afterStarts = ReadStarts(pBytes, track.mStarts);
    if (afterStarts == 0)
        return "no table of starting positions was found, so this is not a layout I understand";

    const std::size_t end = std::min(FindBulkStart(pBytes, afterStarts), pBytes.size());
    std::size_t offset = afterStarts;
    while (offset + 28 < end && static_cast<int>(track.mRooms.size()) < kMaximumRooms)
    {
        std::int32_t corners = 0;
        std::int32_t floorHeight = 0;
        std::int32_t ceilingHeight = 0;
        std::int32_t box[4] = {0, 0, 0, 0};
        bool matched = false;
        if (ReadInt(pBytes, offset, corners) && corners >= kMinimumCorners && corners <= kMaximumCorners
            && ReadInt(pBytes, offset + 4, floorHeight) && ReadInt(pBytes, offset + 8, ceilingHeight)
            && floorHeight < ceilingHeight && floorHeight >= -100000 && ceilingHeight <= 100000
            && ReadInt(pBytes, offset + 12, box[0]) && ReadInt(pBytes, offset + 16, box[1])
            && ReadInt(pBytes, offset + 20, box[2]) && ReadInt(pBytes, offset + 24, box[3])
            && box[0] < box[2] && box[1] < box[3])
        {
            std::vector<std::int32_t> xs;
            std::vector<std::int32_t> ys;
            std::vector<std::int32_t> lengths;
            bool good = true;
            for (int corner = 0; corner < corners && good; ++corner)
            {
                const std::size_t base = offset + 28 + 12 * static_cast<std::size_t>(corner);
                std::int32_t x = 0;
                std::int32_t y = 0;
                std::int32_t length = 0;
                good = ReadInt(pBytes, base, x) && ReadInt(pBytes, base + 4, y)
                    && ReadInt(pBytes, base + 8, length);
                xs.push_back(x);
                ys.push_back(y);
                lengths.push_back(length);
            }
            if (good)
            {
                good = *std::min_element(xs.begin(), xs.end()) == box[0]
                    && *std::min_element(ys.begin(), ys.end()) == box[1]
                    && *std::max_element(xs.begin(), xs.end()) == box[2]
                    && *std::max_element(ys.begin(), ys.end()) == box[3];
                for (int corner = 0; corner < corners && good; ++corner)
                {
                    const int next = (corner + 1) % corners;
                    const double distance = std::hypot(static_cast<double>(xs[next]) - xs[corner],
                                                       static_cast<double>(ys[next]) - ys[corner]);
                    good = std::fabs(distance - lengths[corner]) <= 5.0;
                }
            }
            if (good)
            {
                TrkRoom room;
                room.mFloor = floorHeight / 1000.0;
                room.mCeiling = ceilingHeight / 1000.0;
                for (int corner = 0; corner < corners; ++corner)
                    room.mCorners.push_back({xs[corner] / 1000.0, ys[corner] / 1000.0});
                track.mRooms.push_back(room);
                offset += 28 + 12 * static_cast<std::size_t>(corners);
                matched = true;
            }
        }
        if (!matched)
            offset += 4;
    }
    if (track.mRooms.size() < 3)
        return "fewer than three rooms were found, so this is not a layout I understand";
    pOut = track;
    return std::string();
}
