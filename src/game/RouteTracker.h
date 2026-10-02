// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_ROUTE_TRACKER_H
#define OPENHOVER_ROUTE_TRACKER_H

#include "Course.h"

// Follows one craft's total distance along the route over a whole race, however many laps. The
// result is monotonic in normal driving, so the difference between two trackers is how far one
// craft is ahead of the other along the road.
class RouteTracker
{
public:
    void Reset();
    // Call every frame with the craft's position; returns the cumulative route distance.
    double Update(const Course& pCourse, double pX, double pY);
    double Distance() const { return mCumulative; }

private:
    bool mStarted = false;
    double mPrevious = 0.0;
    double mCumulative = 0.0;
};

#endif
