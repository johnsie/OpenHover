// SPDX-License-Identifier: MIT OR Apache-2.0
#include "InputRecording.h"

void InputRecording::Clear()
{
    mInputs.clear();
}

void InputRecording::Record(const HovercraftInput& pInput)
{
    mInputs.push_back(pInput);
}

bool InputRecording::InputAt(std::size_t pFrame, HovercraftInput& pOutInput) const
{
    if (pFrame >= mInputs.size())
        return false;
    pOutInput = mInputs[pFrame];
    return true;
}