// SPDX-License-Identifier: MIT OR Apache-2.0
#include "RouteTracker.h"

namespace
{
// How far along the route a craft could move between two updates, with plenty of margin.
constexpr double kSearchWindow = 120.0;
}

void RouteTracker::Reset()
{
    mStarted = false;
    mPrevious = 0.0;
    mCumulative = 0.0;
}

double RouteTracker::Update(const Course& pCourse, double pX, double pY)
{
    const double length = pCourse.RouteLength();
    if (length <= 0.0)
        return mCumulative;
    if (!mStarted)
    {
        mPrevious = pCourse.RouteDistanceNear(pX, pY);
        mCumulative = mPrevious;
        mStarted = true;
        return mCumulative;
    }
    const double current = pCourse.RouteDistanceNear(pX, pY, mPrevious, kSearchWindow);
    double step = current - mPrevious;
    // Crossing the start of the route wraps the distance back to zero (or the other way).
    if (step < -length * 0.5)
        step += length;
    else if (step > length * 0.5)
        step -= length;
    mCumulative += step;
    mPrevious = current;
    return mCumulative;
}
