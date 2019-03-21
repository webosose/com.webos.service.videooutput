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

#include <iostream>
#include <functional>
#include <memory>

#include "servicepoint.hpp"

namespace LSHelpers {

typedef std::function<void(const char*, bool)> ServerStatusCallback;

/**
 * @brief Notifies when luna becomes available / does down.
 * Also see PersistentSubscription if you want to subscribe to a call when service is up.
 *
 * Multithreading: This class is **not** thread safe. The status update callback is called in luna handle context.
 *
 * Example:
 * @code
	class MyClass
	{
	public:
	  MyClass(LS::Handle* handle)
	  {
		mStatus.set(handle, "com.webos.service.stuff", this, &MyClass::serviceStatus);
	  }
	private:
	  void serviceStatus(const char* name, bool isUp)
	  {
		if (isUp)
		{
			std::cout << "service " << name << " is up!";
		}
	  }

	  ServerStatus mStatus;
	};
	 * @endcode
	 *
	 */
class ServerStatus
{
public:
	ServerStatus() : mHandle(nullptr), mCookie(nullptr) { }

	ServerStatus(const ServerStatus &) = delete;
	ServerStatus& operator=(const ServerStatus &) = delete;

	ServerStatus(ServerStatus &&other) noexcept
			: mHandle(other.mHandle)
			, mCookie(other.mCookie)
			, mCallback(std::move(other.mCallback))
	{
		other.mCookie = nullptr;
		other.mHandle = nullptr;
	}

	ServerStatus &operator=(ServerStatus &&other)
	{
		cancel();
		mHandle = other.mHandle;
		mCookie = other.mCookie;
		mCallback = std::move(other.mCallback);

		other.mCookie = nullptr;
		other.mHandle = nullptr;

		return *this;
	}

	~ServerStatus()
	{
		if (mCookie)
		{
			LS::Error error;
			if (!LSCancelServerStatus(mHandle, mCookie, error.get()))
				error.logError("LS_FAILED_TO_UNREG_SRV_STAT");
		}
	}

	/**
	 * @brief Register a callback to be called when the server goes down or comes up.
	 * Callback may be called in this context if
	 * the server is already up.
	 *
	 * @param handle service handle.
	 * @param service_name service name
	 * @param callback callback function
	 */
	void set(LS::Handle *handle, const char *service_name, const ServerStatusCallback &callback)
	{
		set(handle->get(), service_name, callback);
	}

	/**
	 * @brief Register a callback to be called when the server goes down or comes up.
	 * Callback may be called this context if the server is already up, otherwise will be called
	 * in service handle context.
	 *
	 * @param servicePoint service point object.
	 * @param service_name service name
	 * @param callback callback function
	 */
	void set(LSHelpers::ServicePoint* servicePoint, const std::string& service_name, const ServerStatusCallback &callback)
	{
		set(servicePoint->getHandle(), service_name.c_str(), callback);
	}

	/**
	 * @brief Register a callback to be called when the server goes down or comes up.
	 * Callback may be called in this context if
	 * the server is already up.
	 *
	 * @param handle service handle.
	 * @param service_name service name
	 * @param callback callback function
	 */
	void set(LS::Handle *handle, const std::string& service_name, const ServerStatusCallback &callback)
	{
		set(handle, service_name.c_str(), callback);
	}

	/**
	 * @brief Register a callback to be called when the server goes down or comes up.
	 * Callback may be called in this context if
	 * the server is already up.
	 *
	 * @param handle service handle.
	 * @param service_name service name
	 * @param callback callback function
	 */
	void set(LSHandle *handle, const char *service_name, const ServerStatusCallback &callback)
	{
		cancel();

		mHandle = handle;
		mCallback.reset( new ServerStatusCallback(callback));

		LS::Error error;
		if (!LSRegisterServerStatusEx(mHandle, service_name, ServerStatus::callbackFunc,
		                              mCallback.get(), &mCookie, error.get()))
		{
			throw error;
		}
	}


	/**
	 * @brief Cancel server status monitoring. Frees the associated callback function.
	 */
	void cancel()
	{
		if (mCookie)
		{
			LS::Error error;
			if (!LSCancelServerStatus(mHandle, mCookie, error.get()))
				throw error;

			mCookie = nullptr;
			mCallback.reset(nullptr);
		}
	}

	explicit operator bool() const { return mCookie; }

private:
	LSHandle *mHandle;
	void *mCookie;
	std::unique_ptr<ServerStatusCallback> mCallback;// Referenced from callbackFunc.

private:
	static bool callbackFunc(LSHandle *, const char *serviceName, bool connected, void *ctx)
	{
		ServerStatusCallback callback = *(static_cast<ServerStatusCallback *>(ctx));

		callback(serviceName, connected);
		return true;
	}

	friend std::ostream &operator<<(std::ostream &os, const ServerStatus &status)
	{ return os << "LUNA SERVER STATUS [" << status.mCookie << "]"; }
};

} //namespace LSHelpers;
