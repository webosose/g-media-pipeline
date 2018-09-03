// Copyright (c) 2018 LG Electronics, Inc.
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

#ifndef SRC_PARSER_PARSER_H_
#define SRC_PARSER_PARSER_H_

#include <type_traits>
#include <pbnjson.hpp>
#include <string>
#include <regex>
#include <gst/gst.h>
#include "log/log.h"

namespace gmp { namespace parser {

struct parser_error : std::runtime_error {
  explicit parser_error(const char * what) : runtime_error(what) {}
};

class Parser {
 public:
  explicit Parser(const char * message);

  template<typename T, typename std::enable_if<
          !std::is_arithmetic<T>::value
          && !std::is_same<T, std::string>::value>::type* = nullptr>
  T get() {
    throw parser_error("Unsupported type");
  }

  template<typename T, typename std::enable_if<
          std::is_same<T, bool>::value>::type* = nullptr>
  T get() {
    T val;
    auto cr = _dom.asBool(val);
    if (cr != CONV_OK)
      throw parser_error("Type conversion failure");
    return val;
  }

  template<typename T, typename std::enable_if<
           std::is_same<T, std::string>::value>::type* = nullptr>
  T get() {
    T str;
    auto cr = _dom.asString(str);
    if (cr != CONV_OK)
      throw parser_error("Type conversion failure");
    return str;
  }

  template<typename T, typename std::enable_if<
          std::is_arithmetic<T>::value
          && !std::is_same<T, bool>::value>::type* = nullptr>
  T get() {
    T val;
    auto cr = _dom.asNumber<T>(val);
    if (cr != CONV_OK)
      throw parser_error("Type conversion failure");
    return val;
  }

  template<typename T, typename std::enable_if<
          !std::is_arithmetic<T>::value
          && !std::is_same<T, std::string>::value>::type* = nullptr>
  T get(const char *) {
    throw parser_error("Unsupported type");
  }

  template<typename T, typename std::enable_if<
          std::is_same<T, bool>::value>::type* = nullptr>
  T get(const char * key) {
    T val;
    auto cr = _dom[key].asBool(val);
    if (cr != CONV_OK)
      throw parser_error("Type conversion failure");
    return val;
  }

  template<typename T, typename std::enable_if<
          std::is_same<T, std::string>::value>::type* = nullptr>
  T get(const char * key) {
    T str;
    auto cr = _dom[key].asString(str);
    if (cr != CONV_OK)
      throw parser_error("Type conversion failure");
    return str;
  }

  template<typename T, typename std::enable_if<
          std::is_arithmetic<T>::value
          && !std::is_same<T, bool>::value>::type* = nullptr>
  T get(const char * key) {
    T val;
    auto cr = _dom[key].asNumber<T>(val);
    if (cr != CONV_OK)
      throw parser_error("Type conversion failure");
    return val;
  }

  gint64 get_start_time(void) {
    gint64 start_time = 0;
    for(auto i = _dom.begin(); i != _dom.end(); ++i) {
      _serialized = pbnjson::JGenerator::serialize((*i).second, true);

      try {
        std::regex re("\\w+");
        std::sregex_iterator next(_serialized.begin(), _serialized.end(), re);
        std::sregex_iterator end;
        while (next != end) {
          std::smatch match = *next;
          if (match.str() == "start") {
            ++next;
            match = *next;
            start_time = std::stoll(match.str(), NULL, 10);
            break;
          }
          ++next;
        }
      } catch (std::regex_error& e) {
        GMP_DEBUG_PRINT("Syntax error in the regular expression");
      }

      if (start_time > 0)
        break;
    }

    return start_time * GST_USECOND;
  }

 private:
  pbnjson::JValue _dom;
  std::string _serialized;
};

}  // namespace parser
}  // namespace gmp

#endif  // SRC_PARSER_PARSER_H_
