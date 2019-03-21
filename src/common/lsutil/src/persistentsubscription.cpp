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

#include "persistentsubscription.hpp"
#include "util.hpp"

namespace LSHelpers {

void PersistentSubscription::subscribe(LS::Handle* handle,
                                       const std::string& uri,
                                       const pbnjson::JValue& params,
                                       const JsonResponse::Handler& handler)
{
	cancel();
	LS::Error e;

	if (!handle)
	{
		_LSErrorSet(e.get(), MSGID_LS_NO_HANDLE, -EINVAL, "Handle is null");
		throw e;
	}

	if (!handler)
	{
		_LSErrorSet(e.get(), MSGID_LS_NO_HANDLER, -EINVAL, "Handler is null");
		throw e;
	}

	if (!params.isValid())
	{
		_LSErrorSet(e.get(), MSGID_LS_INVALID_JVALUE, -EINVAL, "Params not valid");
		throw e;
	}

	size_t first_slash = uri.find("://");
	size_t second_slash = uri.find("/", first_slash + 3);

	if (first_slash == std::string::npos || second_slash == std::string::npos)
	{
		_LSErrorSet(e.get(), MSGID_LS_INVALID_URI, -EINVAL, "Invalid service URI");
		throw e;
	}

	mHandle = handle->get();
	mUri = uri;
	mParams = pbnjson::JGenerator::serialize(params, pbnjson::JSchema::AllSchema());
	mResultHandler = handler;

	std::string serviceName = uri.substr(first_slash+3, second_slash - first_slash - 3);

	mServiceStatus.set(handle, serviceName, std::bind(
			&PersistentSubscription::onServiceStatusResponse,
			this,
			std::placeholders::_2));
}

void PersistentSubscription::cancel()
{
	mParams = "";
	mResultHandler = nullptr; //Frees any associated closures.

	mServiceStatus.cancel();
	cancelSubscription();
}

void PersistentSubscription::cancelSubscription()
{
	if (mSubscriptionCall != LSMESSAGE_TOKEN_INVALID)
	{
		// Dont propagate the cancel error, just log it.
		LSCallCancel(mHandle, mSubscriptionCall, nullptr);
		mSubscriptionCall = LSMESSAGE_TOKEN_INVALID;
	}
}

bool PersistentSubscription::onServiceStatusResponse(bool serviceUp)
{
	if (serviceUp && mSubscriptionCall == LSMESSAGE_TOKEN_INVALID)
	{
		LS::Error error;
		LSCall(mHandle, mUri.c_str(), mParams.c_str(),
		       &PersistentSubscription::onCallResponse, this,
		       &mSubscriptionCall, error.get());

		if (error.isSet())
		{
			throw error;
		}
	}
	else if (!serviceUp && mSubscriptionCall != LSMESSAGE_TOKEN_INVALID)
	{
		cancelSubscription();
	}

	return true;
}

bool PersistentSubscription::onCallResponse(LSHandle *, LSMessage *msg, void *method_context)
{
	PersistentSubscription* sub = static_cast<PersistentSubscription*> (method_context);
	return JsonResponse::handleLunaResponse(msg, sub->mResultHandler);
}

} //namespace LSHelpers
