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

#include <memory>
#include "util.hpp"

std::string string_format_valist(const std::string& fmt_str, va_list ap)
{
	size_t n = fmt_str.size() * 2;
	std::unique_ptr<char[]> formatted(new char[n]);
	va_list apCopy;
	va_copy(apCopy, ap);

	int final_n = vsnprintf(&formatted[0], n, fmt_str.c_str(), ap);
	if (final_n < 0 || final_n >= (int)n)
	{
		/* There was not enough space, retry */
		/* MS implements < 0 as not large enough */
		n = (size_t) (abs(final_n) + 1);

		formatted.reset(new char[n]);
		vsnprintf(&formatted[0], n, fmt_str.c_str(), apCopy);
	}
	va_end(apCopy);

	return std::string(formatted.get());
}
