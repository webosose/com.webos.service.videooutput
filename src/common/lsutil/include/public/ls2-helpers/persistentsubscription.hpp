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

#include "jsonresponse.hpp"
#include "serverstatus.hpp"

namespace LSHelpers {

/**
 * @brief
 * Helper class to manage a persistent subscription.
 * Tracks service online state and performs the call every time the service goes online.
 * The handler method receives all responses, including a failed response if the service is terminated.
 *
 * Multithreading: This class is **not** thread safe. The response handler method is called in luna handle context.
 *
 * Example:
 * @code
 * class MyClass{
 *   MyClass(LS::Handle* handle)
 *     : client(handle)
 *    {
 *
 *     getVolumeSubscription.subscribe(client.getHandle(),
 *                                     "luna://com.webos.audio/getVolume",
 *                                     JObject{{"subscribe", true}},
 *                                     [](LunaResponse& response)
 *        {
 *           int volume;
 *           response.param("volume", volume);
 *           response.finishParseOrThrow(false);
 *
 *           std::cerr << "Volume now is : " << volume << std::endl;
 *        });
 *   }
 *
 *   private:
 *      LunaServiceClient client;
 *
 *      PersistentSubscription getVolumeSubscription;
 * }
 * @endcode
 */
class PersistentSubscription
{
public:
	PersistentSubscription():
			mHandle(nullptr),
			mSubscriptionCall(LSMESSAGE_TOKEN_INVALID)
	{}

	~PersistentSubscription()
	{
		cancel();
	}

	/** Non copyable */
	PersistentSubscription(const PersistentSubscription&) = delete;
	PersistentSubscription& operator=(const PersistentSubscription&) = delete;

	/**
	 * Start persistent subscription.
	 * @param handle luna service handle to use.
	 * @param uri URI to subscribe to
	 * @param params Json parameters for subscribe call
	 * @param handler - mandatory response handler function.
	 * @throw LS::Error on luna error.
	 */
	void subscribe(LS::Handle* handle, const std::string& uri, const pbnjson::JValue& params, const JsonResponse::Handler& handler);

	/**
	 * Wrapper method that accepts a class method.
	 * @param handle luna service handle to use.
	 * @param uri URI to subscribe to
	 * @param params Json parameters for subscribe call
	 * @param object pointer to a class object
	 * @param handler pointer to a class method to call
	 * @throw LS::Error on luna error, std::logic_error if no response handler.
	 */
	template<typename T>
	void subscribe(LS::Handle* handle,
	               const std::string& uri,
	               const pbnjson::JValue& params,
	               T* object,
	               void  (T::* handler) (JsonResponse& response))
	{
		subscribe(handle, uri, params, std::bind(handler, object, std::placeholders::_1));
	}

	/**
	 * Wrapper method that accepts a service point and class method.
	 * @param handle luna service point.
	 * @param uri URI to subscribe to
	 * @param params Json parameters for subscribe call
	 * @param object pointer to a class object
	 * @param handler pointer to a class method to call
	 * @throw LS::Error on luna error, std::logic_error if no response handler.
	 */
	template<typename T>
	void subscribe(LSHelpers::ServicePoint& service,
	               const std::string& uri,
	               const pbnjson::JValue& params,
	               T* object,
	               void  (T::* handler) (JsonResponse& response))
	{
		subscribe(service.getHandle(), uri, params, std::bind(handler, object, std::placeholders::_1));
	}

	/**
	 * Wrapper method that accepts a service point.
	 * @param handle luna service point.
	 * @param uri URI to subscribe to
	 * @param params Json parameters for subscribe call
	 * @param object pointer to a class object
	 * @param handler pointer to a class method to call
	 * @throw LS::Error on luna error, std::logic_error if no response handler.
	 */
	template<typename T>
	void subscribe(LSHelpers::ServicePoint& service,
	               const std::string& uri,
	               const pbnjson::JValue& params,
	               const JsonResponse::Handler& handler)
	{
		subscribe(service.getHandle(), uri, params, handler);
	}

	/**
	 * Checks if subscribed service is currently active
	 * Note that this will be false immediately after the subscribe call.
	 * As the call is async.
	 * @return
	 */
	inline bool isServiceActive()
	{
		return mSubscriptionCall != LSMESSAGE_TOKEN_INVALID;
	}

	/**
	 * Cancel subscription.
	 */
	void cancel();

private:
	bool onServiceStatusResponse(bool);
	static bool onCallResponse(LSHandle *, LSMessage *msg, void *method_context);

	void cancelSubscription();

	LSHandle* mHandle;
	LSMessageToken mSubscriptionCall;
	ServerStatus mServiceStatus;

	std::string mUri;
	std::string mParams;
	JsonResponse::Handler mResultHandler;
};

} // namespace LSHelpers;
