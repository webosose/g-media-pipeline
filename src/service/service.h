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

#include <player/PlayerTypes.h>
#include <base/types.h>
#include <mediaplayerclient/MediaPlayerClient.h>

class UMSConnector;
class UMSConnectorHandle;
class UMSConnectorMessage;

namespace gmp { namespace service {
class Service {
 public:
  static Service* GetInstance(const std::string& service_name);
  ~Service();

  void Notify(const gint notification, const gint64 numValue, const gchar *strValue, void *payload = nullptr);
  bool Wait();
  bool Stop();

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
  static bool SetUriEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool SetPlayRateEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool SelectTrackEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool SetUpdateIntervalEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool SetUpdateIntervalKVEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool ChangeResolutionEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool SetStreamQualityEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool SetPropertyEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool SetVolumeEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool SetPlaneEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);

  // pipeline state query API
  static bool GetPipelineStateEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool LogPipelineStateEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool GetActivePipelinesEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);
  static bool SetPipelineDebugStateEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);

  // exit
  static bool ExitEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt);

 private:
  explicit Service(const std::string& service_name);
  Service(const Service& s) = delete;
  void operator=(const Service& s) = delete;

  static Service *instance_;

  std::unique_ptr<UMSConnector> umc_;
  std::unique_ptr<gmp::player::MediaPlayerClient> media_player_client_;

  std::string media_id_;  // connection_id
  std::string app_id_;
};

}  // namespace service
}  // namespace gmp

#endif  // SRC_SERVICE_SERVICE_H_
