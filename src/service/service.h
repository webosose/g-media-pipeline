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

#ifndef SRC_SERVICE_SERVICE_H_
#define SRC_SERVICE_SERVICE_H_

#include <memory>
#include <string>
#include <boost/core/noncopyable.hpp>

#include "PlayerTypes.h"
#include "types.h"

class UMSConnector;
class UMSConnectorHandle;
class UMSConnectorMessage;

namespace gmp { namespace player { class Player; }}
namespace gmp { namespace base { struct source_info_t; }}
namespace gmp { namespace resource { class ResourceRequestor; }}

#define PLANE_MAP_SIZE 4

namespace gmp { namespace service {
class IService : private boost::noncopyable {
 public:
  virtual void Notify(const NOTIFY_TYPE_T notification) = 0;
  virtual void Notify(const NOTIFY_TYPE_T notification, const void *payload) = 0;
  virtual bool Wait() = 0;
  virtual bool Stop()= 0;
  virtual bool acquire(gmp::base::source_info_t   &source_info, const int32_t display_path = 0) = 0;
};

class Service : public IService {
 public:
  ~Service();
  static Service *GetInstance(const char *service_name);

  void Notify(const NOTIFY_TYPE_T notification) override;
  void Notify(const NOTIFY_TYPE_T notification, const void *payload) override;

  bool Wait() override;
  bool Stop() override;
  void Initialize(gmp::player::Player *player);
  bool acquire(gmp::base::source_info_t   &source_info, const int32_t display_path = 0) override;

  // uMediaserver public API
  static bool LoadEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool AttachEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool UnloadEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);

  // media operations
  static bool PlayEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool PauseEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool SeekEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool StateChangeEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool UnsubscribeEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool SetPlayRateEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool SetVolumeEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool SetPlaneEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);

  // Resource Manager API
  static bool RegisterPipelineEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool UnregisterPipelineEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool AcquireEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool TryAcquireEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool ReleaseEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool NotifyForegroundEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool NotifyBackgroundEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool NotifyActivityEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool TrackAppProcessesEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);

 private:
  explicit Service(const char *service_name);

  UMSConnector *umc_;
  std::string media_id_;  // connection_id
  gmp::player::Player *player_;
  static Service *instance_;
  std::shared_ptr<gmp::resource::ResourceRequestor> res_requestor_;

};

}  // namespace service
}  // namespace gmp

#endif  // SRC_SERVICE_SERVICE_H_
