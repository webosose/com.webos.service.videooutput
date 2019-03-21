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

#include "picturemode.h"
#include "logging.h"
#include "ls2-helpers.hpp"

using namespace pbnjson;
const pbnjson::JValue PictureMode::defaultJson =
    pbnjson::JObject{{"color", 55},     {"brightness", 50}, {"hSharpness", 25}, {"vSharpness", 25},
                     {"sharpness", 25}, {"tint", 0},        {"contrast", 95}};

PictureMode::PictureMode(const JValue modeJson) { setProperties(modeJson); }

bool PictureMode::setProperties(pbnjson::JValue modeJson)
{
    LSHelpers::JsonParser modeParser{modeJson};
    modeParser.get("contrast", mContrast).optional().min(MIN_BASICPQ_VALUE).max(MAX_BASICPQ_VALUE);
    modeParser.get("brightness", mBrightness).optional().min(MIN_BASICPQ_VALUE).max(MAX_BASICPQ_VALUE);
    modeParser.get("color", mColor).optional().min(MIN_BASICPQ_VALUE).max(MAX_BASICPQ_VALUE);
    modeParser.get("tint", mTint).optional().min(-MAX_TINT_VALUE).max(MAX_TINT_VALUE);
    modeParser.get("sharpness", mSharpness).optional().min(MIN_SHARPNESS).max(MAX_SHARPNESS);
    modeParser.get("hSharpness", mHSharpness).optional().min(MIN_SHARPNESS).max(MAX_SHARPNESS);
    modeParser.get("vSharpness", mVSharpness).optional().min(MIN_SHARPNESS).max(MAX_SHARPNESS);
    FINISH_PARSE_OR_RETURN_FALSE(modeParser);
    return true;
}
