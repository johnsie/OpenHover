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
    // Length of the closed route in world units.
    double RouteLength() const { return mRouteLength; }
    // Distance along the route from its first waypoint to the point of the route nearest
    // (pX, pY), in [0, RouteLength()). With pHintDistance >= 0 only stretches of the route within
    // pWindow of the hint (wrapping round the lap) are considered, which keeps a car on the right
    // road where two roads cross.
    double RouteDistanceNear(double pX, double pY, double pHintDistance = -1.0, double pWindow = 0.0) const;
    double HalfWidth() const { return mHalfWidth; }

private:
    std::vector<RaceGate> mWaypoints;
    double mHalfWidth;
    std::vector<double> mSegmentStart; // route distance at the start of each segment
    double mRouteLength = 0.0;
};

#endif