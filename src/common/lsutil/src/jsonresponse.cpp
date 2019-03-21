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
#include <pbnjson.hpp>

#include "util.hpp"
#include "jsonresponse.hpp"

using namespace pbnjson;

namespace LSHelpers {

bool JsonResponse::handleLunaResponse(LSMessage* msg,
                                      const JsonResponse::Handler& handler,
                                      const pbnjson::JSchema& schema) noexcept
{
	LS::Message message{msg};

	// Parse the message.
	JValue payload {}; // Null by default
	bool success;

	if (message.isHubError())
	{
		LOG_ERROR(MSGID_LS_HUB_ERROR,
		             0,
		             "Hub error during luna call, method: %s, payload: %s",
		             message.getMethod(),
		             message.getPayload());
		success = false;
	}
	else
	{
		JValue value = JDomParser::fromString(message.getPayload(), schema);

		if (!value.isValid())
		{
			LOG_ERROR(MSGID_LS_RESPONSE_JSON_PARSE_FAILED, 0,
			             "Failed to parse luna response to JSON: %s, error: %s",
			             message.getPayload(),
			             value.errorString().c_str());
			success = false;
		}
		else
		{
			success = true;
			payload = value;
		}
	}

	JsonResponse response {msg, payload, success};

	try
	{
		handler(response);
	}
	catch (const JsonParseError& e)
	{
		LOG_ERROR(MSGID_LS_RESPONSE_PARAMETERS_ERROR, 0, "Response handler failed to parse response parameters: %s", e.what());
	}
	catch (std::exception& e)
	{
		LOG_ERROR(MSGID_LS_UNEXPECTED_EXCEPTION, 0, "Exception thrown while processing luna response handler: %s", e.what());
	}
	catch (...)
	{
		LOG_ERROR(MSGID_LS_UNEXPECTED_EXCEPTION, 0, "Exception thrown while processing luna response handler");
	}

	return true;
}

} // namespace LSHelpers;
