/*
 * Copyright (c) 2008-2020 LG Electronics, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.

 * SPDX-License-Identifier: Apache-2.0
 */

#include <string>
#include <set>
#include <utility>
#include <cmath>
#include <base/types.h>
#include <resource_calculator.h>
#include <ResourceManagerClient.h>
#include "mediaresource/requestor.h"
#include "log/log.h"

#define LOGTAG "ResourceRequestor"

using namespace std;
using mrc::ResourceCalculator;
using namespace pbnjson;
using namespace gmp::base;
namespace gmp { namespace resource {

// FIXME : temp. set to 0 for request max
#define FAKE_WIDTH_MAX 0
#define FAKE_HEIGHT_MAX 0
#define FAKE_FRAMERATE_MAX 0

ResourceRequestor::ResourceRequestor(const std::string& appId, const std::string& connectionId)
  : rc_(shared_ptr<MRC>(MRC::create())),
    appId_(appId),
    cb_(nullptr),
    isUnloading_(false),
    allowPolicy_(true) {
  try {
    if (connectionId.empty()) {
      umsRMC_ = make_shared<uMediaServer::ResourceManagerClient> ();
      GMP_DEBUG_PRINT("ResourceRequestor creation done");
      umsRMC_->registerPipeline("media", appId_);           // only rmc case
      connectionId_ = (umsRMC_->getConnectionID() ?
                std::string(umsRMC_->getConnectionID()) : std::string());   // after registerPipeline
      if (connectionId_.empty()) {
          GMP_DEBUG_PRINT("Failed to get connection ID");
          exit(0);
      }
      // In unmanaged case, we should get display path like this currently
      umsRMC_->getDisplayId(appId_);
    }
    else {
      umsRMC_ = make_shared<uMediaServer::ResourceManagerClient> (connectionId);
      connectionId_ = connectionId;
    }
  }
  catch (const std::exception &e) {
    GMP_DEBUG_PRINT("Failed to create ResourceRequestor [%s]", e.what());
    exit(0);
  }

  if (connectionId_.empty()) {
    GMPASSERT(0);
  }

  umsRMC_->registerPolicyActionHandler(
      std::bind(&ResourceRequestor::policyActionHandler,
        this,
        std::placeholders::_1,
        std::placeholders::_2,
        std::placeholders::_3,
        std::placeholders::_4,
        std::placeholders::_5));
  GMP_DEBUG_PRINT("ResourceRequestor creation done");
}

ResourceRequestor::~ResourceRequestor() {
  releaseResource();
}

bool ResourceRequestor::calcResources(const std::string &display_mode, const int32_t display_path, std::string &resourceString, bool reacquiereInfo) {

  // ResourceCaculator & ResourceManager is changed in WebOS 3.0
  mrc::ResourceList AResource;
  mrc::ResourceList audioOptions;
  mrc::ResourceListOptions VResource;
  mrc::ResourceListOptions DisplayResource;
  mrc::ResourceListOptions finalOptions;

  // TODO(someone) : we have to set real width, hegith to videoResData

  if (reacquiereInfo == false) {
    AResource = rc_->calcAdecResources((MRC::AudioCodecs)translateAudioCodec(audioResData_.acodec),
                                        audioResData_.version,
                                        audioResData_.channel);
    mrc::concatResourceList(&audioOptions, &AResource);
    GMP_DEBUG_PRINT("AResource size:%lu, %s, %d",
                     AResource.size(), AResource.front().type.c_str(), AResource.front().quantity);
    finalOptions.push_back(audioOptions);
  }

  VResource = rc_->calcVdecResourceOptions((MRC::VideoCodecs)translateVideoCodec(videoResData_.vcodec),
                                            videoResData_.width,
                                            videoResData_.height,
                                            videoResData_.frameRate,
                                            (MRC::ScanType)translateScanType(videoResData_.escanType),
                                            (MRC::_3DType)translate3DType(videoResData_.e3DType));
  mrc::concatResourceListOptions(&finalOptions, &VResource);

  if (reacquiereInfo == false) {
    if (display_mode == "PunchThrough") {
      DisplayResource = rc_->calcDisplayPlaneResourceOptions(mrc::ResourceCalculator::RenderMode::kModePunchThrough);
    } else if (display_mode == "Textured") {
      DisplayResource = rc_->calcDisplayPlaneResourceOptions(mrc::ResourceCalculator::RenderMode::kModeTexture);
    } else {
      GMP_DEBUG_PRINT("Wrong display mode: %s", display_mode.c_str());
      return false;
    }
    mrc::concatResourceListOptions(&finalOptions, &DisplayResource);
  }

  JSchemaFragment input_schema("{}");
  JGenerator serializer(nullptr);

  JValue objArray = pbnjson::Array();
  for (auto const & option : finalOptions) {
    for (auto const & it : option) {
      JValue obj = pbnjson::Object();
      obj.put("resource", it.type + (it.type == "DISP" ? to_string(display_path) : ""));
      obj.put("qty", it.quantity);
      GMP_DEBUG_PRINT("calculator return : %s, %d", it.type.c_str(), it.quantity);
      objArray << obj;
    }
  }

  if (!serializer.toString(objArray, input_schema, resourceString)) {
    GMP_DEBUG_PRINT("[%s], fail to serializer to string", __func__);
    return false;
  }
  return true;
}


bool ResourceRequestor::acquireResources(void* meta, PortResource_t& resourceMMap, const std::string &display_mode, gmp::base::disp_res_t & res, const int32_t display_path) {

  string payload;

  if (!calcResources(display_mode, display_path, payload, false)) {
      GMP_DEBUG_PRINT("acquire calculation of resource error");
      return false;
  }

  JSchemaFragment input_schema("{}");
  JGenerator serializer(nullptr);
  string response;

  GMP_DEBUG_PRINT("send acquire to uMediaServer payload:%s", payload.c_str());

  if (!umsRMC_->acquire(payload, response)) {
    GMP_DEBUG_PRINT("fail to acquire!!! response : %s", response.c_str());
    return false;
  }

  try {
    parsePortInformation(response, resourceMMap, res);
    parseResources(response, adecResource_, vdecResource_, false);
  } catch (const std::runtime_error & err) {
    GMP_DEBUG_PRINT("[%s:%d] err=%s, response:%s",
          __func__, __LINE__, err.what(), response.c_str());
    return false;
  }

  return true;
}


bool ResourceRequestor::releaseResource() {
  bool returnValue = true;

  if (!adecResource_.empty()) {
    if (!umsRMC_->release(adecResource_))
      returnValue = false;
    adecResource_ = "";
  }

  if (!vdecResource_.empty()) {
    if (!umsRMC_->release(vdecResource_))
      returnValue = false;
    vdecResource_ = "";
  }

  return returnValue;
}

bool ResourceRequestor::reacquireResources(void* meta, PortResource_t& resourceMMap, const std::string &display_mode, gmp::base::disp_res_t & res, const int32_t display_path) {
  std::string new_resource;
  string response;

  if (!calcResources(display_mode, display_path, new_resource, true)) {
      GMP_DEBUG_PRINT("reacquire: calculation of resource error");
      return false;
  }

  JValue reaquire_obj = pbnjson::Object();
  reaquire_obj.put("new", new_resource);
  reaquire_obj.put("old", vdecResource_);

  JSchemaFragment input_schema("{}");
  JGenerator serializer(nullptr);
  string payload;

  if (!serializer.toString(reaquire_obj, input_schema, payload)) {
    GMP_DEBUG_PRINT("[%s], fail to serializer to string", __func__);
    return false;
  }

  if (!umsRMC_->reacquire(payload, response)) {
    GMP_DEBUG_PRINT("fail to reacquire!!! response : %s", response.c_str());
    return false;
  }

  try {
    parsePortInformation(response, resourceMMap, res);
    parseResources(response, adecResource_, vdecResource_, true);
  } catch (const std::runtime_error & err) {
    GMP_DEBUG_PRINT("[%s:%d] err=%s, response:%s",
          __func__, __LINE__, err.what(), response.c_str());
    return false;
  }

  return true;
}

bool ResourceRequestor::notifyForeground() const {
  return umsRMC_->notifyForeground();
}

bool ResourceRequestor::notifyBackground() const {
  return umsRMC_->notifyBackground();
}

bool ResourceRequestor::notifyActivity() const {
  return umsRMC_->notifyActivity();
}

bool ResourceRequestor::notifyPipelineStatus(const std::string& status) const {
  umsRMC_->notifyPipelineStatus(status);
  return true;
}

void ResourceRequestor::allowPolicyAction(const bool allow) {
  allowPolicy_ = allow;
}

bool ResourceRequestor::policyActionHandler(const char *action,
    const char *resources,
    const char *requestorType,
    const char *requestorName,
    const char *connectionId) {
  bool returnValue = true;

  GMP_DEBUG_PRINT("policyActionHandler action:%s, resources:%s, type:%s, name:%s, id:%s",
        action, resources, requestorType, requestorName, connectionId);
  if (allowPolicy_) {
    if ((nullptr != cb_) && !isUnloading_) {
      cb_();
    }
    if (umsRMC_->release(adecResource_) == false) {
      GMP_DEBUG_PRINT("release error in policyActionHandler: %s", adecResource_.c_str());
      returnValue = false;
    }
    if (umsRMC_->release(vdecResource_) == false) {
      GMP_DEBUG_PRINT("release error in policyActionHandler: %s", vdecResource_.c_str());
      returnValue = false;
    }
    if (returnValue == false) {
      return false;
    }
  }

  return allowPolicy_;
}

bool ResourceRequestor::parsePortInformation(const std::string& payload, PortResource_t& resourceMMap, gmp::base::disp_res_t & res) {
  JDomParser parser;
  JSchemaFragment input_schema("{}");
  if (!parser.parse(payload, input_schema)) {
    throw std::runtime_error("payload parsing failure during parsePortInformation");
  }

  JValue parsed = parser.getDom();
  if (!parsed.hasKey("resources")) {
    throw std::runtime_error("payload must have \"resources key\"");
  }

  for (int i=0; i < parsed["resources"].arraySize(); ++i) {
    string resource = parsed["resources"][i]["resource"].asString();
    int32_t value = parsed["resources"][i]["index"].asNumber<int32_t>();
    resourceMMap.insert(std::make_pair(resource, value));
  }


  for (auto& it : resourceMMap) {
    GMP_DEBUG_PRINT("port Resource - %s, : [%d] ", it.first.c_str(), it.second);
  }

  return true;
}

bool ResourceRequestor::parseResources(const std::string& payload,
                                       std::string& adecResource,
                                       std::string& vdecResource,
                                       bool reacquireInfo) {
  JDomParser parser;
  JSchemaFragment input_schema("{}");
  if (!parser.parse(payload, input_schema)) {
    throw std::runtime_error("payload parsing failure during parseResources");
  }
  JValue parsed = parser.getDom();
  if (!parsed.hasKey("resources")) {
    throw std::runtime_error("payload must have \"resources key\"");
  }
  if (reacquireInfo == false) {
      adecResource = getResourceString(parsed, std::string("ADEC"));
  }

  vdecResource = getResourceString(parsed, std::string("VDEC"));

  return true;
}


std::string ResourceRequestor::getResourceString(
    const JValue& parsed, const std::string& resource_type) {
  JSchemaFragment input_schema("{}");
  JValue objArray = pbnjson::Array();
  for (int i=0; i < parsed["resources"].arraySize(); ++i) {
    if (parsed["resources"][i]["resource"] == resource_type.c_str()) {
      JValue obj = pbnjson::Object();
      obj.put("resource", parsed["resources"][i]["resource"].asString());
      obj.put("index", parsed["resources"][i]["index"].asNumber<int32_t>());
      objArray << obj;
    }
  }
  std::string resource_str;
  JGenerator serializer(nullptr);
  if (!serializer.toString(objArray, input_schema, resource_str)) {
    throw std::runtime_error("fail to serializer toString during parseResources");
  }
  return resource_str;
}

int ResourceRequestor::translateVideoCodec(const GMP_VIDEO_CODEC vcodec) const {
  MRC::VideoCodec ev = MRC::kVideoEtc;
  switch (vcodec) {
    case GMP_VIDEO_CODEC_NONE:
      ev = MRC::kVideoEtc;    break;
    case GMP_VIDEO_CODEC_H264:
      ev = MRC::kVideoH264;   break;
    case GMP_VIDEO_CODEC_H265:
      ev = MRC::kVideoH265;   break;
    case GMP_VIDEO_CODEC_MPEG2:
      ev = MRC::kVideoMPEG;   break;
    case GMP_VIDEO_CODEC_MPEG4:
      ev = MRC::kVideoMPEG4;   break;
    case GMP_VIDEO_CODEC_VP8:
      ev = MRC::kVideoVP8;    break;
    case GMP_VIDEO_CODEC_VP9:
      ev = MRC::kVideoVP9;    break;
    case GMP_VIDEO_CODEC_MJPEG:
      ev = MRC::kVideoMJPEG;  break;
    default:
      // ev = MRC::kVideoEtc;
      // TODO(someone) : temp. always use H264 decoder.
      ev = MRC::kVideoH264;
      break;
  }

  GMP_DEBUG_PRINT("vcodec[%d] => ev[%d]", vcodec, ev);

  return static_cast<int>(ev);
}

int ResourceRequestor::translateAudioCodec(const GMP_AUDIO_CODEC acodec) const {
  MRC::AudioCodec ea = MRC::kAudioEtc;

  switch (acodec) {
    // currently webOS only considers "audio/mpeg" or not
    case GMP_AUDIO_CODEC_MP3:
      ea = MRC::kAudioMPEG;
      break;
    case GMP_AUDIO_CODEC_PCM:
      ea = MRC::kAudioPCM;
      break;
    case GMP_AUDIO_CODEC_AC3:
    case GMP_AUDIO_CODEC_EAC3:
    case GMP_AUDIO_CODEC_AAC:
    default:
      // ea = MRC::kAudioEtc;
      // TODO(someone) : temp. always use mp3 decoder.
      ea = MRC::kAudioMPEG;
      break;
  }

  GMP_DEBUG_PRINT("acodec[%d] => ea[%d]", acodec, ea);
  return static_cast<int>(ea);
}

int ResourceRequestor::translateScanType(const /*NDL_ESP_SCAN_TYPE*/int escanType) const {
  MRC::ScanType scan = MRC::kScanProgressive;

  switch (escanType) {
    // TODO(someone) : temp. always use progressive.
#if 0
    case SCANTYPE_PROGRESSIVE:
      scan = MRC::kScanProgressive;
      break;
    case SCANTYPE_INTERLACED:
      scan = MRC::kScanInterlaced;
      break;
#endif
    default:
      break;
  }

  return static_cast<int>(scan);
}

int ResourceRequestor::translate3DType(const /*NDL_ESP_3D_TYPE*/ int e3DType) const {
  MRC::_3DType my3d = MRC::k3DNone;

  switch (e3DType) {
#if 0
    case E3DTYPE_NONE:
      my3d = MRC::k3DNone;
      break;
      // TODO(someone) : resource calculator defines below 2 types. but not used.
      /*
         case E3DTYPE_SEQUENTIAL:
         my3d = MRC::k3DSequential;
         break;
         case E3DTYPE_MULTISTREAM:
         my3d = MRC::k3DMultiStream;
         break;
         */
#endif
    default:
      my3d = MRC::k3DNone;
      break;
  }

  return static_cast<int>(my3d);
}

bool ResourceRequestor::setSourceInfo(const gmp::base::source_info_t &sourceInfo) {
  // TODO(anonymous): Support multiple video/audio stream case
  if (sourceInfo.video_streams.empty() && sourceInfo.audio_streams.empty()) {
    GMP_DEBUG_PRINT("Invalid video/audio stream size error");
    return false;
  }

  if (!sourceInfo.video_streams.empty()) {
    gmp::base::video_info_t video_stream_info = sourceInfo.video_streams.front();
    videoResData_.width = video_stream_info.width;
    videoResData_.height = video_stream_info.height;
    videoResData_.vcodec = static_cast<GMP_VIDEO_CODEC>(video_stream_info.codec);
    videoResData_.frameRate = std::round(static_cast<float>(video_stream_info.frame_rate.num) /
                                         static_cast<float>(video_stream_info.frame_rate.den));
    videoResData_.escanType = 0;
  }

  if (!sourceInfo.audio_streams.empty()) {
    gmp::base::audio_info_t audio_stream_info = sourceInfo.audio_streams.front();
    audioResData_.acodec  = static_cast<GMP_AUDIO_CODEC>(audio_stream_info.codec);
    audioResData_.channel = audio_stream_info.channels;
    audioResData_.version = 0;
  }
  return true;
}



void ResourceRequestor::planeIdHandler(int32_t planePortIdx) {
  GMP_DEBUG_PRINT("planePortIndex = %d", planePortIdx);
  if (nullptr != planeIdCb_) {
    bool res = planeIdCb_(planePortIdx);
    GMP_DEBUG_PRINT("PlanePort[%d] register : %s", planePortIdx, res ? "success!" : "fail!");
  }
}
void ResourceRequestor::setAppId(std::string id) {
  appId_ = id;
}

int32_t ResourceRequestor::getDisplayPath() {
  return umsRMC_->getDisplayID();
}


}  // namespace resource
}  // namespace gmp
