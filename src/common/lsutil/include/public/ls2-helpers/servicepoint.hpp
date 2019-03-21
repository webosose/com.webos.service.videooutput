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
#include <vector>
#include <algorithm>
#include <luna-service2/lunaservice.hpp>

#include "jsonrequest.hpp"
#include "jsonresponse.hpp"

namespace LSHelpers {

/**
 * @brief Luna client object. Manages lifetime of a collection of method handlers and ongoing luna calls.
 * Tracks ongoing calls and cancels them if the ServicePoint is destroyed.
 * There can be multiple ServicePoints per luna handle.
 *
 * Recommended usage is to have a single ServicePoint for each C++ class with handler methods.
 * That way all pending calls and subscriptions will be stopped on object destruction.
 * Ensuring no access after free conditions.
 *
 * Multithreading:
 *
 * The setup functions - registerSignal, registerMethod are not thread safe.
 * The call, subscribe, cancelCall methods are thread safe.
 *
 * The method handler and response callback methods are executed in the context of the event loop
 * used by luna handle regardless of the thread it was called from.
 *
 * Example:
 * @code
	class MyClass
	{
	public:
		MyClass(LS::Handle* handle):mLunaClient(handle)
		{
			mLunaClient.registerMethod("/", "myMethod", this, &MyClass::handleMyMethod);

			mLunaClient.callMultiReply(
				"luna://com.webos.service.stuff/getStatus",
				pbnjson::JObject {{"subscribe", true}},
				this, &MyClass::getStatusResponse);
		}

		pbnjson::JValue MyClass::handleMyMethod(JsonRequest& request)
		{
			std::string data;

			request.get("data", data);
			request.finishParseOrThrow();

			if (data.length() == 0)
			{
				return ErrorResponse(104, "Data is empty");
			}

			//Do something with data...

			return true;
		}

		void getStatusResponse(LunaResponse& response)
		{
			std::string status;
			std::string payload;
			response.get("status",status);
			response.get("payload",payload).optional();
			response.finishParseOrThrow();

			if (status == "ping")
			{
				mLunaClient.callOneReply(
					"luna://com.webos.service.stuff/pong",
					pbnjson::JObject {{"payload", payload}},
					nullptr);
			}
		}

		void doStuff(const std::string& data)
		{
			mLunaClient.callOneReply(
				"luna://com.webos.service.stuff/doStuff",
				pbnjson::JObject {{"data", data}},
				nullptr);
		}


		void doStuffAndReactToResult(const std::string& data)
		{
			mLunaClient.callOneReply(
				"luna://com.webos.service.stuff/doStuff",
				pbnjson::JObject {{"data", data}},
				[](LSHelpers::JsonResponse& response)
				{
					if (response.isSuccess())
					{
						// Do suff on succcess
					}
				});
		}

		ServicePoint mLunaClient;
	};
 * @endcode
 */
class ServicePoint
{
public:
	explicit ServicePoint(LS::Handle* handle);
	~ServicePoint();

	/**
	 * Not copyable.
	 */
	ServicePoint(const ServicePoint&) = delete;
	ServicePoint& operator=(const ServicePoint&) = delete;

	inline LS::Handle* getHandle()
	{
		return this->mHandle;
	}

	/**
	 * Registers a new method on the bus.
	 * Helper method that accepts a object pointer and method pointer.
	 * @param category category name. For example "/"
	 * @param methodName the method name
	 * @param handler handler method or lambda to call.
	 * @param schema json schema to use. If set, will validate the request against the schema before calling the handler method.
	 * @throws std::logic_error if a method is already registered with specified category and name.
	 */
	void registerMethod(const std::string& category,
	                    const std::string& methodName,
	                    const JsonRequest::Handler& handler,
	                    const pbnjson::JSchema& schema = pbnjson::JSchema::AllSchema());

	/**
	 * Helper method that accepts a object pointer and method pointer.
	 * Example: @code lunaService.registerMethod("/", "myMethod", this, &MyObj::myMethod); @endcode
	 * @param category category name. For example "/"
	 * @param methodName the method name
	 * @param object pointer to the object to call
	 * @param handler pointer to object's member method
	 * @param schema json schema to use. If set, will validate the request against the schema before calling the handler method.
	 * @throws std::logic_error if a method is already registered with specified category and name.
	 */
	template<typename T>
	void registerMethod(const std::string& category,
	                    const std::string& methodName,
	                    T* object,
	                    pbnjson::JValue (T::* handler) (JsonRequest& request),
	                    const pbnjson::JSchema& schema = pbnjson::JSchema::AllSchema())
	{
		registerMethod(category, methodName, std::bind(handler, object,  std::placeholders::_1), schema);
	};

	/**
	 * Registers a new signal on the bus.
	 * This just makes the signal visible to introspection.
	 * Note that all services share a common namespace for signal categories.
	 * So your category should be unique. The service name is disregarded when handling signals.
	 *
	 * Example: @code  service.registerSignal("/myservice","signal"); @endcode
	 * @param category category name. For example "/myservice"
	 * @param methodName the method name.
	 */
	void registerSignal(const std::string& category,
	                    const std::string& methodName);

	/**
	 * Make a one reply call.
	 * If this call succeeds (does not throw) the handler method is guaranteed to be eventually called.
	 * Regardless of any error condition within the luna bus.
	 * You can use response.isSuccess() to check if the response is a success or not, response.finishParse does this internally.
	 * @param uri
	 * @param params
	 * @param handler - if set the handler method will be called. If not set, nothing will be called.
	 * @return luna message token that can be used ot cancel the call (even when no callback is set).
	 * @throw LS::Error on luna error
	 */
	inline LSMessageToken callOneReply(const std::string& uri,
	                                   const pbnjson::JValue& params,
	                                   const JsonResponse::Handler& handler)
	{
		return makeCall(uri, params, true, handler);
	}

	/**
	 * Make a one reply call - convenience method that accepts pointer and member method instead of std::function.
	 * @tparam T
	 * @param uri
	 * @param params
	 * @param object
	 * @param handler
	 * @return
	 */
	template<typename T>
	inline LSMessageToken callOneReply(const std::string& uri,
	                                   const pbnjson::JValue& params,
	                                   T* object,
	                                   void  (T::* handler) (JsonResponse& response))
	{
		return callOneReply(uri, params, std::bind(handler, object, std::placeholders::_1));
	}

	/**
	 * Make a multi reply call. The call is active until cancelCall or client is deleted.
	 * If this call succeeds (does not throw) the handler method is guaranteed to be eventually called at least once.
	 * Regardless of any error condition within the luna bus.
	 * You can use response.isSuccess to check if the response is a success or not.
	 * @param uri
	 * @param params
	 * @param handler - mandatory response handler.
	 * @return luna token, that can be used ot cancel the call.
	 * @throw LS::Error on luna error, std::logic_error if no response handler.
	 */
	inline LSMessageToken callMultiReply(const std::string& uri,
	                                    const pbnjson::JValue& params,
	                                    const JsonResponse::Handler& handler)
	{
		return makeCall(uri, params, false, handler);
	}

	/**
	 * Make a multi reply call - convenience method that accepts pointer and member method instead of std::function.
	 * @tparam T
	 * @param uri
	 * @param params
	 * @param object
	 * @param handler
	 * @return
	 */
	template<typename T>
	inline LSMessageToken callMultiReply(const std::string& uri,
	                                     const pbnjson::JValue& params,
	                                     T* object,
	                                     void  (T::* handler) (JsonResponse& response))
	{
		return callMultiReply(uri, params, std::bind(handler, object, std::placeholders::_1));
	}

	/**
	 * Sends a signal to all subscribers.
	 * @param category - signal category
	 * @param method - signal method
	 * @param payload - payload to send
	 */
	void sendSignal(const std::string& category,
	                const std::string& method,
	                const pbnjson::JValue& payload);

	/**
	 * Subscribe to a signal.
	 * Note that the response handler receives only signal responses.
	 * Or error response from hub if there is error while subscribing.
	 * @param category   category name to monitor
	 * @param methodName method name to monitor
	 * @param handler    callback function
	 * @return luna token, can be used to cancel the call.
	 */
	inline LSMessageToken subscribeToSignal(const std::string& category,
	                                        const std::string& methodName,
	                                        const JsonResponse::Handler& handler)
	{
		// Consume the first response if it's successful.
		std::shared_ptr<bool> firstResponse { new bool (true) };

		auto wrapperFunc = [handler, firstResponse](JsonResponse& response) -> void {
			if (*firstResponse.get() && response.isSuccess())
			{
				*firstResponse.get() = false;
				return;
			}

			handler(response);
		};

		return makeCall("luna://com.webos.service.bus/signal/addmatch",
		                pbnjson::JObject{{ "category",category}, {"method", methodName}},
		                false,
		                wrapperFunc);
	}

	/**
	 * Subscribe to a signal.
	 *
	 * @param category   category name to monitor
	 * @param methodName method name to monitor
	 * @param object     object to call callback function on
	 * @param handler    callback function
	 * @return luna token, can be used to cancel the call.
	 */
	template<typename T>
	inline LSMessageToken subscribeToSignal(const std::string& category,
	                                        const std::string& methodName,
	                                        T* object,
	                                        void  (T::* handler) (JsonResponse& response))
	{
		return callSignal(category, methodName, std::bind(handler, object, std::placeholders::_1));
	}

	/**
	 * Cancels the call and removes any queued replies.
	 * The handler method will not be called after this.
	 * @param token
	 */
	void cancelCall(LSMessageToken token);

private:
	// Internal call object
	struct Call
	{
		Call(ServicePoint* _service,
		     LSMessageToken _token,
		     const JsonResponse::Handler& _handler,
		     bool _oneReply)
				:service(_service)
				, token(_token)
				, handler(_handler)
				, oneReply(_oneReply)
		{}

		ServicePoint* service;
		LSMessageToken token;
		JsonResponse::Handler handler;
		bool oneReply;
	};
	//Internal method object
	struct MethodInfo
	{
		MethodInfo(ServicePoint* _service,
		           const JsonRequest::Handler& _handler,
		           const pbnjson::JSchema& _schema,
		           const std::string& _category,
		           const std::string& _method)
				: service(_service)
				, handler(_handler)
				, schema(_schema)
				, category(_category)
				, method(_method)
		{}

		ServicePoint* service;
		JsonRequest::Handler handler;
		pbnjson::JSchema schema;
		std::string category;
		std::string method;
	};

	LSMessageToken makeCall(const std::string& uri, const pbnjson::JValue& params, bool oneReply, const JsonResponse::Handler& handler);
	void cancelCall(Call* call);

	void registerMethodImpl(MethodInfo& method);
	void unregisterMethodImpl(MethodInfo& method);

	static bool methodHandler(LSHandle *sh, LSMessage *msg, void *method_context);
	static bool removedMethodHandler(LSHandle *sh, LSMessage *msg, void *method_context);
	static bool callResponseHandler(LSHandle *sh, LSMessage *reply, void *ctx);

	// Instance variables
	LS::Handle* mHandle;

	// Need unique ptr, because we will be passing pointers around.
	std::vector<std::unique_ptr<MethodInfo> > mMethods;
	std::unordered_map<LSMessageToken, std::unique_ptr<Call> > mCalls;
	std::mutex mCallsMutex; // Lock access to mCalls.
};

} // namespace LSHelpers;
