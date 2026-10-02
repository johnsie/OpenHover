// SPDX-License-Identifier: MIT OR Apache-2.0
#include "Course.h"

#include <cmath>
#include <iostream>
#include <vector>

int main()
{
    std::vector<RaceGate> waypoints;
    waypoints.push_back({0.0, 0.0, 1.0});
    waypoints.push_back({10.0, 0.0, 1.0});
    waypoints.push_back({10.0, 10.0, 1.0});
    waypoints.push_back({0.0, 10.0, 1.0});
    Course course(waypoints, 2.0);

    if (!course.IsOnRoad(5.0, 1.5) || course.IsOnRoad(5.0, 2.5))
    {
        std::cerr << "course road width was not enforced\n";
        return 1;
    }

    double projectedX = 0.0;
    double projectedY = 0.0;
    course.ProjectToRoad(5.0, 7.0, projectedX, projectedY);
    if (std::fabs(projectedX - 5.0) > 0.0001 || std::fabs(projectedY - 10.0) > 0.0001)
    {
        std::cerr << "course did not project to the nearest road segment\n";
        return 1;
    }

    return 0;
}