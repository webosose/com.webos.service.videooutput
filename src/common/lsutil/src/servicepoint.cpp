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
#include <mutex>

#include "util.hpp"
#include "servicepoint.hpp"

using namespace pbnjson;

namespace LSHelpers  {


ServicePoint::ServicePoint(LS::Handle* handle)
		: mHandle(handle)
{
}

ServicePoint::~ServicePoint()
{
	for (auto& method: mMethods)
	{
		unregisterMethodImpl(*method);
	}

	//TODO: Potential raciness with call responses being dispatched in other threads.
	// There is no clean way to kill a call (it might be in the callback method).

	// Cancel all the calls in progress, so we don't get any callbacks on destroyed object.
	std::lock_guard<std::mutex> lock(mCallsMutex);
	for (auto& iter: mCalls)
	{
		LSCallCancel(mHandle->get(), iter.first, nullptr);
		// Don't care if call cancel errors out.
	}

	mCalls.clear();
}

void ServicePoint::registerMethod(const std::string& category,
                                      const std::string& methodName,
                                      const JsonRequest::Handler& handler,
                                      const JSchema& schema)
{
	if (unlikely(!mHandle))
	{
		LS::Error error;
		_LSErrorSet(error.get(), MSGID_LS_NO_HANDLE, -EINVAL, "Service handle not set");
		throw error;
	}

	// Check for duplicate registration
	for (auto& method: mMethods)
	{
		if (method->method == methodName && method->category == category)
		{
			std::stringstream error;
			error << "Duplicate registration of method " << category << "/" << methodName;
			throw std::logic_error(error.str());
		}
	}

	//Check if valid names
	if (category.length() == 0u || category[0] != '/')
	{
		LS::Error error;
		_LSErrorSet(error.get(), MSGID_LS_INVALID_CATEGORY_NAME, -EINVAL, "Category empty or does not start with /");
		throw error;
	}

	if (methodName.length() == 0u || methodName.find('/')  != std::string::npos)
	{
		LS::Error error;
		_LSErrorSet(error.get(), MSGID_LS_INVALID_METHOD_NAME, -EINVAL, "Method name empty or contains /");
		throw error;
	}

	std::unique_ptr<MethodInfo> method {new MethodInfo(this, handler, schema, category, methodName)};
	registerMethodImpl(*method);
	mMethods.emplace_back(std::move(method));
}

void ServicePoint::registerMethodImpl(ServicePoint::MethodInfo& method)
{
	LSMethod methods[2];
	methods[0]= {method.method.c_str(),
	             &ServicePoint::methodHandler,
	             LUNA_METHOD_FLAGS_NONE};
	methods[1] = {nullptr, nullptr, LUNA_METHOD_FLAGS_NONE};

	mHandle->registerCategoryAppend(method.category.c_str(), methods, nullptr);
	//Might be an issue if called from other thread than main loop, see API doc.
	mHandle->setMethodData(method.category.c_str(), method.method.c_str(), &method);
}

void ServicePoint::unregisterMethodImpl(ServicePoint::MethodInfo& method)
{
	LSMethod methods[2];
	methods[0]= {method.method.c_str(),
	             &ServicePoint::removedMethodHandler,
	             LUNA_METHOD_FLAGS_NONE};
	methods[1] = {nullptr, nullptr, LUNA_METHOD_FLAGS_NONE};

	mHandle->registerCategoryAppend(method.category.c_str(), methods, nullptr);
	//Might be an issue if called from other thread than main loop, see API doc.
	mHandle->setMethodData(method.category.c_str(), method.method.c_str(), nullptr);
}


void ServicePoint::registerSignal(const std::string& category, const std::string& methodName)
{
	if (unlikely(!mHandle))
	{
		LS::Error error;
		_LSErrorSet(error.get(), MSGID_LS_NO_HANDLE, -EINVAL, "Service handle not set");
		throw error;
	}

	//Check if valid names
	if (category.length() == 0 || category[0] != '/')
	{
		LS::Error error;
		_LSErrorSet(error.get(), MSGID_LS_INVALID_CATEGORY_NAME, -EINVAL, "Category empty or does not start with /");
		throw error;
	}

	if (methodName.length() == 0 || methodName.find('/') != std::string::npos)
	{
		LS::Error error;
		_LSErrorSet(error.get(), MSGID_LS_INVALID_METHOD_NAME, -EINVAL, "Method name empty or contains /");
		throw error;
	}

	LSSignal signals[2];
	signals[0]= {methodName.c_str(), LUNA_SIGNAL_FLAGS_NONE};
	signals[1] = {nullptr, LUNA_SIGNAL_FLAGS_NONE};

	mHandle->registerCategoryAppend(category.c_str(), nullptr, signals);
}

// ---------------------
// Section: calls
// ---------------------

LSMessageToken ServicePoint::makeCall(const std::string& uri,
                                          const pbnjson::JValue& params,
                                          bool oneReply,
                                          const JsonResponse::Handler& handler)
{
	LSMessageToken token = 0;
	LS::Error error;

	if (unlikely(!mHandle))
	{
		_LSErrorSet(error.get(), MSGID_LS_NO_HANDLE, -EINVAL, "Service handle not set");
		throw error;
	}

	if (handler)
	{
		std::unique_ptr<Call> call {new Call(this, 0, handler, oneReply)};

		LSCall(mHandle->get(), uri.c_str(),
		       JGenerator::serialize(params, JSchema::AllSchema()).c_str(),
		       &ServicePoint::callResponseHandler, call.get(), &token, error.get());

		if (error.isSet())
		{
			throw error;
		}

		call->token = token;
		// Save to be able to clean up on destructor.
		std::lock_guard<std::mutex> lock(mCallsMutex);
		mCalls[token] = std::move(call);
	}
	else // Fire and forget
	{
		if (!oneReply)
		{
			throw std::logic_error("Multi reply requires handler method");
		}

		LSCallOneReply(mHandle->get(), uri.c_str(),
		               JGenerator::serialize(params, JSchema::AllSchema()).c_str(),
		               nullptr, nullptr, &token, error.get());

		if (error.isSet())
		{
			throw error;
		}
	}

	return token;
}

void ServicePoint::cancelCall(LSMessageToken token)
{
	std::lock_guard<std::mutex> lock(mCallsMutex);
	auto iter = mCalls.find(token);

	if (iter == mCalls.end())
	{
		// Might be a call with no callback, forward the cancel to luna service.
		LSCallCancel(mHandle->get(), token, nullptr);
	}
	else
	{
		auto& call = iter->second;
		LSCallCancel(mHandle->get(), call->token, nullptr);
		mCalls.erase(call->token);
	}
}

void ServicePoint::cancelCall(ServicePoint::Call* call)
{
	LSCallCancel(mHandle->get(), call->token, nullptr);
	std::lock_guard<std::mutex> lock(mCallsMutex);
	mCalls.erase(call->token);
}

// ---------------------------
// Section signals:
// ---------------------------

void ServicePoint::sendSignal(const std::string& category, const std::string& method, const pbnjson::JValue& payload)
{
	LS::Error error;
	if (unlikely(!mHandle))
	{
		_LSErrorSet(error.get(), MSGID_LS_NO_HANDLE, -EINVAL, "Service handle not set");
		throw error;
	}

	if (category.length() == 0 || category[0] != '/')
	{
		_LSErrorSet(error.get(), MSGID_LS_INVALID_CATEGORY_NAME, -EINVAL, "Category empty or does not start with /");
		throw error;
	}

	if (method.length() == 0 || method.find('/') != std::string::npos)
	{
		_LSErrorSet(error.get(), MSGID_LS_INVALID_METHOD_NAME, -EINVAL, "Method name empty or contains /");
		throw error;
	}

	// The SignalSend cares only about category and method.
	std::string uri = "luna://com.bogusuri" + category + "/" + method;
	//JValue is reference counted, this is a shallow copy to remove const.
	JValue payloadLocal = payload;

	if (!LSSignalSend(mHandle->get(), uri.c_str(), payloadLocal.stringify().c_str(), error.get()))
	{
		throw error;
	}
}

//----------------------------
// Section: callback handler methods.
//----------------------------

bool ServicePoint::methodHandler(LSHandle *, LSMessage *msg, void *method_context)
{
	MethodInfo* method = static_cast<MethodInfo*>(method_context);

	if (unlikely(!method))
	{
		//Should never happen
		LOG_ERROR(MSGID_LS_NO_HANDLER, 0, "No handler for method %s %s", LSMessageGetCategory(msg), LSMessageGetMethod(msg));
		return false;
	}

	return JsonRequest::handleLunaCall(msg, method->handler, method->schema);
}

/**
 * Send canned response that the method handler is removed.
 * @param msg
 * @return
 */
bool ServicePoint::removedMethodHandler(LSHandle *, LSMessage *msg, void *)
{
	LS::Message message{msg};

	try
	{
		message.respond(API_ERROR_REMOVED.stringify().c_str());
		return true;
	}
	catch (const std::exception& e)
	{
		LOG_ERROR(MSGID_LS_UNEXPECTED_EXCEPTION, 0, "Method '%s' handler throws exception: %s",
		             message.getMethod(),
		             e.what());
		return false;
	}
	catch (...)
	{
		LOG_ERROR(MSGID_LS_UNEXPECTED_EXCEPTION, 0, "Method '%s' handler throws exception",
		             message.getMethod());
		return false;
	}
}

bool ServicePoint::callResponseHandler(LSHandle* sh, LSMessage* msg, void* ctx)
{
	ServicePoint::Call* call = static_cast<ServicePoint::Call* >(ctx);

	// Clean up before the handler method. Handler may delete the client and we will not be able to
	// do it afterwards.
	auto handler = call->handler;
	if (call->oneReply)
	{
		// Invalidates call object!!!
		call->service->cancelCall(call);
	}

	return JsonResponse::handleLunaResponse(msg, handler, JSchema::AllSchema());
}

} // Namespace LSHelpers
