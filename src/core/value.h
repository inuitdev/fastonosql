/*  Copyright (C) 2014-2017 FastoGT. All right reserved.

    This file is part of FastoNoSQL.

    FastoNoSQL is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    FastoNoSQL is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with FastoNoSQL.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <common/value.h>  // for ArrayValue (ptr only), etc

namespace fastonosql {
namespace core {

class JsonValue : public common::Value {  // simple json class value, only save string without validation
 public:
  static const common::Value::Type TYPE_JSON = static_cast<common::Value::Type>(common::Value::USER_TYPES + 1);
  explicit JsonValue(const std::string& json_value);
  virtual ~JsonValue();

  virtual bool GetAsString(std::string* out_value) const override WARN_UNUSED_RESULT;
  virtual JsonValue* DeepCopy() const override;
  virtual bool Equals(const Value* other) const override;

  static bool IsValidJson(const std::string& json);

 private:
  std::string value_;
  DISALLOW_COPY_AND_ASSIGN(JsonValue);
};

class GraphValue : public common::Value {
 public:
  static const common::Value::Type TYPE_GRAPH = static_cast<common::Value::Type>(common::Value::USER_TYPES + 2);
  GraphValue();
  virtual ~GraphValue();

  virtual GraphValue* DeepCopy() const override;
  virtual bool Equals(const Value* other) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GraphValue);
};

class SearchValue : public common::Value {
 public:
  static const common::Value::Type TYPE_FT_INDEX = static_cast<common::Value::Type>(common::Value::USER_TYPES + 3);
  static const common::Value::Type TYPE_FT_TERM = static_cast<common::Value::Type>(common::Value::USER_TYPES + 4);
  virtual ~SearchValue();

  static SearchValue* CreateSearchIndex();
  static SearchValue* CreateSearchDocument();

  virtual SearchValue* DeepCopy() const override;
  virtual bool Equals(const Value* other) const override;

 private:
  SearchValue(common::Value::Type type);

  DISALLOW_COPY_AND_ASSIGN(SearchValue);
};

common::Value* CreateEmptyValueFromType(common::Value::Type value_type);
const char* GetTypeName(common::Value::Type value_type);

namespace detail {
bool have_space(const std::string& data);
std::string hex_string(const common::buffer_t& value);
std::string hex_string(const std::string& value);
std::string string_from_hex(const common::buffer_t& value);
std::string string_from_hex(const std::string& value);
}  // namespace detail

std::string ConvertValue(common::Value* value, const std::string& delimiter, bool for_cmd);
std::string ConvertValue(common::ArrayValue* array, const std::string& delimiter, bool for_cmd);
std::string ConvertValue(common::SetValue* set, const std::string& delimiter, bool for_cmd);
std::string ConvertValue(common::ZSetValue* zset, const std::string& delimiter, bool for_cmd);
std::string ConvertValue(common::HashValue* hash, const std::string& delimiter, bool for_cmd);
std::string ConvertValue(common::FundamentalValue* value, const std::string& delimiter, bool for_cmd);
std::string ConvertValue(common::StringValue* value, const std::string& delimiter, bool for_cmd);
std::string ConvertValue(common::ByteArrayValue* value, const std::string& delimiter, bool for_cmd);
// extended
std::string ConvertValue(JsonValue* value, const std::string& delimiter, bool for_cmd);

std::string ConvertToHumanReadable(common::Value* value, const std::string& delimiter = std::string());

}  // namespace core
}  // namespace fastonosql
