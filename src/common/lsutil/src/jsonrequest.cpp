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

#include <errno.h>
#include <sstream>

#include "util.hpp"

#include "jsonrequest.hpp"

using namespace pbnjson;

namespace LSHelpers {

JsonRequest::JsonRequest(const LS::Message& message, const pbnjson::JValue params)
		: JsonParser(params)
		, mMessage(message)
		, mDeferred(false)
		, mResponded(false)
{

}

JsonRequest::~JsonRequest()
{
	if (unlikely(!mResponded)) // The request was not responded. Send a stock response.
	{
		respond(API_ERROR_NO_RESPONSE);
	}
}

bool JsonRequest::handleLunaCall(LSMessage* msg, const JsonRequest::Handler& handler, const JSchema& schema)
{
	LS::Message message{msg};

	try
	{
		const char* payload = message.getPayload();
		JValue value = JDomParser::fromString(payload, schema);

		if (unlikely(!value.isValid()))
		{
			LOG_ERROR(MSGID_LS_CALL_JSON_PARSE_FAILED, 0,
			             "Failed to validate luna request against schema: %s, error: %s",
			             payload,
			             value.errorString().c_str());

			//Figure out if the Josn is invalid or just does not validate against the schema.
			if (!JDomParser::fromString(payload, JSchema::AllSchema()).isValid())
			{
				throw API_ERROR_MALFORMED_JSON;
			}
			else
			{
				throw API_ERROR_SCHEMA_VALIDATION( "Failed to validate luna request against schema");
			}
		}

		std::shared_ptr<JsonRequest> request( new JsonRequest{message, value} );
		request->mWeakPtr = request;
		request->mResponded = true; // For the exception cases

		JValue result = handler(*request.get());

		if (!request->mDeferred)
		{
			request->respond(result);
		}
		else
		{
			request->mResponded = false;
		}

		return true;
	}
	catch (const JsonParseError& e)
	{
		message.respond(API_ERROR_SCHEMA_VALIDATION(e.what()).stringify().c_str());
		return true;
	}
	catch (ErrorResponse& e)
	{
		message.respond(e.stringify().c_str());
		return true;
	}
	catch (const std::exception& e)
	{
		LOG_ERROR(MSGID_LS_UNEXPECTED_EXCEPTION, 0, "Method '%s' handler throws exception: %s",
		             message.getMethod(), e.what());
		return false;
	}
	catch (...)
	{
		LOG_ERROR(MSGID_LS_UNEXPECTED_EXCEPTION, 0, "Method '%s' handler throws exception",
		             message.getMethod());
		return false;
	}
}

JsonRequest::DeferredResponseFunction JsonRequest::defer()
{
	if (unlikely(mDeferred))
	{
		LOG_ERROR(MSGID_LS_DOUBLE_DEFER, 0, "Trying to defer a request that's already deferred");
	}

	mDeferred = true;
	std::shared_ptr<JsonRequest> request = mWeakPtr.lock();

	// Include a shared_ptr to the request in the capture.
	// This will ensure that LunaRequest is alive as long as there is a copy of this lambda.
	// And will free memory once all copies have been destroyed.
	// Similar to how LS::Message internally works

	return [request](const pbnjson::JValue& response)
	{
		request->respond(response);
		// Captured shared ptr release will delete the request object.
	};
}

void JsonRequest::respond(const pbnjson::JValue& response)
{
	// Get away from const, this is reference counted pointer, no copying.
	JValue result = response;

	if (!result.isObject())
	{
		if (result.isBoolean() && result.asBool())
		{
			// This is just a "true", converted to JValue.
			// Replace with a basic {"returnValue":true}.
			result = JObject{{"returnValue", true}};
		}
		else
		{
			LS::Error err;
			_LSErrorSet(err.get(), MSGID_LS_INVALID_RESPONSE, -EINVAL, "Response is not a JSON object");
			throw err;
		}
	}

	mMessage.respond(result.stringify().c_str());
	mResponded = true;
}

ErrorResponse::ErrorResponse(int error_code, const char* format, ...)
		: pbnjson::JObject{{"returnValue", false}, {"errorCode", error_code}}
{
	va_list args;
	va_start(args, format);
	this->put("errorMessage", string_format_valist(format, args));
	va_end(args);
}

} // Namespace LSHelpers
