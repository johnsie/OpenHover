// SPDX-License-Identifier: MIT OR Apache-2.0
#ifndef OPENHOVER_INPUT_RECORDING_H
#define OPENHOVER_INPUT_RECORDING_H

#include "Hovercraft.h"

#include <cstddef>
#include <vector>

class InputRecording
{
public:
    void Clear();
    void Record(const HovercraftInput& pInput);
    bool InputAt(std::size_t pFrame, HovercraftInput& pOutInput) const;
    std::size_t FrameCount() const { return mInputs.size(); }
    bool Empty() const { return mInputs.empty(); }

private:
    std::vector<HovercraftInput> mInputs;
};

#endif