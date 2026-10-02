// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_COURSE_H
#define OPENHOVER_COURSE_H

#include "Race.h"

#include <vector>

class Course
{
public:
    Course(const std::vector<RaceGate>& pWaypoints, double pHalfWidth);

    bool IsOnRoad(double pX, double pY) const;
    void ProjectToRoad(double pX, double pY, double& pOutX, double& pOutY) const;
    // Direction of travel along the route at the point of the road nearest to (pX, pY).
    double RouteHeadingNear(double pX, double pY) const;
    double HalfWidth() const { return mHalfWidth; }

private:
    std::vector<RaceGate> mWaypoints;
    double mHalfWidth;
};

#endif