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
#include <pbnjson.hpp>
#include <luna-service2/lunaservice.hpp>

#include "serverstatus.hpp"
#include "jsonrequest.hpp"

namespace LSHelpers {

/**
 * @brief Represents a publishing point for a sender service.
 * @details Contains a list of subscribed clients and allows sending subscription updates to them.
 * All sending is performed asynchronously on luna service thread.
 *
 * Multithreading: This class is fully thread safe.
 *
 * Example:
 *
 * @code
class TestService
{
public:
	TestService(LS::Handle* handle)
			: mLunaClient{handle}
			, mSubscription{handle}
	{
		mLunaClient.registerMethod("/","subscribe", this, &TestService::subscribe);
		mSubscription.setDeduplicate(true);
	}

	pbnjson::JValue subscribe(LSHelpers::JsonRequest& request)
	{
		bool subscribe;
		request.get("subscribe", subscribe);
		request.finishParseOrThrow(true);

		if (subscribe)
		{
			mSubscription.addSubscription(request.getMessage());
		}

		return JObject{{"subscribed", true}, {"firstResponse", true}};
	}

	bool post(int data)
	{
		return mSubscription.post(JObject{{"returnValue", true},
		                                  {"subscribed", true},
		                                  {"data", data}});
	}

private:
	LSHelpers::SubscriptionPoint mSubscription;
	LSHelpers::ServicePoint mLunaClient;
};
 * @endcode
 */
class SubscriptionPoint
{

	struct SubscriptionItem
	{

		friend class SubscriptionPoint;

	private:
		SubscriptionItem(LS::Message _message, SubscriptionPoint *_parent)
				: message { std::move(_message) }
				, parent { _parent }
		{ }

	public:

		SubscriptionItem(const SubscriptionItem &) = delete;
		SubscriptionItem &operator=(const SubscriptionItem &) = delete;
		SubscriptionItem(const SubscriptionItem &&) = delete;
		SubscriptionItem &operator=(const SubscriptionItem &&) = delete;

	private:
		LS::Message message;
		SubscriptionPoint *parent;
		ServerStatus status;
	};

	friend struct SubscriptionItem;

public:
	explicit
	SubscriptionPoint(LS::Handle* service = nullptr)
			: mServiceHandle {nullptr }
			, mDeduplicate { false }
	{
		setServiceHandle(service);
	}

	/**
	 * Delete the subscription point.
	 * Sends out any pending messages before deletion.
	 */
	~SubscriptionPoint()
	{
		unsetCancelNotificationCallback();
	}

	SubscriptionPoint(const SubscriptionPoint &) = delete;
	SubscriptionPoint &operator=(const SubscriptionPoint &) = delete;
	SubscriptionPoint(SubscriptionPoint &&) = delete;
	SubscriptionPoint &operator=(SubscriptionPoint &&) = delete;

	void setDeduplicate(bool deduplicate)
	{
		mDeduplicate = deduplicate;
	}

	/**
	 * Speficy service to use for sending subscription replies.
	 * Optional - the service handle will be derived from the first subscription added, if not set.
	 * @param handle
	 */
	void setServiceHandle(LSHandle* handle)
	{
		unsetCancelNotificationCallback();
		mServiceHandle = handle;
		setCancelNotificationCallback();
	}

	/**
	 * Speficy service to use for sending subscription replies.
	 * Optional - the service handle will be derived from the first subscription added, if not set.
	 * @param handle
	 */
	void setServiceHandle(LS::Handle* handle)
	{
		setServiceHandle(handle ? handle->get() : nullptr);
	}

	/**
	 * Process subscription message. Subscribe sender of the given message.
	 * @param message subscription message to process.
	 * @return Returns true if the method succeeds in adding the sender of the message as a subscriber
	 */
	void addSubscription(const LS::Message& message);

	/**
	 * Convenience method - takes request rather than message.
	 * @param request
	 */
	inline void addSubscription(const LSHelpers::JsonRequest& request){
		addSubscription(request.getMessage());
	}

	/**
	 * Post payload to all subscribers
	 * @param payload posted data
	 * @return Returns true if replies were posted successfully
	 */
	bool post(const char *payload) noexcept;

	/**
	 * Post payload to all subscribers
	 * @param payload posted data
	 * @return Returns true if replies were posted successfully
	 */
	bool post(const pbnjson::JValue& payload) noexcept
	{
		pbnjson::JValue p = payload;
		return post(p.stringify().c_str());
	}

	/**
	 * Returns if service has subscribers
	 */
	bool hasSubscribers() const
	{
		// Not locking the mutex here, should be atomic read.
		return !mSubscriptions.empty();
	}

private:
	LSHandle *mServiceHandle;
	std::vector<std::unique_ptr<SubscriptionItem> > mSubscriptions; //Active subscriptions
	bool mDeduplicate;
	std::string mPreviousPayload;
	std::mutex mSubscriptonsMutex; // Lock to access mSubscriptions and mPreviousPayload

	void setCancelNotificationCallback()
	{
		if (mServiceHandle)
			LSCallCancelNotificationAdd(mServiceHandle, subscriberCancelCB, this, LS::Error().get());
	}

	void unsetCancelNotificationCallback()
	{
		if (mServiceHandle)
			LSCallCancelNotificationRemove(mServiceHandle, subscriberCancelCB, this, LS::Error().get());
	}

	static bool subscriberCancelCB(LSHandle *sh, const char *uniqueToken, void *context);
	void subscriberStatusCB(SubscriptionItem* item, bool isUp);
	static bool postSubscriptions(gpointer user_data);
	static bool doSubscribe(gpointer user_data);
};

} // namespace LSHelpers;
