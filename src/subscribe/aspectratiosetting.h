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

#pragma once

#include "aspectratiocontrol.h"
#include "subscribeadapter.h"
#include <ls2-helpers/ls2-helpers.hpp>

class AspectRatioSetting;
class VideoService;

class AspectRatioSetting
{
public:
    const std::string SettingService = "luna://com.webos.service.settings/getSystemSettings";

    AspectRatioSetting(LS::Handle &serviceHandle, VideoService &video);
    pbnjson::JValue responseParserCb(JsonResponse &response);

private:
    void handleResponseCb(pbnjson::JValue &settingsResponse);
    void fetchAspectRatioforApp(std::string &appId);

    SubscribeAdapter<AspectRatioSetting> mAdapter;
    VideoService &mVideoService;

    ARC_MODE_NAME_MAP_T mCurrentAspectMode;

    int32_t mAllDirZoomVRatio;    // 0 to 15
    int32_t mAllDirZoomVPosition; //-15 to +15

    int32_t mAllDirZoomHRatio;    // 0-15
    int32_t mAllDirZoomHPosition; //-15 to +15

    int32_t mVertZoomVRatio;    //-8 to +9
    int32_t mVertZoomVPosition; //-18 or -17 to 18

    bool mJustScan;
};
