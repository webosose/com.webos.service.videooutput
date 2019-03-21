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

#include <luna-service2/lunaservice.hpp>

#include "jsonparser.hpp"

namespace LSHelpers {

/**
 * @brief Luna response object for luna rensponse handler.
 *
 * Example:
 * @code

	service.callOneReply(
		"luna://com.webos.service.stuff/getCount",
		pbnjson::JObject {{"name", "someName"}},
		&jonResponseHandler);

	void jonResponseHandler(LSHelpers::JsonResponse& response)
	{
		std::string name;
		int count;

		response.param("name", name);
		response.param("count", count);

		if (response.finishParse() && response.isSuccess())
		{
			// Do suff on succcess
		}
		else
		{
			// Do suff on fail.
		}
	}
 * @endcode
 *
 * Example2:
 * @code
	service.callOneReply(
		"luna://com.webos.service.stuff/getCount",
		pbnjson::JObject {{"name", "someName"}},
		[](LSHelpers::JsonResponse& response){
	        std::string name;
	        int count;

	        response.param("name", name);
	        response.param("count", count);

			if (response.finishParse() && response.isSuccess())
			{
				// Do suff on succcess
			}
			else
			{
				// Do suff on fail.
			}
		});
 * @endcode
 */
class JsonResponse: public JsonParser
{
public:
	/**
	 * Call response handler function signature.
	 * @param response response object. Check response.isSuccess if the call succeeded.
	 */
	typedef std::function<void (JsonResponse& response)> Handler;

	/**
	 * Handler method - parses the message and calls handler.
	 * Catches any exceptions and logs them.
	 * @param msg the luna message to handle
	 * @param handler handler function to call.
	 * @param schema schema to use for validation (optional).
	 * @return true
	 */
	static bool handleLunaResponse(LSMessage* msg,
	                               const Handler& handler,
	                               const pbnjson::JSchema& schema = pbnjson::JSchema::AllSchema()) noexcept;

	/**
	 * @return the token of the call that this response replies to.
	 */
	inline LSMessageToken getCallToken() { return LSMessageGetResponseToken(mMessage); }
	inline LS::Message getMessage() { return LS::Message(mMessage); }

	/**
	 * Check if the response is successful (contains returnValue:true)
	 * @return true if successful, false if not.
	 */
	inline bool isSuccess(){return mSuccess;}

	/**
	 * Calls finishParse and checks if finishParse or the call itself has an error.
	 */
	inline bool hasErrors(){return !mSuccess || !JsonParser::finishParse(false);};

private:
	/**
	 * Initalize luna request with specified message.
	 */
	JsonResponse(LSMessage* message, pbnjson::JValue value, bool isSuccess)
			: JsonParser(value)
			, mMessage(message)
			, mSuccess(isSuccess)
	{}

	LSMessage* mMessage;
	bool mSuccess;
};

} // namespace LSHelpers;
