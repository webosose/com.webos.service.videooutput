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

#include <ls2-helpers/ls2-helpers.hpp>

#include "picturemode.h"
#include "subscribeadapter.h"

using namespace pbnjson;
class VideoService;

class PictureSettings
{
public:
    const std::string SettingService = "luna://com.webos.service.settings/getSystemSettings";

    PictureSettings(LS::Handle &serviceHandle, VideoService &video);
    pbnjson::JValue responseParserCb(JsonResponse &response);

private:
    void handleResponseCb(JValue &settingsJValue);
    void fetchPictureModeParams(const std::string &modeName);

    // TODO::Where and how to load the Tables.

    std::string mCurrentMode;
    PictureMode mModeData;

    SubscribeAdapter<PictureSettings> mAdapter;
    VideoService &mVideoService;
};
