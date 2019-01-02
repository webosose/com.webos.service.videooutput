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

#include "videoinfotypes.h"
#include "logging.h"
#include <string>

using namespace pbnjson;
using namespace LSHelpers;

VideoInfo *VideoInfo::init(std::string contentType, std::string sourceName, pbnjson::JValue info)
{
    VideoInfo *videoinfo = nullptr;

    LOG_DEBUG("init for %s", contentType.c_str());

    if (contentType == "movie" || contentType == "photo") {
        videoinfo = new VideoInfoMedia(contentType, sourceName);
    } else if (contentType == "hdmi") {
        videoinfo = new VideoInfoHDMI(contentType, sourceName);
    } else {
        LOG_DEBUG("No videoinfo for %s", contentType.c_str());
        return nullptr;
    }

    videoinfo->set(info);
    videoinfo->debug_print("Init");
    return videoinfo;
}

bool VideoInfo::isValidSource(std::string name)
{
    std::string supportSource[VIDEO_SOURCE_TYPE_MAX] = {"VDEC", "HDMI", "JPEG"};

    for (auto source : supportSource) {
        if (source == name)
            return true;
    }
    return false;
}

/* For VideoInfoMedia
 * ******************************************************************************************************/
VideoInfoMedia::VideoInfoMedia(std::string contentType, std::string sourceName)
{
    LOG_DEBUG("Create VideoInfoMedia sourceName: %s", sourceName.c_str());

    if (sourceName != "VDEC")
        LOG_DEBUG("Invalid sourceName: %s", sourceName.c_str());

    this->contentType = contentType;
    this->sourceName  = sourceName;
}

pbnjson::JValue VideoInfoMedia::toJValue()
{
    if (!isValidSource(sourceName)) {
        LOG_DEBUG("Invalid sourceName: %s", sourceName.c_str());
        return JValue();
    }

    pbnjson::JValue pixelAspectRatio_jval;
    pbnjson::JValue vui_jval;
    pbnjson::JValue sei_jval;

    pixelAspectRatio_jval =
        pbnjson::JValue{{"width", this->pixelAspectRatio.width}, {"height", this->pixelAspectRatio.height}};

    sei_jval = pbnjson::JValue{
        {"displayPrimariesX0", this->sei.displayPrimariesX0}, {"displayPrimariesX1", this->sei.displayPrimariesX1},
        {"displayPrimariesX2", this->sei.displayPrimariesX2}, {"displayPrimariesY0", this->sei.displayPrimariesY0},
        {"displayPrimariesY1", this->sei.displayPrimariesY1}, {"displayPrimariesY2", this->sei.displayPrimariesY2}};

    vui_jval = pbnjson::JValue{{"transferCharacteristics", this->vui.transferCharacteristics},
                               {"colorPrimaries", this->vui.colorPrimaries},
                               {"matrixCoeffs", this->vui.matrixCoeffs},
                               {"videoFullRangerFlag", this->vui.videoFullRangerFlag}};

    return pbnjson::JValue{{"hdrType", this->hdrType},   {"afd", this->afd},
                           {"rotation", this->rotation}, {"adaptive", this->adaptive},
                           {"path", this->path},         {"pixelAspectRatio", pixelAspectRatio_jval},
                           {"mediaVui", vui_jval},       {"mediaSei", sei_jval}};
}

bool VideoInfoMedia::set(pbnjson::JValue videoinfo)
{
    std::string hdrType;
    int16_t afd;
    std::string rotation;
    bool adaptive;
    std::string path;
    PIXEL_ASPECT_RATIO_T pixelAspectRatio;
    VUI_T vui;
    SEI_T sei;
    pbnjson::JValue pixelAspectRatio_jval;
    pbnjson::JValue vui_jval;
    pbnjson::JValue sei_jval;

    if (!isValidSource(sourceName)) {
        LOG_DEBUG("invalid sourceName: %s", sourceName.c_str());
        return false;
    }

    videoinfo_jval = videoinfo;

    JsonParser parser{videoinfo};

    // TODO(ekwang): review optional, deafultvalue
    parser.get("hdrType", hdrType);
    parser.get("afd", afd);
    parser.get("pixelAspectRatio", pixelAspectRatio_jval);
    parser.get("rotation", rotation).optional(true).defaultValue("Deg0");
    parser.get("adaptive", adaptive).optional(true).defaultValue(false);
    parser.get("path", path);
    parser.get("vui", vui_jval);
    parser.get("sei", sei_jval);
    parser.finishParseOrThrow();

    this->hdrType  = hdrType;
    this->afd      = afd;
    this->rotation = rotation;
    this->adaptive = adaptive;
    this->path     = path;

    // TODO : parse pixelAspectRatio_jval to pixelAspectRatio
    JsonParser arc_parser{pixelAspectRatio_jval};
    arc_parser.get("width", pixelAspectRatio.width);
    arc_parser.get("height", pixelAspectRatio.height);
    arc_parser.finishParseOrThrow();
    this->pixelAspectRatio = pixelAspectRatio;

    // TODO : parse mediaVui_jval to mediaVui
    JsonParser vui_parser{vui_jval};
    vui_parser.get("transferCharacteristics", vui.transferCharacteristics);
    vui_parser.finishParseOrThrow();
    this->vui = vui;

    // TODO : parse mediaSei_jval to mediaSei
    JsonParser sei_parser{sei_jval};
    sei_parser.get("displayPrimariesX0", sei.displayPrimariesX0);
    sei_parser.finishParseOrThrow();
    this->sei = sei;

    return true;
}

void VideoInfoMedia::debug_print(std::string prefix) const
{
    LOG_DEBUG("VideoInfoMedia %s [sourceName:%s, hdrType:%s, rotation:%s, path:%s]", prefix.c_str(), sourceName.c_str(),
              hdrType.c_str(), rotation.c_str(), path.c_str());
}

/* For VideoInfoHDMI
 * ***********************************************************************************************************/

VideoInfoHDMI::VideoInfoHDMI(std::string contentType, std::string sourceName)
{
    LOG_DEBUG("Create VideoInfoHDMI sourceName: %s", sourceName.c_str());

    if (sourceName != "HDMI")
        LOG_DEBUG("Invalid sourceName: %s", sourceName.c_str());

    this->contentType = contentType;
    this->sourceName  = sourceName;
}

pbnjson::JValue VideoInfoHDMI::toJValue()
{
    if (!isValidSource(sourceName)) {
        LOG_DEBUG("Invalid sourceName: %s", sourceName.c_str());
        return JValue();
    }

    return pbnjson::JValue{
        {"hdrType", this->hdrType},
        {"afd", this->afd},
        {"enableJustScan", this->enableJustScan},
        {"timingMode", this->timingMode},
        {"HDMIMode", this->HDMIMode},
        {"pixelEncoding", this->pixelEncoding},
        {"colormetry", this->colormetry},
        {"extendedColormetry", this->extendedColormetry},
    };
}

bool VideoInfoHDMI::set(pbnjson::JValue videoinfo)
{
    std::string hdrType;
    uint16_t afd;
    bool enableJustScan;
    std::string timingMode;
    std::string HDMIMode;
    std::string pixelEncoding;
    std::string colormetry;
    std::string extendedColormetry;

    if (!isValidSource(sourceName)) {
        LOG_DEBUG("invalid sourceName: %s", sourceName.c_str());
        return false;
    }

    videoinfo_jval = videoinfo;

    JsonParser parser{videoinfo};

    parser.get("hdrType", hdrType);
    parser.get("afd", afd);
    parser.get("enableJustScan", enableJustScan).optional(true).defaultValue(false);
    parser.get("timingMode", timingMode);
    parser.get("HDMIMode", HDMIMode);
    parser.get("pixelEncoding", pixelEncoding);
    parser.get("colormetry", colormetry).optional(true).defaultValue("none");
    parser.get("extendedColormetry", extendedColormetry).optional(true).defaultValue("none");
    parser.finishParseOrThrow();

    this->hdrType            = hdrType;
    this->afd                = afd;
    this->enableJustScan     = enableJustScan;
    this->timingMode         = timingMode;
    this->HDMIMode           = HDMIMode;
    this->pixelEncoding      = pixelEncoding;
    this->colormetry         = colormetry;
    this->extendedColormetry = extendedColormetry;

    return true;
}

void VideoInfoHDMI::debug_print(std::string prefix) const
{
    LOG_DEBUG("VideoInfoHDMI %s [sourceName:%s, HDMIMode:%s, timingMode:%s]", prefix.c_str(), sourceName.c_str(),
              HDMIMode.c_str(), timingMode.c_str());
}
