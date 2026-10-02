// SPDX-License-Identifier: MIT OR Apache-2.0
#include <SDL.h>
#include <SDL_opengl.h>

#include "BoostPad.h"
#include "Championship.h"
#include "Course.h"
#include "CraftClass.h"
#include "FixedStepClock.h"
#include "Hovercraft.h"
#include "InputRecording.h"
#include "LapTiming.h"
#include "Race.h"
#include "RaceMode.h"
#include "RacePosition.h"
#include "RaceStart.h"
#include "RacerCollision.h"
#include "RecoveryAssist.h"
#include "RivalController.h"
#include "RouteGuidance.h"
#include "SteeringAssist.h"
#include "TrackDefinition.h"
#include "WallCollision.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
const int kWindowWidth = 1280;
const int kWindowHeight = 720;
const int kRivalCount = 2;
const double kPi = 3.14159265358979323846;

enum class FrontScreen
{
    Welcome,
    HowToPlay,
    Settings,
    LocalSetup,
    RaceSetup
};

double ControllerAxis(Sint16 pValue)
{
    const double normalized = pValue < 0 ? pValue / 32768.0 : pValue / 32767.0;
    return std::fabs(normalized) < 0.15 ? 0.0 : normalized;
}

void BounceOffCourseWall(Hovercraft& pHovercraft, const Course& pCourse)
{
    HovercraftState state = pHovercraft.State();
    if (ResolveCourseWallCollision(state, pCourse))
        pHovercraft.Reset(state);
}

void ApplyBoostPads(Hovercraft& pHovercraft, const std::vector<BoostPad>& pPads)
{
    HovercraftState state = pHovercraft.State();
    for (const BoostPad& pad : pPads)
    {
        if (ApplyBoostPad(state, pad))
            pHovercraft.Reset(state);
    }
}

void ApplyHazardZones(Hovercraft& pHovercraft, const std::vector<HazardZone>& pZones,
                      double pSeconds)
{
    HovercraftState state = pHovercraft.State();
    bool affected = false;
    for (const HazardZone& zone : pZones)
        affected = ApplyHazardZone(state, zone, pSeconds) || affected;
    if (affected)
        pHovercraft.Reset(state);
}

void ApplyRaisedSections(Hovercraft& pHovercraft, const std::vector<RaisedSection>& pSections)
{
    HovercraftState state = pHovercraft.State();
    for (const RaisedSection& section : pSections)
    {
        if (ResolveRaisedSectionCollision(state, section))
        {
            pHovercraft.Reset(state);
            return;
        }
    }
}

bool ShouldJumpRaisedSection(const HovercraftState& pState,
                             const std::vector<RaisedSection>& pSections)
{
    const double travelX = std::cos(pState.mTravelHeading);
    const double travelY = std::sin(pState.mTravelHeading);
    for (const RaisedSection& section : pSections)
    {
        const double forwardX = std::cos(section.mHeading);
        const double forwardY = std::sin(section.mHeading);
        const double sideX = -forwardY;
        const double sideY = forwardX;
        const double deltaX = section.mX - pState.mX;
        const double deltaY = section.mY - pState.mY;
        const double forwardDistance = deltaX * travelX + deltaY * travelY;
        const double sidewaysDistance = std::fabs(deltaX * sideX + deltaY * sideY);
        const double jumpLead = std::fmax(4.0, std::fabs(pState.mSpeed) * 0.4);
        if (forwardDistance > 0.0 && forwardDistance <= section.mHalfLength + jumpLead
            && sidewaysDistance <= section.mHalfWidth + 0.9)
        {
            return true;
        }
    }
    return false;
}

void SetPerspective(double pAspect, double pSpeed)
{
    const double nearPlane = 0.2;
    const double farPlane = 400.0;
    const double speedFraction = std::fmin(1.0, std::fabs(pSpeed) / 55.0);
    const double fieldOfView = 54.0 + speedFraction * 7.0;
    const double top = nearPlane * std::tan(fieldOfView * kPi / 360.0);
    glFrustum(-top * pAspect, top * pAspect, -top, top, nearPlane, farPlane);
}

void SetChaseCamera(const HovercraftState& pState, double pDistance)
{
    const double forwardX = std::cos(pState.mHeading);
    const double forwardZ = std::sin(pState.mHeading);
    const double eyeX = pState.mX - forwardX * pDistance;
    const double eyeY = 4.4 + pDistance * 0.11;
    const double eyeZ = pState.mY - forwardZ * pDistance;
    const double targetX = pState.mX + forwardX * pDistance * 0.8;
    const double targetY = 0.7;
    const double targetZ = pState.mY + forwardZ * pDistance * 0.8;
    double viewX = targetX - eyeX;
    double viewY = targetY - eyeY;
    double viewZ = targetZ - eyeZ;
    const double viewLength = std::sqrt(viewX * viewX + viewY * viewY + viewZ * viewZ);
    viewX /= viewLength;
    viewY /= viewLength;
    viewZ /= viewLength;
    double sideX = -viewZ;
    double sideZ = viewX;
    const double sideLength = std::sqrt(sideX * sideX + sideZ * sideZ);
    sideX /= sideLength;
    sideZ /= sideLength;
    const double upX = -viewY * sideZ;
    const double upY = sideZ * viewX - sideX * viewZ;
    const double upZ = viewY * sideX;
    const GLdouble matrix[16] = {
        sideX, upX, -viewX, 0.0,
        0.0, upY, -viewY, 0.0,
        sideZ, upZ, -viewZ, 0.0,
        0.0, 0.0, 0.0, 1.0
    };
    glMultMatrixd(matrix);
    glTranslated(-eyeX, -eyeY, -eyeZ);
}

GLuint CreateRoadTexture()
{
    const int textureSize = 64;
    unsigned char pixels[textureSize * textureSize * 3];
    for (int y = 0; y < textureSize; ++y)
    {
        for (int x = 0; x < textureSize; ++x)
        {
            const bool seam = x % 21 < 2 || y % 16 < 2;
            const bool alternateTile = (x / 21 + y / 16) % 2 != 0;
            const int pixel = (y * textureSize + x) * 3;
            pixels[pixel] = static_cast<unsigned char>(seam ? 58 : (alternateTile ? 150 : 166));
            pixels[pixel + 1] = static_cast<unsigned char>(seam ? 86 : (alternateTile ? 178 : 194));
            pixels[pixel + 2] = static_cast<unsigned char>(seam ? 90 : (alternateTile ? 184 : 200));
        }
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureSize, textureSize, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, pixels);
    return texture;
}

GLuint CreateWallTexture()
{
    const int textureSize = 64;
    unsigned char pixels[textureSize * textureSize * 3];
    for (int y = 0; y < textureSize; ++y)
    {
        for (int x = 0; x < textureSize; ++x)
        {
            const bool panelSeam = x % 16 < 2 || y % 22 < 2;
            const bool warningBand = y >= 29 && y < 36;
            const int pixel = (y * textureSize + x) * 3;
            pixels[pixel] = static_cast<unsigned char>(panelSeam ? 52 : (warningBand ? 214 : 134));
            pixels[pixel + 1] = static_cast<unsigned char>(panelSeam ? 74 : (warningBand ? 178 : 166));
            pixels[pixel + 2] = static_cast<unsigned char>(panelSeam ? 80 : (warningBand ? 72 : 178));
        }
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureSize, textureSize, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, pixels);
    return texture;
}

void DrawCourseGrid(const HovercraftState& pState)
{
    const int centerX = static_cast<int>(pState.mX / 16.0) * 16;
    const int centerZ = static_cast<int>(pState.mY / 16.0) * 16;
    glColor3f(0.07f, 0.14f, 0.18f);
    glBegin(GL_QUADS);
    glNormal3d(0.0, 1.0, 0.0);
    glVertex3d(centerX - 96, -0.02, centerZ - 96);
    glVertex3d(centerX + 96, -0.02, centerZ - 96);
    glVertex3d(centerX + 96, -0.02, centerZ + 96);
    glVertex3d(centerX - 96, -0.02, centerZ + 96);
    glEnd();

    glColor3f(0.12f, 0.24f, 0.29f);
    glBegin(GL_LINES);
    for (int offset = -80; offset <= 80; offset += 16)
    {
        glVertex3d(centerX + offset, 0.0, centerZ - 80);
        glVertex3d(centerX + offset, 0.0, centerZ + 80);
        glVertex3d(centerX - 80, 0.0, centerZ + offset);
        glVertex3d(centerX + 80, 0.0, centerZ + offset);
    }
    glEnd();
}

void DrawRoadSegment(double pStartX, double pStartZ, double pEndX, double pEndZ,
                     double pHalfWidth)
{
    const double roadHeight = 0.35;
    const double wallHeight = 1.9;
    const double directionX = pEndX - pStartX;
    const double directionZ = pEndZ - pStartZ;
    const double length = std::sqrt(directionX * directionX + directionZ * directionZ);
    const double sideX = -directionZ / length * pHalfWidth;
    const double sideZ = directionX / length * pHalfWidth;
    glColor3f(0.68f, 0.76f, 0.78f);
    glBegin(GL_QUADS);
    glVertex3d(pStartX + sideX, roadHeight, pStartZ + sideZ);
    glVertex3d(pStartX - sideX, roadHeight, pStartZ - sideZ);
    glVertex3d(pEndX - sideX, roadHeight, pEndZ - sideZ);
    glVertex3d(pEndX + sideX, roadHeight, pEndZ + sideZ);
    glEnd();

    glColor3f(0.84f, 0.9f, 0.91f);
    glBegin(GL_QUADS);
    glVertex3d(pStartX + sideX, 0.0, pStartZ + sideZ);
    glVertex3d(pStartX + sideX, wallHeight, pStartZ + sideZ);
    glVertex3d(pEndX + sideX, wallHeight, pEndZ + sideZ);
    glVertex3d(pEndX + sideX, 0.0, pEndZ + sideZ);
    glVertex3d(pStartX - sideX, wallHeight, pStartZ - sideZ);
    glVertex3d(pStartX - sideX, 0.0, pStartZ - sideZ);
    glVertex3d(pEndX - sideX, 0.0, pEndZ - sideZ);
    glVertex3d(pEndX - sideX, wallHeight, pEndZ - sideZ);
    glEnd();

    glColor3f(0.08f, 0.74f, 0.8f);
    glBegin(GL_LINES);
    glVertex3d(pStartX + sideX, wallHeight, pStartZ + sideZ);
    glVertex3d(pEndX + sideX, wallHeight, pEndZ + sideZ);
    glVertex3d(pStartX - sideX, wallHeight, pStartZ - sideZ);
    glVertex3d(pEndX - sideX, wallHeight, pEndZ - sideZ);
    for (double offset = 0.0; offset <= length; offset += 4.0)
    {
        const double postX = pStartX + directionX / length * std::fmin(offset, length);
        const double postZ = pStartZ + directionZ / length * std::fmin(offset, length);
        glVertex3d(postX + sideX, roadHeight, postZ + sideZ);
        glVertex3d(postX + sideX, wallHeight, postZ + sideZ);
        glVertex3d(postX - sideX, roadHeight, postZ - sideZ);
        glVertex3d(postX - sideX, wallHeight, postZ - sideZ);
    }
    glEnd();

    glColor3f(0.96f, 0.48f, 0.14f);
    glBegin(GL_QUADS);
    glVertex3d(pStartX + sideX * 0.82, roadHeight + 0.012, pStartZ + sideZ * 0.82);
    glVertex3d(pEndX + sideX * 0.82, roadHeight + 0.012, pEndZ + sideZ * 0.82);
    glVertex3d(pEndX + sideX, roadHeight + 0.012, pEndZ + sideZ);
    glVertex3d(pStartX + sideX, roadHeight + 0.012, pStartZ + sideZ);
    glVertex3d(pStartX - sideX, roadHeight + 0.012, pStartZ - sideZ);
    glVertex3d(pEndX - sideX, roadHeight + 0.012, pEndZ - sideZ);
    glVertex3d(pEndX - sideX * 0.82, roadHeight + 0.012, pEndZ - sideZ * 0.82);
    glVertex3d(pStartX - sideX * 0.82, roadHeight + 0.012, pStartZ - sideZ * 0.82);
    glEnd();

    glColor3f(0.45f, 0.54f, 0.57f);
    glBegin(GL_LINES);
    for (double offset = 0.0; offset <= length; offset += 2.0)
    {
        const double markerOffset = std::fmin(offset, length);
        const double centerX = pStartX + directionX / length * markerOffset;
        const double centerZ = pStartZ + directionZ / length * markerOffset;
        glVertex3d(centerX + sideX * 0.8, roadHeight + 0.016, centerZ + sideZ * 0.8);
        glVertex3d(centerX - sideX * 0.8, roadHeight + 0.016, centerZ - sideZ * 0.8);
    }
    glEnd();

    glColor3f(0.11f, 0.6f, 0.66f);
    glBegin(GL_QUADS);
    for (double offset = 3.0; offset < length; offset += 6.0)
    {
        const double centerX = pStartX + directionX / length * offset;
        const double centerZ = pStartZ + directionZ / length * offset;
        const double forwardX = directionX / length;
        const double forwardZ = directionZ / length;
        glVertex3d(centerX - forwardX * 0.8 + sideX * 1.01, 0.62,
                   centerZ - forwardZ * 0.8 + sideZ * 1.01);
        glVertex3d(centerX + forwardX * 0.8 + sideX * 1.01, 0.62,
                   centerZ + forwardZ * 0.8 + sideZ * 1.01);
        glVertex3d(centerX + forwardX * 0.8 + sideX * 1.01, 1.2,
                   centerZ + forwardZ * 0.8 + sideZ * 1.01);
        glVertex3d(centerX - forwardX * 0.8 + sideX * 1.01, 1.2,
                   centerZ - forwardZ * 0.8 + sideZ * 1.01);
    }
    glEnd();
}

void DrawConnectedTrack(const std::vector<RaceGate>& pWaypoints, double pHalfWidth,
                        float pRoadRed, float pRoadGreen, float pRoadBlue,
                        float pWallRed, float pWallGreen, float pWallBlue, GLuint pRoadTexture,
                        GLuint pWallTexture)
{
    if (pWaypoints.size() < 3)
        return;

    struct EdgePoint
    {
        double mLeftX;
        double mLeftZ;
        double mRightX;
        double mRightZ;
    };
    std::vector<EdgePoint> edges;
    for (int index = 0; index < static_cast<int>(pWaypoints.size()); ++index)
    {
        const RaceGate& previous = pWaypoints[(index + static_cast<int>(pWaypoints.size()) - 1)
                                               % pWaypoints.size()];
        const RaceGate& current = pWaypoints[index];
        const RaceGate& next = pWaypoints[(index + 1) % pWaypoints.size()];
        double previousX = current.mX - previous.mX;
        double previousZ = current.mY - previous.mY;
        double nextX = next.mX - current.mX;
        double nextZ = next.mY - current.mY;
        const double previousLength = std::sqrt(previousX * previousX + previousZ * previousZ);
        const double nextLength = std::sqrt(nextX * nextX + nextZ * nextZ);
        previousX /= previousLength;
        previousZ /= previousLength;
        nextX /= nextLength;
        nextZ /= nextLength;
        double normalX = -previousZ - nextZ;
        double normalZ = previousX + nextX;
        const double normalLength = std::sqrt(normalX * normalX + normalZ * normalZ);
        normalX /= normalLength;
        normalZ /= normalLength;
        const double miterScale = std::fmin(pHalfWidth * 1.6,
            pHalfWidth / std::fmax(0.45, normalX * -nextZ + normalZ * nextX));
        edges.push_back({current.mX + normalX * miterScale, current.mY + normalZ * miterScale,
                         current.mX - normalX * miterScale, current.mY - normalZ * miterScale});
    }

    const double roadHeight = 0.35;
    const double wallHeight = 1.9;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, pRoadTexture);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor3f(pRoadRed, pRoadGreen, pRoadBlue);
    glBegin(GL_QUAD_STRIP);
    glNormal3d(0.0, 1.0, 0.0);
    double textureDistance = 0.0;
    for (int index = 0; index <= static_cast<int>(edges.size()); ++index)
    {
        const EdgePoint& edge = edges[index % edges.size()];
        if (index > 0)
        {
            const EdgePoint& previous = edges[(index - 1) % edges.size()];
            const double previousCenterX = (previous.mLeftX + previous.mRightX) * 0.5;
            const double previousCenterZ = (previous.mLeftZ + previous.mRightZ) * 0.5;
            const double centerX = (edge.mLeftX + edge.mRightX) * 0.5;
            const double centerZ = (edge.mLeftZ + edge.mRightZ) * 0.5;
            const double deltaX = centerX - previousCenterX;
            const double deltaZ = centerZ - previousCenterZ;
            textureDistance += std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
        }
        glTexCoord2d(textureDistance / 12.0, 0.0);
        glVertex3d(edge.mLeftX, roadHeight, edge.mLeftZ);
        glTexCoord2d(textureDistance / 12.0, 3.0);
        glVertex3d(edge.mRightX, roadHeight, edge.mRightZ);
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);

    glColor3f(0.12f, 0.62f, 0.68f);
    glBegin(GL_LINE_LOOP);
    for (const EdgePoint& edge : edges)
    {
        glVertex3d(edge.mLeftX * 0.9 + edge.mRightX * 0.1, roadHeight + 0.014,
                   edge.mLeftZ * 0.9 + edge.mRightZ * 0.1);
    }
    glEnd();
    glBegin(GL_LINE_LOOP);
    for (const EdgePoint& edge : edges)
    {
        glVertex3d(edge.mLeftX * 0.1 + edge.mRightX * 0.9, roadHeight + 0.014,
                   edge.mLeftZ * 0.1 + edge.mRightZ * 0.9);
    }
    glEnd();
    glColor3f(0.48f, 0.56f, 0.59f);
    glBegin(GL_LINES);
    for (int index = 0; index < static_cast<int>(edges.size()); ++index)
    {
        const EdgePoint& start = edges[index];
        const EdgePoint& end = edges[(index + 1) % edges.size()];
        for (int column = 1; column < 3; ++column)
        {
            const double across = column / 3.0;
            glVertex3d(start.mLeftX + (start.mRightX - start.mLeftX) * across,
                       roadHeight + 0.012,
                       start.mLeftZ + (start.mRightZ - start.mLeftZ) * across);
            glVertex3d(end.mLeftX + (end.mRightX - end.mLeftX) * across,
                       roadHeight + 0.012,
                       end.mLeftZ + (end.mRightZ - end.mLeftZ) * across);
        }
        const double length = std::sqrt((end.mLeftX - start.mLeftX) * (end.mLeftX - start.mLeftX)
            + (end.mLeftZ - start.mLeftZ) * (end.mLeftZ - start.mLeftZ));
        for (double distance = 2.5; distance < length; distance += 2.5)
        {
            const double progress = distance / length;
            glVertex3d(start.mLeftX + (end.mLeftX - start.mLeftX) * progress, roadHeight + 0.012,
                       start.mLeftZ + (end.mLeftZ - start.mLeftZ) * progress);
            glVertex3d(start.mRightX + (end.mRightX - start.mRightX) * progress, roadHeight + 0.012,
                       start.mRightZ + (end.mRightZ - start.mRightZ) * progress);
        }
    }
    glEnd();

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, pWallTexture);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor3f(pWallRed, pWallGreen, pWallBlue);
    double wallTextureDistance = 0.0;
    glBegin(GL_QUAD_STRIP);
    for (int index = 0; index <= static_cast<int>(edges.size()); ++index)
    {
        const EdgePoint& edge = edges[index % edges.size()];
        if (index > 0)
        {
            const EdgePoint& previous = edges[(index - 1) % edges.size()];
            const double deltaX = edge.mLeftX - previous.mLeftX;
            const double deltaZ = edge.mLeftZ - previous.mLeftZ;
            wallTextureDistance += std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
        }
        const double normalLength = std::sqrt((edge.mLeftX - edge.mRightX) * (edge.mLeftX - edge.mRightX)
            + (edge.mLeftZ - edge.mRightZ) * (edge.mLeftZ - edge.mRightZ));
        glNormal3d((edge.mLeftX - edge.mRightX) / normalLength, 0.0,
                   (edge.mLeftZ - edge.mRightZ) / normalLength);
        glTexCoord2d(wallTextureDistance / 5.0, 0.0);
        glVertex3d(edge.mLeftX, roadHeight, edge.mLeftZ);
        glTexCoord2d(wallTextureDistance / 5.0, 1.0);
        glVertex3d(edge.mLeftX, wallHeight, edge.mLeftZ);
    }
    glEnd();
    wallTextureDistance = 0.0;
    glBegin(GL_QUAD_STRIP);
    for (int index = 0; index <= static_cast<int>(edges.size()); ++index)
    {
        const EdgePoint& edge = edges[index % edges.size()];
        if (index > 0)
        {
            const EdgePoint& previous = edges[(index - 1) % edges.size()];
            const double deltaX = edge.mRightX - previous.mRightX;
            const double deltaZ = edge.mRightZ - previous.mRightZ;
            wallTextureDistance += std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
        }
        const double normalLength = std::sqrt((edge.mRightX - edge.mLeftX) * (edge.mRightX - edge.mLeftX)
            + (edge.mRightZ - edge.mLeftZ) * (edge.mRightZ - edge.mLeftZ));
        glNormal3d((edge.mRightX - edge.mLeftX) / normalLength, 0.0,
                   (edge.mRightZ - edge.mLeftZ) / normalLength);
        glTexCoord2d(wallTextureDistance / 5.0, 1.0);
        glVertex3d(edge.mRightX, wallHeight, edge.mRightZ);
        glTexCoord2d(wallTextureDistance / 5.0, 0.0);
        glVertex3d(edge.mRightX, roadHeight, edge.mRightZ);
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);

    glColor3f(0.08f, 0.74f, 0.8f);
    glBegin(GL_LINE_LOOP);
    for (const EdgePoint& edge : edges)
        glVertex3d(edge.mLeftX, wallHeight, edge.mLeftZ);
    glEnd();
    glBegin(GL_LINE_LOOP);
    for (const EdgePoint& edge : edges)
        glVertex3d(edge.mRightX, wallHeight, edge.mRightZ);
    glEnd();

    glColor3f(0.22f, 0.9f, 1.0f);
    glBegin(GL_TRIANGLES);
    for (int side = 0; side < 2; ++side)
    {
        for (int index = 0; index < static_cast<int>(edges.size()); ++index)
        {
            const EdgePoint& start = edges[index];
            const EdgePoint& end = edges[(index + 1) % edges.size()];
            const double startX = side == 0 ? start.mLeftX : start.mRightX;
            const double startZ = side == 0 ? start.mLeftZ : start.mRightZ;
            const double endX = side == 0 ? end.mLeftX : end.mRightX;
            const double endZ = side == 0 ? end.mLeftZ : end.mRightZ;
            const double segmentX = endX - startX;
            const double segmentZ = endZ - startZ;
            const double segmentLength = std::sqrt(segmentX * segmentX + segmentZ * segmentZ);
            if (segmentLength == 0.0)
                continue;
            const double forwardX = segmentX / segmentLength;
            const double forwardZ = segmentZ / segmentLength;
            const double acrossX = start.mLeftX - start.mRightX;
            const double acrossZ = start.mLeftZ - start.mRightZ;
            const double acrossLength = std::sqrt(acrossX * acrossX + acrossZ * acrossZ);
            const double outwardX = (side == 0 ? acrossX : -acrossX) / acrossLength * 0.02;
            const double outwardZ = (side == 0 ? acrossZ : -acrossZ) / acrossLength * 0.02;
            for (double distance = 5.0; distance < segmentLength - 1.5; distance += 9.0)
            {
                const double centerX = startX + forwardX * distance + outwardX;
                const double centerZ = startZ + forwardZ * distance + outwardZ;
                glVertex3d(centerX + forwardX * 1.25, 1.15, centerZ + forwardZ * 1.25);
                glVertex3d(centerX - forwardX * 1.0, 0.7, centerZ - forwardZ * 1.0);
                glVertex3d(centerX - forwardX * 1.0, 1.6, centerZ - forwardZ * 1.0);
            }
        }
    }
    glEnd();
}

void DrawLandmarkTower(double pX, double pZ, double pWidth, double pHeight,
                       float pRed, float pGreen, float pBlue)
{
    const double halfWidth = pWidth * 0.5;
    glColor3f(pRed, pGreen, pBlue);
    glBegin(GL_QUADS);
    glVertex3d(pX - halfWidth, 0.0, pZ - halfWidth);
    glVertex3d(pX + halfWidth, 0.0, pZ - halfWidth);
    glVertex3d(pX + halfWidth, pHeight, pZ - halfWidth);
    glVertex3d(pX - halfWidth, pHeight, pZ - halfWidth);
    glVertex3d(pX + halfWidth, 0.0, pZ - halfWidth);
    glVertex3d(pX + halfWidth, 0.0, pZ + halfWidth);
    glVertex3d(pX + halfWidth, pHeight, pZ + halfWidth);
    glVertex3d(pX + halfWidth, pHeight, pZ - halfWidth);
    glVertex3d(pX + halfWidth, pHeight, pZ + halfWidth);
    glVertex3d(pX - halfWidth, pHeight, pZ + halfWidth);
    glVertex3d(pX - halfWidth, pHeight, pZ - halfWidth);
    glVertex3d(pX + halfWidth, pHeight, pZ - halfWidth);
    glEnd();
}

void DrawTrackLandmarks(const std::vector<RaceGate>& pWaypoints)
{
    if (pWaypoints.empty())
        return;
    double centerX = 0.0;
    double centerZ = 0.0;
    for (const RaceGate& waypoint : pWaypoints)
    {
        centerX += waypoint.mX;
        centerZ += waypoint.mY;
    }
    centerX /= pWaypoints.size();
    centerZ /= pWaypoints.size();
    for (int index = 0; index < static_cast<int>(pWaypoints.size()); ++index)
    {
        const RaceGate& waypoint = pWaypoints[index];
        double outwardX = waypoint.mX - centerX;
        double outwardZ = waypoint.mY - centerZ;
        const double length = std::sqrt(outwardX * outwardX + outwardZ * outwardZ);
        if (length == 0.0)
            continue;
        outwardX /= length;
        outwardZ /= length;
        DrawLandmarkTower(waypoint.mX + outwardX * 15.0, waypoint.mY + outwardZ * 15.0,
                          3.5, 5.0 + (index % 3) * 2.5, 0.12f, 0.32f, 0.38f);
    }
}

void DrawFinishZone(const std::vector<RaceGate>& pWaypoints, double pTrackHalfWidth)
{
    if (pWaypoints.size() < 2 || pTrackHalfWidth <= 0.0)
        return;
    const RaceGate& finish = pWaypoints.front();
    const RaceGate& next = pWaypoints[1];
    double forwardX = next.mX - finish.mX;
    double forwardZ = next.mY - finish.mY;
    const double length = std::sqrt(forwardX * forwardX + forwardZ * forwardZ);
    if (length == 0.0)
        return;
    forwardX /= length;
    forwardZ /= length;
    const double sideX = -forwardZ;
    const double sideZ = forwardX;
    const double halfWidth = pTrackHalfWidth * 0.86;
    for (int band = 0; band < 4; ++band)
    {
        const double start = -1.4 + band * 0.72;
        const double end = start + 0.44;
        if (band % 2 == 0)
            glColor3f(0.96f, 0.7f, 0.16f);
        else
            glColor3f(0.18f, 0.88f, 0.92f);
        glBegin(GL_QUADS);
        glNormal3d(0.0, 1.0, 0.0);
        glVertex3d(finish.mX + forwardX * start + sideX * halfWidth, 0.382,
                   finish.mY + forwardZ * start + sideZ * halfWidth);
        glVertex3d(finish.mX + forwardX * end + sideX * halfWidth, 0.382,
                   finish.mY + forwardZ * end + sideZ * halfWidth);
        glVertex3d(finish.mX + forwardX * end - sideX * halfWidth, 0.382,
                   finish.mY + forwardZ * end - sideZ * halfWidth);
        glVertex3d(finish.mX + forwardX * start - sideX * halfWidth, 0.382,
                   finish.mY + forwardZ * start - sideZ * halfWidth);
        glEnd();
    }
}

void DrawGate(const RaceGate& pGate, double pDirectionX, double pDirectionZ, bool pActive,
              bool pFinish, double pTrackHalfWidth)
{
    const double length = std::sqrt(pDirectionX * pDirectionX + pDirectionZ * pDirectionZ);
    const double sideX = -pDirectionZ / length;
    const double sideZ = pDirectionX / length;
    if (pFinish)
        glColor3f(0.95f, 0.95f, 1.0f);
    else if (pActive)
        glColor3f(1.0f, 0.5f, 0.08f);
    else
        glColor3f(0.18f, 0.7f, 0.85f);
    const double forwardX = pDirectionX / length;
    const double forwardZ = pDirectionZ / length;
    const double gateHalfWidth = pTrackHalfWidth;
    const double postHeight = 3.8;
    const double postHalfWidth = 0.22;
    for (int side = -1; side <= 1; side += 2)
    {
        const double postX = pGate.mX + sideX * gateHalfWidth * side;
        const double postZ = pGate.mY + sideZ * gateHalfWidth * side;
        glBegin(GL_QUADS);
        glVertex3d(postX - forwardX * postHalfWidth, 0.36, postZ - forwardZ * postHalfWidth);
        glVertex3d(postX + forwardX * postHalfWidth, 0.36, postZ + forwardZ * postHalfWidth);
        glVertex3d(postX + forwardX * postHalfWidth, postHeight, postZ + forwardZ * postHalfWidth);
        glVertex3d(postX - forwardX * postHalfWidth, postHeight, postZ - forwardZ * postHalfWidth);
        glEnd();
    }
    glBegin(GL_QUADS);
    glVertex3d(pGate.mX - sideX * gateHalfWidth, postHeight - 0.32,
               pGate.mY - sideZ * gateHalfWidth);
    glVertex3d(pGate.mX + sideX * gateHalfWidth, postHeight - 0.32,
               pGate.mY + sideZ * gateHalfWidth);
    glVertex3d(pGate.mX + sideX * gateHalfWidth, postHeight,
               pGate.mY + sideZ * gateHalfWidth);
    glVertex3d(pGate.mX - sideX * gateHalfWidth, postHeight,
               pGate.mY - sideZ * gateHalfWidth);
    glEnd();
    glLineWidth(2.5f);
    glBegin(GL_LINE_LOOP);
    glVertex3d(pGate.mX - sideX * gateHalfWidth, 0.38, pGate.mY - sideZ * gateHalfWidth);
    glVertex3d(pGate.mX - sideX * gateHalfWidth, postHeight, pGate.mY - sideZ * gateHalfWidth);
    glVertex3d(pGate.mX + sideX * gateHalfWidth, postHeight, pGate.mY + sideZ * gateHalfWidth);
    glVertex3d(pGate.mX + sideX * gateHalfWidth, 0.38, pGate.mY + sideZ * gateHalfWidth);
    glEnd();
    glLineWidth(1.0f);
}

void DrawBoostPad(const BoostPad& pPad)
{
    const double innerRadius = pPad.mRadius * 0.55;
    glColor3f(0.08f, 0.82f, 1.0f);
    glBegin(GL_QUADS);
    glNormal3d(0.0, 1.0, 0.0);
    glVertex3d(pPad.mX - pPad.mRadius, 0.38, pPad.mY - pPad.mRadius);
    glVertex3d(pPad.mX + pPad.mRadius, 0.38, pPad.mY - pPad.mRadius);
    glVertex3d(pPad.mX + pPad.mRadius, 0.38, pPad.mY + pPad.mRadius);
    glVertex3d(pPad.mX - pPad.mRadius, 0.38, pPad.mY + pPad.mRadius);
    glEnd();
    glColor3f(0.8f, 0.98f, 1.0f);
    glBegin(GL_QUADS);
    glVertex3d(pPad.mX - innerRadius, 0.4, pPad.mY - innerRadius);
    glVertex3d(pPad.mX + innerRadius, 0.4, pPad.mY - innerRadius);
    glVertex3d(pPad.mX + innerRadius, 0.4, pPad.mY + innerRadius);
    glVertex3d(pPad.mX - innerRadius, 0.4, pPad.mY + innerRadius);
    glEnd();
}

void DrawHazardZone(const HazardZone& pZone)
{
    glColor3f(0.92f, 0.22f, 0.08f);
    glBegin(GL_QUADS);
    glNormal3d(0.0, 1.0, 0.0);
    glVertex3d(pZone.mX - pZone.mRadius, 0.37, pZone.mY - pZone.mRadius);
    glVertex3d(pZone.mX + pZone.mRadius, 0.37, pZone.mY - pZone.mRadius);
    glVertex3d(pZone.mX + pZone.mRadius, 0.37, pZone.mY + pZone.mRadius);
    glVertex3d(pZone.mX - pZone.mRadius, 0.37, pZone.mY + pZone.mRadius);
    glEnd();
    glColor3f(1.0f, 0.72f, 0.16f);
    glBegin(GL_LINE_LOOP);
    glVertex3d(pZone.mX - pZone.mRadius, 0.38, pZone.mY - pZone.mRadius);
    glVertex3d(pZone.mX + pZone.mRadius, 0.38, pZone.mY - pZone.mRadius);
    glVertex3d(pZone.mX + pZone.mRadius, 0.38, pZone.mY + pZone.mRadius);
    glVertex3d(pZone.mX - pZone.mRadius, 0.38, pZone.mY + pZone.mRadius);
    glEnd();
}

void DrawRaisedSection(const RaisedSection& pSection)
{
    const double deckHeight = pSection.mClearHeight - 0.1;
    glPushMatrix();
    glTranslated(pSection.mX, 0.0, pSection.mY);
    glRotated(-pSection.mHeading * 180.0 / kPi, 0.0, 1.0, 0.0);
    glColor3f(0.12f, 0.16f, 0.2f);
    glBegin(GL_QUADS);
    glNormal3d(0.0, 1.0, 0.0);
    glVertex3d(-pSection.mHalfLength, deckHeight, -pSection.mHalfWidth);
    glVertex3d(pSection.mHalfLength, deckHeight, -pSection.mHalfWidth);
    glVertex3d(pSection.mHalfLength, deckHeight, pSection.mHalfWidth);
    glVertex3d(-pSection.mHalfLength, deckHeight, pSection.mHalfWidth);
    glColor3f(0.84f, 0.36f, 0.08f);
    glVertex3d(-pSection.mHalfLength, 0.38, -pSection.mHalfWidth);
    glVertex3d(pSection.mHalfLength, 0.38, -pSection.mHalfWidth);
    glVertex3d(pSection.mHalfLength, deckHeight, -pSection.mHalfWidth);
    glVertex3d(-pSection.mHalfLength, deckHeight, -pSection.mHalfWidth);
    glVertex3d(pSection.mHalfLength, 0.38, pSection.mHalfWidth);
    glVertex3d(-pSection.mHalfLength, 0.38, pSection.mHalfWidth);
    glVertex3d(-pSection.mHalfLength, deckHeight, pSection.mHalfWidth);
    glVertex3d(pSection.mHalfLength, deckHeight, pSection.mHalfWidth);
    glEnd();
    glColor3f(0.74f, 0.84f, 0.88f);
    glBegin(GL_LINES);
    for (double x = -pSection.mHalfLength + 0.35; x < pSection.mHalfLength; x += 0.7)
    {
        glVertex3d(x, deckHeight + 0.01, -pSection.mHalfWidth);
        glVertex3d(x, deckHeight + 0.01, pSection.mHalfWidth);
    }
    glEnd();
    glPopMatrix();
}

void DrawHovercraft(const HovercraftState& pState, bool pRival, bool pGhost = false,
                    CraftClass pCraftClass = CraftClass::Balanced)
{
    const double hoverOffset = std::fmax(0.0, pState.mHeight - 1.2);
    const double shadowScale = std::fmax(0.42, 1.0 - hoverOffset * 0.5);
    const float shadowAlpha = static_cast<float>(std::fmax(0.1, 0.38 - hoverOffset * 0.14));
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glColor4f(0.01f, 0.02f, 0.025f, shadowAlpha);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3d(pState.mX, 0.382, pState.mY);
    for (int degree = 0; degree <= 360; degree += 15)
    {
        const double angle = degree * kPi / 180.0;
        glVertex3d(pState.mX + std::cos(angle) * 1.75 * shadowScale, 0.382,
                   pState.mY + std::sin(angle) * 1.18 * shadowScale);
    }
    glEnd();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    glPushMatrix();
    glTranslated(pState.mX, pState.mHeight, pState.mY);
    glRotated(-pState.mHeading * 180.0 / kPi, 0.0, 1.0, 0.0);
    glRotated(-std::sin(pState.mHeading - pState.mTravelHeading) * 14.0, 1.0, 0.0, 0.0);
    const double verticalPitch = pState.mVerticalSpeed >= 0.0
        ? std::fmin(13.0, pState.mVerticalSpeed * 1.7)
        : std::fmax(-22.0, pState.mVerticalSpeed * 3.0);
    glRotated(verticalPitch, 0.0, 0.0, 1.0);
    const float accentRed = pGhost ? 0.12f : (pRival ? 0.16f : 0.9f);
    const float accentGreen = pGhost ? 0.92f : (pRival ? 0.66f : 0.08f);
    const float accentBlue = pGhost ? 0.82f : (pRival ? 0.82f : 0.12f);
    glColor3f(0.06f, 0.075f, 0.09f);
    glBegin(GL_TRIANGLE_FAN);
    glNormal3d(0.0, -1.0, 0.0);
    glVertex3d(0.0, -0.1, 0.0);
    for (int degree = 0; degree <= 360; degree += 15)
    {
        const double angle = degree * kPi / 180.0;
        glVertex3d(std::cos(angle) * 1.5, -0.1, std::sin(angle) * 1.02);
    }
    glEnd();
    glColor3f(0.13f, 0.16f, 0.18f);
    glBegin(GL_QUAD_STRIP);
    for (int degree = 0; degree <= 360; degree += 15)
    {
        const double angle = degree * kPi / 180.0;
        const double skirtX = std::cos(angle) * 1.5;
        const double skirtZ = std::sin(angle) * 1.02;
        glVertex3d(skirtX, -0.36, skirtZ);
        glVertex3d(skirtX, -0.1, skirtZ);
    }
    glEnd();
    glColor3f(accentRed, accentGreen, accentBlue);
    glBegin(GL_TRIANGLE_FAN);
    glNormal3d(0.0, 1.0, 0.0);
    glVertex3d(0.16, 0.02, 0.0);
    for (int degree = 0; degree <= 360; degree += 15)
    {
        const double angle = degree * kPi / 180.0;
        glVertex3d(0.16 + std::cos(angle) * 1.1, 0.02, std::sin(angle) * 0.7);
    }
    glEnd();
    glColor3f(0.88f, 0.92f, 0.94f);
    glBegin(GL_TRIANGLE_FAN);
    glNormal3d(0.0, 1.0, 0.0);
    glVertex3d(0.28, 0.06, 0.0);
    for (int degree = 0; degree <= 360; degree += 20)
    {
        const double angle = degree * kPi / 180.0;
        glVertex3d(0.28 + std::cos(angle) * 0.68, 0.06, std::sin(angle) * 0.42);
    }
    glEnd();
    glColor3f(0.025f, 0.09f, 0.13f);
    glBegin(GL_TRIANGLE_FAN);
    glNormal3d(0.0, 1.0, 0.0);
    glVertex3d(0.24, 0.64, 0.0);
    for (int degree = 0; degree <= 360; degree += 20)
    {
        const double angle = degree * kPi / 180.0;
        glVertex3d(0.24 + std::cos(angle) * 0.42, 0.16,
                   std::sin(angle) * 0.3);
    }
    glEnd();
    glColor3f(accentRed, accentGreen, accentBlue);
    glBegin(GL_TRIANGLES);
    glVertex3d(1.62, 0.18, 0.0);
    glVertex3d(0.62, 0.1, 0.62);
    glVertex3d(0.62, 0.1, -0.62);
    glVertex3d(1.62, 0.18, 0.0);
    glVertex3d(0.76, 0.42, 0.34);
    glVertex3d(0.76, 0.42, -0.34);
    glEnd();
    glColor3f(0.8f, 0.88f, 0.9f);
    glBegin(GL_TRIANGLES);
    glVertex3d(1.46, 0.22, 0.0);
    glVertex3d(0.86, 0.16, 0.18);
    glVertex3d(0.86, 0.16, -0.18);
    glEnd();
    glColor3f(0.05f, 0.1f, 0.13f);
    glBegin(GL_QUADS);
    glVertex3d(-0.08, 0.42, -0.14);
    glVertex3d(0.18, 0.42, -0.14);
    glVertex3d(0.18, 0.72, -0.14);
    glVertex3d(-0.08, 0.72, -0.14);
    glVertex3d(-0.08, 0.42, 0.14);
    glVertex3d(0.18, 0.42, 0.14);
    glVertex3d(0.18, 0.72, 0.14);
    glVertex3d(-0.08, 0.72, 0.14);
    glEnd();
    glColor3f(0.76f, 0.82f, 0.84f);
    for (int latitude = 0; latitude < 5; ++latitude)
    {
        const double lower = -kPi * 0.5 + latitude * kPi / 5.0;
        const double upper = -kPi * 0.5 + (latitude + 1) * kPi / 5.0;
        glBegin(GL_QUAD_STRIP);
        for (int longitude = 0; longitude <= 10; ++longitude)
        {
            const double angle = longitude * 2.0 * kPi / 10.0;
            glVertex3d(0.12 + std::cos(angle) * std::cos(lower) * 0.22,
                       0.92 + std::sin(lower) * 0.27,
                       std::sin(angle) * std::cos(lower) * 0.22);
            glVertex3d(0.12 + std::cos(angle) * std::cos(upper) * 0.22,
                       0.92 + std::sin(upper) * 0.27,
                       std::sin(angle) * std::cos(upper) * 0.22);
        }
        glEnd();
    }
    glColor3f(0.025f, 0.08f, 0.12f);
    glBegin(GL_QUADS);
    glVertex3d(0.35, 0.9, -0.17);
    glVertex3d(0.35, 1.04, -0.17);
    glVertex3d(0.35, 1.04, 0.17);
    glVertex3d(0.35, 0.9, 0.17);
    glEnd();
    glColor3f(accentRed, accentGreen, accentBlue);
    for (int side = -1; side <= 1; side += 2)
    {
        glBegin(GL_TRIANGLES);
        glVertex3d(-0.62, 0.1, side * 0.68);
        glVertex3d(-1.38, 0.16, side * 1.02);
        glVertex3d(-1.06, 0.68, side * 0.8);
        glEnd();
    }
    for (int side = -1; side <= 1; side += 2)
    {
        const double ductZ = side * 0.82;
        glColor3f(accentRed, accentGreen, accentBlue);
        glBegin(GL_TRIANGLE_FAN);
        glVertex3d(-0.72, 0.86, ductZ);
        for (int degree = 0; degree <= 360; degree += 20)
        {
            const double angle = degree * kPi / 180.0;
            glVertex3d(-0.72, 0.86 + std::sin(angle) * 0.42,
                       ductZ + std::cos(angle) * 0.42);
        }
        glEnd();
        glColor3f(0.03f, 0.04f, 0.05f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex3d(-0.75, 0.86, ductZ);
        for (int degree = 0; degree <= 360; degree += 20)
        {
            const double angle = degree * kPi / 180.0;
            glVertex3d(-0.75, 0.86 + std::sin(angle) * 0.27,
                       ductZ + std::cos(angle) * 0.27);
        }
        glEnd();
        glColor3f(0.72f, 0.78f, 0.8f);
        glBegin(GL_LINES);
        for (int degree = 0; degree < 360; degree += 45)
        {
            const double angle = degree * kPi / 180.0;
            glVertex3d(-0.77, 0.86, ductZ);
            glVertex3d(-0.77, 0.86 + std::sin(angle) * 0.24,
                       ductZ + std::cos(angle) * 0.24);
        }
        glEnd();
    }
    if (pCraftClass == CraftClass::Sprint)
    {
        glColor3f(accentRed, accentGreen, accentBlue);
        glBegin(GL_TRIANGLES);
        glVertex3d(1.48, 0.2, 0.0);
        glVertex3d(0.42, 0.42, 0.0);
        glVertex3d(0.78, 1.04, 0.0);
        glEnd();
    }
    else if (pCraftClass == CraftClass::Control)
    {
        glColor3f(accentRed, accentGreen, accentBlue);
        for (int side = -1; side <= 1; side += 2)
        {
            glBegin(GL_TRIANGLES);
            glVertex3d(0.82, 0.12, side * 0.58);
            glVertex3d(0.18, 0.28, side * 1.32);
            glVertex3d(-0.42, 0.16, side * 0.7);
            glEnd();
        }
    }
    if (pState.mBoosting)
    {
        glColor3f(0.2f, 0.9f, 1.0f);
        glBegin(GL_TRIANGLES);
        glVertex3d(-1.05, 0.0, 0.5);
        glVertex3d(-2.1, 0.0, 0.0);
        glVertex3d(-1.05, 0.0, 0.1);
        glVertex3d(-1.05, 0.0, -0.1);
        glVertex3d(-2.1, 0.0, 0.0);
        glVertex3d(-1.05, 0.0, -0.5);
        glEnd();
    }
    glPopMatrix();
}

int PixelGlyphIndex(char pCharacter)
{
    if (pCharacter >= 'A' && pCharacter <= 'Z')
        return pCharacter - 'A';
    if (pCharacter >= '0' && pCharacter <= '9')
        return 26 + pCharacter - '0';
    return -1;
}

void DrawPixelText(const char* pText, int pLeft, int pTop, int pScale)
{
    static const unsigned char kGlyphs[36][7] = {
        {0x0e,0x11,0x11,0x1f,0x11,0x11,0x11},{0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e},
        {0x0f,0x10,0x10,0x10,0x10,0x10,0x0f},{0x1e,0x11,0x11,0x11,0x11,0x11,0x1e},
        {0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f},{0x1f,0x10,0x10,0x1e,0x10,0x10,0x10},
        {0x0f,0x10,0x10,0x17,0x11,0x11,0x0f},{0x11,0x11,0x11,0x1f,0x11,0x11,0x11},
        {0x1f,0x04,0x04,0x04,0x04,0x04,0x1f},{0x07,0x02,0x02,0x02,0x02,0x12,0x0c},
        {0x11,0x12,0x14,0x18,0x14,0x12,0x11},{0x10,0x10,0x10,0x10,0x10,0x10,0x1f},
        {0x11,0x1b,0x15,0x15,0x11,0x11,0x11},{0x11,0x19,0x15,0x13,0x11,0x11,0x11},
        {0x0e,0x11,0x11,0x11,0x11,0x11,0x0e},{0x1e,0x11,0x11,0x1e,0x10,0x10,0x10},
        {0x0e,0x11,0x11,0x11,0x15,0x12,0x0d},{0x1e,0x11,0x11,0x1e,0x14,0x12,0x11},
        {0x0f,0x10,0x10,0x0e,0x01,0x01,0x1e},{0x1f,0x04,0x04,0x04,0x04,0x04,0x04},
        {0x11,0x11,0x11,0x11,0x11,0x11,0x0e},{0x11,0x11,0x11,0x11,0x11,0x0a,0x04},
        {0x11,0x11,0x11,0x15,0x15,0x15,0x0a},{0x11,0x11,0x0a,0x04,0x0a,0x11,0x11},
        {0x11,0x11,0x0a,0x04,0x04,0x04,0x04},{0x1f,0x01,0x02,0x04,0x08,0x10,0x1f},
        {0x0e,0x11,0x13,0x15,0x19,0x11,0x0e},{0x04,0x0c,0x04,0x04,0x04,0x04,0x0e},
        {0x0e,0x11,0x01,0x02,0x04,0x08,0x1f},{0x1e,0x01,0x01,0x0e,0x01,0x01,0x1e},
        {0x02,0x06,0x0a,0x12,0x1f,0x02,0x02},{0x1f,0x10,0x10,0x1e,0x01,0x01,0x1e},
        {0x0e,0x10,0x10,0x1e,0x11,0x11,0x0e},{0x1f,0x01,0x02,0x04,0x08,0x08,0x08},
        {0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e},{0x0e,0x11,0x11,0x0f,0x01,0x01,0x0e}
    };
    int cursorX = pLeft;
    glBegin(GL_QUADS);
    for (const char* character = pText; *character != '\0'; ++character)
    {
        const int glyphIndex = PixelGlyphIndex(*character);
        if (glyphIndex < 0)
        {
            cursorX += pScale * 4;
            continue;
        }
        for (int row = 0; row < 7; ++row)
        {
            for (int column = 0; column < 5; ++column)
            {
                if ((kGlyphs[glyphIndex][row] & (1 << (4 - column))) == 0)
                    continue;
                const int left = cursorX + column * pScale;
                const int top = pTop + row * pScale;
                glVertex2i(left, top);
                glVertex2i(left + pScale, top);
                glVertex2i(left + pScale, top + pScale);
                glVertex2i(left, top + pScale);
            }
        }
        cursorX += pScale * 6;
    }
    glEnd();
}

void DrawSetupOverlay(int pWidth, int pHeight)
{
    glColor3f(0.2f, 0.9f, 1.0f);
    DrawPixelText("RACE STARTING", 24, 92, 3);
    glColor3f(0.82f, 0.9f, 0.92f);
    DrawPixelText("A D STEER  S BRAKE  UP JUMP", 24, 120, 2);
    DrawPixelText("SHIFT ACCEL  CTRL BOOST", 24, 138, 2);
}

void DrawResultOverlay(int pWinner, int pPlayerPosition, int pCompetitorCount,
                       bool pChampionship, const char* pChampionshipPoints,
                       double pPlayerElapsedSeconds, int pWidth, int pHeight)
{
    const int panelWidth = 420;
    const int panelHeight = pChampionship ? 220 : 180;
    const int left = (pWidth - panelWidth) / 2;
    const int top = (pHeight - panelHeight) / 2 - 10;
    glColor3f(0.02f, 0.05f, 0.08f);
    glBegin(GL_QUADS);
    glVertex2i(left, top);
    glVertex2i(left + panelWidth, top);
    glVertex2i(left + panelWidth, top + panelHeight);
    glVertex2i(left, top + panelHeight);
    glEnd();
    glColor3f(pWinner == 1 ? 0.2f : 0.95f, pWinner == 1 ? 0.9f : 0.24f,
              pWinner == 1 ? 1.0f : 0.18f);
    glBegin(GL_LINE_LOOP);
    glVertex2i(left, top);
    glVertex2i(left + panelWidth, top);
    glVertex2i(left + panelWidth, top + panelHeight);
    glVertex2i(left, top + panelHeight);
    glEnd();
    if (pWinner == 1)
        DrawPixelText("FINISH", left + 138, top + 18, 5);
    else
        DrawPixelText("RIVAL FINISH", left + 78, top + 18, 4);
    char placement[32];
    std::snprintf(placement, sizeof(placement), "PLACE %d OF %d", pPlayerPosition, pCompetitorCount);
    glColor3f(0.82f, 0.9f, 0.92f);
    DrawPixelText(placement, left + 108, top + 72, 3);
    const int elapsedSeconds = static_cast<int>(pPlayerElapsedSeconds);
    char raceTime[32];
    std::snprintf(raceTime, sizeof(raceTime), "TIME %d M %d S", elapsedSeconds / 60, elapsedSeconds % 60);
    DrawPixelText(raceTime, left + 110, top + 102, 3);
    if (pChampionship)
    {
        glColor3f(0.2f, 0.9f, 1.0f);
        DrawPixelText("SERIES POINTS", left + 122, top + 132, 3);
        glColor3f(0.82f, 0.9f, 0.92f);
        DrawPixelText(pChampionshipPoints, left + 42, top + 162, 2);
    }
    glColor3f(1.0f, 0.78f, 0.12f);
    DrawPixelText(pChampionship ? "PRESS R NEXT" : "PRESS R RESTART",
                  pChampionship ? left + 102 : left + 78, pChampionship ? top + 188 : top + 138, 3);
}

void DrawMenuHovercraft(int pCenterX, int pCenterY)
{
    glColor3f(0.02f, 0.06f, 0.08f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2i(pCenterX, pCenterY);
    for (int degree = 0; degree <= 360; degree += 15)
    {
        const double angle = degree * kPi / 180.0;
        glVertex2d(pCenterX + std::cos(angle) * 155.0, pCenterY + std::sin(angle) * 74.0);
    }
    glEnd();
    glColor3f(0.12f, 0.72f, 0.82f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2i(pCenterX + 18, pCenterY - 12);
    for (int degree = 0; degree <= 360; degree += 15)
    {
        const double angle = degree * kPi / 180.0;
        glVertex2d(pCenterX + 18 + std::cos(angle) * 106.0,
                   pCenterY - 12 + std::sin(angle) * 48.0);
    }
    glEnd();
    glColor3f(0.96f, 0.66f, 0.16f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2i(pCenterX + 30, pCenterY - 26);
    for (int degree = 0; degree <= 360; degree += 15)
    {
        const double angle = degree * kPi / 180.0;
        glVertex2d(pCenterX + 30 + std::cos(angle) * 47.0,
                   pCenterY - 26 + std::sin(angle) * 27.0);
    }
    glEnd();
    glColor3f(0.03f, 0.11f, 0.15f);
    glBegin(GL_TRIANGLES);
    glVertex2i(pCenterX - 150, pCenterY + 6);
    glVertex2i(pCenterX - 205, pCenterY + 42);
    glVertex2i(pCenterX - 126, pCenterY + 36);
    glVertex2i(pCenterX + 136, pCenterY + 10);
    glVertex2i(pCenterX + 202, pCenterY + 44);
    glVertex2i(pCenterX + 112, pCenterY + 39);
    glEnd();
    glColor3f(0.2f, 0.9f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2i(pCenterX - 66, pCenterY + 70);
    glVertex2i(pCenterX + 42, pCenterY + 70);
    glVertex2i(pCenterX + 72, pCenterY + 95);
    glVertex2i(pCenterX - 94, pCenterY + 95);
    glEnd();
}

void DrawFrontScreen(FrontScreen pScreen, int pSelection, int pCameraDistanceSetting,
                     int pTrackIndex, int pLaps, bool pWeaponsAllowed, int pWidth, int pHeight)
{
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, pWidth, pHeight, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(0.015f, 0.04f, 0.055f);
    glBegin(GL_QUADS);
    glVertex2i(0, 0);
    glVertex2i(pWidth, 0);
    glVertex2i(pWidth, pHeight);
    glVertex2i(0, pHeight);
    glEnd();
    glColor3f(0.08f, 0.28f, 0.34f);
    glBegin(GL_LINES);
    for (int x = -pWidth; x <= pWidth * 2; x += 72)
    {
        glVertex2i(pWidth / 2, pHeight / 2 + 90);
        glVertex2i(x, pHeight);
    }
    for (int y = pHeight / 2 + 100; y < pHeight; y += 44)
    {
        glVertex2i(0, y);
        glVertex2i(pWidth, y);
    }
    glEnd();

    if (pScreen == FrontScreen::Welcome)
    {
        glColor3f(0.2f, 0.9f, 1.0f);
        DrawPixelText("OPENHOVER", 62, 62, 7);
        glColor3f(1.0f, 0.78f, 0.12f);
        DrawPixelText("ORIGINAL HOVER RACING", 64, 132, 3);
        DrawMenuHovercraft(pWidth / 4, pHeight / 2 + 125);

        const char* options[] = {"PLAY LOCAL GAME", "HOW TO PLAY", "SETTINGS", "QUIT"};
        const int panelLeft = pWidth / 2 - 200;
        const int panelTop = 175;
        for (int option = 0; option < 4; ++option)
        {
            const int top = panelTop + option * 86;
            const bool selected = option == pSelection;
            glColor3f(selected ? 0.12f : 0.03f, selected ? 0.52f : 0.1f,
                      selected ? 0.62f : 0.14f);
            glBegin(GL_QUADS);
            glVertex2i(panelLeft, top);
            glVertex2i(panelLeft + 400, top);
            glVertex2i(panelLeft + 400, top + 62);
            glVertex2i(panelLeft, top + 62);
            glEnd();
            glColor3f(selected ? 1.0f : 0.56f, selected ? 0.82f : 0.72f,
                      selected ? 0.22f : 0.76f);
            DrawPixelText(options[option], panelLeft + 68, top + 20, 3);
        }
        glColor3f(0.72f, 0.82f, 0.84f);
        DrawPixelText("UP DOWN TO SELECT", pWidth / 2 - 132, pHeight - 92, 3);
        DrawPixelText("ENTER TO CONFIRM", pWidth / 2 - 120, pHeight - 62, 3);
    }
    else if (pScreen == FrontScreen::HowToPlay)
    {
        glColor3f(0.2f, 0.9f, 1.0f);
        DrawPixelText("HOW TO PLAY", pWidth / 2 - 165, 70, 5);
        glColor3f(0.82f, 0.9f, 0.92f);
        DrawPixelText("SHIFT ACCEL", pWidth / 2 - 98, 190, 3);
        DrawPixelText("CTRL BOOST", pWidth / 2 - 90, 230, 3);
        DrawPixelText("A D STEER", pWidth / 2 - 72, 270, 3);
        DrawPixelText("S BRAKE", pWidth / 2 - 60, 310, 3);
        DrawPixelText("UP JUMP", pWidth / 2 - 60, 350, 3);
        DrawPixelText("FOLLOW THE CYAN FLOW MARKERS", pWidth / 2 - 225, 410, 3);
        glColor3f(1.0f, 0.78f, 0.12f);
        DrawPixelText("ENTER TO RETURN", pWidth / 2 - 120, pHeight - 70, 3);
    }
    else if (pScreen == FrontScreen::LocalSetup)
    {
        const char* trackNames[] = {"HARBOR LOOP", "GLASS SWITCHBACK", "VELOCITY RING"};
        const char* labels[] = {"TRACK", "LAPS", "WEAPONS", "START RACE", "BACK"};
        glColor3f(0.2f, 0.9f, 1.0f);
        DrawPixelText("LOCAL RACE", pWidth / 2 - 150, 70, 5);
        for (int option = 0; option < 5; ++option)
        {
            const int top = 160 + option * 72;
            const bool selected = option == pSelection;
            glColor3f(selected ? 0.12f : 0.03f, selected ? 0.52f : 0.1f,
                      selected ? 0.62f : 0.14f);
            glBegin(GL_QUADS);
            glVertex2i(pWidth / 2 - 250, top);
            glVertex2i(pWidth / 2 + 250, top);
            glVertex2i(pWidth / 2 + 250, top + 52);
            glVertex2i(pWidth / 2 - 250, top + 52);
            glEnd();
            glColor3f(selected ? 1.0f : 0.72f, selected ? 0.82f : 0.86f,
                      selected ? 0.22f : 0.9f);
            DrawPixelText(labels[option], pWidth / 2 - 218, top + 16, 3);
            if (option == 0)
                DrawPixelText(trackNames[pTrackIndex], pWidth / 2 + 10, top + 16, 3);
            else if (option == 1)
            {
                char laps[16];
                std::snprintf(laps, sizeof(laps), "%d", pLaps);
                DrawPixelText(laps, pWidth / 2 + 180, top + 16, 3);
            }
            else if (option == 2)
                DrawPixelText(pWeaponsAllowed ? "ALLOWED" : "OFF", pWidth / 2 + 120, top + 16, 3);
        }
        glColor3f(0.72f, 0.82f, 0.84f);
        DrawPixelText("UP DOWN SELECT  LEFT RIGHT CHANGE", pWidth / 2 - 230, pHeight - 94, 2);
        glColor3f(1.0f, 0.78f, 0.12f);
        DrawPixelText("ENTER CONFIRM  ESC BACK", pWidth / 2 - 168, pHeight - 62, 2);
    }
    else
    {
        glColor3f(0.2f, 0.9f, 1.0f);
        DrawPixelText("SETTINGS", pWidth / 2 - 120, 90, 5);
        glColor3f(0.82f, 0.9f, 0.92f);
        DrawPixelText("CAMERA DISTANCE", pWidth / 2 - 135, 200, 3);
        const char* cameraOptions[] = {"NEAR", "STANDARD", "FAR"};
        for (int option = 0; option < 3; ++option)
        {
            if (option == pCameraDistanceSetting)
                glColor3f(1.0f, 0.78f, 0.12f);
            else
                glColor3f(0.82f, 0.9f, 0.92f);
            DrawPixelText(cameraOptions[option], pWidth / 2 - 128 + option * 104, 245, 3);
        }
        glColor3f(0.82f, 0.9f, 0.92f);
        DrawPixelText("LEFT RIGHT TO CHANGE", pWidth / 2 - 165, 290, 3);
        glColor3f(1.0f, 0.78f, 0.12f);
        DrawPixelText("ENTER TO RETURN", pWidth / 2 - 120, pHeight - 70, 3);
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FOG);
    glEnable(GL_LIGHTING);
}

void DrawHud(const RaceProgress& pPlayerProgress, int pTargetLaps,
             const std::vector<RaceProgress>& pRivalProgresses,
             const HovercraftState& pPlayerState, const std::vector<HovercraftState>& pRivalStates,
             const RaceGate& pActiveGate, const std::vector<RaceGate>& pWaypoints,
             bool pShowRivals, int pWinner,
             int pPlayerPosition, int pCompetitorCount, bool pChampionship, int pStartLights,
             bool pShowSetupOverlay, const char* pChampionshipPoints,
             double pPlayerElapsedSeconds, int pWidth, int pHeight)
{
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, pWidth, pHeight, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(0.02f, 0.05f, 0.08f);
    glBegin(GL_QUADS);
    glVertex2i(24, 24);
    glVertex2i(284, 24);
    glVertex2i(284, 46);
    glVertex2i(24, 46);
    glEnd();
    glColor3f(0.2f, 0.9f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2i(27, 27);
    glVertex2i(27 + static_cast<int>(254.0 * pPlayerState.mBoostEnergy), 27);
    glVertex2i(27 + static_cast<int>(254.0 * pPlayerState.mBoostEnergy), 43);
    glVertex2i(27, 43);
    glEnd();

    const int speed = static_cast<int>(std::fabs(pPlayerState.mSpeed));
    const double gateDeltaX = pActiveGate.mX - pPlayerState.mX;
    const double gateDeltaY = pActiveGate.mY - pPlayerState.mY;
    const int gateDistance = static_cast<int>(std::sqrt(gateDeltaX * gateDeltaX + gateDeltaY * gateDeltaY));
    char speedLabel[24];
    char gateLabel[24];
    std::snprintf(speedLabel, sizeof(speedLabel), "SPEED %d", speed);
    std::snprintf(gateLabel, sizeof(gateLabel), "NEXT %d", gateDistance);
    glColor3f(0.82f, 0.9f, 0.92f);
    DrawPixelText(speedLabel, 300, 27, 2);
    glColor3f(0.2f, 0.9f, 1.0f);
    DrawPixelText(gateLabel, 300, 45, 2);

    const int displayedLap = pPlayerProgress.mCompletedLaps + 1 > pTargetLaps
        ? pTargetLaps : pPlayerProgress.mCompletedLaps + 1;
    const int elapsedSeconds = static_cast<int>(pPlayerProgress.mElapsedSeconds);
    char lapLabel[24];
    char timeLabel[32];
    char positionLabel[24];
    std::snprintf(lapLabel, sizeof(lapLabel), "LAP %d OF %d", displayedLap, pTargetLaps);
    std::snprintf(timeLabel, sizeof(timeLabel), "TIME %d M %d S", elapsedSeconds / 60, elapsedSeconds % 60);
    std::snprintf(positionLabel, sizeof(positionLabel), "PLACE %d OF %d", pPlayerPosition, pCompetitorCount);
    glColor3f(0.82f, 0.9f, 0.92f);
    DrawPixelText(lapLabel, pWidth / 2 + 36, 27, 2);
    DrawPixelText(timeLabel, pWidth / 2 + 36, 45, 2);
    glColor3f(1.0f, 0.78f, 0.12f);
    DrawPixelText(positionLabel, pWidth / 2 + 36, 63, 2);

    const double targetHeading = std::atan2(pActiveGate.mY - pPlayerState.mY,
                                            pActiveGate.mX - pPlayerState.mX);
    const double headingOffset = targetHeading - pPlayerState.mHeading;
    const double forwardX = std::sin(headingOffset);
    const double forwardY = -std::cos(headingOffset);
    const double sideX = std::cos(headingOffset);
    const double sideY = std::sin(headingOffset);
    const int pointerX = pWidth / 2;
    const int pointerY = pHeight - 86;
    glColor3f(0.02f, 0.05f, 0.08f);
    glBegin(GL_QUADS);
    glVertex2i(pointerX - 18, pointerY - 15);
    glVertex2i(pointerX + 18, pointerY - 15);
    glVertex2i(pointerX + 18, pointerY + 15);
    glVertex2i(pointerX - 18, pointerY + 15);
    glEnd();
    glColor3f(0.2f, 0.9f, 1.0f);
    glBegin(GL_TRIANGLES);
    glVertex2d(pointerX + forwardX * 11.0, pointerY + forwardY * 11.0);
    glVertex2d(pointerX - forwardX * 6.0 + sideX * 6.0, pointerY - forwardY * 6.0 + sideY * 6.0);
    glVertex2d(pointerX - forwardX * 6.0 - sideX * 6.0, pointerY - forwardY * 6.0 - sideY * 6.0);
    glEnd();

    if (!pWaypoints.empty())
    {
        double minimumX = pWaypoints.front().mX;
        double maximumX = minimumX;
        double minimumY = pWaypoints.front().mY;
        double maximumY = minimumY;
        for (const RaceGate& waypoint : pWaypoints)
        {
            minimumX = std::fmin(minimumX, waypoint.mX);
            maximumX = std::fmax(maximumX, waypoint.mX);
            minimumY = std::fmin(minimumY, waypoint.mY);
            maximumY = std::fmax(maximumY, waypoint.mY);
        }
        const double rangeX = std::fmax(1.0, maximumX - minimumX);
        const double rangeY = std::fmax(1.0, maximumY - minimumY);
        const int mapSize = 132;
        const int mapLeft = pWidth - mapSize - 24;
        const int mapTop = 24;
        const auto mapX = [&](double x)
        {
            return mapLeft + 8 + static_cast<int>((x - minimumX) * (mapSize - 16) / rangeX);
        };
        const auto mapY = [&](double y)
        {
            return mapTop + mapSize - 8 - static_cast<int>((y - minimumY) * (mapSize - 16) / rangeY);
        };

        glColor3f(0.02f, 0.05f, 0.08f);
        glBegin(GL_QUADS);
        glVertex2i(mapLeft, mapTop);
        glVertex2i(mapLeft + mapSize, mapTop);
        glVertex2i(mapLeft + mapSize, mapTop + mapSize);
        glVertex2i(mapLeft, mapTop + mapSize);
        glEnd();
        glColor3f(0.2f, 0.8f, 0.9f);
        glBegin(GL_LINE_LOOP);
        for (const RaceGate& waypoint : pWaypoints)
            glVertex2i(mapX(waypoint.mX), mapY(waypoint.mY));
        glEnd();
        glColor3f(0.2f, 0.8f, 0.9f);
        glPointSize(4.0f);
        glBegin(GL_POINTS);
        glVertex2i(mapX(pWaypoints.front().mX), mapY(pWaypoints.front().mY));
        glEnd();
        glColor3f(1.0f, 0.78f, 0.12f);
        glPointSize(8.0f);
        glBegin(GL_POINTS);
        glVertex2i(mapX(pPlayerState.mX), mapY(pPlayerState.mY));
        glEnd();
        if (pShowRivals)
        {
            glColor3f(0.95f, 0.22f, 0.18f);
            glBegin(GL_POINTS);
            for (const HovercraftState& rivalState : pRivalStates)
                glVertex2i(mapX(rivalState.mX), mapY(rivalState.mY));
            glEnd();
        }
        glPointSize(1.0f);
    }

    for (int lap = 0; lap < pTargetLaps; ++lap)
    {
        const int left = 24 + lap * 34;
        glColor3f(lap < pPlayerProgress.mCompletedLaps ? 1.0f : 0.22f,
                  lap < pPlayerProgress.mCompletedLaps ? 0.78f : 0.25f,
                  lap < pPlayerProgress.mCompletedLaps ? 0.12f : 0.28f);
        glBegin(GL_QUADS);
        glVertex2i(left, 58);
        glVertex2i(left + 28, 58);
        glVertex2i(left + 28, 70);
        glVertex2i(left, 70);
        glEnd();

        for (int rivalIndex = 0; rivalIndex < static_cast<int>(pRivalProgresses.size()); ++rivalIndex)
        {
            const RaceProgress& rivalProgress = pRivalProgresses[rivalIndex];
            const int top = 76 + rivalIndex * 18;
            glColor3f(lap < rivalProgress.mCompletedLaps ? 0.94f : 0.3f,
                      lap < rivalProgress.mCompletedLaps ? 0.22f : 0.12f,
                      lap < rivalProgress.mCompletedLaps ? 0.18f : 0.14f);
            glBegin(GL_QUADS);
            glVertex2i(left, top);
            glVertex2i(left + 28, top);
            glVertex2i(left + 28, top + 12);
            glVertex2i(left, top + 12);
            glEnd();
        }
    }

    if (pWinner != 0)
    {
        if (pWinner == 1)
            glColor3f(0.12f, 0.8f, 0.18f);
        else if (pWinner == 2)
            glColor3f(0.7f, 0.12f, 0.18f);
        else
            glColor3f(0.92f, 0.68f, 0.12f);
        glBegin(GL_QUADS);
        glVertex2i(pWidth / 2 - 110, 28);
        glVertex2i(pWidth / 2 + 110, 28);
        glVertex2i(pWidth / 2 + 110, 42);
        glVertex2i(pWidth / 2 - 110, 42);
        glEnd();
    }

    if (pShowSetupOverlay)
        DrawSetupOverlay(pWidth, pHeight);
    else if (pWinner != 0)
        DrawResultOverlay(pWinner, pPlayerPosition, pCompetitorCount, pChampionship,
                          pChampionshipPoints, pPlayerElapsedSeconds, pWidth, pHeight);

    for (int light = 0; light < 3; ++light)
    {
        glColor3f(light < pStartLights ? 1.0f : 0.18f,
                  light < pStartLights ? (light == 2 ? 0.8f : 0.18f) : 0.12f,
                  light < pStartLights ? 0.12f : 0.14f);
        glBegin(GL_QUADS);
        glVertex2i(pWidth / 2 - 44 + light * 32, pHeight - 48);
        glVertex2i(pWidth / 2 - 20 + light * 32, pHeight - 48);
        glVertex2i(pWidth / 2 - 20 + light * 32, pHeight - 24);
        glVertex2i(pWidth / 2 - 44 + light * 32, pHeight - 24);
        glEnd();
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FOG);
    glEnable(GL_LIGHTING);
}
}

int main()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
    {
        std::fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_Window* window = SDL_CreateWindow("OpenHover", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, kWindowWidth, kWindowHeight,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_GLContext context = window == nullptr ? nullptr : SDL_GL_CreateContext(window);
    if (context == nullptr)
    {
        std::fprintf(stderr, "SDL OpenGL window creation failed: %s\n", SDL_GetError());
        if (window != nullptr)
            SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_SetSwapInterval(1);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FOG);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_NORMALIZE);
    const GLfloat ambientLight[] = {0.26f, 0.3f, 0.34f, 1.0f};
    const GLfloat diffuseLight[] = {0.88f, 0.92f, 0.84f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
    const GLfloat fogColor[] = {0.14f, 0.28f, 0.32f, 1.0f};
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 35.0f);
    glFogf(GL_FOG_END, 110.0f);
    const GLuint roadTexture = CreateRoadTexture();
    const GLuint wallTexture = CreateWallTexture();

    SDL_GameController* controller = nullptr;
    for (int joystick = 0; joystick < SDL_NumJoysticks(); ++joystick)
    {
        if (SDL_IsGameController(joystick))
        {
            controller = SDL_GameControllerOpen(joystick);
            break;
        }
    }

    const std::vector<TrackDefinition>& builtInTracks = BuiltInTracks();
    int trackIndex = 0;
    TrackDefinition selectedTrack = builtInTracks[trackIndex];
    CraftClass playerCraftClass = CraftClass::Balanced;
    Hovercraft hovercraft(CraftClassTuning(playerCraftClass));
    Hovercraft replayGhost(CraftClassTuning(playerCraftClass));
    HovercraftState spawn;
    std::vector<RaceGate> checkpoints = selectedTrack.Checkpoints();
    RaceGate finish = selectedTrack.Finish();
    std::vector<RaceGate> courseWaypoints = selectedTrack.mWaypoints;
    Course course(courseWaypoints, selectedTrack.mRoadHalfWidth);
    std::vector<BoostPad> boostPads = selectedTrack.mBoostPads;
    std::vector<HazardZone> hazardZones = selectedTrack.mHazardZones;
    std::vector<RaisedSection> raisedSections = selectedTrack.mRaisedSections;
    RaceMode raceMode = RaceMode::SingleRace;
    RivalDifficulty rivalDifficulty = RivalDifficulty::Standard;
    int rivalCount = kRivalCount;
    int targetLaps = 3;
    Championship championship(static_cast<int>(builtInTracks.size()));
    Race race(checkpoints, finish, targetLaps);
    std::vector<Race> rivalRaces(kRivalCount, Race(checkpoints, finish, targetLaps));
    LapTimer lapTimer;
    RaceStart raceStart;
    std::vector<RaceGate> rivalRoute = checkpoints;
    rivalRoute.push_back(finish);
    std::vector<RivalTuning> rivalTunings(kRivalCount);
    rivalTunings[0].mPace = 1.0;
    rivalTunings[0].mSteeringGain = 1.35;
    rivalTunings[0].mBoostHeadingError = 0.26;
    rivalTunings[1].mPace = 0.88;
    rivalTunings[1].mSteeringGain = 1.85;
    rivalTunings[1].mBoostHeadingError = 0.14;
    rivalTunings[1].mBoostDistanceMultiplier = 2.8;
    std::vector<RivalController> rivalControllers;
    const auto rebuildRivalControllers = [&]()
    {
        rivalControllers.clear();
        for (int rivalIndex = 0; rivalIndex < kRivalCount; ++rivalIndex)
            rivalControllers.emplace_back(rivalRoute,
                TuneRivalForDifficulty(rivalTunings[rivalIndex], rivalDifficulty));
    };
    rebuildRivalControllers();
    std::vector<Hovercraft> rivals(kRivalCount);
    std::vector<HovercraftState> rivalSpawns(kRivalCount);
    const auto configureStartGrid = [&]()
    {
        const RaceGate& firstCheckpoint = checkpoints.empty() ? finish : checkpoints.front();
        double forwardX = firstCheckpoint.mX - finish.mX;
        double forwardY = firstCheckpoint.mY - finish.mY;
        const double length = std::sqrt(forwardX * forwardX + forwardY * forwardY);
        if (length > 0.0)
        {
            forwardX /= length;
            forwardY /= length;
        }
        else
        {
            forwardX = 1.0;
            forwardY = 0.0;
        }
        spawn = HovercraftState();
        spawn.mX = finish.mX + forwardX * 7.0;
        spawn.mY = finish.mY + forwardY * 7.0;
        spawn.mHeading = std::atan2(forwardY, forwardX);
        spawn.mTravelHeading = spawn.mHeading;
        const double laneX = -forwardY;
        const double laneY = forwardX;
        for (int rivalIndex = 0; rivalIndex < kRivalCount; ++rivalIndex)
        {
            const double laneOffset = rivalIndex == 0 ? -3.2 : 3.2;
            rivalSpawns[rivalIndex] = spawn;
            rivalSpawns[rivalIndex].mX += laneX * laneOffset;
            rivalSpawns[rivalIndex].mY += laneY * laneOffset;
        }
    };
    configureStartGrid();
    hovercraft.Reset(spawn);
    for (int rivalIndex = 0; rivalIndex < kRivalCount; ++rivalIndex)
        rivals[rivalIndex].Reset(rivalSpawns[rivalIndex]);

    Uint64 previousTick = SDL_GetPerformanceCounter();
    const double tickFrequency = static_cast<double>(SDL_GetPerformanceFrequency());
    FixedStepClock simulationClock;
    int winner = 0;
    bool steeringAssistEnabled = false;
    bool brakingAssistEnabled = false;
    bool showControls = false;
    FrontScreen frontScreen = FrontScreen::Welcome;
    int frontSelection = 0;
    int localSetupSelection = 0;
    int cameraDistanceSetting = 1;
    bool weaponsAllowed = false;
    InputRecording activeRecording;
    InputRecording ghostRecording;
    std::size_t ghostFrame = 0;
    bool ghostActive = false;
    const auto resetRace = [&](bool pStartCountdown = true)
    {
        if (!activeRecording.Empty())
        {
            ghostRecording = activeRecording;
            replayGhost = Hovercraft(CraftClassTuning(playerCraftClass));
            replayGhost.Reset(spawn);
            ghostFrame = 0;
            ghostActive = true;
        }
        activeRecording.Clear();
        hovercraft.Reset(spawn);
        race.Reset();
        lapTimer.Reset();
        raceStart.Reset();
        for (int rivalIndex = 0; rivalIndex < kRivalCount; ++rivalIndex)
        {
            rivalRaces[rivalIndex].Reset();
            rivals[rivalIndex].Reset(rivalSpawns[rivalIndex]);
            rivalControllers[rivalIndex].Reset();
        }
        simulationClock.Reset();
        winner = 0;
        if (pStartCountdown)
            raceStart.Begin();
    };
    const auto loadTrack = [&](bool pStartCountdown = true)
    {
        activeRecording.Clear();
        ghostRecording.Clear();
        ghostFrame = 0;
        ghostActive = false;
        selectedTrack = builtInTracks[trackIndex];
        checkpoints = selectedTrack.Checkpoints();
        finish = selectedTrack.Finish();
        courseWaypoints = selectedTrack.mWaypoints;
        course = Course(courseWaypoints, selectedTrack.mRoadHalfWidth);
        boostPads = selectedTrack.mBoostPads;
        hazardZones = selectedTrack.mHazardZones;
        raisedSections = selectedTrack.mRaisedSections;
        race = Race(checkpoints, finish, targetLaps);
        rivalRaces.assign(kRivalCount, Race(checkpoints, finish, targetLaps));
        rivalRoute = checkpoints;
        rivalRoute.push_back(finish);
        configureStartGrid();
        rebuildRivalControllers();
        resetRace(pStartCountdown);
    };
    bool running = true;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0)
        {
            if (event.type == SDL_QUIT)
                running = false;
            if (frontScreen != FrontScreen::RaceSetup)
            {
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                {
                    if (frontScreen == FrontScreen::HowToPlay || frontScreen == FrontScreen::Settings
                        || frontScreen == FrontScreen::LocalSetup)
                        frontScreen = FrontScreen::Welcome;
                    else
                        running = false;
                }
                else if (event.type == SDL_KEYDOWN && (frontScreen == FrontScreen::HowToPlay
                         || frontScreen == FrontScreen::Settings)
                         && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER))
                    frontScreen = FrontScreen::Welcome;
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::Settings
                         && event.key.keysym.sym == SDLK_LEFT)
                    cameraDistanceSetting = cameraDistanceSetting == 0 ? 2 : cameraDistanceSetting - 1;
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::Settings
                         && event.key.keysym.sym == SDLK_RIGHT)
                    cameraDistanceSetting = (cameraDistanceSetting + 1) % 3;
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::LocalSetup)
                {
                    if (event.key.keysym.sym == SDLK_UP)
                        localSetupSelection = (localSetupSelection + 4) % 5;
                    else if (event.key.keysym.sym == SDLK_DOWN)
                        localSetupSelection = (localSetupSelection + 1) % 5;
                    else if (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_RIGHT)
                    {
                        const int direction = event.key.keysym.sym == SDLK_LEFT ? -1 : 1;
                        if (localSetupSelection == 0)
                        {
                            trackIndex = (trackIndex + direction + static_cast<int>(builtInTracks.size()))
                                % static_cast<int>(builtInTracks.size());
                            loadTrack(false);
                        }
                        else if (localSetupSelection == 1)
                        {
                            targetLaps = direction > 0 ? (targetLaps == 5 ? 1 : targetLaps + 1)
                                                       : (targetLaps == 1 ? 5 : targetLaps - 1);
                            loadTrack(false);
                        }
                        else if (localSetupSelection == 2)
                            weaponsAllowed = !weaponsAllowed;
                    }
                    else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)
                    {
                        if (localSetupSelection == 3)
                        {
                            frontScreen = FrontScreen::RaceSetup;
                            resetRace();
                        }
                        else if (localSetupSelection == 4)
                            frontScreen = FrontScreen::Welcome;
                    }
                }
                else if (event.type == SDL_KEYDOWN && frontScreen == FrontScreen::Welcome)
                {
                    if (event.key.keysym.sym == SDLK_UP)
                        frontSelection = (frontSelection + 3) % 4;
                    else if (event.key.keysym.sym == SDLK_DOWN)
                        frontSelection = (frontSelection + 1) % 4;
                    else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)
                    {
                        if (frontSelection == 0)
                        {
                            frontScreen = FrontScreen::LocalSetup;
                            localSetupSelection = 0;
                            loadTrack(false);
                        }
                        else if (frontSelection == 1)
                            frontScreen = FrontScreen::HowToPlay;
                        else if (frontSelection == 2)
                            frontScreen = FrontScreen::Settings;
                        else
                            running = false;
                    }
                }
                continue;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                running = false;
                    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r && winner != 0)
                    {
                        if (raceMode == RaceMode::Championship)
                        {
                            if (championship.AdvanceEvent())
                            {
                                trackIndex = championship.CurrentEvent();
                                loadTrack();
                            }
                            else if (championship.Complete())
                            {
                                championship.Reset();
                                trackIndex = championship.CurrentEvent();
                                loadTrack();
                            }
                            else
                                resetRace();
                        }
                        else
                            resetRace();
                    }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_v)
                steeringAssistEnabled = !steeringAssistEnabled;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_b)
                brakingAssistEnabled = !brakingAssistEnabled;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_x && raceStart.Started()
                && winner == 0)
            {
                const RaceGate& recoveryGate = race.Progress().mNextCheckpoint
                    < static_cast<int>(checkpoints.size())
                    ? checkpoints[race.Progress().mNextCheckpoint] : finish;
                HovercraftState recoveryState = hovercraft.State();
                if (RecoverHovercraftToRoute(recoveryState, course, recoveryGate))
                    hovercraft.Reset(recoveryState);
            }
            if (event.type == SDL_CONTROLLERDEVICEADDED && controller == nullptr
                && SDL_IsGameController(event.cdevice.which))
                controller = SDL_GameControllerOpen(event.cdevice.which);
            if (event.type == SDL_CONTROLLERDEVICEREMOVED && controller != nullptr
                && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller)) == event.cdevice.which)
            {
                SDL_GameControllerClose(controller);
                controller = nullptr;
            }
        }

        const Uint64 currentTick = SDL_GetPerformanceCounter();
        const double frameSeconds = (currentTick - previousTick) / tickFrequency;
        previousTick = currentTick;
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        HovercraftInput input;
        input.mThrottle = (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]
            || keys[SDL_SCANCODE_W] ? 1.0 : 0.0)
            - (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN] ? 1.0 : 0.0);
        input.mSteering = (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT] ? 1.0 : 0.0)
            - (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT] ? 1.0 : 0.0);
        input.mBoost = keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL];
        input.mJump = keys[SDL_SCANCODE_UP];
        if (controller != nullptr)
        {
            input.mSteering += ControllerAxis(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX));
            input.mThrottle += SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 32767.0;
            input.mThrottle -= SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 32767.0;
            input.mBoost = input.mBoost || SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A);
            input.mJump = input.mJump || SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B);
        }
        const int simulationSteps = simulationClock.Consume(frameSeconds);
        for (int step = 0; step < simulationSteps; ++step)
        {
            const double seconds = simulationClock.StepSeconds();
            raceStart.Update(seconds);
            if (raceStart.Started() && winner == 0)
            {
                const RaceGate& simulationGate = race.Progress().mNextCheckpoint
                    < static_cast<int>(checkpoints.size())
                    ? checkpoints[race.Progress().mNextCheckpoint] : finish;
                HovercraftInput playerInput = steeringAssistEnabled
                    ? ApplySteeringAssist(input, hovercraft.State(), simulationGate) : input;
                if (brakingAssistEnabled)
                    playerInput = ApplyBrakingAssist(playerInput, hovercraft.State(), simulationGate);
                hovercraft.Step(playerInput, seconds);
                activeRecording.Record(playerInput);
                if (ghostActive)
                {
                    HovercraftInput ghostInput;
                    if (ghostRecording.InputAt(ghostFrame, ghostInput))
                    {
                        replayGhost.Step(ghostInput, seconds);
                        BounceOffCourseWall(replayGhost, course);
                        ApplyRaisedSections(replayGhost, raisedSections);
                        ApplyBoostPads(replayGhost, boostPads);
                        ++ghostFrame;
                    }
                    else
                        ghostActive = false;
                }
                if (RaceModeUsesRivals(raceMode))
                {
                    for (int rivalIndex = 0; rivalIndex < rivalCount; ++rivalIndex)
                    {
                        rivalControllers[rivalIndex].Update(rivals[rivalIndex].State());
                        HovercraftInput rivalInput = rivalControllers[rivalIndex].InputFor(
                            rivals[rivalIndex].State());
                        rivalInput.mJump = ShouldJumpRaisedSection(rivals[rivalIndex].State(),
                                                                   raisedSections);
                        rivals[rivalIndex].Step(rivalInput, seconds);
                    }
                    for (int rivalIndex = 0; rivalIndex < rivalCount; ++rivalIndex)
                    {
                        HovercraftState playerState = hovercraft.State();
                        HovercraftState rivalState = rivals[rivalIndex].State();
                        if (ResolveRacerCollision(playerState, rivalState))
                        {
                            hovercraft.Reset(playerState);
                            rivals[rivalIndex].Reset(rivalState);
                        }
                    }
                    for (int firstRival = 0; firstRival < rivalCount; ++firstRival)
                    {
                        for (int secondRival = firstRival + 1; secondRival < rivalCount; ++secondRival)
                        {
                            HovercraftState firstState = rivals[firstRival].State();
                            HovercraftState secondState = rivals[secondRival].State();
                            if (ResolveRacerCollision(firstState, secondState))
                            {
                                rivals[firstRival].Reset(firstState);
                                rivals[secondRival].Reset(secondState);
                            }
                        }
                    }
                    for (int rivalIndex = 0; rivalIndex < rivalCount; ++rivalIndex)
                    {
                        Hovercraft& rival = rivals[rivalIndex];
                        BounceOffCourseWall(rival, course);
                        ApplyRaisedSections(rival, raisedSections);
                        ApplyBoostPads(rival, boostPads);
                        ApplyHazardZones(rival, hazardZones, seconds);
                    }
                }
                BounceOffCourseWall(hovercraft, course);
                ApplyRaisedSections(hovercraft, raisedSections);
                ApplyBoostPads(hovercraft, boostPads);
                ApplyHazardZones(hovercraft, hazardZones, seconds);
            }

            const HovercraftState& simulationState = hovercraft.State();
            if (raceStart.Started() && winner == 0)
            {
                race.Update(simulationState.mX, simulationState.mY, seconds);
                lapTimer.Update(race.Progress());
                if (RaceModeUsesRivals(raceMode))
                {
                    for (int rivalIndex = 0; rivalIndex < rivalCount; ++rivalIndex)
                    {
                        const HovercraftState& rivalState = rivals[rivalIndex].State();
                        rivalRaces[rivalIndex].Update(rivalState.mX, rivalState.mY, seconds);
                    }
                }
            }
        }
        const HovercraftState& state = hovercraft.State();
        if (winner == 0 && RaceModeHasFinish(raceMode))
        {
            if (race.Progress().mFinished)
                winner = 1;
            else if (RaceModeUsesRivals(raceMode))
            {
                for (int rivalIndex = 0; rivalIndex < rivalCount; ++rivalIndex)
                {
                    if (rivalRaces[rivalIndex].Progress().mFinished)
                    {
                        winner = 2;
                        break;
                    }
                }
            }
        }
        std::vector<RaceProgress> raceProgresses;
        raceProgresses.push_back(race.Progress());
        std::vector<HovercraftState> rivalStates;
        for (int rivalIndex = 0; rivalIndex < rivalCount; ++rivalIndex)
        {
            raceProgresses.push_back(rivalRaces[rivalIndex].Progress());
            rivalStates.push_back(rivals[rivalIndex].State());
        }
        const int playerPosition = RaceModeUsesRivals(raceMode)
            ? CalculateRacePosition(raceProgresses, 0) : 1;
        const RaceGate& activeGate = race.Progress().mNextCheckpoint < static_cast<int>(checkpoints.size())
            ? checkpoints[race.Progress().mNextCheckpoint] : finish;
        const bool wrongWay = !race.Progress().mFinished && IsHeadingAwayFromGate(state, activeGate);
        if (raceMode == RaceMode::Championship && winner != 0 && !championship.EventRecorded())
        {
            std::vector<int> eventPositions;
            for (int competitorIndex = 0; competitorIndex < static_cast<int>(raceProgresses.size()); ++competitorIndex)
                eventPositions.push_back(CalculateRacePosition(raceProgresses, competitorIndex));
            championship.RecordResults(eventPositions);
        }
        char title[320];
        char championshipStandings[96];
        char championshipOverlayPoints[96];
        if (championship.CompetitorCount() == 3)
        {
            std::snprintf(championshipStandings, sizeof(championshipStandings),
                          "You %d | R1 %d | R2 %d", championship.CompetitorPoints(0),
                          championship.CompetitorPoints(1), championship.CompetitorPoints(2));
            std::snprintf(championshipOverlayPoints, sizeof(championshipOverlayPoints),
                          "YOU %d R1 %d R2 %d", championship.CompetitorPoints(0),
                          championship.CompetitorPoints(1), championship.CompetitorPoints(2));
        }
        else if (championship.CompetitorCount() == 2)
        {
            std::snprintf(championshipStandings, sizeof(championshipStandings), "You %d | R1 %d",
                          championship.CompetitorPoints(0), championship.CompetitorPoints(1));
            std::snprintf(championshipOverlayPoints, sizeof(championshipOverlayPoints), "YOU %d R1 %d",
                          championship.CompetitorPoints(0), championship.CompetitorPoints(1));
        }
        else
        {
            std::snprintf(championshipStandings, sizeof(championshipStandings), "You %d",
                          championship.PlayerPoints());
            std::snprintf(championshipOverlayPoints, sizeof(championshipOverlayPoints), "YOU %d",
                          championship.PlayerPoints());
        }
        if (frontScreen != FrontScreen::RaceSetup)
            std::snprintf(title, sizeof(title), "OpenHover | Welcome");
        else if (raceStart.Ready())
        {
            if (showControls)
                std::snprintf(title, sizeof(title), "OpenHover | R start | T track | M mode | C craft | L laps | 0/1/2 rivals | D difficulty | V/B assists | F1 close");
            else if (raceMode == RaceMode::Championship)
                std::snprintf(title, sizeof(title), "OpenHover | Championship %d/%d | %s | %d laps | %d rivals | %s AI | %s craft | R start | F1 controls",
                              championship.CurrentEvent() + 1, championship.EventCount(),
                              championshipStandings, targetLaps, rivalCount,
                              RivalDifficultyName(rivalDifficulty), CraftClassName(playerCraftClass));
            else
                std::snprintf(title, sizeof(title), "OpenHover | %s | %s | %d laps | %d rivals | %s AI | %s craft | Assist S:%s B:%s | R start | F1 controls",
                              selectedTrack.mName.c_str(), RaceModeName(raceMode), targetLaps, rivalCount,
                              RivalDifficultyName(rivalDifficulty), CraftClassName(playerCraftClass),
                              steeringAssistEnabled ? "on" : "off",
                              brakingAssistEnabled ? "on" : "off");
        }
        else if (raceStart.CountdownActive())
            std::snprintf(title, sizeof(title), "OpenHover | Starting | %d lights", raceStart.LightsLit());
        else if (winner == 1)
        {
            if (raceMode == RaceMode::Championship)
                std::snprintf(title, sizeof(title), "OpenHover | You finish P%d/%d: %.2fs | %s | Press R to continue",
                              playerPosition, rivalCount + 1, race.Progress().mElapsedSeconds,
                              championshipStandings);
            else
                std::snprintf(title, sizeof(title), "OpenHover | Finish P%d/%d: %.2fs | Press R to restart",
                              playerPosition, RaceModeUsesRivals(raceMode) ? rivalCount + 1 : 1,
                              race.Progress().mElapsedSeconds);
        }
        else if (winner == 2)
        {
            if (raceMode == RaceMode::Championship)
                std::snprintf(title, sizeof(title), "OpenHover | You finish P%d/%d | %s | Press R to continue",
                              playerPosition, rivalCount + 1, championshipStandings);
            else
                std::snprintf(title, sizeof(title), "OpenHover | Rival finishes first | You P%d/%d | Press R to restart",
                              playerPosition, RaceModeUsesRivals(raceMode) ? rivalCount + 1 : 1);
        }
        else if (raceMode == RaceMode::Practice)
            std::snprintf(title, sizeof(title), "OpenHover | %s | Practice | Speed %.1f | Boost %.0f%%",
                          selectedTrack.mName.c_str(), state.mSpeed, state.mBoostEnergy * 100.0);
        else
        {
            const LapTiming& lapTiming = lapTimer.Timing();
            if (race.Progress().mNextCheckpoint == static_cast<int>(checkpoints.size()))
                std::snprintf(title, sizeof(title), "OpenHover | P%d/%d | Lap %d/%d | Finish! | %.2fs | Split %.2fs",
                              playerPosition, RaceModeUsesRivals(raceMode) ? rivalCount + 1 : 1,
                              race.Progress().mCompletedLaps + 1, race.TargetLaps(),
                              lapTiming.mCurrentSeconds, lapTiming.mLastSplitSeconds);
            else
                std::snprintf(title, sizeof(title), "OpenHover | P%d/%d | Lap %d/%d | %s %d/%d | %.2fs | Split %.2fs",
                              playerPosition, RaceModeUsesRivals(raceMode) ? rivalCount + 1 : 1,
                              race.Progress().mCompletedLaps + 1, race.TargetLaps(),
                              wrongWay ? "Wrong way, checkpoint" : "Checkpoint",
                              race.Progress().mNextCheckpoint + 1, static_cast<int>(checkpoints.size()),
                              lapTiming.mCurrentSeconds, lapTiming.mLastSplitSeconds);
        }
        SDL_SetWindowTitle(window, title);

        int drawableWidth = 0;
        int drawableHeight = 0;
        SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);
        glViewport(0, 0, drawableWidth, drawableHeight);
        if (frontScreen != FrontScreen::RaceSetup)
        {
            DrawFrontScreen(frontScreen,
                            frontScreen == FrontScreen::LocalSetup ? localSetupSelection : frontSelection,
                            cameraDistanceSetting, trackIndex, targetLaps, weaponsAllowed,
                            drawableWidth, drawableHeight);
            SDL_GL_SwapWindow(window);
            continue;
        }
        const GLfloat atmosphere[] = {selectedTrack.mAtmosphereRed, selectedTrack.mAtmosphereGreen,
                          selectedTrack.mAtmosphereBlue, 1.0f};
        glFogfv(GL_FOG_COLOR, atmosphere);
        glClearColor(selectedTrack.mAtmosphereRed, selectedTrack.mAtmosphereGreen,
                 selectedTrack.mAtmosphereBlue, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        SetPerspective(static_cast<double>(drawableWidth) / drawableHeight, state.mSpeed);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        const double cameraDistances[] = {4.6, 5.8, 7.4};
        SetChaseCamera(state, cameraDistances[cameraDistanceSetting]);
        const GLfloat sunDirection[] = {-0.35f, 0.82f, 0.45f, 0.0f};
        glLightfv(GL_LIGHT0, GL_POSITION, sunDirection);
        DrawCourseGrid(state);
        DrawTrackLandmarks(courseWaypoints);
        DrawConnectedTrack(courseWaypoints, selectedTrack.mRoadHalfWidth,
                   selectedTrack.mRoadRed, selectedTrack.mRoadGreen, selectedTrack.mRoadBlue,
                   selectedTrack.mWallRed, selectedTrack.mWallGreen, selectedTrack.mWallBlue, roadTexture,
                   wallTexture);
        DrawFinishZone(courseWaypoints, selectedTrack.mRoadHalfWidth);
        for (const BoostPad& pad : boostPads)
            DrawBoostPad(pad);
        for (const HazardZone& zone : hazardZones)
            DrawHazardZone(zone);
        for (const RaisedSection& section : raisedSections)
            DrawRaisedSection(section);

        if (RaceModeUsesRivals(raceMode))
        {
            for (const HovercraftState& rivalState : rivalStates)
                DrawHovercraft(rivalState, true);
        }
        if (ghostActive)
            DrawHovercraft(replayGhost.State(), false, true, playerCraftClass);
        DrawHovercraft(state, false, false, playerCraftClass);
        std::vector<RaceProgress> rivalProgresses(raceProgresses.begin() + 1, raceProgresses.end());
        DrawHud(race.Progress(), race.TargetLaps(), rivalProgresses, state, rivalStates, activeGate,
            courseWaypoints,
            RaceModeUsesRivals(raceMode), winner, playerPosition,
            RaceModeUsesRivals(raceMode) ? rivalCount + 1 : 1,
            raceMode == RaceMode::Championship, raceStart.LightsLit(), raceStart.CountdownActive(),
            championshipOverlayPoints, race.Progress().mElapsedSeconds, drawableWidth, drawableHeight);
        SDL_GL_SwapWindow(window);
    }

    if (controller != nullptr)
        SDL_GameControllerClose(controller);
    glDeleteTextures(1, &roadTexture);
    glDeleteTextures(1, &wallTexture);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}