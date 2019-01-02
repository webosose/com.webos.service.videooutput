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

#pragma once

#include <ls2-helpers/ls2-helpers.hpp>

// General service errors
#define API_ERROR_UNKNOWN LSHelpers::ErrorResponse(1, "Unknown error")
#define API_ERROR_SCHEMA_VALIDATION(...) LSHelpers::ErrorResponse(3, __VA_ARGS__)
#define API_ERROR_INVALID_PARAMETERS(...) LSHelpers::ErrorResponse(4, __VA_ARGS__)
#define API_ERROR_INVALID_STATUS(...) LSHelpers::ErrorResponse(5, __VA_ARGS__)
#define API_ERROR_NOT_IMPLEMENTED LSHelpers::ErrorResponse(10, "Not implemented")

// HAL errors
#define API_ERROR_HAL_ERROR LSHelpers::ErrorResponse(20, "Driver error while executing the command")

// Video errors
#define API_ERROR_VIDEO_NOT_CONNECTED LSHelpers::ErrorResponse(100, "Video not connected")
#define API_ERROR_DOWNSCALE_LIMIT(...) LSHelpers::ErrorResponse(102, __VA_ARGS__)
#define API_ERROR_UPSCALE_LIMIT(...) LSHelpers::ErrorResponse(103, __VA_ARGS__)

class FatalException : public std::exception
{
public:
    FatalException(const char *file, int line, const char *format, ...);
    const char *what() const noexcept override;

private:
    std::string mMessage;
};

#define THROW_FATAL_EXCEPTION(...) throw FatalException(__FILE__, __LINE__, __VA_ARGS__)
