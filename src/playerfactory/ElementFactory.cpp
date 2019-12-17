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
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// SPDX-License-Identifier: Apache-2.0

#include <gst/gst.h>

#include "ElementFactory.h"
#include "log/log.h"
#include "PlayerTypes.h"

namespace gmp { namespace pf {

const char ElementFactory::gst_element_json_path[] = "/etc/g-media-pipeline/gst_elements.conf";

GstElement * ElementFactory::Create(const std::string &pipelineType,
  const std::string &elementTypeName, uint32_t displayPath) {
  GstElement * element = GetGstElement(pipelineType, elementTypeName);
  pbnjson::JValue root = pbnjson::JDomParser::fromFile(gst_element_json_path);
  if (!root.isObject()) {
    GMP_DEBUG_PRINT("Gst element file parsing error");
    return NULL;
  }

  pbnjson::JValue elements = root["gst_elements"];
  int i = GetPipelineType(pipelineType);
  if (elements[i].hasKey(elementTypeName)
    && elements[i][elementTypeName].hasKey("name")) {
    if (elements[i][elementTypeName].hasKey("properties")) {
      for(auto it : elements[i][elementTypeName]["properties"].children()) {
        SetProperty(element, it.first, it.second);
      }
    }

    if (elements[i][elementTypeName].hasKey("device")) {
      pbnjson::JValue el = elements[i][elementTypeName]["device"];
      if (el.isArray() && displayPath < el.arraySize()
        && el[displayPath].isString()) {
        GMP_DEBUG_PRINT("Select device %d - name : %s",
          displayPath, el[displayPath].asString().c_str());
        SetProperty(element, "device", el[displayPath].asString());
      }
    }
  } else {
    GMP_DEBUG_PRINT("elementTypeName : %s is not exist",
      elementTypeName.c_str());
  }

  return element;
}

gint32 ElementFactory::GetUseAudioProperty() {
  gint32 ret = 1;
  pbnjson::JValue root = pbnjson::JDomParser::fromFile(gst_element_json_path);
  if (!root.isObject()) {
    GMP_DEBUG_PRINT("Gst element file parsing error");
    return ret;
  }

  if (root.hasKey("use_audio")) {
    ret = root["use_audio"].asNumber<int32_t>();
  }

  return ret;
}

std::string ElementFactory::GetPlatform(void)
{
  pbnjson::JValue root = pbnjson::JDomParser::fromFile(gst_element_json_path);
  std::string strReturn = std::string("");

  if (!root.isObject()) {
    GMP_DEBUG_PRINT("Gst element file parsing error");
  }

  pbnjson::JValue jvPlatform = root["platform"];
  if (!jvPlatform.isString()) {
    GMP_DEBUG_PRINT("Please check the json file in %s", gst_element_json_path);
  } else {
    strReturn = root["platform"].asString();
    GMP_DEBUG_PRINT("Platform : %s", strReturn.c_str());
  }
  return strReturn;
}

std::string ElementFactory::GetPreferredElementName(const std::string &pipelineType,
  const std::string & elementTypeName) {
  std::string elementName = std::string("");
  pbnjson::JValue root = pbnjson::JDomParser::fromFile(gst_element_json_path);
  if (!root.isObject()) {
    GMP_DEBUG_PRINT("Gst element file parsing error");
    return std::string("");
  }

  pbnjson::JValue elements = root["gst_elements"];
  int i = GetPipelineType(pipelineType);
  if (elements[i].hasKey(elementTypeName)
    && elements[i][elementTypeName].hasKey("name")) {
    elementName = elements[i][elementTypeName]["name"].asString();
    GMP_DEBUG_PRINT("%s : %s", elementTypeName.c_str(), elementName.c_str());
  } else {
    GMP_DEBUG_PRINT("elementTypeName : %s is not exist",
      elementTypeName.c_str());
  }

  return elementName;
}

GstElement * ElementFactory::GetGstElement(const std::string &pipelineType, const std::string &name) {
  GstElement * element = NULL;
  std::string elementName = GetPreferredElementName(pipelineType, name);
  element = gst_element_factory_make(elementName.c_str(), name.c_str());

  return element;
}

gint32 ElementFactory::GetPipelineType(const std::string &pipelineType)
{
  if (pipelineType == "playbin") {
    return PLAYBIN_PIPELINE;
  }
  else if (pipelineType == "custom") {
    return CUSTOM_PLAYER_PIPELINE;
  }
  else if (pipelineType == "hdmi") {
    return HDMI_PIPELINE;
  }
  else {
    return -1;
  }
}

void ElementFactory::SetProperty(GstElement * element,
  const pbnjson::JValue &prop, const pbnjson::JValue &value) {
  const pbnjson::JValue &objProp = prop;
  const pbnjson::JValue &objValue = value;
  if (objProp.isString()) {
    std::string strProp = objProp.asString();
    if (objValue.isNumber()) {
      gint32 num = objValue.asNumber<gint32>();
      GMP_DEBUG_PRINT("property - %s : %d", strProp.c_str(), num);
      g_object_set(G_OBJECT(element), strProp.c_str(), num, nullptr);
    } else if (objValue.isString()) {
      GMP_DEBUG_PRINT("property - %s : %s", strProp.c_str(), objValue.asString().c_str());
      g_object_set(G_OBJECT(element), strProp.c_str(), objValue.asString().c_str(),  nullptr);
    } else if (objValue.isBoolean()) {
      GMP_DEBUG_PRINT("property - %s : %s", strProp.c_str(), objValue.asBool() ? "true" : "false");
      g_object_set(G_OBJECT(element), strProp.c_str(), objValue.asBool(),  nullptr);
    } else {
      GMP_DEBUG_PRINT("Please check the value type of %s",
        strProp.c_str());
    }
  } else {
    GMP_DEBUG_PRINT("A property name should be string. \
      Please check the json file.");
  }
}

void ElementFactory::SetAllproperties(const std::string &pipelineType,
  const std::string &elementTypeName, GstElement * element) {
  pbnjson::JValue root = pbnjson::JDomParser::fromFile(gst_element_json_path);
  if (!root.isObject()) {
    GMP_DEBUG_PRINT("Gst element file parsing error");
    return;
  }

  pbnjson::JValue elements = root["gst_elements"];
  int i = GetPipelineType(pipelineType);
  if (elements[i].hasKey(elementTypeName)
    && elements[i][elementTypeName].hasKey("name")) {
    if (elements[i][elementTypeName].hasKey("properties")) {
      for(auto it : elements[i][elementTypeName]["properties"].children()) {
        SetProperty(element, it.first, it.second);
      }
    }
  } else {
    GMP_DEBUG_PRINT("elementTypeName : %s is not exist",
      elementTypeName.c_str());
  }
}

}  // namespace pf
}  // namespace gmp
