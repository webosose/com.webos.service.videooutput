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

#include "picturesettings.h"
#include "picturemode.h"
#include "videoservice.h"

using namespace LSHelpers;

PictureSettings::PictureSettings(LS::Handle &handle, VideoService &video)
    : mModeData(PictureMode::defaultJson), mAdapter(handle, this, &PictureSettings::handleResponseCb),
      mVideoService(video)
{
    mAdapter.subscribeTo(pbnjson::JObject{{"subscribe", true}, {"category", "picture"}}, SettingService, this,
                         &PictureSettings::responseParserCb);
}

pbnjson::JValue PictureSettings::responseParserCb(JsonResponse &response)
{
    bool returnValue;
    std::string category;
    pbnjson::JValue settings;
    pbnjson::JValue errorKey;
    bool subscribed;
    std::string method;
    std::string app_id;
    std::string caller;

    response.get("returnValue", returnValue);
    response.get("category", category);
    response.get("method", method);
    response.get("settings", settings);
    response.get("subscribed", subscribed).optional();
    response.get("app_id", app_id).optional();
    response.get("caller", caller).optional();
    response.get("errorKey", errorKey).optional();
    response.finishParse();

    LOG_DEBUG("responseParserCb ret:%d, category:%s, method:%s", returnValue, category.c_str(), method.c_str());

    if (returnValue == false) {
        LOG_WARNING(MSGID_GET_SYSTEM_SETTINGS_ERROR, 0,
                    "Could not register requested settings. category(%s), method(%s)", category.c_str(),
                    method.c_str());
        return pbnjson::JValue();
    }

    if (category == "picture") {
        return response.getJson();
    }
    return pbnjson::JValue();
}

// Called on both picture mode change and new picture more settings read and picture mode settings change.
void PictureSettings::handleResponseCb(JValue &settingsRespone)
{
    std::string modeChangeName;
    std::string pictureMode;
    JsonParser parser{settingsRespone};
    JsonParser dimObj      = parser.getObject("dimension");
    JsonParser settingsObj = parser.getObject("settings");

    settingsObj.get("pictureMode", modeChangeName).optional().defaultValue("");
    // When getting response from mode settings read, picture mode is in dimension.pictureMode.
    dimObj.get("pictureMode", pictureMode).optional().defaultValue("");

    LOG_DEBUG("handleResponseCb pictureMode:%s, modechange:%s", pictureMode.c_str(), modeChangeName.c_str());

    FINISH_PARSE_OR_RETURN_FALSE(dimObj);
    FINISH_PARSE_OR_RETURN_FALSE(parser);
    FINISH_PARSE_OR_RETURN_FALSE(settingsObj);

    if (!modeChangeName.empty() && modeChangeName != mCurrentMode) {
        mCurrentMode = modeChangeName;
        // new mode, fetch the settings for it.
        fetchPictureModeParams(modeChangeName);
    } else if (!pictureMode.empty() &&
               pictureMode == mCurrentMode) // response from new picture mode params query or subscription update
    {
        mModeData.setProperties(settingsObj.getJson());

        // Update Settings that were changed
        if (settingsObj.hasKey("brightness") || settingsObj.hasKey("contrast") || settingsObj.hasKey("color") ||
            settingsObj.hasKey("tint")) {
            mVideoService.setBasicPictureCtrl(mModeData.brightness(), mModeData.contrast(), mModeData.color(),
                                              mModeData.tint());
        }
        if (settingsObj.hasKey("sharpness") || settingsObj.hasKey("hSharpness") || settingsObj.hasKey("vSharpness")) {
            mVideoService.setSharpness(mModeData.sharpness(), mModeData.hSharpness(), mModeData.vSharpness());
        }
    }
    return;
}

// This cancels previous fetch call.
void PictureSettings::fetchPictureModeParams(const std::string &modeName)
{
    mAdapter.makeOneCall(
        pbnjson::JObject{{"dimension", {{"_3dStatus", "2d"}, {"input", "default"}, {"pictureMode", modeName}}},
                         {"category", "picture"}});
};
