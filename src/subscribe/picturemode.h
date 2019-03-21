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

#include "errors.h"
#include <map>
#include <pbnjson.h>
#include <string>

const uint8_t MIN_SHARPNESS = 0;
const uint8_t MAX_SHARPNESS = 50;

const uint8_t MIN_BASICPQ_VALUE = 0;
const uint8_t MAX_BASICPQ_VALUE = 100;

const uint8_t MAX_TINT_VALUE = 50;

class PictureMode
{

public:
    PictureMode(const pbnjson::JValue modeJson);

    bool setProperties(pbnjson::JValue modeJson);

    int8_t tint() { return mTint; }
    uint8_t color() { return mColor; }
    uint8_t contrast() { return mContrast; }
    uint8_t brightness() { return mBrightness; }

    uint8_t sharpness() { return mSharpness; }
    uint8_t hSharpness() { return mHSharpness; }
    uint8_t vSharpness() { return mVSharpness; }

    const static pbnjson::JValue defaultJson;

private:
    uint8_t mBrightness = MAX_BASICPQ_VALUE;
    uint8_t mContrast   = MAX_BASICPQ_VALUE;

    uint8_t mColor = MAX_BASICPQ_VALUE;

    int8_t mTint = 0;

    uint8_t mSharpness  = MAX_SHARPNESS;
    uint8_t mHSharpness = MAX_SHARPNESS;
    uint8_t mVSharpness = MAX_SHARPNESS;
};
