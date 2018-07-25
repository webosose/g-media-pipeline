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

#ifndef SRC_PLAYER_PLAYER_H_
#define SRC_PLAYER_PLAYER_H_

#include <glib.h>
#include <glib-unix.h>
#include <gst/gst.h>
#include <string>
#include <memory>
#include <thread>
#include <gst/player/player.h>
#include <gst/pbutils/pbutils.h>

#include "types.h"

namespace gmp { namespace service { class IService; }}
namespace gmp { namespace resource { class ResourceRequestor; }}

namespace gmp { namespace player {
class Player {
 public:
  Player()
    : pipeline_(NULL)
    , service_(NULL)
    , play_rate_(1.0)
    , duration_(0)
    , load_complete_(false)
    , seeking_(false)
    , current_position_(0) {}
  virtual ~Player() {}

  virtual bool Load(const std::string & uri) = 0;
  virtual bool Unload() = 0;
  virtual bool Play() = 0;
  virtual bool Pause() = 0;
  virtual bool SetPlayRate(const double rate) = 0;
  virtual bool Seek(const int64_t position) = 0;
  virtual bool SetVolume(int volume) = 0;
  virtual bool SetPlane(int planeId) = 0;
  virtual void Initialize(gmp::service::IService *service) = 0;

 protected:
  GstElement *pipeline_;
  gmp::service::IService *service_;
  double play_rate_;
  gint64 duration_;
  bool load_complete_;
  bool seeking_;
  gint64 current_position_;
};

class UriPlayer : public Player {
 public:
  UriPlayer();
  ~UriPlayer();
  bool Load(const std::string &uri) override;
  bool Unload() override;
  bool Play() override;
  bool Pause() override;
  bool SetPlayRate(const double rate) override;
  bool Seek(const int64_t position) override;
  bool SetVolume(int volume) override;
  bool SetPlane(int planeId) override;
  void Initialize(gmp::service::IService *service) override;
  static gboolean HandleBusMessage(GstBus *bus,
                                   GstMessage *message, gpointer user_data);
  static gboolean NotifyCurrentTime(gpointer user_data);
  static gboolean NotifyBufferingTime(gpointer user_data);

 private:
  void NotifySourceInfo();
  bool GetSourceInfo();
  bool LoadPipeline();
  base::error_t HandleErrorMessage(GstMessage *message);
  int32_t ConvertErrorCode(GQuark domain, gint code);
  void SetGstreamerDebug();
  bool FindQueue2Element();
  base::playback_state_t GetPlayerState() const { return current_state_; }
  bool SetPlayerState(base::playback_state_t state) {
    current_state_ = state;
    return true;
  }

 private:
  base::source_info_t source_info_;
  std::string uri_;
  guint positionTimer_id_;
  std::shared_ptr<gmp::resource::ResourceRequestor> res_requestor_;
  std::string connectID_;
  int32_t planeId_;

  /* buffering variable */
  GstElement *queue2_;
  guint bufferingTimer_id_;
  bool buffering_;
  gint64 buffered_time_;
  base::playback_state_t current_state_;
  const gint queue2MaxSizeBytes = 24 * 1024 * 1024;
  const gint64 queue2MaxSizeTime = 10 * GST_SECOND;
  const gint64 queue2MaxSizeMsec = GST_TIME_AS_MSECONDS(queue2MaxSizeTime);
};

}  // namespace player
}  // namespace gmp
#endif  // SRC_PLAYER_PLAYER_H_
