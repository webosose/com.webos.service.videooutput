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

#include <sstream>
#include <string>
#include <unordered_set>
#include <val/val_video.h>
#include <vector>

#include "errors.h"
#include "logging.h"
#include "videoservice.h"

using namespace pbnjson;
using namespace LSHelpers;

VideoService::VideoService(LS::Handle &handle) : val(NULL), mService(&handle), mDualVideoEnabled(false)
{
    val = VAL::getInstance();
    if (!val) {
        LOG_ERROR(MSGID_HAL_INIT_ERROR, 0, "Can't get val instance");
        return;
    }

    std::vector<VAL_PLANE_T> supportedPlanes = val->video->getVideoPlanes();

    // setup the sinks
    uint32_t wid = static_cast<uint32_t>(VAL_VIDEO_WID_0);
    for (uint8_t i = 0; i < supportedPlanes.size(); i++, wid++) {
        std::string plane = supportedPlanes[i].planeName;
        LOG_DEBUG("push to mSink. planes name:%s", plane.c_str());
        mSinks.push_back(VideoSink(plane, i, static_cast<VAL_VIDEO_WID_T>(wid)));
    }

    mService.registerMethod("/", "register", this, &VideoService::_register);
    mService.registerMethod("/", "unregister", this, &VideoService::unregister);
    mService.registerMethod("/", "connect", this, &VideoService::connect);
    mService.registerMethod("/", "disconnect", this, &VideoService::disconnect);
    mService.registerMethod("/", "setVideoData", this, &VideoService::setVideoData);
    mService.registerMethod("/", "blankVideo", this, &VideoService::blankVideo);
    mService.registerMethod("/", "getStatus", this, &VideoService::getStatus);

    // TODO(ekwang): defined but not used except setCompositing and setDisplayWindow
    mService.registerMethod("/display", "getVideoLimits", this, &VideoService::getVideoLimits);
    mService.registerMethod("/display", "getOutputCapabilities", this, &VideoService::getOutputCapabilities);
    mService.registerMethod("/display", "getSupportedResolutions", this, &VideoService::getSupportedResolutions);
    mService.registerMethod("/display", "setDisplayWindow", this, &VideoService::setDisplayWindow);
    mService.registerMethod("/display", "setDisplayResolution", this, &VideoService::setDisplayResolution);
    mService.registerMethod("/display", "setCompositing", this, &VideoService::setCompositing);
    mService.registerMethod("/display", "setParam", this, &VideoService::setParam);
    mService.registerMethod("/display", "getParam", this, &VideoService::getParam);
}

VideoService::~VideoService()
{
    for (auto sink : mSinks) {
        doDisconnectVideo(sink);
    }
    mSinks.clear();
    mClients.clear();
}

pbnjson::JValue VideoService::_register(LSHelpers::JsonRequest &request)
{
    std::string clientId;

    request.get("context", clientId);
    if (!request.finishParse())
        return API_ERROR_SCHEMA_VALIDATION(request.getError());

    LOG_DEBUG("register clientId: %s", clientId.c_str());

    // push client object
    if (!addClientInfo(clientId))
        return API_ERROR_INVALID_PARAMETERS("%s is already registered", clientId.c_str());

    return JObject{{"returnValue", true}};
}

pbnjson::JValue VideoService::unregister(LSHelpers::JsonRequest &request)
{
    std::string clientId;

    request.get("context", clientId);
    if (!request.finishParse())
        return API_ERROR_SCHEMA_VALIDATION(request.getError());

    LOG_DEBUG("unregister clientId: %s", clientId.c_str());

    // TODO(ekwang) : Should we disconnect sink if it still connected?

    // erase client object
    if (!removeClientInfo(clientId))
        return API_ERROR_INVALID_PARAMETERS("%s is not registered.", clientId.c_str());

    return JObject{{"returnValue", true}};
}

pbnjson::JValue VideoService::connect(LSHelpers::JsonRequest &request)
{
    std::string videoSource, videoSinkName, purpose, appId("unknown"), clientId("unknown");
    uint8_t videoSourcePort;
    bool cIdSet;

    request.get("appId", appId).optional(true);
    request.get("context", clientId).optional(true).checkValueRead(cIdSet);
    request.get("source", videoSource);
    request.get("sourcePort", videoSourcePort);
    request.get("sink", videoSinkName);

    if (!request.finishParse())
        return API_ERROR_SCHEMA_VALIDATION(request.getError());

    LOG_DEBUG("Video connect source:%s, sourcePort:%d, sinkname:%s, clientId:%s", videoSource.c_str(), videoSourcePort,
              videoSinkName.c_str(), clientId.c_str());

    VideoSink *videoSink = getVideoSink(videoSinkName);
    if (!videoSink) {
        return API_ERROR_INVALID_PARAMETERS("Invalid sink: %s", videoSinkName.c_str());
    }

    VAL_VSC_INPUT_SRC_INFO_T vscInput = {VAL_VSC_INPUTSRC_MAX, 0, 0};

    if (videoSource == "VDEC") {
        vscInput.type          = VAL_VSC_INPUTSRC_VDEC;
        vscInput.attr          = 1; // Not used for VDEC
        vscInput.resourceIndex = videoSourcePort;
    } else if (videoSource == "HDMI") {
        vscInput.type          = VAL_VSC_INPUTSRC_HDMI;
        vscInput.resourceIndex = videoSourcePort; // HDMI port number
    } else if (videoSource == "JPEG") {
        vscInput.type = VAL_VSC_INPUTSRC_JPEG;
    } else {
        return API_ERROR_INVALID_PARAMETERS("unsupported videoSource type:%s", videoSource.c_str());
    }

    if (videoSinkName.find("SUB") != std::string::npos) {
        // TODO(ekwang) : Check if it is necessary
        this->setDualVideo(true);
    }

    if (videoSink->connected) {
        doDisconnectVideo(*videoSink);
        this->sendSinkUpdateToSubscribers();

        // connectedClientId should be cleared after update info to subscribers
        videoSink->connectedClientId = "unknown";
    }

    unsigned int plane;
    if (!val->video->connect(videoSink->wId, vscInput, VAL_VSC_OUTPUT_DISPLAY_MODE, &plane)) {
        return API_ERROR_HAL_ERROR;
    }

    videoSink->connected = true;

    this->readVideoCapabilities(*videoSink);

    // TODO(ekwang) : check using applyVideoFilters in here
    if (!this->applyVideoFilters(*videoSink, videoSource)) {
        return API_ERROR_HAL_ERROR;
    }

    if (!cIdSet) {
        /* It means there was no calling register()
         * In this case we create client in here for RP that doesn't use register()
         */

        // push client object
        if (!addClientInfo(videoSinkName))
            return API_ERROR_INVALID_PARAMETERS("%s is already registered", videoSinkName.c_str());

        clientId = videoSinkName;
    }

    VideoClient *client = getClientInfo(clientId);

    if (!client)
        return API_ERROR_INVALID_PARAMETERS("Invalid clientId: %s", clientId.c_str());

    videoSink->connectedClientId = client->clientId;

// set videosink info using preloaded client info and apply it's Rect automatically
#if 0 // ekwang : test scenario
    if ((client->available == true) && LoadClientInfotoVideoSink(*videoSink, *client)) {
        client->activation = true;
        LOG_DEBUG("Load clientId: %s's preloaded info and apply it's rect automatically");

        if (!this->applyVideoOutputRects(*videoSink, *client, videoSink->appliedInputRect, videoSink->scaledOutputRect,
                                         client->sourceRect))
            return API_ERROR_HAL_ERROR;

        // TODO(ekwang) : unmute?
        if (!val->video->setWindowBlanking(videoSink->wId, false, videoSink->appliedInputRect.toVALRect(),
                                           videoSink->scaledOutputRect.toVALRect()))
            return API_ERROR_HAL_ERROR;
    }
    else
#endif
    {
        client->sourceName = videoSource;
        client->sourcePort = videoSourcePort;
        client->sinkName   = videoSinkName;
        client->activation = true;
    }

    mAppIdChangedNotify(appId);

    /*if (videoSource == "HDMI")
    {
            readHdmiTimingInfo(*client);
    }*/

    LOG_DEBUG("Video connect success. planeId:%d", plane);
    this->sendSinkUpdateToSubscribers();

    return JObject{{"returnValue", true}, {"planeID", (int)plane}};
}

pbnjson::JValue VideoService::getVideoLimits(LSHelpers::JsonRequest &request)
{
    std::string sinkName;

    request.get("sink", sinkName);
    if (!request.finishParse())
        return API_ERROR_SCHEMA_VALIDATION(request.getError());

    VideoSink *videoSink = getVideoSink(sinkName);

    if (!videoSink)
        return API_ERROR_INVALID_PARAMETERS("Invalid sink: %s", sinkName.c_str());

    if (!videoSink->connected)
        return API_ERROR_VIDEO_NOT_CONNECTED;

    return JObject{{"returnValue", true},
                   {"sink", sinkName},
                   {"displaySize", videoSink->maxUpscaleSize.toJValue()},
                   {"minDownscaleSize", videoSink->minDownscaleSize.toJValue()},
                   {"maxUpscaleSize", videoSink->maxUpscaleSize.toJValue()}};
}

pbnjson::JValue VideoService::getOutputCapabilities(LSHelpers::JsonRequest &request)
{
    std::vector<VAL_PLANE_T> valPlanes = val->video->getVideoPlanes();
    size_t planeCount                  = valPlanes.size();
    JArray planesInfo;

    for (auto plane : valPlanes) {
        planesInfo.append(pbnjson::JValue{
            {"sinkId", plane.planeName},
            {"maxDownscaleSize", pbnjson::JValue{{"width", plane.minSizeT.w}, {"height", plane.minSizeT.h}}},
            {"maxUpscaleSize", pbnjson::JValue{{"width", plane.maxSizeT.w}, {"height", plane.maxSizeT.h}}}});
    }
    return JObject{{"returnValue", true}, {"numPlanes", static_cast<int>(planeCount)}, {"planes", planesInfo}};
}

pbnjson::JValue VideoService::disconnect(LSHelpers::JsonRequest &request)
{
    std::string videoSinkName, clientId("unknown");
    bool cIdSet;

    request.get("sink", videoSinkName);
    request.get("context", clientId).optional(true).checkValueRead(cIdSet);

    if (!request.finishParse())
        return API_ERROR_SCHEMA_VALIDATION(request.getError());

    LOG_DEBUG("Video disconnect sink: %s", videoSinkName.c_str());

    VideoSink *videoSink = getVideoSink(videoSinkName);

    if (!videoSink)
        return API_ERROR_INVALID_PARAMETERS("Invalid sink: %s", videoSinkName.c_str());

    if (!videoSink->connected)
        return API_ERROR_VIDEO_NOT_CONNECTED;

    if (!this->doDisconnectVideo(*videoSink))
        return API_ERROR_HAL_ERROR;

    LOG_DEBUG("Video disconnect success. sink: %s", videoSinkName.c_str());
    this->sendSinkUpdateToSubscribers();

    // connectedClientId should be cleared after update info to subscribers
    videoSink->connectedClientId = "unknown";

    if (!cIdSet) {
        /* It means there will be no calling unregister()
         * In this case we remove client in here for RP
         */
        // erase client object
        if (!removeClientInfo(videoSinkName))
            return API_ERROR_INVALID_PARAMETERS("%s is not registered.", videoSinkName.c_str());
    } else {
        VideoClient *client = getClientInfo(clientId);
        if (client) {
            client->activation = false;
        }
    }

    return true;
}

bool VideoService::doDisconnectVideo(VideoSink &video)
{
    if (!video.connected) {
        LOG_DEBUG("sink: %s is not connected", video.name.c_str());
        return true;
    }

    // Clear state first - will be disconnected even if some calls fail.
    // Allows caller to retry connecting if failed state

    // Reset all video sink related fields
    video.connected        = false;
    video.muted            = false;
    video.opacity          = 0;
    video.zOrder           = 0;
    video.scaledOutputRect = VideoRect();
    video.appliedInputRect = VideoRect();
    video.maxUpscaleSize   = VideoSize();
    video.minDownscaleSize = VideoSize();

    bool success = true;

    success &= val->video->disconnect(video.wId);

    if (video.name.find("SUB") != std::string::npos)
        success &= this->setDualVideo(false);

    return success;
}

pbnjson::JValue VideoService::blankVideo(LSHelpers::JsonRequest &request)
{
    std::string sinkName;
    bool enableBlank;

    request.get("sink", sinkName);
    request.get("blank", enableBlank);
    if (!request.finishParse())
        return API_ERROR_SCHEMA_VALIDATION(request.getError());

    LOG_DEBUG("blankVideo sink:%s, set blank to %d", sinkName.c_str(), enableBlank);

    VideoSink *videoSink = getVideoSink(sinkName);

    if (!videoSink)
        return API_ERROR_INVALID_PARAMETERS("Invalid sink: %s", sinkName.c_str());

#if 0 // blank can be called before connected status under RP
    if (!videoSink->connected)
        return API_ERROR_VIDEO_NOT_CONNECTED;
#endif

    if (enableBlank && videoSink->muted) {
        LOG_DEBUG("Already muted, do nothing");
        return true;
    }

    if (!val->video->setWindowBlanking(videoSink->wId, enableBlank, videoSink->appliedInputRect.toVALRect(),
                                       videoSink->scaledOutputRect.toVALRect())) {
        return API_ERROR_HAL_ERROR;
    }

    videoSink->muted = enableBlank;
    this->sendSinkUpdateToSubscribers();

    return true;
}

pbnjson::JValue VideoService::setVideoData(LSHelpers::JsonRequest &request)
{
    std::string videoSinkName;
    std::string clientId;
    std::string contentType;
    uint16_t width;
    uint16_t height;
    std::string scanType;
    bool cIdSet;
    bool videoInfoSet;
    double frameRate;
    JValue videoInfo;

    request.get("sink", videoSinkName).optional(true);
    request.get("context", clientId).optional(true).checkValueRead(cIdSet);
    request.get("contentType", contentType).optional(true).defaultValue("unknown");
    request.get("width", width);
    request.get("height", height);
    request.get("frameRate", frameRate).min(0.0);
    request.get("scanType", scanType)
        .optional(true)
        .allowedValues({"interlaced", "progressive", "VIDEO_PROGRESSIVE", "VIDEO_INTERLACED"});
    request.get("videoInfo", videoInfo).optional(true).checkValueRead(videoInfoSet);

    if (!request.finishParse())
        return API_ERROR_SCHEMA_VALIDATION(request.getError());

    LOG_DEBUG("setVideoData called for sink %s with contentType %s, width %u, height %u, scanType %s",
              videoSinkName.c_str(), contentType.c_str(), width, height, scanType.c_str());

    if (!cIdSet) {
        clientId = videoSinkName;
    }

    VideoClient *client = getClientInfo(clientId);

    if (!client) {
        return API_ERROR_INVALID_PARAMETERS("Invalid clientId: %s", clientId.c_str());
    }

    VideoSink *videoSink = getVideoSink(client->sinkName);

    if (!videoSink) {
        return API_ERROR_INVALID_PARAMETERS("Invalid sink: %s", videoSinkName.c_str());
    }

    if (!videoSink->connected) {
        return API_ERROR_VIDEO_NOT_CONNECTED;
    }

    // Just save the frame rect and wait for setDisplayWindow to update output window.
    client->sourceRect.w = width;
    client->sourceRect.h = height;
    client->contentType  = contentType;
    client->frameRate    = frameRate;
    client->scanType =
        scanType == "progressive" || scanType == "VIDEO_PROGRESSIVE" ? ScanType::PROGRESSIVE : ScanType::INTERLACED;

    // TODO(ekwang) : check, this code reset the videosink's inputRect. Why?
    client->inputRect = VideoRect(); // Invalidate input rect, need to call setDisplayWindow to set new input rect.

    if (videoInfoSet) {
        if (!client->videoinfoObj) {
            LOG_DEBUG("new videoinfoobj");
            client->videoinfoObj = VideoInfo::init(client->contentType, client->sourceName, videoInfo);
        } else {
            LOG_DEBUG("Update videoinfo");
            client->videoinfoObj->set(videoInfo);
        }
    }

    if (videoSink->scaledOutputRect.isValid() ||
        client->fullScreen) { // only if setDisplayWindow was called earlier apply Video
        VideoRect input  = client->sourceRect;
        VideoRect output = videoSink->scaledOutputRect;

        if (client->fullScreen) {
            VideoRect sinkWindowSize = VideoRect(videoSink->maxUpscaleSize.w, videoSink->maxUpscaleSize.h);
            mAspectRatioControl.scaleWindow(sinkWindowSize, client->sourceRect, input, output);
        }

        this->applyVideoOutputRects(*videoSink, *client, input, output, client->sourceRect);
    }

    this->sendSinkUpdateToSubscribers();
    return true;
}

pbnjson::JValue VideoService::setDisplayWindow(LSHelpers::JsonRequest &request)
{
    std::string videoSinkName, clientId("unknown");
    bool supportNegativePos = false;
    bool fullScreen;
    bool opacitySet;
    bool cIdSet;
    uint8_t opacity;
    bool displayOutputSet, sourceInputSet;
    VideoRect displayOutput;
    VideoRect inputRect;

    request.get("sink", videoSinkName).optional(true);
    request.get("context", clientId).optional(true).checkValueRead(cIdSet);
    request.get("fullScreen", fullScreen);
    request.get("displayOutput", displayOutput).optional(true).checkValueRead(displayOutputSet);
    request.get("sourceInput", inputRect).optional(true).checkValueRead(sourceInputSet);
    request.get("opacity", opacity).optional(true).defaultValue(0).checkValueRead(opacitySet);

    if (!request.finishParse())
        return API_ERROR_SCHEMA_VALIDATION(request.getError());

    LOG_DEBUG("setDisplayWindow called for sink %s with fullScreen %d, displayOutput {x:%d, y:%d, w:%u, h:%u},"
              "inputRect {x:%d, y:%d, w:%u, h:%u}, opacity %u",
              videoSinkName.c_str(), fullScreen, displayOutput.x, displayOutput.y, displayOutput.w, displayOutput.h,
              inputRect.x, inputRect.y, inputRect.w, inputRect.h, opacity);

    if (!cIdSet)
        clientId = videoSinkName;

    VideoClient *client = getClientInfo(clientId);

    if (!client)
        return API_ERROR_INVALID_PARAMETERS("Invalid client: %s", clientId.c_str());

    VideoSink *videoSink = getVideoSink(client->sinkName);

    if (!videoSink)
        return API_ERROR_INVALID_PARAMETERS("Invalid sink: %s", videoSinkName.c_str());

    VideoRect sinkWindowSize = VideoRect(videoSink->maxUpscaleSize.w, videoSink->maxUpscaleSize.h);

    if (fullScreen) {
        displayOutput = sinkWindowSize;
    } else {
        // TODO(ekwang) : check this fixed value 1080
        // Scale to emulate 1080p output resolution.
        double outputScaling = 1; // videoSink->maxUpscaleSize.h / 2160;
        displayOutput        = displayOutput.scale(outputScaling);
    }

    if (!videoSink->connected) {
        return API_ERROR_VIDEO_NOT_CONNECTED;
    } else if (!supportNegativePos && !sinkWindowSize.contains(displayOutput)) {
        return API_ERROR_INVALID_PARAMETERS("displayOutput outside screen");
    } else if (client->sourceRect.isValid() && inputRect.isValid() && !client->sourceRect.contains(inputRect)) {
        return API_ERROR_INVALID_PARAMETERS("inputRect outside video size");
    } else if (displayOutput.w == 0 && displayOutput.h == 0) {
        return API_ERROR_INVALID_PARAMETERS("need to specify displayOutput when fullscreen = false");
    } else if ((displayOutput.w < inputRect.w && displayOutput.w < videoSink->minDownscaleSize.w) ||
               (displayOutput.h < inputRect.h && displayOutput.h < videoSink->minDownscaleSize.h)) {
        return API_ERROR_DOWNSCALE_LIMIT("unable to downscale below %d,%d, requested, %d,%d",
                                         videoSink->minDownscaleSize.w, videoSink->minDownscaleSize.h, displayOutput.w,
                                         displayOutput.h);
    } else if ((displayOutput.w > inputRect.w && displayOutput.w > videoSink->maxUpscaleSize.w) ||
               (displayOutput.h > inputRect.h && displayOutput.h > videoSink->maxUpscaleSize.h)) {
        return API_ERROR_UPSCALE_LIMIT("unable to upscale above %d,%d, requested, %d,%d", videoSink->maxUpscaleSize.w,
                                       videoSink->maxUpscaleSize.h, displayOutput.w, displayOutput.h);
    }

    // Store the original values
    client->fullScreen = fullScreen;
    if (displayOutputSet)
        client->outputRect = displayOutput;
    if (sourceInputSet)
        client->inputRect = inputRect;
    else
        inputRect = client->sourceRect;

    inputRect.debug_print("setdisplaywindow-inputRect");
    displayOutput.debug_print("setdisplaywindow-displayOutput");

    // reflect negative x,y position
    if (supportNegativePos) {
        double w_ratio = (double)displayOutput.w / (double)inputRect.w; //(displayOutput.w > inputRect.w) ?
                                                                        //(double)displayOutput.w / (double)inputRect.w
                                                                        //: (double)inputRect.w /
                                                                        //(double)displayOutput.w;
        double h_ratio = (double)displayOutput.h / (double)inputRect.h; //(displayOutput.h > inputRect.h) ?
                                                                        //(double)displayOutput.h / (double)inputRect.h
                                                                        //: (double)inputRect.h /
                                                                        //(double)displayOutput.h;

        LOG_DEBUG("w_ratio:%f, h_ratio:%f", w_ratio, h_ratio);

        // reflect negative x,y position to displayOutput
        if (displayOutput.x < 0) { // x has negative value
            LOG_DEBUG("minus x");
            if (displayOutput.w + displayOutput.x > 0) {
                LOG_DEBUG("  calculate width");
                LOG_DEBUG("    w:%u = %lf / %lf", (uint16_t)((double)(displayOutput.w + displayOutput.x) / w_ratio),
                          (double)(displayOutput.w + displayOutput.x), w_ratio);
                inputRect.w     = (uint16_t)((double)(displayOutput.w + displayOutput.x) / w_ratio);
                displayOutput.w = displayOutput.w + displayOutput.x;
            } else {
                inputRect.w     = 0;
                displayOutput.w = 0;
            }
            LOG_DEBUG("  calculate x pos");
            LOG_DEBUG("    x:%d = %lf / %lf", (int16_t)((double)(inputRect.x - displayOutput.x) / w_ratio),
                      (double)(inputRect.x - displayOutput.x), w_ratio);
            inputRect.x     = (int16_t)((double)(inputRect.x - displayOutput.x) / w_ratio);
            displayOutput.x = 0;
        } else if ((uint16_t)displayOutput.x + displayOutput.w >
                   videoSink->maxUpscaleSize.w) { // x+w value over display width
            LOG_DEBUG("plus x");
            LOG_DEBUG("  calculate width");
            // crop when inputRect out of displayRect
            LOG_DEBUG("    w:%u = %u / %lf",
                      (uint16_t)((double)(videoSink->maxUpscaleSize.w - (uint16_t)displayOutput.x) / w_ratio),
                      (videoSink->maxUpscaleSize.w - (uint16_t)displayOutput.x), w_ratio);
            inputRect.w     = (uint16_t)((double)(videoSink->maxUpscaleSize.w - (uint16_t)displayOutput.x) / w_ratio);
            displayOutput.w = videoSink->maxUpscaleSize.w - (uint16_t)displayOutput.x;
        }

        if (displayOutput.y < 0) {
            LOG_DEBUG("minus y\n");
            // TODO(ekwang_: reflect negative x,y position to inputRect
            if (displayOutput.h + displayOutput.y > 0) {
                LOG_DEBUG("  calculate height");
                LOG_DEBUG("    h:%u = %lf / %lf", (uint16_t)((double)(displayOutput.h + displayOutput.y) / h_ratio),
                          (double)(displayOutput.h + displayOutput.y), h_ratio);
                inputRect.h     = (uint16_t)((double)(displayOutput.h + displayOutput.y) / h_ratio);
                displayOutput.h = displayOutput.h + displayOutput.y;
            } else {
                inputRect.h     = 0;
                displayOutput.h = 0;
            }
            LOG_DEBUG("  calculate y pos");
            LOG_DEBUG("    y:%d = %lf / %lf", (int16_t)((double)(inputRect.y - displayOutput.y) / h_ratio),
                      (double)(inputRect.y - displayOutput.y), h_ratio);
            inputRect.y     = (int16_t)((double)(inputRect.y - displayOutput.y) / h_ratio);
            displayOutput.y = 0;
        } else if ((uint16_t)displayOutput.y + displayOutput.h > videoSink->maxUpscaleSize.h) {
            LOG_DEBUG("plus y");
            LOG_DEBUG("  calculate height");
            // crop when inputRect out of displayRect
            LOG_DEBUG("    h:%u = %u / %lf",
                      (uint16_t)((double)(videoSink->maxUpscaleSize.h - (uint16_t)displayOutput.y) / h_ratio),
                      (videoSink->maxUpscaleSize.h - (uint16_t)displayOutput.y), h_ratio);
            inputRect.h     = (uint16_t)((double)(videoSink->maxUpscaleSize.h - (uint16_t)displayOutput.y) / h_ratio);
            displayOutput.h = videoSink->maxUpscaleSize.h - (uint16_t)displayOutput.y;
        }
    }

    VideoRect scaledOutput = displayOutput;
    if (client->fullScreen) {
        mAspectRatioControl.scaleWindow(displayOutput, client->sourceRect, inputRect, scaledOutput);
    }

    scaledOutput.debug_print("setdisplaywindow-scaledOutput");
    inputRect.debug_print("setdisplaywindow-appliedinputRect");

    if (!this->applyVideoOutputRects(*videoSink, *client, inputRect, scaledOutput, client->sourceRect)) {
        return API_ERROR_HAL_ERROR;
    }

    // TEMPORARY CODE start: after AV Mute Manager done, this part will BE DELETED!!! mayyoon_181106
    if (!val->video->setWindowBlanking(videoSink->wId, false, videoSink->appliedInputRect.toVALRect(),
                                       videoSink->scaledOutputRect.toVALRect())) {
        return API_ERROR_HAL_ERROR;
    }
    // TEMPORARY CODE end

    client->available = true;
    LOG_DEBUG("all info are filled for client");

    if (opacitySet) {
        videoSink->opacity = opacity;
    }

    this->sendSinkUpdateToSubscribers();

    return true;
}

pbnjson::JValue VideoService::setCompositing(LSHelpers::JsonRequest &request)
{
    std::vector<Composition> composeOrdering;
    request.getArray("composeOrder", composeOrdering);
    if (!request.finishParse())
        return API_ERROR_SCHEMA_VALIDATION(request.getError());

    int maxZOrder = mSinks.size() - 1;
    std::unordered_set<int> uniqueZorders;
    std::unordered_set<std::string> inputSinks;

    // Validate input array of composition objects
    for (Composition &composition : composeOrdering) {
        LOG_DEBUG("%s: Sink %s, opacity %d, zorder %d", __func__, composition.sink.c_str(), composition.opacity,
                  composition.zOrder);
        VideoSink *vsink = this->getVideoSink(composition.sink);

        if (!vsink) {
            return API_ERROR_INVALID_PARAMETERS("Invalid sink value");
        }

        if (composition.opacity > 255 || composition.opacity < 0 || composition.zOrder > maxZOrder ||
            composition.zOrder < 0) {
            return API_ERROR_INVALID_PARAMETERS(
                "Zorder values must be in the range 0-%d and opacity values must be in the range 0-255", maxZOrder);
        }

        // Make sure the given zorders are unique
        if (!(uniqueZorders.insert(composition.zOrder)).second) {
            return API_ERROR_INVALID_PARAMETERS("Two windows cannot have the same zOrder");
        }
        inputSinks.insert(composition.sink);
    }

    // Sanity test that no two zorders are the same - out of given sinks and rest of the sinks
    for (auto &sink : mSinks) {
        // This sink's zorder is already registered in uniqueZorder
        if (inputSinks.find(sink.name) != inputSinks.end())
            continue;

        if (!(uniqueZorders.insert(sink.zOrder)).second) {
            return API_ERROR_INVALID_PARAMETERS("Two windows cannot have the same zOrder");
        }
    }

    // Input is good - set the zorders
    for (Composition &comp : composeOrdering) {
        VideoSink *vsink = this->getVideoSink(comp.sink);
        vsink->opacity   = comp.opacity;
        vsink->zOrder    = comp.zOrder;

        LOG_DEBUG("Setting opacity %d, zorder %d for sink %s", vsink->opacity, vsink->zOrder, vsink->name.c_str());
    }

    if (!this->applyCompositing()) {
        // TODO: Roll back the vsink zorders values
        return API_ERROR_HAL_ERROR;
    }

    this->sendSinkUpdateToSubscribers();

    return true;
}

pbnjson::JValue VideoService::setDisplayResolution(LSHelpers::JsonRequest &request)
{
    uint16_t w, h;
    uint8_t display_path;

    request.get("w", w);
    request.get("h", h);
    request.get("display-path", display_path);

    if (!request.finishParse())
        return API_ERROR_SCHEMA_VALIDATION(request.getError());

    VAL_VIDEO_SIZE_T res;
    res.h = h;
    res.w = w;
    val->video->setDisplayResolution(res, display_path);

    return true;
}

pbnjson::JValue VideoService::getSupportedResolutions(LSHelpers::JsonRequest &request)
{
    JArray dispArray;
    int ret;
    int numDisplay;
    pbnjson::JValue response;
    pbnjson::JValue param = pbnjson::JValue();

    response = val->video->getParam(VAL_CTRL_NUM_CONNECTOR, param);

    JsonParser parser{response};
    parser.get("returnValue", ret);
    if (ret == true) {
        parser.get("numConnector", numDisplay);
    }
    if (!parser.finishParse())
        return API_ERROR_SCHEMA_VALIDATION(parser.getError());

    std::string dispStr = "disp";
    for (int i = 0; i < numDisplay; i++) {
        auto modeList = val->video->getSupportedResolutions(i);
        JArray modeArray;
        for (auto m : modeList) {
            std::stringstream s;
            s << m.w << "x" << m.h;
            std::cout << s.str() << std::endl;
            JValue mode = JObject{{"name", s.str()}, {"w", m.w}, {"h", m.h}};
            modeArray.append(mode);
        }
        JValue disp = JObject{{dispStr + std::to_string(i), modeArray}};
        dispArray.append(disp);
    }
    return JObject{{"returnValue", true}, {"modes", dispArray}};
}

pbnjson::JValue VideoService::getStatus(LSHelpers::JsonRequest &request)
{
    bool subscribe = false;

    request.get("subscribe", subscribe).optional(true).defaultValue(false);

    if (!request.finishParse())
        return API_ERROR_SCHEMA_VALIDATION(request.getError());

    JValue response = this->buildStatus();
    response.put("subscribed", subscribe);
    response.put("returnValue", true);

    if (subscribe) {
        this->mSinkStatusSubscription.post(response);
        this->mSinkStatusSubscription.addSubscription(request);
    } else {
        // TODO: no way to unsubscription. LSHelpers doesn't provide method.
    }

    // TODO(ekwang): temp for debug
    for (uint16_t i = 0; i < mClients.size(); i++)
        mClients[i].debug_print("client info");

    return response;
}

void VideoService::sendSinkUpdateToSubscribers()
{
    if (!this->mSinkStatusSubscription.hasSubscribers()) {
        return;
    }

    JValue response = this->buildStatus();
    response.put("subscribed", true);
    this->mSinkStatusSubscription.post(response);
}

pbnjson::JValue VideoService::buildStatus()
{
    JArray videoStatus;
    for (VideoSink &sink : mSinks) {
        videoStatus.append(buildVideoSinkStatus(sink));
    }

    return JObject{{"video", videoStatus}};
}

pbnjson::JValue VideoService::buildVideoSinkStatus(VideoSink &vsink)
{
    VideoClient *client            = nullptr;
    pbnjson::JValue videoinfo_jval = JValue();

    if (vsink.connected) {
        client = getClientInfo(vsink.name, true);
        if (!client) {
            // Note : In this case, unregister is called before disconnect
            return API_ERROR_INVALID_PARAMETERS("Invalid client: %s", vsink.name.c_str());
        }

#if 0 // TODO(ekwang) : code for reference. should be removed. videoInfo should be return proper object for each
      // sourceName
        if (client->videoinfoObj && client->videoinfoObj->sourceName == "VDEC") {
            // TODO(ekwang) : bypass videoinfo from client to subscriber or for more processing you can use toJValue
            // videoinfo_jval = client->videoinfoObj->toJValue();
            videoinfo_jval = client->videoinfoObj->videoinfo_jval;

            // TODO(ekwang): for test to get member info
            VideoInfoMedia *videoinfomedia = static_cast<VideoInfoMedia *>(client->videoinfoObj);
            LOG_DEBUG("get videoinfo_val's hdrType:%s", videoinfomedia->hdrType.c_str());
        }
#endif
    }

    LOG_DEBUG("buildVideoSinkStatus sink: %s, connected:%d", vsink.name.c_str(), vsink.connected);

    return JObject{
        {"sink", vsink.name},
        {"connected", vsink.connected},
        {"context", vsink.connectedClientId},
        {"muted", vsink.muted},
        {"opacity", vsink.opacity},
        {"zOrder", vsink.zOrder},
        {"displayOutput", vsink.scaledOutputRect.toJValue()},
        {"sourceInput", vsink.appliedInputRect.toJValue()},
        {"connectedSource", client ? client->sourceName : JValue()}, // Set to null when not connected
        {"connectedSourcePort", client ? client->sourcePort : 0},
        {"frameRate", client ? client->frameRate : 0.0},
        {"contentType", client ? client->contentType : "unknown"},
        {"scanType", client ? (client->scanType == ScanType::INTERLACED ? "interlaced" : "progressive") : "unknown"},
        {"width", client ? client->sourceRect.w : 0},
        {"height", client ? client->sourceRect.h : 0},
        {"fullScreen", client ? client->fullScreen : false},
        {"videoInfo", videoinfo_jval}};
}

pbnjson::JValue VideoService::setParam(LSHelpers::JsonRequest &request)
{
    int ret = false;
    int wId;
    std::string command;
    std::string sinkName;
    pbnjson::JValue response;

    request.get("command", command);
    request.get("sink", sinkName);

    VideoSink *videoSink = getVideoSink(sinkName);
    if (!videoSink) {
        return API_ERROR_INVALID_PARAMETERS("Invalid sink: %s", sinkName.c_str());
    }

    wId = videoSink->wId;

    LOG_DEBUG("command:%s, sink:%s, wId:%d", command.c_str(), sinkName.c_str(), wId);

    if (VAL_DEV_RPI == val->getDevice()) {
        return API_ERROR_NOT_IMPLEMENTED;
    } else {
        return API_ERROR_NOT_IMPLEMENTED;
    }

    if (!request.finishParse())
        return API_ERROR_SCHEMA_VALIDATION(request.getError());

    return JObject{{"returnValue", (bool)ret}};
}

pbnjson::JValue VideoService::getParam(LSHelpers::JsonRequest &request)
{
    std::string command;
    std::string sinkName;
    pbnjson::JValue response;
    int wId          = 0;
    int ret          = false;
    bool sinkNameSet = false;

    request.get("command", command);
    request.get("sink", sinkName).optional(true).checkValueRead(sinkNameSet);

    LOG_DEBUG("command:%s", command.c_str());

    if (sinkNameSet) {
        VideoSink *videoSink = getVideoSink(sinkName);
        if (!videoSink) {
            return API_ERROR_INVALID_PARAMETERS("Invalid sink: %s", sinkName.c_str());
        }
        wId = videoSink->wId;
        LOG_DEBUG("sink:%s, wId:%d", sinkName.c_str(), wId);
    }

    if (VAL_DEV_RPI == val->getDevice()) {
        pbnjson::JValue param = JValue();

        if (command == VAL_CTRL_DRM_RESOURCES) {
            int planeId = 0;
            int crtcId  = 0;
            int connId  = 0;
            std::string rsp_sink;

            param    = pbnjson::JValue{{"wId", wId}};
            response = val->video->getParam(command, param);
            response.put("sink", sinkName);

            // parse response to check validataion
            JsonParser parser{response};
            parser.get("returnValue", ret);
            if (ret == true) {
                parser.get("sink", rsp_sink);
                parser.get("planeId", planeId);
                parser.get("crtcId", crtcId);
                parser.get("connId", connId);
            }
            if (!parser.finishParse())
                return API_ERROR_SCHEMA_VALIDATION(parser.getError());

            LOG_DEBUG("command:%s ret:%d value:(sink:%s, plane:%d, crtc:%d, conn:%d)", command.c_str(), ret,
                      rsp_sink.c_str(), planeId, crtcId, connId);
        } else if (command == VAL_CTRL_NUM_CONNECTOR) {
            int numConnector = 0;

            response = val->video->getParam(command, param);

            // parse response to check validataion
            JsonParser parser{response};
            parser.get("returnValue", ret);
            if (ret == true) {
                parser.get("numConnector", numConnector);
            }
            if (!parser.finishParse())
                return API_ERROR_SCHEMA_VALIDATION(parser.getError());

            LOG_DEBUG("command:%s ret:%d value:(numCon:%d)", command.c_str(), ret, numConnector);
        } else {
            // Unknown command
            if (!request.finishParse())
                return API_ERROR_SCHEMA_VALIDATION(request.getError());

            return API_ERROR_INVALID_PARAMETERS("Unknown command %s", command.c_str());
        }
    } else {
        return API_ERROR_NOT_IMPLEMENTED;
    }

    if (!request.finishParse())
        return API_ERROR_SCHEMA_VALIDATION(request.getError());

    return response;
}

// TODO(ekwang) : almost same as getSupportedResolution()
void VideoService::readVideoCapabilities(VideoSink &sink)
{
    VAL_VIDEO_SIZE_T minDownSize, maxUpScale;

    std::vector<VAL_PLANE_T> supportedPlanes = val->video->getVideoPlanes();
    if (supportedPlanes.size() > sink.wId) {
        sink.minDownscaleSize = supportedPlanes[sink.wId].minSizeT;
        sink.maxUpscaleSize   = supportedPlanes[sink.wId].maxSizeT;
    } else {
        LOG_ERROR(MSGID_SINK_SETUP_ERROR, 0, "Invalid SinkId");
    }
}

// TODO:: Move this to AspectRatioSetting (Rename AspectRatioSetting to appropriate name)
bool VideoService::applyVideoOutputRects(VideoSink &sink, VideoClient client, VideoRect &inputRect,
                                         VideoRect &outputRect, VideoRect &sourceRect)
{
    LOG_DEBUG("applyVideoOutputRects called with inputRect {x:%d, y:%d, w:%u, h:%u},"
              "outputRect {x:%d, y:%d, w:%u, h:%u}, sourceRect {x:%d, y:%d, w:%u, h:%u}",
              inputRect.x, inputRect.y, inputRect.w, inputRect.h, outputRect.x, outputRect.y, outputRect.w,
              outputRect.h, sourceRect.x, sourceRect.y, sourceRect.w, sourceRect.h);

    // already set
    if (inputRect == sink.appliedInputRect && outputRect == sink.scaledOutputRect && sourceRect == client.sourceRect) {
        LOG_DEBUG("\n av  Rectangle are same");
        return true;
    }

    sink.scaledOutputRect = outputRect;

    if (!sourceRect.isValid()) {
        LOG_DEBUG("\n input Rectangle is invalid");
        // Wait for frame rect to be set (setVideoMediaData) before setting up outputs.
        // setVideoMediaData will set a different frame rect and continue the execution here.
        return true;
    }

    sink.appliedInputRect = inputRect.isValid() ? inputRect : sourceRect;

    if (!sink.scaledOutputRect.isValid()) {
        LOG_DEBUG("\n output Rectangle invalid");
        // Wait for output rect to be set.
        // setDisplayWidow will set a different output rect and continue the execution here.
        return true;
    }

    bool adaptive = false;

    VideoInfoMedia *videoinfomedia;
    if (client.sourceName == "VDEC" && client.videoinfoObj) {
        videoinfomedia = static_cast<VideoInfoMedia *>(client.videoinfoObj);
        adaptive       = videoinfomedia->adaptive;
    }

    return val->video->applyScaling(sink.wId, client.sourceRect.toVALRect(), adaptive,
                                    sink.appliedInputRect.toVALRect(), sink.scaledOutputRect.toVALRect());
}

bool VideoService::applyCompositing()
{
    // The parameters work like this:
    // [0].wId = windowId for TOP layer
    // [0].uAlpha - TOP layer opacity
    // [1].wId = windowId for next layer
    // [1].uAlpha - Next layer opacity
    // so on

    std::vector<VAL_WINDOW_INFO_T> zorder;
    zorder.resize(mSinks.size());

    for (VideoSink &sink : mSinks) {
        int zOrdering = sink.zOrder;

        zorder[zOrdering].wId          = sink.wId;
        zorder[zOrdering].uAlpha       = sink.opacity;
        zorder[zOrdering].inputRegion  = sink.appliedInputRect.toVALRect();
        zorder[zOrdering].outputRegion = sink.scaledOutputRect.toVALRect();
    }

    LOG_DEBUG("The zorder array is ");
    for (VAL_WINDOW_INFO_T zsink : zorder)
        LOG_DEBUG("wId %d, uAlpha %d", zsink.wId, zsink.uAlpha);

    return val->video->setCompositionParams(zorder);
}

// TODO: move this to PQ section!!!
bool VideoService::applyVideoFilters(VideoSink &sink, const std::string &sourceName)
{
    // Just a copy of what TVService is calling, with parameters taken from tvservice as well.
    int32_t sharpness_control[7];

    /* * set black level
     *	- UINT8 *pBlVal :
     *		[0] : uBlackLevel, 0:low,1:high
     *		[1] : nInputInfo(see HAL_VPQ_INPUT_T)
     *		[2] : nHDRmode, 0:off,1:hdr709,2:hdr2020,3:dolby709,4:dolby2020
     *	- void *pstData :
     *		see CHIP_CSC_COEFF_T
    */
    int32_t black_levels[3]   = {0, 0, 0};
    int32_t picture_control[] = {25, 25, 25, 25};
    if (sourceName == "VDEC") {
        sharpness_control[0] = 0;  // sSharpnessCtrlType, 0:normal, 1:h,v seperated
        sharpness_control[1] = 25; // sSharpnessValue, 0~50
        sharpness_control[2] = 10; // sHSharpnessValue, 0~50
        sharpness_control[3] = 10; // sVSharpnessValue, 0~50
        sharpness_control[4] = 2;  // sEdgeEnhancerValue, 0,1:off,on
        sharpness_control[5] = 1;  // sSuperResValue, 0~3 off,low,medium,high
        sharpness_control[6] = VAL_VPQ_INPUT_MEDIA_MOVIE;

        black_levels[1] = VAL_VPQ_INPUT_MEDIA_MOVIE;
    } else if (sourceName == "HDMI") {
        sharpness_control[0] = 0;  // sSharpnessCtrlType, 0:normal, 1:h,v seperated
        sharpness_control[1] = 25; // sSharpnessValue, 0~50
        sharpness_control[2] = 10; // sHSharpnessValue, 0~50
        sharpness_control[3] = 10; // sVSharpnessValue, 0~50
        sharpness_control[4] = 1;  // sEdgeEnhancerValue, 0,1:off,on
        sharpness_control[5] = 2;  // sSuperResValue, 0~3 off,low,medium,high
        sharpness_control[6] = VAL_VPQ_INPUT_HDMI_TV;

        // HAL_METHOD_CHECK_RETURN_FALSE(HAL_VSC_SetRGB444Mode(FALSE));
    } else if (sourceName == "RGB") {
        black_levels[1] = VAL_VPQ_INPUT_RGB_PC;
        // HAL_METHOD_CHECK_RETURN_FALSE(HAL_VSC_SetRGB444Mode(FALSE));
    } else {
        LOG_ERROR(MSGID_UNKNOWN_SOURCE_NAME, 0, "Internal error - unknown source name for picture quality: %s",
                  sourceName.c_str());
        return true;
    }

    // Don't consider there calls return value. These HAL calls are product dependent.
    val->controls->configureVideoSettings(SHARPNESS_Control, sink.wId, sharpness_control);
    val->controls->configureVideoSettings(PQ_Control, sink.wId, picture_control);
    val->controls->configureVideoSettings(BLACK_LEVEL_Control, sink.wId, black_levels);

    return true;
}

pbnjson::JValue VideoService::setAspectRatio(ARC_MODE_NAME_MAP_T currentAspectMode, int32_t allDirZoomHPosition,
                                             int32_t allDirZoomHRatio, int32_t allDirZoomVPosition,
                                             int32_t allDirZoomVRatio, int32_t vertZoomVRatio,
                                             int32_t vertZoomVPosition)
{
    mAspectRatioControl.setParams(currentAspectMode, allDirZoomHPosition, allDirZoomHRatio, allDirZoomVPosition,
                                  allDirZoomVRatio, vertZoomVRatio, vertZoomVPosition);

    // TODO:: update to cater to both subSink and mainSink
    // TODO(ekwang) : check using 0 directly. Is this function only for MAINsink?
    // TODO(ekwang) : check client activation when calling setAspectRatio
    VideoSink mainSink  = mSinks[0];
    VideoClient *client = getClientInfo(mainSink.name, true);

    if (!client)
        return API_ERROR_INVALID_PARAMETERS("Invalid client: %s", mainSink.name.c_str());

    if (client->fullScreen) {
        VideoRect input, output;

        VideoRect sinkWindowSize = VideoRect(mainSink.maxUpscaleSize.w, mainSink.maxUpscaleSize.h);
        mAspectRatioControl.scaleWindow(sinkWindowSize, client->sourceRect, input, output);

        if (!this->applyVideoOutputRects(mainSink, *client, input, output, client->sourceRect)) {
            return API_ERROR_HAL_ERROR;
        }
    }

    return true;
}

pbnjson::JValue VideoService::setBasicPictureCtrl(int8_t brightness, int8_t contrast, int8_t saturation, int8_t hue)
{
    LOG_DEBUG("set basic pictureControl properties %d %d %d %d", brightness, contrast, saturation, hue);
    int32_t uiVal[] = {brightness, contrast, saturation, hue};
    return val->controls->configureVideoSettings(PQ_Control, VAL_VIDEO_WID_1, uiVal);
}

pbnjson::JValue VideoService::setSharpness(int8_t sharpness, int8_t hSharpness, int8_t vSharpness)
{
    LOG_DEBUG("set setSharpness properties %d %d %d", sharpness, hSharpness, vSharpness);

    int32_t uiVal[] = {1, sharpness, hSharpness, vSharpness, 1, 0, 7};
    return val->controls->configureVideoSettings(SHARPNESS_Control, VAL_VIDEO_WID_1, uiVal);
}

bool VideoService::setDualVideo(bool enable)
{
    if (enable == this->mDualVideoEnabled) {
        return true;
    }

    if (!val->video->setDualVideo(enable)) {
        return false;
    }

    this->mDualVideoEnabled = enable;
    return true;
}

VideoSink *VideoService::getVideoSink(const std::string &sinkName)
{
    for (VideoSink &sink : mSinks) {
        LOG_DEBUG("compare msink name:%s, sinkname:%s", sink.name.c_str(), sinkName.c_str());
        if (sink.name == sinkName)
            return &sink;
    }

    return nullptr;
}

bool VideoService::addClientInfo(std::string clientId)
{
    if (nullptr == getClientInfo(clientId)) {
        mClients.push_back(VideoClient(clientId));
        LOG_DEBUG("addClientInfo %s, mClients size:%d", clientId.c_str(), mClients.size());
        return true;
    }

    // Already exist in list
    return false;
}

bool VideoService::removeClientInfo(std::string clientId)
{
    LOG_DEBUG("removeClientInfo %s", clientId.c_str());

    for (std::vector<VideoClient>::iterator iter = mClients.begin(); iter != mClients.end();) {
        if ((*iter).clientId == clientId) {
            if (iter->videoinfoObj)
                delete iter->videoinfoObj;
            mClients.erase(iter);
            LOG_DEBUG("clientId:%s earased. mClients size:%d", clientId.c_str(), mClients.size());
            return true;
        } else
            ++iter;
    }

    // Can not find in list
    return false;
};

VideoClient *VideoService::getClientInfo(const std::string clientId)
{
    LOG_DEBUG("getClientInfo Id: %s", clientId.c_str());

    for (VideoClient &client : mClients) {
        LOG_DEBUG("compare pid : %s, %s", client.clientId.c_str(), clientId.c_str());
        if (client.clientId == clientId) {
            return &client;
        }
    }

    LOG_DEBUG("no matched info for %s", clientId.c_str());
    return nullptr;
};

VideoClient *VideoService::getClientInfo(const std::string sinkName, bool activation)
{
    LOG_DEBUG("getClientInfo with sinkname: %s", sinkName.c_str());

    for (VideoClient &client : mClients) {
        LOG_DEBUG("compare sink : %s, %s", client.sinkName.c_str(), sinkName.c_str());
        if (client.activation == activation && client.sinkName == sinkName) {
            return &client;
        }
    }

    LOG_DEBUG("no matched info for %s", sinkName.c_str());
    return nullptr;
};

bool VideoService::LoadClientInfotoVideoSink(VideoSink &sink, VideoClient &client)
{
    LOG_DEBUG("LoadClientInfotoVideoSink");

    client.debug_print("load client");

    sink.appliedInputRect = client.inputRect;
    sink.scaledOutputRect = client.outputRect;

    return true;
};

void VideoService::converToDisplayResolution(VideoRect &outputRect)
{
    // TODO: get from ConfigD mayyoon_181031
    uint16_t osdWidth      = 1920;
    uint16_t osdHeight     = 1080;
    uint16_t displayWidth  = 3840;
    uint16_t displayHeight = 2160;

    outputRect.x = outputRect.x * displayWidth / osdWidth;
    outputRect.y = outputRect.y * displayHeight / osdHeight;
    outputRect.w = outputRect.w * displayWidth / osdWidth;
    outputRect.h = outputRect.h * displayHeight / osdHeight;
}
