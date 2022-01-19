// Copyright (c) 2018-2022 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// SPDX-License-Identifier: Apache-2.0

#ifndef SRC_PLAYERFACTORY_ELEMENTCREATOR_H_
#define SRC_PLAYERFACTORY_ELEMENTCREATOR_H_

#include <string>
#include <memory>
#include <gst/gst.h>
#include <pbnjson.hpp>

#include "PlayerTypes.h"

namespace gmp { namespace pf {
class ElementFactory {
 public:
  ElementFactory() {
  };
  ~ElementFactory() {
  };

  static GstElement * Create(const std::string &pipelineType,
    const std::string &elementTypeName, uint32_t displayPath = DEFAULT_DISPLAY);
  static std::string GetPlatform(void);
  static gint32 GetUseAudioProperty(void);

  static void SetAllproperties(const std::string &pipelineType,
    const std::string &elementTypeName, GstElement * element);
  static std::string streamtype[2];
 private:
  static std::string GetPreferredElementName(const std::string &pipelineType,
    const std::string & elementTypeName);
  static GstElement * GetGstElement(const std::string &pipelineType,
    const std::string &name);
  static gint32 GetPipelineType(const std::string &pipelineType);
  static void SetProperty(GstElement * element,
    const pbnjson::JValue &prop, const pbnjson::JValue &value);
  static const char gst_element_json_path[];
};

}  // namespace pf
}  // namespace gmp
#endif  // SRC_PLAYERFACTORY_ELEMENTCREATOR_H_
