// Copyright (c) 2016-2018 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "aspectratiocontrol.h"
#include "logging.h"

#define OVERSCAN_HPIXEL 42
#define OVERSCAN_VPIXEL 24

void AspectRatioControl::setParams(ARC_MODE_NAME_MAP_T currentAspectMode, int32_t allDirZoomHPosition,
                                   int32_t allDirZoomHRatio, int32_t allDirZoomVPosition, int32_t allDirZoomVRatio,
                                   int32_t vertZoomVRatio, int32_t vertZoomVPosition)
{
    LOG_DEBUG("setParams currentAspectMode:%d, mJustScan:%d", currentAspectMode, mJustScan);

    mCurrentAspectMode   = currentAspectMode;
    mAllDirZoomHPosition = allDirZoomHPosition;
    mAllDirZoomHRatio    = allDirZoomHRatio;
    mAllDirZoomVPosition = allDirZoomVPosition;
    mAllDirZoomVRatio    = allDirZoomVRatio;
    mVertZoomVRatio      = vertZoomVRatio;
    mVertZoomVPosition   = vertZoomVPosition;
    mJustScan            = true; // TODO(ekwang):init with temp value
}

void AspectRatioControl::applyOverScan(VideoRect &inputRect, const VideoRect &sourceRect)
{
    if (!mJustScan && sourceRect.w > OVERSCAN_HPIXEL && sourceRect.h > OVERSCAN_VPIXEL) {
        // TODO:: How much overscan pixels.
        // Fixing it to 24 and 42
        inputRect.w = sourceRect.w - OVERSCAN_HPIXEL;
        inputRect.h = sourceRect.h - OVERSCAN_VPIXEL;
    }
}

// inputRect and outputRect are output parameters
// screenRect and sourceRect are sink's screen and frame rects
bool AspectRatioControl::scaleWindow(const VideoRect &screenRect, const VideoRect &sourceRect, VideoRect &inputRect,
                                     VideoRect &outputRect)
{
    if (!sourceRect.isValid()) {
        // Correct values would be handled in setMediadata
        LOG_DEBUG("Invalid frame rectangle");
        return false;
    }
    // InputRect and FrameRect are the same if overscan is applied.

    outputRect  = screenRect; // sinks screenRect
    inputRect.x = sourceRect.x;
    inputRect.y = sourceRect.y;
    inputRect.w = sourceRect.w;
    inputRect.h = sourceRect.h; // sink's inputRect

    applyOverScan(inputRect, sourceRect);

    LOG_DEBUG("scaleWindow currentAspectMode:%d", mCurrentAspectMode);

    if (mCurrentAspectMode == MODE_16_9) {
        outputRect.w = screenRect.h * Ratio16X9; // w = 3840, h = 2160
    } else if (mCurrentAspectMode == MODE_4_3) {
        outputRect.w = screenRect.h * 1.0 / Ratio4X3; // w = 2880, h = 2160
        outputRect.x = (screenRect.w - outputRect.w) * 1.0 / 2;
    } else if (mCurrentAspectMode == MODE_ORIGINAL) {
        // source info or framerect for livetv:   {x = 0, y = 0, w = 1280, h = 720},{x = 0, y = 0, w = 720, h = 480}
        // translates to
        // input rectangle respectively:          {x = 32, y = 18, w = 1216, h = 684}, {x = 18, y = 22, w = 684, h =
        // 446}
        // outputRectangle                        {x = 0, y = 0, w = 3840, h = 2160}, {x = 480, y = 0, w = 2880, h =
        // 2160}
        // For Streaming video source info and input rectangles are same. The output Rectangle is always 3840x2160.

        outputRect.w = screenRect.w;
        outputRect.h = sourceRect.h * (1.0 * screenRect.w) / sourceRect.w;
        outputRect.x = (screenRect.w - outputRect.w) * 1.0 / 2;
    } else if (mCurrentAspectMode == MODE_VERTICALZOOM) {
        outputRect.h = screenRect.h;
        outputRect.w = screenRect.h * Ratio16X9; // w = 3840, h = 2160

        // Changes only to inputRect
        auto reSizeStep     = 2.0 * sourceRect.h / 100; // random -- estimated from hal-debugs
        auto rePositionStep = 1.0 * reSizeStep / 2;

        inputRect.h = inputRect.h + reSizeStep * mVertZoomVRatio;
        inputRect.y = (sourceRect.h - inputRect.h) * 1.0 / 2;
        inputRect.y = inputRect.y + rePositionStep * mVertZoomVPosition; // revisit
    } else if (mCurrentAspectMode == MODE_ALLDIRECTIONZOOM) {
        auto vReSizeStep     = 2.0 * sourceRect.h / 100; // random
        auto vRePositionStep = 1.0 * vReSizeStep / 2;
        auto hReSizeStep     = 2.0 * sourceRect.w / 100;
        auto hRePositionStep = 1.0 * hReSizeStep / 2;

        outputRect.h = screenRect.h;
        outputRect.w = screenRect.h * Ratio16X9; // w = 3840, h = 2160 //TODO::Is this required?

        inputRect.h = inputRect.h - (vReSizeStep * mAllDirZoomVRatio);
        // inputRect.y = inputRect.y + vRePositionStep/2.0 * (mAllDirZoomVRatio - AllDirZoomRange/2.0);
        inputRect.y = (sourceRect.h - inputRect.h) * 1.0 / 2;
        inputRect.y = inputRect.y + (vRePositionStep * mAllDirZoomVPosition);
        // TODO:: Bogus

        inputRect.w = inputRect.w - (hReSizeStep * mAllDirZoomHRatio);

        inputRect.x = (sourceRect.w - inputRect.w) * 1.0 / 2;
        inputRect.x = inputRect.x + (hRePositionStep * mAllDirZoomHPosition);
    }
    return true;
}
