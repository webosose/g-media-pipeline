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

#ifndef SRC_PLAYER_ABSTRACTPLAYER_H_
#define SRC_PLAYER_ABSTRACTPLAYER_H_

#include <memory>

#include "Player.h"
#include "lunaserviceclient/LunaServiceClient.h"

#define VIDEO_SCALE_WIDTH 1080
#define VIDEO_SCALE_HEIGHT 540

namespace gmp { namespace player {

class AbstractPlayer : public Player {
 public:
  virtual ~AbstractPlayer();

  virtual bool Load(const std::string &str);
  virtual bool Unload();
  virtual bool UnloadImpl();
  virtual bool Play();
  virtual bool Pause();
  virtual bool SetPlayRate(const double rate);
  virtual bool Seek(const int64_t position);
  virtual bool SetVolume(int volume);
  virtual bool SetPlane(int planeId);
  virtual bool SetDisplayPath(const uint32_t display_path);

  virtual bool Load(const MEDIA_LOAD_DATA_T* loadData);
  virtual void notifyFunctionUMSPolicyAction();
  virtual bool UpdateVideoResData(const gmp::base::source_info_t &sourceInfo);
  virtual void RegisterCbFunction(CALLBACK_T);
  virtual bool PushEndOfStream();
  virtual bool Flush();
  virtual MEDIA_STATUS_T Feed(const guint8* pBuffer, guint32 bufferSize,
                        guint64 pts, MEDIA_DATA_CHANNEL_T esData);

  CALLBACK_T cbFunction_ = nullptr;
  virtual GstElement* GetPipeline();

 protected:
  AbstractPlayer();
  void SetGstreamerDebug();
  void SetUseAudio();

  void SetReloading(const gint64 &start_time);
  void DoReloading();

  bool attachSurface(bool allow_no_window = false);
  bool detachSurface();

  GstElement *pipeline_ = nullptr;

  base::source_info_t source_info_;
  uint32_t display_path_ = DEFAULT_DISPLAY;
  std::string track_id_;

  double play_rate_ = 1.0;
  gint64 duration_ = 0;
  bool load_complete_ = false;
  gint32 use_audio = 1;
  bool seeking_ = false;
  gint64 current_position_ = 0;
  bool reloading_ = false;
  gint64 reload_seek_position_ = 0;

  int32_t planeId_ = -1;
  guint currPosTimerId_ = 0;
  std::recursive_mutex recursive_mutex_;

  /* GAV Features */
  LSM::Connector lsm_connector_;
  std::string display_mode_ = "Default";
  std::string window_id_;
  std::unique_ptr<gmp::LunaServiceClient> lsClient_;
};

}  // namespace player
}  // namespace gmp
#endif  // SRC_PLAYER_ABSTRACTPLAYER_H_
