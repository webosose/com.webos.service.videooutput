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

#include <string>
#include <unordered_map>
#include <vector>

#include "ls2-helpers.hpp"
#include "aspectratiosetting.h"
#include "picturesettings.h"
#include "videoinfotypes.h"
#include "videoservicetypes.h"
#include <val_api.h>

class VideoService
{
public:
    VideoService(LS::Handle &handle);
    VideoService(const VideoService &) = delete;
    VideoService &operator=(const VideoService &) = delete;
    ~VideoService();

    // Luna handlers
    pbnjson::JValue _register(LSHelpers::JsonRequest &request);
    pbnjson::JValue unregister(LSHelpers::JsonRequest &request);
    pbnjson::JValue connect(LSHelpers::JsonRequest &request);
    pbnjson::JValue disconnect(LSHelpers::JsonRequest &request);
    pbnjson::JValue blankVideo(LSHelpers::JsonRequest &request);
    pbnjson::JValue setDisplayWindow(LSHelpers::JsonRequest &request);
    pbnjson::JValue setVideoData(LSHelpers::JsonRequest &request);
    pbnjson::JValue setCompositing(LSHelpers::JsonRequest &request);
    pbnjson::JValue getVideoLimits(LSHelpers::JsonRequest &request);
    pbnjson::JValue getOutputCapabilities(LSHelpers::JsonRequest &request);
    pbnjson::JValue getStatus(LSHelpers::JsonRequest &request);
    pbnjson::JValue getSupportedResolutions(LSHelpers::JsonRequest &request);
    pbnjson::JValue setDisplayResolution(LSHelpers::JsonRequest &request);
    pbnjson::JValue getParam(LSHelpers::JsonRequest &request);
    pbnjson::JValue setParam(LSHelpers::JsonRequest &request);

    inline void setAppIdChangedObserver(AspectRatioSetting *object,
                                        void (AspectRatioSetting::*callbackHandler)(std::string &appId))
    {
        mAppIdChangedNotify = std::bind(callbackHandler, object, std::placeholders::_1);
    };

    pbnjson::JValue setAspectRatio(ARC_MODE_NAME_MAP_T settings, int32_t i, int32_t i1, int32_t i2, int32_t i3,
                                   int32_t i4, int32_t i5);
    pbnjson::JValue setBasicPictureCtrl(int8_t brightness, int8_t contrast, int8_t saturation, int8_t hue);
    pbnjson::JValue setSharpness(int8_t sharpness, int8_t hSharpness, int8_t vSharpness);

private:
    VAL *val;

    VideoSink *getVideoSink(const std::string &sinkName);
    bool setDualVideo(bool enable);

    void sendSinkUpdateToSubscribers();

    void readVideoCapabilities(VideoSink &sink);
    bool applyVideoOutputRects(VideoSink &sink, VideoClient client, VideoRect &inputRect, VideoRect &outputRect,
                               VideoRect &SourceRect);
    bool applyVideoFilters(VideoSink &sink, const std::string &sourceName);
    bool applyCompositing();

    bool doDisconnectVideo(VideoSink &video);

    bool initI2C();

    bool addClientInfo(std::string clientId);
    bool removeClientInfo(std::string clientId);
    VideoClient *getClientInfo(const std::string clientId);
    VideoClient *getClientInfo(const std::string sinkName, bool activation);
    bool LoadClientInfotoVideoSink(VideoSink &sink, VideoClient &client);

    pbnjson::JValue buildStatus();
    pbnjson::JValue buildVideoSinkStatus(VideoSink &vsink);

    void converToDisplayResolution(VideoRect &outputRect);

    // Data members
    std::vector<VideoSink> mSinks;
    std::vector<VideoClient> mClients;

    LSHelpers::ServicePoint mService;
    LSHelpers::SubscriptionPoint mSinkStatusSubscription;

    bool mDualVideoEnabled;

    AspectRatioControl mAspectRatioControl;

    typedef std::function<void(std::string &)> AppIDChangeSettingsCallback;
    AppIDChangeSettingsCallback mAppIdChangedNotify;
};
