// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_GHOST_LIBRARY_H
#define OPENHOVER_GHOST_LIBRARY_H

#include "InputRecording.h"

#include <string>
#include <vector>

// Increment when hovercraft handling, collision, or other physics changes enough that an old
// recording would no longer replay to the same line. Combined with the built-in content version
// to form the tag that saved ghosts must match.
constexpr int kGhostPhysicsVersion = 3;

// The fastest completed run for each track and craft class, kept as the inputs that replay it.
class GhostLibrary
{
public:
    // Stores the run when it beats the current best (or none exists). Returns true if stored.
    bool Submit(const std::string& pTrackId, int pCraftClass, double pSeconds,
                const InputRecording& pRecording);
    const InputRecording* Find(const std::string& pTrackId, int pCraftClass) const;
    double BestSeconds(const std::string& pTrackId, int pCraftClass) const;
    int Count() const { return static_cast<int>(mEntries.size()); }

    // Text form that carries pVersionTag. Parse accepts it only when the tag matches and the
    // whole text is well formed; otherwise the library is left unchanged and false is returned.
    std::string Serialize(int pVersionTag) const;
    bool Parse(const std::string& pText, int pVersionTag);

private:
    struct Entry
    {
        std::string mTrackId;
        int mCraftClass = 0;
        double mSeconds = 0.0;
        InputRecording mRecording;
    };

    const Entry* FindEntry(const std::string& pTrackId, int pCraftClass) const;

    std::vector<Entry> mEntries;
};

#endif
