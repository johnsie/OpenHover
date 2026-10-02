// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_CAMERA_RIG_H
#define OPENHOVER_CAMERA_RIG_H

enum class CameraMotion
{
    Standard, // locked to craft heading, field of view widens with speed
    Smooth,   // heading follows the craft with a short lag through turns
    Reduced   // locked heading, constant field of view
};

CameraMotion NextCameraMotion(CameraMotion pMotion, int pDirection);
const char* CameraMotionName(CameraMotion pMotion);

class CameraRig
{
public:
    void Reset(double pHeading);
    // Returns the heading the camera should face this frame.
    double Update(double pCraftHeading, double pFrameSeconds, CameraMotion pMotion);
    // Smoothed height the camera should rise by so the craft stays framed during jumps and
    // over raised sections.
    double UpdateRise(double pCraftHeight, double pFrameSeconds, CameraMotion pMotion);
    static double SpeedZoomScale(CameraMotion pMotion);

private:
    double mHeading = 0.0;
    double mHeight = 0.0;
};

#endif
