// SPDX-License-Identifier: MIT OR Apache-2.0
#include "Ground.h"

#include "Hovercraft.h"

#include <algorithm>
#include <cmath>

namespace
{
// Where along a segment (0..1) a point projects, and how far the point is from the segment.
double Project(const RaceGate& pFrom, const RaceGate& pTo, double pX, double pY, double& pDistance)
{
    const double dx = pTo.mX - pFrom.mX;
    const double dy = pTo.mY - pFrom.mY;
    const double lengthSquared = dx * dx + dy * dy;
    double t = lengthSquared > 0.0 ? ((pX - pFrom.mX) * dx + (pY - pFrom.mY) * dy) / lengthSquared : 0.0;
    t = std::max(0.0, std::min(1.0, t));
    pDistance = std::hypot(pX - (pFrom.mX + dx * t), pY - (pFrom.mY + dy * t));
    return t;
}
}

GroundProfile::GroundProfile(const std::vector<RaceGate>& pRoute, const std::vector<double>& pHeights)
{
    if (pRoute.size() >= 2 && pHeights.size() == pRoute.size())
    {
        mRoute = pRoute;
        mHeights = pHeights;
    }
}

double GroundProfile::HeightAt(double pX, double pY) const
{
    if (mRoute.empty())
        return 0.0;
    const std::size_t count = mRoute.size();
    double bestDistance = 1e18;
    double height = 0.0;
    for (std::size_t index = 0; index < count; ++index)
    {
        double distance = 0.0;
        Project(mRoute[index], mRoute[(index + 1) % count], pX, pY, distance);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            height = mHeights[index];
        }
    }
    return height;
}

double GroundProfile::SegmentHeight(std::size_t pIndex) const
{
    return mRoute.empty() ? 0.0 : mHeights[pIndex % mHeights.size()];
}

std::string GroundProfileProblem(const std::vector<RaceGate>& pRoute, const std::vector<double>& pHeights)
{
    if (pHeights.empty())
        return std::string();
    if (pHeights.size() != pRoute.size())
        return "there must be one ground height for each waypoint";
    for (const double height : pHeights)
        if (!(std::fabs(height) <= kMaximumGroundHeight))
            return "a ground height is outside the allowed range";
    const std::size_t count = pRoute.size();
    // Crossing roads share a level: the bridge, not the ground, takes one over the other.
    for (std::size_t first = 0; first < count; ++first)
    {
        for (std::size_t second = first + 2; second < count; ++second)
        {
            if (first == 0 && second == count - 1)
                continue;
            const RaceGate& a = pRoute[first];
            const RaceGate& b = pRoute[(first + 1) % count];
            const RaceGate& c = pRoute[second];
            const RaceGate& d = pRoute[(second + 1) % count];
            const double rx = b.mX - a.mX;
            const double ry = b.mY - a.mY;
            const double sx = d.mX - c.mX;
            const double sy = d.mY - c.mY;
            const double denominator = rx * sy - ry * sx;
            if (std::fabs(denominator) < 1e-9)
                continue;
            const double t = ((c.mX - a.mX) * sy - (c.mY - a.mY) * sx) / denominator;
            const double u = ((c.mX - a.mX) * ry - (c.mY - a.mY) * rx) / denominator;
            if (t < 0.0 || t > 1.0 || u < 0.0 || u > 1.0)
                continue;
            if (std::fabs(pHeights[first] - pHeights[second]) > 0.75)
                return "roads that cross must be at the same height";
        }
    }
    return std::string();
}

bool ResolveGroundStep(HovercraftState& pState, const GroundProfile& pGround)
{
    const double ground = pGround.HeightAt(pState.mX, pState.mY);
    const double rise = ground - pState.mGroundHeight;
    // A move of a few metres at most is driving; anything more is a placement (a grid slot, a
    // recovery) and may put the craft anywhere.
    const double moved = std::hypot(pState.mX - pState.mPreviousX, pState.mY - pState.mPreviousY);
    if (rise > 0.1 && pState.mHasPreviousPosition && moved < 5.0 && pState.mHeight - ground < kStepClearance)
    {
        pState.mX = pState.mPreviousX;
        pState.mY = pState.mPreviousY;
        pState.mSpeed *= -0.25;
        pState.mInPit = true;
        return true;
    }
    pState.mGroundHeight = ground;
    if (pState.mSpeed > 8.0)
        pState.mInPit = false; // driving freely again
    if (rise < -0.5)
        pState.mFalling = true;
    if (pState.mHeight < ground)
    {
        pState.mHeight = ground + HovercraftTuning().mHoverHeight;
        pState.mVerticalSpeed = 0.0;
    }
    return false;
}

bool ShouldJumpGroundStep(const HovercraftState& pState, const GroundProfile& pGround)
{
    if (pGround.Flat() || pState.mSpeed < 4.0)
        return false;
    const auto ahead = [&](double pDistance)
    {
        return pGround.HeightAt(pState.mX + std::cos(pState.mTravelHeading) * pDistance,
                                pState.mY + std::sin(pState.mTravelHeading) * pDistance)
            - pState.mGroundHeight;
    };
    // A pit has to be jumped from its very edge, so that the jump carries the craft across the gap
    // instead of down into it.
    if (ahead(pState.mSpeed * 0.04 + 1.0) < -0.5)
        return true;
    // A step up has to be jumped late enough to carry on over what follows it, but early enough to
    // be high enough when it arrives: the higher the step, the earlier.
    const double reach = pState.mSpeed * 0.6 + 3.0;
    for (double distance = 1.0; distance <= reach; distance += 1.0)
    {
        const double rise = ahead(distance);
        if (rise <= 0.1)
            continue;
        const double gain = std::max(0.0, rise - (1.2 - kStepClearance) + 0.1);
        const double launch = HovercraftTuning().mJumpImpulse;
        const double gravity = HovercraftTuning().mFallGravity;
        const double discriminant = launch * launch - 2.0 * gravity * gain;
        const double seconds = discriminant > 0.0 ? (launch - std::sqrt(discriminant)) / gravity : launch / gravity;
        return distance <= pState.mSpeed * seconds + 1.5;
    }
    return false;
}
