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

#include <functional>
#include <ls2-helpers/ls2-helpers.hpp>

#include "errors.h"
#include "logging.h"

using namespace pbnjson;
using namespace LSHelpers;

template <typename T>
class SubscribeAdapter
{
public:
    typedef std::function<void(JValue &responseJValue)> ResponseHandler;
    typedef std::function<pbnjson::JValue(JsonResponse &response)> ResponseParser;

    SubscribeAdapter(LS::Handle &handle, T *object, void (T::*callbackHandler)(JValue &responseJval))
        : mLunaClient(&handle), mSubscription(), mCallToken(0), mService("unknown")
    {
        mResponseHandler = std::bind(callbackHandler, object, std::placeholders::_1);
    };

    ~SubscribeAdapter(){};
    SubscribeAdapter(const SubscribeAdapter &) = delete;
    SubscribeAdapter &operator=(const SubscribeAdapter &) = delete;

    void subscribeTo(pbnjson::JValue jobject, std::string service, T *object,
                     pbnjson::JValue (T::*callbackHandler)(JsonResponse &response))
    {
        mService        = service;
        mResponseParser = std::bind(callbackHandler, object, std::placeholders::_1);
        mSubscription.subscribe(mLunaClient, mService, jobject, this, &SubscribeAdapter::responseHandler);
    };

    // Cancels previous call if still in progress.
    void makeOneCall(pbnjson::JValue jobject)
    {
        if (mCallToken) {
            mLunaClient.cancelCall(mCallToken);
        }

        mCallToken = mLunaClient.callOneReply(mService, jobject, [this](JsonResponse &response) {
            mCallToken = 0;
            responseHandler(response);
        });
    };

    void responseHandler(JsonResponse &response)
    {
        LOG_DEBUG("Got response from %s", mService.c_str());
        pbnjson::JValue retResponse;

        retResponse = mResponseParser(response);
        mResponseHandler(retResponse);
    };

private:
    LSHelpers::ServicePoint mLunaClient;
    PersistentSubscription mSubscription;
    ResponseHandler mResponseHandler;
    ResponseParser mResponseParser;
    LSMessageToken mCallToken;
    std::string mService;
};
