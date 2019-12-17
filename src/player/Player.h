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

#ifndef SRC_PLAYER_PLAYER_H_
#define SRC_PLAYER_PLAYER_H_

#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <uMediaTypes.h>
#include <lsm_connector.h>

#include <base/types.h>

#include "PlayerTypes.h"
#include "mediaplayerclient/MediaPlayerClient.h"
#include "log/log.h"

static constexpr char const *waylandDisplayHandleContextType =
	"GstWaylandDisplayHandleContextType";

namespace gmp { namespace player {
class Player {
 public:
  Player() {}
  virtual ~Player() {}

  virtual bool Load(const std::string &str) = 0;
  virtual bool Load(const MEDIA_LOAD_DATA_T* loadData) = 0;
  virtual bool Unload() = 0;
  virtual bool Play() = 0;
  virtual bool Pause() = 0;
  virtual bool SetPlayRate(const double rate) = 0;
  virtual bool Seek(const int64_t position) = 0;
  virtual bool SetVolume(int volume) = 0;
  virtual bool SetPlane(int planeId) = 0;
  virtual bool SetDisplayPath(const uint32_t display_path) = 0;

  virtual void notifyFunctionUMSPolicyAction() = 0;
  virtual bool UpdateVideoResData(const gmp::base::source_info_t &sourceInfo) = 0;
  virtual MEDIA_STATUS_T Feed(const guint8* pBuffer, guint32 bufferSize,
                        guint64 pts, MEDIA_DATA_CHANNEL_T esData) = 0;
  virtual bool Flush() = 0;
  virtual void RegisterCbFunction(CALLBACK_T) = 0;
  virtual bool PushEndOfStream() = 0;
  virtual GstElement* GetPipeline() = 0;
};

}  // namespace player
}  // namespace gmp
#endif  // SRC_PLAYER_PLAYER_H_
