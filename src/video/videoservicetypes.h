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

#include "videoinfotypes.h"
#include <cmath>
#include <val_api.h>

enum class ScanType { INTERLACED = 0, PROGRESSIVE = 1 };

class VideoSize
{
public:
    VideoSize() : w(0), h(0){};
    VideoSize &operator=(const VAL_VIDEO_SIZE_T &valSize)
    {
        w = valSize.w;
        h = valSize.h;
        return *this;
    }

public:
    uint16_t w;
    uint16_t h;

    pbnjson::JValue toJValue();
};

class VideoRect : public LSHelpers::JsonDataObject
{
public:
    VideoRect(int16_t x, int16_t y, uint16_t w, uint16_t h) : x(x), y(y), w(w), h(h){};
    VideoRect(uint16_t w, uint16_t h) : x(0), y(0), w(w), h(h){};
    VideoRect() : x(0), y(0), w(0), h(0){};
    // TODO:: Can we Move this to val or use val_video_rect
    VideoRect(VAL_VIDEO_RECT_T valRect) : x(valRect.x), y(valRect.y), w(valRect.w), h(valRect.h){};
    void parseFromJson(const pbnjson::JValue &value) override;
    pbnjson::JValue toJValue();
    bool contains(VideoRect &inside);

    bool operator==(VideoRect &other);

    VideoRect &operator=(const VideoRect &other)
    {
        x = other.x;
        y = other.y;
        w = other.w;
        h = other.h;

        return *this;
    }

    VAL_VIDEO_RECT_T toVALRect() { return VAL_VIDEO_RECT_T{(uint16_t)x, (uint16_t)y, w, h}; }

    // TODO(ekwang): scale with different ratio of width and height
    VideoRect scale(double scale)
    {
        return VideoRect((int16_t)round(x * scale), (int16_t)round(y * scale), (uint16_t)round(w * scale),
                         (uint16_t)round(h * scale));
    }

    bool isValid() const { return w > 0 && h > 0; }

    void debug_print(std::string prefix) const;

public:
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
};

class Composition : public LSHelpers::JsonDataObject
{
public:
    std::string sink;
    int opacity; // alpha
    int zOrder;

    void parseFromJson(const pbnjson::JValue &value) override
    {
        LSHelpers::JsonParser::parseValue(value["sink"], sink);
        LSHelpers::JsonParser::parseValue(value["opacity"], opacity);
        LSHelpers::JsonParser::parseValue(value["zOrder"], zOrder);
    }
};

// Here are the status information that the video device is currently set.
class VideoSink
{
public:
    VideoSink(const std::string &_name, uint8_t _zorder, VAL_VIDEO_WID_T _wId)
        : name(_name), wId(_wId), connected(false), muted(true), opacity(255), zOrder(_zorder)
    {
    }

    // Sink Basic Infomation
    std::string name;    // "MAIN", "SUB"
    VAL_VIDEO_WID_T wId; // 0 = main, 1 = sub
    bool connected;
    bool muted;

    std::string connectedClientId; // Connected clientId of VideoClient

    // Values from device-cap
    VideoSize maxUpscaleSize;   // max video size on screen if it's bigger than actual size
    VideoSize minDownscaleSize; // min video size on screen if it's smaller than actual size

    // Configure by videoclient Infomation
    VideoRect scaledOutputRect; // scaled output rect
    VideoRect appliedInputRect; // real input rect used for val

    // Zorder related things.
    uint8_t opacity; // Alpha
    uint8_t zOrder;
};

// Here is information from the client that should be kept even if disconnected.
// A client can be a pipeline or MDC in UMS.
// In MDC use case, clientId is same with sinkName
class VideoClient
{
public:
    VideoClient(const std::string &_pID)
        : activation(false), available(false), fullScreen(false), frameRate(0.), clientId(_pID), sinkName("unknown"),
          sourceName("unknown"), sourcePort(0), scanType(ScanType::PROGRESSIVE), videoinfoObj(nullptr)
    {
    }

    void debug_print(std::string prefix) const;

    bool activation; // set true when video connected using this object
    bool available;  // set true when values are filled with valid value
    bool fullScreen;
    double frameRate;       // Hz
    std::string clientId;   // clientId
    std::string sinkName;   // sinkName this object connected "MAIN", "SUB"
    std::string sourceName; // "VDEC", "HDMI" ..
    uint8_t sourcePort;

    VideoRect sourceRect; // video image size from client
    VideoRect inputRect;  // original value from client
    VideoRect outputRect; // original value from client

    ScanType scanType;       // TODO(ekwang): check necessary
    std::string contentType; // check necessary

    VideoInfo *videoinfoObj;
};
