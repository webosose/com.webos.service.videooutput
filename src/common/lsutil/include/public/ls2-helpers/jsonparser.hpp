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

#include <exception>
#include <functional>
#include <string>
#include <sstream>
#include <unordered_map>
#include <pbnjson.hpp>

namespace LSHelpers {

template<typename T> class JsonParseContext;

/**
 * @brief Base class for parse errors that can be thrown in JsonDataObject::parseFromJson implementation.
 * @see @link LSHelpers::JsonDataObject @endlink
 *
 */
class JsonParseError : public std::exception
{
public:
	explicit JsonParseError(const std::string& format, ...);

	const char* what() const noexcept
	{ return message.c_str(); }

public:
	std::string message;
};

/**
 * @brief Interface class for custom objects that can be parsed from JSON using JsonParser.

 * Example:
 * @code
 *class MyRect: public JsonDataObject {
 *public:
 *   int x;
 *   int y;
 *   int width;
 *   int height;
 *
 *   void parseFromJson(const pbnjson::JValue& value) override {
 *		JsonParser p { value };
 *		p.get("x", x);
 *		p.get("y", y);
 *		p.get("width", width);
 *		p.get("height", height);
 *  	p.finishParseOrThrow();
 *
 *  	if (width * height > 10000000)
 *  	{
 *  		throw JsonParseError("rect are too large, limit 10 million");
 *  	}
*    }
 * }
 *
 * pbnjson::JValue someMethod(LSUtils::LunaRequest& request)
 * {
 *   int id;
 *   MyRect rect;
 *   request.get("id", id);
 *   request.get("rect", rect);
 *   request.finishParseOrThrow();
 *
 * }
 * @endcode
 */
class JsonDataObject
{
public:
	/**
	 * Method to parse from JSON. Implementations should parse the value and store the fields class data members.
	 * @param value json value to parse.
	 * @throw JsonParseError on error, to send custom error message.
	 *   A prefix description will be added to your message to indicate the location within the overall data structure.
	 */
	virtual void parseFromJson(const pbnjson::JValue& value) = 0;
};

/**
 * @brief Helper class to validate JSON against schema and parse to C++ objects.
 * With the aim to provide a clean and concise syntax.
 *
 * Example:
 * @code
 *	std::string contextName;
 *	AudioTypeEnum audioType;
 *	uint8_t portNumber;
 *	std::vector<AVResource> resourceList; // AVResource implements JsonDataObject
 *
 *  JsonParser parser(jvalueObject);
 *	parser.get("context", contextName);
 *	parser.get("portNumber", portNumber).optional(true).defaultValue(255);
 *	parser.getArray("resourceList", resourceList);
 *	parser.getAndMap<std::string, AudioTypeEnum>("audioType", audioType, {
 *		{"Analog", AUTIODYPE_ANALOG},
 *		{"Digital", AUTIODYPE_DIGITAL}});
 *
 *	if (!parser.finishParse())
 *	{
 *		std::cerr << parser.getError();
 *	}
 *	@endcode
 */
class JsonParser
{
public:
	/**
	 * Initalize parser with specified json data.
	 */
	explicit JsonParser(pbnjson::JValue json)
			: _jsonValue(json)
			, _numberOfFields(0)
		{
			if (!_jsonValue.isValid())
			{
				_parseError = "Malformed JSON.";
			}
		}

	/**
	 * Initalize parser with specified json data.
	*/
	explicit JsonParser(const char* json)
			: _jsonValue()
			, _numberOfFields(0)
	{
		if (json != nullptr)
		{
			_jsonValue = pbnjson::JDomParser::fromString(json, pbnjson::JSchema::AllSchema());
		}

		if (!_jsonValue.isValid())
		{
			_parseError = "Malformed JSON.";
		}
	}

	/**
	 * Initalize parser with specified json data.
	*/
	explicit JsonParser(const std::string& json)
			: JsonParser(json.c_str())
	{}

	/**
	 * Returns the json data.
	 */
	inline const pbnjson::JValue getJson() const
	{
		return _jsonValue;
	}

	/**
	 * Returns if an error has occurred during parsing.
	 * Normally it suffices to call finishParse at the end of parsing.
	 * @see finishParse
	 * @see finishParseOrThrow
	 */
	inline bool hasError() const
	{
		return !_parseError.empty();
	}

	/**
	 * Returns current error.
	 * Returns empty string if no error.
	 * JsonParser stores only the first error encountered.
	 */
	inline std::string getError() const
	{
		return _parseError;
	}

	/**
	 * Clears the error, allowing to continue parsing even if error occurred.
	 */
	inline void clearError()
	{
		_parseError.clear();
	}

	/** Look up a json field named *name* and store it's value in the *destination*.
	 * This is polymorphic method and the parsing implementation depends on the
	 * type of the destination field.
	 * The following checks are performed and appropriate error messages are sent:
	 * - field does not exist (overrideable with JsonParseContext::optional()
	 * - field type does not correspond to the destination type. (But see below)
	 * - integral field value does not fit in destination type (overflow, underflow).

	 * Additionally if a string is encountered but number expected, an the string is interpreted as a number instead.
	 *
	 * @param name field name
	 * @param destination reference to variable to store the value in
	 * @param parserFunc optional custom function for parsing the field.
	 * @return JsonParseContext for adding more constraints using call chaining.
	 *         The return value shall not be assigned to a variable. It depends on destructor being called to finalize checks.
	 * */
	template<typename T>
	JsonParseContext<T> get(const char* name, T& destination, const std::function<void(const pbnjson::JValue&, T&)>& parserFunc = &parseValueOrDataObject<T>);

	/** Look up a json string field named *name* and reinterpret it as a JSON payload.
	 *  Then apply the regular get logic.
	 *
	 * @param name field name
	 * @param destination reference to variable to store the value in
	 * @param parserFunc optional custom function for parsing the field.
	 * @return JsonParseContext for adding more constraints using call chaining.
	 *         The return value shall not be assigned to a variable. It depends on destructor being called to finalize checks.
	 * */
	template<typename T>
	JsonParseContext<T> getFromString(const char* name, T& destination, const std::function<void(const pbnjson::JValue&, T&)>& parserFunc = &parseValueOrDataObject<T>);

	/** Look up a json field named *name* and translate into value using the provided map.
	 * The following checks are performed and appropriate error messages are sent:
	 * - field does not exist (overrideable with JsonParseContext::optional()
	 * - field type does not correspond to the destination type.
	 * - integral field value does not fit in destination type (overflow, underflow).
	 * @param name field name
	 * @param destination reference to variable to store the value in
	 * @param valueMap map from a JSON value type (string, int, double, bool) into the destination type.
	 * @return JsonParseContext for adding more constraints using call chaining.
	 *         The return value shall not be assigned to a variable. It depends on destructor being called to finalize checks.
	 * */
	template<typename IT, typename T>
	JsonParseContext<T> getAndMap(const char* name, T& destination, const std::unordered_map<IT, T>& valueMap);

	/** Look up a json field named *name* and translate into value using the provided array of pairs.
	 * The following checks are performed and appropriate error messages are sent:
	 * - field does not exist (overrideable with JsonParseContext::optional()
	 * - field type does not correspond to the destination type.
	 * - integral field value does not fit in destination type (overflow, underflow).
	 * @see hasError
	 * @param name field name
	 * @param destination reference to variable to store the value in
	 * @param valueMap map from a JSON value type (string, int, double, bool) into the destination type.
	 * @return JsonParseContext for adding more constraints using call chaining.
	 *         The return value shall not be assigned to a variable. It depends on destructor being called to finalize checks.
	 * */
	template<typename IT, typename T, size_t N>
	JsonParseContext<T> getAndMap(const char* name, T& destination, const std::array< std::pair<IT, T>, N >& valueMap);

	/** Look up a json field named *name* and translate into value using the provided array of pairs.
	 * The following checks are performed and appropriate error messages are sent:
	 * - field does not exist (overrideable with JsonParseContext::optional()
	 * - field type does not correspond to the destination type.
	 * - integral field value does not fit in destination type (overflow, underflow).

	 * Example parsing an string into an enum:
	 * @code
	 * parser.getAndMap<std::string, MyEnum>("someEnum", stringValue, {{"OPTION1", MyEnum::o1}, {"OPTION2", MyEnum::o2}});
	 * @endcode
	 *
	 * @see @ref hasError
	 * @param name field name
	 * @param destination reference to variable to store the value in
	 * @param valueMap initializer list with paris from a JSON value type (string, int, double, bool) into the destination type.
	 * @return JsonParseContext for adding more constraints using call chaining.
	 *         The return value shall not be assigned to a variable. It depends on destructor being called to finalize checks.
	 * */
	template<typename IT, typename T>
	JsonParseContext<T> getAndMap(const char* name, T& destination, std::initializer_list< std::pair<IT, T> > valueMap);

	/**
	 * Parse array object to std::vector.
	 * Individual items will be parsed using parserFunc.
	 *
	 * @param name field name
	 * @param destination reference to array to store the values in
	 * @param parserFunc optional - a function to parse individual items.
	 *                   Throw JsonParseException to indicate parse error.
	 * @return JsonParseContext for adding more constraints using call chaining.
	 *         The return value shall not be assigned to a variable. It depends on destructor being called to finalize checks.
	 */
	template<typename T>
	JsonParseContext< std::vector<T> > getArray(const char* name,
	                                              std::vector<T>& destination,
	                                              const std::function< void (const pbnjson::JValue& , T& )>& parserFunc = &parseValueOrDataObject<T>);

	/**
	 * Get a JsonParser object for a sub-object.
	 *
	 * @param name the json key that contains the object.
	 * @return JsonParser for the sub-object. Stores error and returns empty JsonParser on error.
	 *
	 * Example:
	 * @code
	 * input: {"video":{"available":true}}

	 * bool available;
	 * auto video = parser.getObject(video);
	 * video.get("available", available);
	 * video.finishParseOrThrow();
	 * @endcode
	 *
	 */
	JsonParser getObject(const char* name);

	/**
	 * Test if has a key with specified name.
	 */
	inline bool hasKey(const char* name)
	{
		return _jsonValue.hasKey(name);
	}

	/**
	 * Indicate that there will be no more fields in the request.
	 * Does final checks if parsing is successful.
	 *
	 * @param strict - if true, considers extra fields in request as an error.
	 * @throw JsonParseError if parse errors encountered.
	 */
	void finishParseOrThrow(bool strict = false);

	/**
	 * Indicate that there will be no more fields in the request.
	 * Does final checks if parsing is successful.
	 * @param strict - if true, considers extra fields in request as an error.
	 * @return true if parse successful, false on error.
	 */
	bool finishParse(bool strict = false);

	/**
	 * Returns true of the input is valid Json string depicting a Json object.
	 * @return
	 */
	inline bool isValidJson(){return _jsonValue.isObject();}

	/**
	 * Template method to do the parsing.
	 * A list of specializations for basic data types is provided.
	 * @param value - the value to parse
	 * @param destination set this value if parsing successful
	 * @throw JsonParseException on parse error
	 */
	template<typename T, typename Enable = void>
	static void parseValue(const pbnjson::JValue& value, T& destination);

	/**
	 * Template method to do the parsing. Accepts both basic types and objects derived from
	 * JsonDataObject.
	 * @param value - the value to parse
	 * @param destination set this value if parsing successful
	 * @throw JsonParseException on parse error
	 */
	template<typename T>
	static void parseValueOrDataObject(const pbnjson::JValue& value, T& destination)
	{
		//This is compile time constant, will compile to one or the other
		if (std::is_base_of<JsonDataObject,T>::value)
		{
			parseValue(value, reinterpret_cast<JsonDataObject&>(destination));
		}
		else
		{
			parseValue(value, destination);
		}
	}

	/**
	 * Records an error.
	 * Used internally by JsonParseContext
	 * Exposing here to allow calls from all template implemetnations.
	 * Not for public use.
	 * @param fieldName
	 * @param message
	 * @param ... message params.
	 */
	void recordError(const char* fieldName, const char* message);

protected:
	/**
	 * Internal implementation of getting value.
	 * Parses the json value in value and stores in destination.
	 * @tparam T
	 * @param name
	 * @param value
	 * @param found
	 * @param destination
	 * @param parserFunc
	 * @return
	 */
	template<typename T>
	JsonParseContext<T> getImpl(const char* name,
	                            const pbnjson::JValue& value,
	                            bool found,
	                            T& destination,
	                            const std::function<void(const pbnjson::JValue&, T&)>& parserFunc);

	std::string _parseError;
	pbnjson::JValue _jsonValue;
	ssize_t _numberOfFields;
};

/**
 * @brief Field parse context helper object. Allows to specify additional constraints for the field.
 * For use with @ref LSHelpers::JsonParser.
 *
 * Example:
 * @code
 *   int v;
 *   bool vSet;
 *
 *   request.get("intField", v).optional(true).defaultValue(5).min(0).max(10).checkValueSet(vSet);
 *  @endcode
 *
 *
 */
template<typename T>
class JsonParseContext
{
public:
	JsonParseContext(JsonParser& parser,
	                 const char* fieldName,
	                 T& destination,
	                 bool valueRead,
	                 bool valueNull) :
			_parser(parser),
			_fieldName(fieldName),
			_destination(destination),
			_valueRead(valueRead),
			_valueNull(valueNull),
			_optional(false),
			_allowNull(false)
	{}

	/// Copy constructor. Acts like a move constructor.
	JsonParseContext(const JsonParseContext<T>& context) :
			_parser(context._parser),
			_fieldName(context._fieldName),
			_destination(context._destination),
			_valueRead(context._valueRead),
			_optional(context._optional),
			_allowNull(context._allowNull)
	{
		//Set _valueRead to true to prevent destructor operations on the original.
		const_cast<JsonParseContext<T>&>(context)._valueRead = true;
	}

	/** No assignments. */
	void operator=(const JsonParseContext<T>& context) = delete;

	/* Called when .get expression ends. Finalize the parse and store errors.*/
	~JsonParseContext()
	{
		finishParse();
	}

	/**
	 * Finishes parsing this field and returns true or false.
	 * Note that for optimization reasons returns false when this field or any previous fields
	 * have failed to parse.
	 * @return
	 */
	inline operator bool()
	{
		finishParse();
		return !_parser.hasError();
	}

	/**
	 * Specify that the parameter is optional. It's mandatory buy default.
	 * @param isOptional
	 */
	inline JsonParseContext& optional(bool isOptional = true)
	{
		_optional = isOptional;
		return *this;
	};

	/**
	 * Specify that the parameter can be null. Works only when the parameter is optional.
	 * @param allowNull if true null value is treated as value not set.
	 */
	inline JsonParseContext& allowNull(bool allowNull = true)
	{
		_allowNull = allowNull;
		return *this;
	};

	/**
	 * Save value set flag into a bool variable.
	 * If the destination is set from JValue, sets value to true.
	 * If the destination is not set or set from default, sets value to false.
	 * @param value reference to boolean that will be set.
	 */
	inline JsonParseContext& checkValueRead(bool& /*out*/ value)
	{
		value = _valueRead;
		return *this;
	};

	/**
	 * Set default value.
	 * If destination is not set from JValue, sets it to this value.
	 * @param value the value to set.
	 */
	inline JsonParseContext& defaultValue(const T& value)
	{
		if (!_valueRead)
		{
			_destination = value;
		}
		return *this;
	};

	/**
	 * Specify minimum value.
	 * @param value - minimal value
	 * Set ParseError if min value check fails.
	 */
	inline JsonParseContext& min(const T& value)
	{
		if (_valueRead && _destination < value)
		{
			_parser.recordError(_fieldName, "value less than minimum");
		}
		return *this;
	};

	/**
	 * Specify maximum value.
	 * @param value - maximum value
	 * Set ParseError if min value check fails.
	 */
	inline JsonParseContext& max(const T& value)
	{
		if (_valueRead && _destination > value)
		{
			_parser.recordError(_fieldName, "value greater than maximum");
		}
		return *this;
	};

	/**
	 * Specify a list of allowed values. The check is performed only if the value
	 * is read from JValue.
	 * @param values - array of values.
	 * Set ParseError value not in allowed list.
	 */
	inline JsonParseContext& allowedValues(std::initializer_list<T> values)
	{
		if (_valueRead)
		{
			for (const auto& value: values)
			{
				if (value == _destination)
				{
					return *this;
				}
			}

			_parser.recordError(_fieldName, "value not in allowed list");
		}
		return *this;
	};

private:
	inline void finishParse() noexcept
	{
		if (!_valueRead)
		{
			if (!_optional)
			{
				_parser.recordError(_fieldName, "mandatory but not present");
			}
			else if (_valueNull && !_allowNull)
			{
				_parser.recordError(_fieldName, "null value is not allowed");
			}
		}
	}

	JsonParser& _parser;
	const char* _fieldName;
	T& _destination;
	bool _valueRead;
	bool _valueNull;
	bool _optional;
	bool _allowNull;
};

/* Template method implementations */

template<typename T>
JsonParseContext<T> JsonParser::get(const char* name,
                                    T& destination,
                                    const std::function<void(const pbnjson::JValue&, T&)>& parserFunc)
{
	if (name == nullptr)
	{
		throw std::runtime_error("Internal error in JsonParseContext.get, field name is null");
	}

	bool hasKey = _jsonValue.hasKey(name);
	pbnjson::JValue v; // = null
	if (hasKey)
	{
		v = _jsonValue[name];
		_numberOfFields++;
	}

	return getImpl(name, v, hasKey, destination, parserFunc);
}

template<typename T>
JsonParseContext<T> JsonParser::getFromString(const char* name,
                                              T& destination,
                                              const std::function<void(const pbnjson::JValue&, T&)>& parserFunc)
{
	std::string stringValue;
	bool valueSet;
	get(name, stringValue).optional().checkValueRead(valueSet);

	pbnjson::JValue value; // = null
	if (valueSet)
	{
		//Create new JValue from parsing the string.
		value = pbnjson::JDomParser::fromString(stringValue, pbnjson::JSchema::AllSchema());
	}

	return getImpl(name, value, valueSet, destination, parserFunc);
}

template<typename T>
JsonParseContext<T> JsonParser::getImpl(const char* name,
                                        const pbnjson::JValue& value,
                                        bool hasKey,
                                        T& destination,
                                        const std::function<void(const pbnjson::JValue&, T&)>& parserFunc)
{
	bool isNull = hasKey && value.isNull();

	if (!hasKey || isNull)
	{
		// Null maps to not set
		return JsonParseContext<T>(*this, name, destination, false, isNull);
	}

	try
	{
		parserFunc(value, destination);
		return JsonParseContext<T>(*this, name, destination, true, false);
	}
	catch (const JsonParseError& e)
	{
		recordError(name, e.message.c_str());
		return JsonParseContext<T>(*this, name, destination, false, isNull);
	}
}

template<typename IT, typename T>
JsonParseContext<T> JsonParser::getAndMap(const char* name, T& destination, const std::unordered_map<IT, T>& valueMap)
{
	IT intermediateValue;
	bool valueRead;
	bool found = false;
	get(name, intermediateValue).optional().checkValueRead(valueRead);

	if (valueRead)
	{
		const auto iter = valueMap.find(intermediateValue);
		if (iter != valueMap.cend())
		{
			found = true;
			destination = iter->second;
		}
		else
		{
			recordError(name, "value not in allowed values list");
		}
	}

	return JsonParseContext<T>(*this, name, destination, valueRead && found, false);
}

template<typename IT, typename T, size_t N>
JsonParseContext<T> JsonParser::getAndMap(const char* name, T& destination, const std::array< std::pair<IT, T>, N >& valueMap)
{
	IT intermediateValue;
	bool valueRead;
	bool found = false;
	get(name, intermediateValue).optional().checkValueRead(valueRead);

	if (valueRead)
	{
		for (size_t i = 0; i < N; i ++)
		{
			if (valueMap[i].first == intermediateValue)
			{
				found = true;
				destination = valueMap[i].second;
				break;
			}
		}

		if (!found)
		{
			recordError(name, "value not in allowed values list");
		}
	}

	return JsonParseContext<T>(*this, name, destination, valueRead && found, false);
}

template<typename IT, typename T>
JsonParseContext<T> JsonParser::getAndMap(const char* name, T& destination, std::initializer_list< std::pair<IT, T> > valueMap)
{
	IT intermediateValue;
	bool valueRead;
	bool found = false;
	get(name, intermediateValue).optional().checkValueRead(valueRead);

	if (valueRead)
	{
		for(auto iter = std::begin(valueMap); iter != std::end(valueMap); ++iter)
		{
			if (iter->first == intermediateValue)
			{
				found = true;
				destination = iter->second;
				break;
			}
		}

		if (!found)
		{
			recordError(name, "value not in allowed values list");
		}
	}

	return JsonParseContext<T>(*this, name, destination, valueRead && found, false);
}

template<typename T>
JsonParseContext< std::vector<T> > JsonParser::getArray(const char* name,
                                                        std::vector<T>& destination,
                                                        const std::function<void(const pbnjson::JValue&,T&)>& parserFunc)
{
	bool valueRead = false;
	bool isNull = false;

	pbnjson::JValue array;
	get(name, array).optional().checkValueRead(valueRead);

	if (valueRead)
	{
		isNull = array.isNull();

		if (!isNull)
		{
			if (!array.isArray())
			{
				recordError(name, "array expected but did not get one.");
				return JsonParseContext< std::vector<T> >(*this, name, destination, valueRead, isNull);
			}

			destination.clear();
			destination.reserve((uint32_t)array.arraySize());

			for (ssize_t i = 0; i < array.arraySize(); i++)
			{
				destination.emplace_back();
				getImpl(name, array[i], true, destination[i], parserFunc);
			}
		}
	}

	return JsonParseContext< std::vector<T> >(*this, name, destination, valueRead, isNull);
}

} // Namespace LSHelpers
