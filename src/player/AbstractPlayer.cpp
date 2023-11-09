// Copyright (c) 2018-2021 LG Electronics, Inc.
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

#include "AbstractPlayer.h"
#include "ElementFactory.h"
#include <gst/pbutils/pbutils.h>
#include <pbnjson.hpp>

namespace gmp {
namespace player {

AbstractPlayer::AbstractPlayer() :
  lsClient_(std::make_unique<gmp::LunaServiceClient>()) {
  SetGstreamerDebug();
  SetUseAudio();
  gst_init(NULL, NULL);
  gst_pb_utils_init();
}

AbstractPlayer::~AbstractPlayer() {
}

bool AbstractPlayer::Load(const std::string &uri) {
  return true;
}

bool AbstractPlayer::Load(const MEDIA_LOAD_DATA_T* loadData) {
  return true;
}

bool AbstractPlayer::Unload() {
  GMP_DEBUG_PRINT("START");

  if (!pipeline_) {
    GMP_DEBUG_PRINT("pipeline is null");
    return false;
  }

  gst_element_set_state(pipeline_, GST_STATE_NULL);
  gst_object_unref(GST_OBJECT(pipeline_));
  pipeline_ = NULL;

  if (currPosTimerId_) {
    g_source_remove(currPosTimerId_);
    currPosTimerId_ = 0;
  }

  UnloadImpl();

  GMP_DEBUG_PRINT("END");
  return true;
}

bool AbstractPlayer::UnloadImpl() {
  return true;
}

bool AbstractPlayer::Play() {
  return true;
}

bool AbstractPlayer::Pause() {
  return true;
}

bool AbstractPlayer::SetPlayRate(const double rate) {
  return true;
}

bool AbstractPlayer::Seek(const int64_t msecond) {
  return true;
}

bool AbstractPlayer::SetVolume(int volume) {
  return false;
}

bool AbstractPlayer::SetPlane(int planeId) {
  GMP_DEBUG_PRINT("SetPlane: planeId(%d)", planeId);
  planeId_ = planeId;
  return true;
}

bool AbstractPlayer::SetDisplayPath(const uint32_t display_path) {
  GMP_DEBUG_PRINT("display_path: %u", display_path);
  display_path_ = (display_path > SECONDARY_DISPLAY) ? DEFAULT_DISPLAY : display_path;
  return true;
}

void AbstractPlayer::notifyFunctionUMSPolicyAction() {
}

bool AbstractPlayer::UpdateVideoResData(
    const gmp::base::source_info_t &sourceInfo) {
  return true;
}

void AbstractPlayer::RegisterCbFunction(CALLBACK_T callBackFunction) {
  cbFunction_ = std::move(callBackFunction);
}

bool AbstractPlayer::PushEndOfStream() {
  return true;
}

bool AbstractPlayer::Flush() {
  return true;
}

MEDIA_STATUS_T AbstractPlayer::Feed(const guint8* pBuffer,
    guint32 bufferSize, guint64 pts, MEDIA_DATA_CHANNEL_T esData) {
  return MEDIA_NOT_IMPLEMENTED;
}

void AbstractPlayer::SetGstreamerDebug() {
  pbnjson::JValue parsed
    = pbnjson::JDomParser::fromFile("/etc/g-media-pipeline/gst_debug.conf");

  if (!parsed.isObject()) {
    GMP_DEBUG_PRINT("Debug file parsing error. Please check gst_debug.conf");
    GMPASSERT(0);
  }

  pbnjson::JValue debug = parsed["gst_debug"];
  for (int i = 0; i < debug.arraySize(); ++i) {
    for(auto it : debug[i].children()) {
      if (it.first.isString() && it.second.isString()
          && !it.second.asString().empty()) {
        setenv(it.first.asString().c_str(),
            it.second.asString().c_str(), 1);
      }
    }
  }
}

void AbstractPlayer::SetUseAudio() {
  use_audio = gmp::pf::ElementFactory::GetUseAudioProperty();
}


void AbstractPlayer::SetReloading(const gint64 &start_time) {
  if (start_time > 0) {
    GMP_DEBUG_PRINT("Reloading. Seek to %" G_GINT64_FORMAT, start_time);
    reloading_ = true;
    reload_seek_position_ = start_time;
  }
}

void AbstractPlayer::DoReloading() {
  if (reloading_) {
    reloading_ = false;
    this->Seek(reload_seek_position_);
    reload_seek_position_ = 0;
  }
}

bool AbstractPlayer::attachSurface(bool allow_no_window) {
  if (!window_id_.empty()) {
    if (!lsm_connector_.registerID(window_id_.c_str(), NULL)) {
      GMP_DEBUG_PRINT("register id to LSM failed!");
      return false;
    }
    if (!lsm_connector_.attachSurface()) {
      GMP_DEBUG_PRINT("attach surface to LSM failed!");
      return false;
    }
    return true;
  } else {
    GMP_DEBUG_PRINT("window id is empty!");
    bool ret = allow_no_window ? true : false;
    return ret;
  }
}

bool AbstractPlayer::detachSurface() {
  if (!window_id_.empty()) {
    if (!lsm_connector_.detachSurface()) {
      GMP_DEBUG_PRINT("detach surface to LSM failed!");
      return false;
    }
    if (!lsm_connector_.unregisterID()) {
      GMP_DEBUG_PRINT("unregister id to LSM failed!");
      return false;
    }
  } else {
    GMP_DEBUG_PRINT("window id is empty!");
  }
  return true;
}

GstElement* AbstractPlayer::GetPipeline() {
  return pipeline_;
}

}  // namespace player
}  // namespace gmp
