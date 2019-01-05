// Copyright (c) 2018-2019 LG Electronics, Inc.
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

#ifndef SRC_PARSER_COMPOSER_H_
#define SRC_PARSER_COMPOSER_H_

#include <pbnjson.hpp>
#include <type_traits>
#include <string>

namespace gmp { namespace parser {

template<typename T>
pbnjson::JValue to_json(const T & value) {
  return pbnjson::JValue(value);
}

template<>
pbnjson::JValue to_json(const base::result_t &);

template<>
pbnjson::JValue to_json(const base::source_info_t &);

template<>
pbnjson::JValue to_json(const base::video_info_t &);

template<>
pbnjson::JValue to_json(const base::audio_info_t &);

template<>
pbnjson::JValue to_json(const base::error_t &);

template<>
pbnjson::JValue to_json(const base::buffer_range_t &);

class Composer {
 public:
  Composer();

  template<typename T>
  void put(const char * key, const T & value) {
    _dom.put(key, to_json(value));
  }

  template<typename T>
  void put(const std::string & key, const T & value) {
    return put(key.c_str(), value);
  }

  std::string result();

 private:
  pbnjson::JValue _dom;
};

}  // namespace parser
}  // namespace gmp

#endif  // SRC_PARSER_COMPOSER_H_
