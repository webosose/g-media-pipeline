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

#ifndef SRC_PLAYER_URI_PLAYER_H_
#define SRC_PLAYER_URI_PLAYER_H_

#include "AbstractPlayer.h"

namespace gmp { namespace player {

class UriPlayer : public AbstractPlayer {
 public:
  ~UriPlayer();
  bool Load(const std::string &str) override;
  bool UnloadImpl() override;
  bool Play() override;
  bool Pause() override;
  bool SetPlayRate(const double rate) override;
  bool Seek(const int64_t position) override;
  bool SetVolume(int volume) override;
  static gboolean HandleBusMessage(GstBus *bus,
                                   GstMessage *message, gpointer user_data);
  static GstBusSyncReply HandleSyncBusMessage(GstBus * bus,
                                   GstMessage * msg, gpointer data);
  static gboolean NotifyCurrentTime(gpointer user_data);
  static gboolean NotifyBufferingTime(gpointer user_data);
  static gboolean SourceChangedData(GstElement* pipeline, gint width, gint height, gint fps_num, gint fps_den,
                             gpointer userData);

  /*for handling track volume*/
  bool RegisterTrack();
  bool UnRegisterTrack();


 protected:
  UriPlayer();
  void NotifySourceInfo();
  bool GetSourceInfo();
  virtual bool LoadPipeline();
  base::error_t HandleErrorMessage(GstMessage *message);
  int32_t ConvertErrorCode(GQuark domain, gint code);
  base::buffer_range_t CalculateBufferingTime();
  base::playback_state_t GetPlayerState() const {
    std::lock_guard<std::mutex> lock(state_lock_);
    return current_state_;
  }
  bool SetPlayerState(base::playback_state_t state) {
    std::lock_guard<std::mutex> lock(state_lock_);
    current_state_ = state;
    return true;
  }
  void ParseOptionString(const std::string &str);

  std::string uri_ = "";
  std::string connectID_;
  bool httpSource_ = false;
  mutable std::mutex state_lock_;

  /* buffering variable */
  GstElement *queue2_ = nullptr;
  GstElement *aSink = nullptr;
  guint bufferingTimer_id_ = 0;
  bool buffering_ = false;
  bool buffering_time_updated_ = false;
  gint64 buffered_time_ = -1;
  base::playback_state_t current_state_ = base::playback_state_t::STOPPED;
  const gint queue2MaxSizeBytes = 24 * 1024 * 1024;
  const gint64 queue2MaxSizeTime = 10 * GST_SECOND;
  const gint64 queue2MaxSizeMsec = GST_TIME_AS_MSECONDS(queue2MaxSizeTime);
  const gint audioQueue2MaxSizeBytes = 4 * 1024 * 1024;
  const gint64 audioQueue2MaxSizeTime = 2 * GST_SECOND;
  const gint64 audioQueue2MaxSizeMsec = GST_TIME_AS_MSECONDS(audioQueue2MaxSizeTime);
};

}  // namespace player
}  // namespace gmp
#endif  // SRC_PLAYER_URI_PLAYER_H
