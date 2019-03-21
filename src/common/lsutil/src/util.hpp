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
#include <luna-service2/lunaservice.h>
#include <PmLogLib.h>

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

/** use these for key-value pair printing */
#define LOG_LS_TRACE(...)                    PMLOG_TRACE(__VA_ARGS__)
#define LOG_LS_DEBUG(...)                    (void) PmLogDebug(PmLogGetLibContext(), ##__VA_ARGS__)
#define LOG_LS_INFO(msgid, kvcount, ...)     (void) PmLogInfo(PmLogGetLibContext(), msgid, kvcount, ##__VA_ARGS__)
#define LOG_LS_WARNING(msgid, kvcount, ...)  (void) PmLogWarning(PmLogGetLibContext(), msgid, kvcount, ##__VA_ARGS__)
#define LOG_ERROR(msgid, kvcount, ...)    (void) PmLogError(PmLogGetLibContext(), msgid, kvcount, ##__VA_ARGS__)
#define LOG_LS_CRITICAL(msgid, kvcount, ...) (void) PmLogCritical(PmLogGetLibContext(), msgid, kvcount, ##__VA_ARGS__)

std::string string_format_valist(const std::string& fmt_str, va_list ap);

//Logger errors
#define MSGID_LS_JSON_PARSE_ERROR             "LS_JSON_PARSE_ERROR"  /* Parse error encountered. */
#define MSGID_LS_INVALID_RESPONSE             "LS_INVALID_RESPONSE"  /* Response is not valid Json*/
#define MSGID_LS_NO_HANDLE                    "LS_NO_HANDLE"  /* No service handle. */
#define MSGID_LS_NO_HANDLER                   "LS_NO_HANDLER"  /* No handler for method (internal error)*/
#define MSGID_LS_INVALID_URI                  "LS_INVALID_URI"  /* Uri not valid */
#define MSGID_LS_INVALID_JVALUE               "LS_INVALID_JVALUE"  /* JValue not valid */
#define MSGID_LS_CALL_JSON_PARSE_FAILED       "LS_CALL_PARSE_FAILED"  /* Failed to parse call payload to json*/
#define MSGID_LS_UNEXPECTED_EXCEPTION         "LS_UNEXPECTED_EXCEPTION"  /* Application throws exception while processing method call*/
#define MSGID_LS_DOUBLE_DEFER                 "LS_DOUBLE_DEFER"  /* A request was deferred twice*/
#define MSGID_LS_CALL_RESPONSE_INVALID_HANDLE "LS_CALL_RESPONSE_INVALID_HANDLE"  /* Invalid handle passed to call response handler*/
#define MSGID_LS_HUB_ERROR                    "LS_HUB_ERROR"  /* Hub error in response. */
#define MSGID_LS_RESPONSE_JSON_PARSE_FAILED   "LS_RESPONSE_JSON_PARSE_FAILED"  /* Json parse of the response failede. */
#define MSGID_LS_RESPONSE_PARAMETERS_ERROR    "LS_RESPONSE_PARAMETERS_ERRO"  /* JsonRespose.get call failed. */
#define MSGID_LS_INVALID_CATEGORY_NAME        "LS_INVALID_CATEGORY_NAME"  /* Category name not valid. */
#define MSGID_LS_INVALID_METHOD_NAME          "LS_INVALID_METHOD_NAME"  /* Method name not valid. */

// API error responses.
#define API_ERROR_UNKNOWN                    ErrorResponse(1, "Unknown error")
#define API_ERROR_MALFORMED_JSON             ErrorResponse(2, "Malformed JSON")
#define API_ERROR_SCHEMA_VALIDATION(...)     ErrorResponse(3, __VA_ARGS__)
#define API_ERROR_NO_RESPONSE                ErrorResponse(4, "The service did not send a reply")
#define API_ERROR_REMOVED                    ErrorResponse(5, "Method is removed")

// Copy of LSError utility functions from luna-service2. Used to generate LS::Errors.

#define LS__FILE__BASENAME (strrchr("/" __FILE__, '/') + 1)

// TODO: referencing ls2 private methods.
extern "C"
bool _LSErrorSetFunc(LSError *lserror,
                     const char *file, int line, const char *function,
                     int error_code, const char *error_message, ...);

#define _LSErrorSetNoPrint(lserror, error_code, ...)              \
do {                                                              \
    _LSErrorSetFunc(lserror, LS__FILE__BASENAME, __LINE__, __FUNCTION__, \
             error_code,                                          \
             __VA_ARGS__);                                        \
} while (0)

#define _LSErrorSetNoPrintLiteral(lserror, error_code, error_message)   \
do {                                                                    \
    _LSErrorSetFunc(lserror, LS__FILE__BASENAME, __LINE__, __FUNCTION__,\
                    error_code, error_message);                         \
} while (0)

/**
 *******************************************************************************
 * @brief Used to set an error with a printf-style error message.
 *
 * @param  lserror      OUT error
 * @param  message_id   IN  message identifier
 * @param  error_code   IN  error code
 * @param  ...          IN  printf-style format
 *******************************************************************************
 */
#define _LSErrorSet(lserror, message_id, error_code, ...) \
do {                                                      \
    LOG_ERROR(message_id, 2,                           \
                 PMLOGKS("FILE", LS__FILE__BASENAME),     \
                 PMLOGKFV("LINE", "%d", __LINE__),        \
                 __VA_ARGS__);                            \
    _LSErrorSetNoPrint(lserror, error_code, __VA_ARGS__); \
} while (0)

/**
 *******************************************************************************
 * @brief Use this instead of _LSErrorSet when the error_message is not a
 * printf-style string (error_message could contain printf() escape
 * sequences)
 *
 * @param  lserror          OUT error
 * @param  message_id       IN  message identifier
 * @param  error_code       IN  code
 * @param  error_message    IN  error_message
 *******************************************************************************
 */
#define _LSErrorSetLiteral(lserror, message_id, error_code, error_message) \
do {                                                                       \
    LOG_ERROR(message_id, 3,                                            \
                 PMLOGKS("ERROR", error_message),                          \
                 PMLOGKS("FILE", LS__FILE__BASENAME),                      \
                 PMLOGKFV("LINE", "%d", __LINE__));                        \
    _LSErrorSetNoPrintLiteral(lserror, error_code, error_message);         \
} while (0)
