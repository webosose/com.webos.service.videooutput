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

#include "jsonparser.hpp"
#include "subscriptionpoint.hpp"
#include "util.hpp"

namespace LSHelpers {

struct PostData
{
	std::string payload;
	std::vector<LS::Message> messages;
};

void SubscriptionPoint::addSubscription(const LS::Message& message)
{
	LSHandle* messageHandle = LSMessageGetConnection(((LS::Message&)message).get());

	if (unlikely(!mServiceHandle))
	{
		setServiceHandle(messageHandle);
	}

	std::unique_ptr<SubscriptionItem> item { new SubscriptionItem(message, this) };

	item->status.set(mServiceHandle,
	                 message.getSender(),
	                 std::bind(&SubscriptionPoint::subscriberStatusCB, this, item.get(), std::placeholders::_2));

	{
		std::lock_guard<std::mutex> lock(mSubscriptonsMutex);
		mSubscriptions.push_back(std::move(item));
	}
}

// Note that this may be run after the subscription is deleted.
bool SubscriptionPoint::postSubscriptions(gpointer user_data)
{
	PostData *data = static_cast<PostData*>(user_data);
	try
	{
		for (auto &message: data->messages)
		{
			message.respond(data->payload.c_str());
		}
	}
	catch(LS::Error &e)
	{
		e.log(PmLogGetLibContext(), "LS_SUBS_POST_FAIL");
	}
	catch(...)
	{
	}

	// remove source from loop
	return G_SOURCE_REMOVE;
}

// Subscription responses are sent from within the same thread that Luna
// uses itself to avoid synchronization between other callbacks (like cancel).
// To avoid race between subscription point, a copy of messages to be responded
// is made and passed into the timeout callback. This also ensures a correct
// snapshot of subscriptions is addressed in case of concurrently added
// subscriptions.
bool SubscriptionPoint::post(const char *payload) noexcept
{
	if (!mServiceHandle)
		return false;

	LS::Error error;
	GMainContext *context = LSGmainGetContext(mServiceHandle, error.get());
	if (!context)
	{
		error.log(PmLogGetLibContext(), "LS_SUBS_POST_FAIL");
		return false;
	}

	std::vector<LS::Message> activeMessages;
	{
		std::lock_guard<std::mutex> lock(mSubscriptonsMutex);

		if (mDeduplicate)
		{
			if (payload == mPreviousPayload)
			{
				return true;
			}
			mPreviousPayload = payload;
		}

		for (auto& item : mSubscriptions)
		{
			activeMessages.push_back(item->message);
		}
	}

	std::unique_ptr<PostData> data(new PostData());
	data->payload = payload;
	data->messages = std::move(activeMessages);

	GSource* source = g_timeout_source_new(0);
	g_source_set_callback(source, (GSourceFunc)postSubscriptions, data.release(),
	                      [](gpointer data)
	                      {
		                      delete static_cast<PostData*>(data);
	                      });

	g_source_attach(source, context);
	g_source_unref(source);

	return true;
}

bool SubscriptionPoint::subscriberCancelCB(LSHandle *sh, const char *uniqueToken, void *context)
{
	SubscriptionPoint *self = static_cast<SubscriptionPoint *>(context);
	std::lock_guard<std::mutex> lock(self->mSubscriptonsMutex);

	auto it = std::find_if(self->mSubscriptions.begin(), self->mSubscriptions.end(),
	                       [uniqueToken](std::unique_ptr<SubscriptionItem>& _item)
	                       {
		                       return !strcmp(uniqueToken, _item->message.getUniqueToken());
	                       }
	);

	if (it != self->mSubscriptions.end())
	{
		self->mSubscriptions.erase(it);
	}

	return true;
}

void SubscriptionPoint::subscriberStatusCB(SubscriptionPoint::SubscriptionItem* item, bool isUp)
{
	if (isUp)
		return;

	std::lock_guard<std::mutex> lock(mSubscriptonsMutex);

	auto it = std::find_if(mSubscriptions.begin(), mSubscriptions.end(),
	                       [item](std::unique_ptr<SubscriptionItem> & _item)
	                       {
		                       return (_item.get() == item);
	                       }
	);

	if (it != mSubscriptions.end())
	{
		mSubscriptions.erase(it);
	}
}

} //namespace LSHelpers
