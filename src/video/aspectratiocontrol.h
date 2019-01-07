// Copyright (c) 2016-2019 LG Electronics, Inc.
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

#pragma once

#include "videoservicetypes.h"

const auto Ratio16X9       = 16.0 / 9;
const auto Ratio4X3        = 4.0 / 3;
const auto VertZoomRange   = 9; //-8 to 9
const auto AllDirZoomRange = 15;
typedef enum {
    MINUMUM,
    MODE_16_9 = 0,
    MODE_ORIGINAL,
    FULLWIDE,
    MODE_4_3,
    MODE_VERTICALZOOM,
    MODE_ALLDIRECTIONZOOM,
    MODE_32_9,
    MODE_32_12,
    MODE_TWINZOOM,
    MODE_T_MAX
} ARC_MODE_NAME_MAP_T;

class AspectRatioControl
{
public:
    bool scaleWindow(const VideoRect &screenRect, const VideoRect &sourceRect, VideoRect &inputRect,
                     VideoRect &outputRect);
    void applyOverScan(VideoRect &inputRect, const VideoRect &sourceRect);
    void setParams(ARC_MODE_NAME_MAP_T currentAspectMode, int32_t allDirZoomHPosition, int32_t allDirZoomHRatio,
                   int32_t allDirZoomVPosition, int32_t allDirZoomVRatio, int32_t vertZoomVRatio,
                   int32_t vertZoomVPosition);

private:
    ARC_MODE_NAME_MAP_T mCurrentAspectMode;

    //
    int32_t mAllDirZoomVRatio;    // 0 to 15
    int32_t mAllDirZoomVPosition; //-15 to +15

    int32_t mAllDirZoomHRatio;    // 0-15
    int32_t mAllDirZoomHPosition; //-15 to +15

    int32_t mVertZoomVRatio;    //-8 to +9
    int32_t mVertZoomVPosition; //-18 or -17 to 18

    bool mJustScan;
};
