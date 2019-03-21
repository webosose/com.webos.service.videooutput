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

#include <cmath>
#include "jsonparser.hpp"
#include "util.hpp"

using namespace pbnjson;

namespace LSHelpers {

JsonParseError::JsonParseError(const std::string& format, ...)
{
	va_list args;
	va_start(args, format);
	this->message = string_format_valist(format, args);
	va_end(args);
}

void checkConversionResultOrThrow(ConversionResultFlags result)
{
	if (result != CONV_OK)
	{
		const char* message;

		if (CONV_HAS_OVERFLOW(result))
		{
			message = "Integer value out of bounds";
		} else if (CONV_HAS_NOT_A_NUM(result))
		{
			message = "Integer value not a number";
		} else if (CONV_HAS_PRECISION_LOSS(result))
		{
			message = "Integer requested, but fractional value provided";
		} else
		{
			message = "parse failed";
		}

		throw JsonParseError(message);
	}
}

void JsonParser::recordError(const char* fieldName, const char* message)
{
	std::stringstream s;
	s << "Failed to validate against schema: Field '" << fieldName << "' " << message;
	LOG_LS_WARNING(MSGID_LS_JSON_PARSE_ERROR, 0, "%s", s.str().c_str());

	if (_parseError.empty())
	{
		_parseError = s.str();
	}
}

/**
 * Converts jvalue to number. Tries to parse string values to numbers as well.
 * @param value
 * @param destination
 * @return
 */
template<typename T>
static ConversionResultFlags asNumber(const JValue& value, T& destination)
{
	ConversionResultFlags f;

	if (value.isNumber())
	{
		f = value.asNumber(destination);
	}
	else if (value.isString()) //Try to parse the string as number
	{
		f = JValue(NumericString(value.asString())).asNumber(destination);
	}
	else
	{
		throw JsonParseError("not a number");
	}

	return f;
}

/* parseValue specializations*/

template<>
void JsonParser::parseValue<uint8_t>(const JValue& value,
                         uint8_t& destination)
{
	int32_t val = 0;
	ConversionResultFlags f = asNumber(value, val);
	if (val < 0)
	{
		f |= CONV_NEGATIVE_OVERFLOW;
	}
	else if (val > UINT8_MAX)
	{
		f |= CONV_POSITIVE_OVERFLOW;
	}

	checkConversionResultOrThrow(f);
	destination = static_cast<uint8_t>(val);
}

template<>
void JsonParser::parseValue<int8_t>(const JValue& value,
                         int8_t& destination)
{
	int32_t val = 0;
	ConversionResultFlags f = asNumber(value, val);
	if (val < INT8_MIN)
	{
		f |= CONV_NEGATIVE_OVERFLOW;
	} else if (val > INT8_MAX)
	{
		f |= CONV_POSITIVE_OVERFLOW;
	}

	checkConversionResultOrThrow(f);
	destination = static_cast<int8_t>(val);
}


template<>
void JsonParser::parseValue<uint16_t>(const JValue& value,
                          uint16_t& destination)
{
	int32_t val = 0;
	ConversionResultFlags f = asNumber(value, val);
	if (val < 0)
	{
		f |= CONV_NEGATIVE_OVERFLOW;
	} else if (val > UINT16_MAX)
	{
		f |= CONV_POSITIVE_OVERFLOW;
	}

	checkConversionResultOrThrow(f);
	destination = static_cast<uint16_t>(val);
}

template<>
void JsonParser::parseValue<int16_t>(const JValue& value,
                          int16_t& destination)
{
	int32_t val = 0;
	ConversionResultFlags f = asNumber(value, val);
	if (val < INT16_MIN)
	{
		f |= CONV_NEGATIVE_OVERFLOW;
	} else if (val > INT16_MAX)
	{
		f |= CONV_POSITIVE_OVERFLOW;
	}

	checkConversionResultOrThrow(f);
	destination = static_cast<int16_t>(val);
}


template<>
void JsonParser::parseValue<int32_t>(const JValue& value,
                         int32_t& destination)
{
	ConversionResultFlags f = asNumber(value, destination);
	checkConversionResultOrThrow(f);
}

template<>
void JsonParser::parseValue<uint32_t>(const JValue& value,
                          uint32_t& destination)
{
	int64_t val = 0;
	ConversionResultFlags f = asNumber(value, val);
	if (val < 0)
	{
		f |= CONV_NEGATIVE_OVERFLOW;
	}
	else if (val > UINT32_MAX)
	{
		f |= CONV_POSITIVE_OVERFLOW;
	}

	checkConversionResultOrThrow(f);
	destination = static_cast<uint32_t>(val);
}

template<>
void JsonParser::parseValue<int64_t>(const JValue& value,
                         int64_t& destination)
{
	ConversionResultFlags f = asNumber(value, destination);
	checkConversionResultOrThrow(f);
}

template<>
void JsonParser::parseValue<uint64_t>(const JValue& value,
                          uint64_t& destination)
{
	double val = 0;
	double intVal = 0;
	ConversionResultFlags f = asNumber(value, val);
	if (val < 0)
	{
		f |= CONV_NEGATIVE_OVERFLOW;
	}
	else if (val > UINT64_MAX)
	{
		f |= CONV_POSITIVE_OVERFLOW;
	}
	else if (std::modf(val,&intVal) != 0)
	{
		f |= CONV_PRECISION_LOSS;
	}

	checkConversionResultOrThrow(f);
	destination = static_cast<uint64_t>(intVal);
}

template<>
void JsonParser::parseValue<double>(const JValue& value,
                        double& destination)
{
	ConversionResultFlags f = asNumber(value, destination);
	// Ignore precision loss - may contain more fraction digits than double can hold.
	f &= ~static_cast<int>(CONV_PRECISION_LOSS);
	checkConversionResultOrThrow(f);
}

template<>
void JsonParser::parseValue<bool>(const JValue& value, bool& destination)
{
	if (!value.isBoolean())
	{
		throw JsonParseError("not a boolean");
	}

	ConversionResultFlags error = value.asBool(destination);
	checkConversionResultOrThrow(error);
}

template<>
void JsonParser::parseValue<std::string>(const JValue& value,
                             std::string& destination)
{
	if (!value.isString())
	{
		throw JsonParseError("not a string");
	}

	ConversionResultFlags f = value.asString(destination);
	checkConversionResultOrThrow(f);
}

template<>
void JsonParser::parseValue<JsonDataObject>(const JValue& value,
                                JsonDataObject& destination)
{
	if (!value.isObject())
	{
		throw JsonParseError("not an object");
	}

	destination.parseFromJson(value);
}

template<>
void JsonParser::parseValue<JValue>(const JValue& value,
                        JValue& destination)
{
	destination = value;
}

JsonParser JsonParser::getObject(const char* name)
{
	pbnjson::JValue obj;
	get(name, obj);

	if (!obj.isObject())
	{
		recordError(name, "object expected but got something else");
		return JsonParser(pbnjson::JValue());
	}

	return JsonParser(obj);
}

bool JsonParser::finishParse(bool strict)
{
	if (strict && this->_numberOfFields != _jsonValue.objectSize())
	{
		recordError("", "unexpected fields in strict mode");
	}

	return !hasError();
}

void JsonParser::finishParseOrThrow(bool strict)
{
	finishParse(strict);

	if (hasError())
	{
		throw JsonParseError(_parseError);
	}
}

} // Namespace LSHelpers
