// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_STALL_DETECTOR_H
#define OPENHOVER_STALL_DETECTOR_H

// Notices a craft that has stopped making progress, for example an AI rival wedged against a wall
// after a collision or missile hit. It reports a stall once the craft has stayed within a small
// area for a few seconds while the race is running.
class StallDetector
{
public:
    static constexpr double kStallSeconds = 3.0;
    static constexpr double kProgressMetres = 6.0;

    void Reset();
    // Call once per simulation step. Returns true when the craft has been stuck long enough; the
    // detector then starts counting afresh.
    bool Update(double pX, double pY, double pSeconds);

    // A second test, for a craft that is moving but going nowhere (bouncing between walls, circling
    // a corner): pass its cumulative distance along the route. A stall is reported when that
    // distance has not improved by kProgressDistance within kProgressWindowSeconds.
    static constexpr double kProgressWindowSeconds = 8.0;
    static constexpr double kProgressDistance = 12.0;
    bool UpdateProgress(double pRouteDistance, double pSeconds);

private:
    bool mStarted = false;
    double mAnchorX = 0.0;
    double mAnchorY = 0.0;
    double mStuckSeconds = 0.0;
    bool mProgressStarted = false;
    double mBestDistance = 0.0;
    double mProgressSeconds = 0.0;
};

#endif
