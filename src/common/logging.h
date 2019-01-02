// Copyright (c) 2016-2018 LG Electronics, Inc.
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

#ifndef LOGGING_H
#define LOGGING_H

#include <PmLogLib.h>

extern PmLogContext logContext;

#define LOG_CRITICAL(msgid, kvcount, ...) PmLogCritical(logContext, msgid, kvcount, ##__VA_ARGS__)

#define LOG_ERROR(msgid, kvcount, ...) PmLogError(logContext, msgid, kvcount, ##__VA_ARGS__)

#define LOG_WARNING(msgid, kvcount, ...) PmLogWarning(logContext, msgid, kvcount, ##__VA_ARGS__)

#define LOG_INFO(msgid, kvcount, ...) PmLogInfo(logContext, msgid, kvcount, ##__VA_ARGS__)

#define LOG_DEBUG(fmt, ...) PmLogDebug(logContext, "%s:%s() " fmt, __FILE__, __FUNCTION__, ##__VA_ARGS__)

#define LOG_ESCAPED_ERRMSG(msgid, errmsg)                           \
    do {                                                            \
        gchar *escaped_errtext = g_strescape(errmsg, NULL);         \
        LOG_ERROR(msgid, 1, PMLOGKS("Error", escaped_errtext), ""); \
        g_free(escaped_errtext);                                    \
    } while (0)

#define FINISH_PARSE_OR_RETURN_FALSE(parser)                                                           \
    if (!(parser).finishParse())                                                                       \
    LOG_WARNING(MSGID_JSON_PARSE_ERROR, 0, "Failed to parse JSON: file %s:%d, %s", __FILE__, __LINE__, \
                (parser).getError().c_str())

#define MSGID_LUNA_SEND_FAILED "LUNA_SEND_FAILED"
#define MSGID_LUNA_CREATE_JSON_FAILED "LUNA_CREATE_JSON_FAILED"
#define MSGID_UNEXPECTED_EXCEPTION "UNEXPECTED_EXCEPTION"
#define MSGID_LS2_DISCONNECTED "LS2_DISCONNECTED"
#define MSGID_LS2_NO_HANDLER "LS2_NO_HANDLER"
#define MSGID_LS2_NOT_SUBSCRIBED "LS2_NOT_SUBSCRIBED"
#define MSGID_LS2_SUBSCRIBE_FAILED "LS2_SUBSCRIBE_FAILED"
#define MSGID_LS2_CALL_PARSE_FAILED "LS2_CALL_PARSE_FAILED"
#define MSGID_LS2_INVALID_SUBSCRIPTION_RESPONSE "LS2_INVALID_SUBSCRIPTION_RESPONSE"
#define MSGID_LS2_INVALID_RESPONSE "LS2_INVALID_RESPONSE"
#define MSGID_LS2_DOUBLE_DEFER "LS2_DOUBLE_DEFER"
#define MSGID_LS2_CALL_RESPONSE_INVALID_HANDLE "LS2_CALL_RESPONSE_INVALID_HANDLE"
#define MSGID_LS2_HUB_ERROR "LS2_HUB_ERROR"
#define MSGID_LS2_RESPONSE_PARSE_FAILED "LS2_RESPONSE_PARSE_FAILED"
#define MSGID_LS2_FAILED_TO_PARSE_PARAMETERS "LS2_FAILED_TO_PARSE_PARAMETERS"
#define MSGID_LS2_REGISTERSERVERSTATUS_FAILED "LS2_REGISTERSERVERSTATUS_FAILED"

#define MSGID_MALFORMED_JSON "MALFORMED_JSON"
#define MSGID_GET_SYSTEM_SETTINGS_ERROR "GET_SYSTEM_SETTINGS_ERROR"
#define MSGID_SCHEMA_VALIDATION "SCHEMA_VALIDATION"
#define MSGID_MULTIPLE_LUNA_REPLIES "MULTIPLE_LUNA_REPLIES"

#define MSGID_HAL_INIT_ERROR "HAL_INIT_ERROR"
#define MSGID_HAL_DEINIT_ERROR "HAL_DEINIT_ERROR"
#define MSGID_TERMINATING "TERMINATING"
#define MSGID_SIGNAL_HANDLER_ERROR "SIGNAL_HANDLER_ERROR"
#define MSGID_UNKNOWN_SOURCE_NAME "UNKNOWN_SOURCE_NAME"

#define MSGID_HAL_ERROR "HAL_ERROR"
#define MSGID_JSON_PARSE_ERROR "JSON_PARSE_ERROR"
#define MSGID_INVALID_PARAMETERS_ERR "INVALID_PARAMETERS"
#define MSGID_SINK_SETUP_ERROR "SINK_SETUP_ERROR"
#define MSGID_DISPLAY_NOT_CONNECTED "MSGID_DISPLAY_NOT_CONNECTED"

#endif // LOGGING_H
