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

#include "jsonparser.hpp"

namespace LSHelpers {

/**
 * @brief Error class to use for sending Luna error response.
 * Throw within a handler method to return early and send error response to caller.
 *
 * Doubles as JValue and can be returned from handler methods.
 *
 * @see @ref LSHelpers::JsonRequest
 */
class ErrorResponse: public pbnjson::JObject
{
public:
	ErrorResponse(int error_code, const std::string& _message):
			pbnjson::JObject{{"returnValue", false}, {"errorCode", error_code}, {"errorMessage", _message}}
	{};

	ErrorResponse(const ErrorResponse& e): pbnjson::JObject(e)
	{}

	ErrorResponse(int error_code, const char* format, ...);
};

/**
 * @brief A wrapper class around a luna request.
 * Provides convenience methods for parsing the request to C++ variables
 * and creating a reply.

 * Example:
 *	@code
 * pbnjson::JValue MyClass::lunaHandlerMethod(JsonRequest& request)
 * {
 *	std::string contextName;
 *	AudioTypeEnum audioType;
 *	uint8_t portNumber;
 *	std::vector<AVResource> resourceList; // AVResource implements JsonDataObject
 *
 *  JsonParser parser(jvalueObject);
 *	parser.get("context", contextName);
 *	parser.get("portNumber", portNumber).optional(true).defaultValue(255);
 *	parser.getArray("resourceList", resourceList);
 *	parser.getAndMap<std::string, AudioTypeEnum>("audioType", audioType, {
 *		{"Analog", AUTIODYPE_ANALOG},
 *		{"Digital", AUTIODYPE_DIGITAL}});
 *
 *	request.finishParseOrThrow();
 *	Context& context = getContextOrThrow(contextName);
 *
 *	if (!context.doSomeStuff())
 *	{
 *	  return ErrorResponse(104, "Failed to do stuff");
 *	}
 *
 *	return true;        // Equivalent to return JObject{{"returnValue":true}}
 * }
 *
 * Context& getContextOrThrow(const std::string& contextName)
 * {
 * 	if (!hasContext(contextName))
 * 	{
 * 		throw ErrorResponse(105, "Context not found");
 * 	}
 *
 * 	*return getContext(contextName);
 * }
 *
 * @endcode
 */
class JsonRequest: public JsonParser
{
public:
	/**
	 * Call handler function signature.
	 * @param request - request object
	 * @return JValue object containing the response.
	 * @throw ErrorResponse to return errors. Corresponding error message is sent to caller.
	 * @throw JsonParseError for input validation errors. Corresponding error message is sent to caller.
	 */
	typedef std::function<pbnjson::JValue(JsonRequest& request)> Handler;

	/**
	 * Deferred response function signature.
	 */
	typedef std::function< void(const pbnjson::JValue& response) > DeferredResponseFunction;

	/**
	 * Handler method - parses the message and calls handler. Sends error responses if failed to parse
	 * the payload.
	 * @param msg the luna message to handle.
	 * @param handler handler method to call.
	 * @param schema schema to use for validation (optional).
	 * @return true if the call was handled. False if an unknown exception was thrown.
	 */
	static bool handleLunaCall(LSMessage* msg,
	                           const Handler& handler,
	                           const pbnjson::JSchema& schema = pbnjson::JSchema::AllSchema());

	~JsonRequest();

	/** Not copyable. */
	JsonRequest(const JsonRequest&) = delete;
	JsonRequest& operator=(const JsonRequest&) = delete;

	/**
	 * Defer the response to this call.
	 * Can call this only once per request.
	 * Can send responses as long as you have the ResponseFunc.
	 * Is automatically cleaned up once all copies of ResponseFunc go out of scope.
	 *
	 * Example:
	 * @code
	 *     JValue handler(LunaRequest& request)
	 *     {
	 *        auto responseFunction = request.defer();
	 *
	 *        // Delay response by 1 second.
	 *        setTimeout(1000, [responseFunction]()
	 *        {
	 *           responseFunction(JObject{{"returnValue", true}, {"payload", "hello"}});
	 *        });
	 *
	 *        return true; // The return value is ignored if the call is deferred.
	 *     }
	 * @endcode
	 *
	 * @return a function object to be called to send response.
	 *     One or more responses can be sent.
	 *
	 */
	DeferredResponseFunction defer();

	/**
	 * @return underlying message.
	 */
	inline const LS::Message getMessage() const { return mMessage; }

private:
	JsonRequest(const LS::Message& message, const pbnjson::JValue params);

	// Send response to caller.
	void respond(const pbnjson::JValue& response);

	LS::Message mMessage;
	std::weak_ptr<JsonRequest> mWeakPtr; // Weak pointer to self, for use in defer
	bool mDeferred; // Response deferred.
	bool mResponded; // If at least one response is sent back.
};

} // namespace LSHelpers;
