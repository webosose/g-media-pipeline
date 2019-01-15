/*
 * Copyright (c) 2008-2019 LG Electronics, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0



 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.

 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SRC_MEDIARESOURCE_REQUESTOR_H_
#define SRC_MEDIARESOURCE_REQUESTOR_H_

#include <string>
#include <functional>
#include <memory>
#include <map>
#include <dto_types.h>

#include "PlayerTypes.h"

namespace mrc { class ResourceCalculator; }
namespace gmp { namespace base { struct source_info_t; }}

namespace uMediaServer {   class ResourceManagerClient; }

class MDCClient;
class MDCContentProvider;

namespace gmp { namespace resource {

struct videoResData_t {
  GMP_VIDEO_CODEC vcodec;

  int width;
  int height;
  int frameRate;
  // GMP_SCAN_TYPE escanType;
  int escanType;
  // GMP_3D_TYPE e3DType;
  int e3DType;

  int parWidth;  // pixel-aspect-ratio width
  int parHeight; // pixel-aspect-ratio height
};

struct audioResData_t {
  GMP_AUDIO_CODEC acodec;

  int version;
  int channel;
};


typedef std::function<void()> Functor;
typedef std::function<bool(int32_t)> PlaneIDFunctor;
typedef mrc::ResourceCalculator MRC;
typedef std::multimap<std::string, int> PortResource_t;

class ResourceRequestor {
 public:
  ResourceRequestor(const std::string& appId);
  explicit ResourceRequestor(const std::string& appId, const std::string& connectionId);
  virtual ~ResourceRequestor();

  const std::string getConnectionId() const { return connectionId_; }
  void registerUMSPolicyActionCallback(Functor callback) { cb_ = callback; }
  void registerPlaneIdCallback(PlaneIDFunctor callback) { planeIdCb_ = callback; }

  void unregisterWithMDC();

  bool acquireResources(void* meta, PortResource_t& resourceMMap, gmp::base::disp_res_t & res, const int32_t display_path = 0);

  bool releaseResource();

  bool notifyForeground() const;
  bool notifyBackground() const;
  bool notifyActivity() const;

  void allowPolicyAction(const bool allow);

  bool setVideoDisplayWindow(const long left, const long top,
      const long width, const long height,
      const bool isFullScreen) const;

  bool setVideoCustomDisplayWindow(const long src_left, const long src_top,
      const long src_width, const long src_height,
      const long dst_left, const long dst_top,
      const long dst_width, const long dst_height,
      const bool isFullScreen) const;

  bool muteAudio(bool mute) { return false; }
  bool muteVideo(bool mute) { return false; }

  bool mediaContentReady(bool state);

  bool setVideoInfo(const gmp::base::video_info_t &videoInfo);
  bool setSourceInfo(const gmp::base::source_info_t &sourceInfo);
  void setAppId(std::string id);

 private:
  void ResourceRequestorInit(const std::string& connectionId);
  void ResourceRequestorInit();
  bool policyActionHandler(const char *action,
      const char *resources,
      const char *requestorType,
      const char *requestorName,
      const char *connectionId);
  void planeIdHandler(int32_t planePortIdx);

  bool parsePortInformation(const std::string& payload, PortResource_t& resourceMMap, gmp::base::disp_res_t & res);
  bool parseResources(const std::string& payload, std::string& resources);

  // translate enum type from omx player to resource calculator
  int translateVideoCodec(const GMP_VIDEO_CODEC vcodec) const;
  int translateAudioCodec(const GMP_AUDIO_CODEC acodec) const;
  int translateScanType(const /*NDL_ESP_SCAN_TYPE*/ int escanType) const;
  int translate3DType(const /*NDL_ESP_3D_TYPE*/ int e3DType) const;

  std::shared_ptr<MRC> rc_;
  std::shared_ptr<uMediaServer::ResourceManagerClient> umsRMC_;
  std::shared_ptr<MDCClient> umsMDC_;
  std::shared_ptr<MDCContentProvider> umsMDCCR_;
  std::string appId_;
  std::string connectionId_;
  Functor cb_;
  PlaneIDFunctor planeIdCb_;
  std::string acquiredResource_;
  videoResData_t videoResData_;
  audioResData_t audioResData_;
  ums::video_info_t video_info_;
  bool allowPolicy_;
};

}  // namespace resource
}  // namespace gmp

#endif  // SRC_MEDIARESOURCE_REQUESTOR_H_
