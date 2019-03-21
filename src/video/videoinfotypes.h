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

#include "ls2-helpers.hpp"

class VideoInfo
{
public:
    VideoInfo() : sourceName("unknown"){};
    virtual ~VideoInfo(){};
    std::string contentType;
    std::string sourceName;

    // define common members
    pbnjson::JValue videoinfo_jval;

    static VideoInfo *init(std::string contentType, std::string sourceName, pbnjson::JValue info);
    static bool isValidSource(std::string name);
    virtual pbnjson::JValue toJValue() { return pbnjson::JValue(); }; // Optional function
    virtual bool set(pbnjson::JValue videoinfo) = 0;
    virtual void debug_print(std::string prefix) const {};
};

typedef enum {
    VIDEO_INFO_TYPE_MEDIA,
    VIDEO_INFO_TYPE_HDMI,
    VIDEO_INFO_TYPE_MAX
} VIDEO_INFO_TYPE_T;

typedef enum {
    VIDEO_SOURCE_TYPE_VDEC,
    VIDEO_SOURCE_TYPE_HDMI,
    VIDEO_SOURCE_TYPE_JPEG,
    VIDEO_SOURCE_TYPE_MAX
} VIDEO_SOURCE_TYPE_T;

/* For VideoInfoMedia */
typedef struct {
    uint8_t transferCharacteristics;
    uint8_t colorPrimaries;
    uint8_t matrixCoeffs;
    bool videoFullRangerFlag;
} VUI_T;

typedef struct {
    uint16_t displayPrimariesX0;
    uint16_t displayPrimariesX1;
    uint16_t displayPrimariesX2;
    uint16_t displayPrimariesY0;
    uint16_t displayPrimariesY1;
    uint16_t displayPrimariesY2;
    uint16_t whitePointX;
    uint16_t whitePointY;
    uint32_t minDisplayMasteringLuminance;
    uint32_t maxDisplayMasteringLuminance;
    uint16_t maxContentLightLevel;
    uint16_t maxPicAverageLightLevel;
} SEI_T;

typedef struct {
    uint16_t width;
    uint16_t height;
} PIXEL_ASPECT_RATIO_T;

class VideoInfoMedia : public VideoInfo
{
public:
    VideoInfoMedia(std::string contentType, std::string sourceName);
    virtual ~VideoInfoMedia(){};

    virtual pbnjson::JValue toJValue();
    virtual bool set(pbnjson::JValue videoinfo);
    virtual void debug_print(std::string prefix) const;

    std::string hdrType; // HDR information
    int16_t afd;         // Active Format Description
    PIXEL_ASPECT_RATIO_T pixelAspectRatio;
    int32_t bitRate;
    bool adaptive;
    std::string rotation; // "Deg0" / "Deg90" / "Deg180" / "Deg270"
    std::string path;     // "file" / "network"
    VUI_T vui;
    SEI_T sei;
};

/* For VideoInfoHDMI*/
class VideoInfoHDMI : public VideoInfo
{
public:
    VideoInfoHDMI(std::string contentType, std::string sourceName);
    virtual ~VideoInfoHDMI(){};

    virtual pbnjson::JValue toJValue();
    virtual bool set(pbnjson::JValue videoinfo);
    virtual void debug_print(std::string prefix) const;

    std::string hdrType; // HDR information
    int16_t afd;         // Active Format Description
    bool enableJustScan;
    std::string timingMode;
    std::string HDMIMode;
    std::string pixelEncoding;
    std::string colormetry;
    std::string extendedColormetry;
};
