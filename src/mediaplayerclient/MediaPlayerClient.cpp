// Copyright (c) 2018-2020 LG Electronics, Inc.
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

#include "MediaPlayerClient.h"
#include "Player.h"
#include "log.h"
#include "ElementFactory.h"
#include "requestor.h"
#include "PlayerFactory.h"
#include "parser/parser.h"

namespace gmp { namespace player {

// static
bool MediaPlayerClient::IsCodecSupported(GMP_VIDEO_CODEC videoCodec) {
  switch(videoCodec) {
    case GMP_VIDEO_CODEC_H264:
      return true;
    case GMP_VIDEO_CODEC_VP8:
      return false;
    case GMP_VIDEO_CODEC_VP9:
      return false;
    default:
      break;
  }
  return false;
}

MediaPlayerClient::MediaPlayerClient(const std::string& appId, const std::string& connectionId)
  : appId_(appId)
  , connectionId_(connectionId)
  , playerType_(GMP_PLAYER_TYPE_NONE) {
  GMP_DEBUG_PRINT("appId: %s, connectionId: %s", appId.c_str(), connectionId.c_str());

  if (appId.empty())
    GMP_DEBUG_PRINT("appId is empty! resourceRequestor is not created");
  else
    resourceRequestor_ = std::make_unique<gmp::resource::ResourceRequestor>(appId, connectionId);
}

MediaPlayerClient::~MediaPlayerClient() {
  GMP_DEBUG_PRINT("");

  if (resourceRequestor_)
    resourceRequestor_->setIsUnloading(true);

  if (isLoaded_) {
    GMP_DEBUG_PRINT("Unload() should be called if it is still loaded");
    Unload();
  }
}

bool MediaPlayerClient::AcquireResources(base::source_info_t &sourceInfo,
                                        const std::string &display_mode, uint32_t display_path) {
  GMP_DEBUG_PRINT("");
  gmp::resource::PortResource_t resourceMMap;
  gmp::base::disp_res_t dispRes = {-1,-1,-1};

  if (resourceRequestor_) {
    if (!resourceRequestor_->setSourceInfo(sourceInfo)) {
      GMP_DEBUG_PRINT("set source info failed");
      return false;
    }

    if (!resourceRequestor_->acquireResources(nullptr, resourceMMap, display_mode, dispRes, display_path)) {
      GMP_INFO_PRINT("resource acquisition failed");
      return false;
    }

    for (auto it : resourceMMap) {
      GMP_DEBUG_PRINT("Resource::[%s]=>index:%d", it.first.c_str(), it.second);
    }
  }

  if (!player_->UpdateVideoResData(sourceInfo)) {
    GMP_DEBUG_PRINT("ERROR: UpdateVideoResData");
    return false;
  }

  return true;
}

bool MediaPlayerClient::ReleaseResources() {
  GMP_DEBUG_PRINT("");
  if (!resourceRequestor_)
    return true;

  return resourceRequestor_->releaseResource();
}

void MediaPlayerClient::LoadCommon() {
  if (!NotifyForeground())
    GMP_DEBUG_PRINT("NotifyForeground fails");

  player_->RegisterCbFunction(
      std::bind(&MediaPlayerClient::NotifyFunction, this,
        std::placeholders::_1, std::placeholders::_2,
        std::placeholders::_3, std::placeholders::_4));

  if (resourceRequestor_) {
    resourceRequestor_->registerUMSPolicyActionCallback([this]() {
      base::error_t error;
      error.errorCode = GMP_ERROR_RES_ALLOC;
      error.errorText = "Resource Alloc Error";
      NotifyFunction(NOTIFY_ERROR, GMP_ERROR_RES_ALLOC, nullptr, static_cast<void*>(&error));
      if (!NotifyBackground())
        GMP_DEBUG_PRINT("NotifyBackground fails");
    });
  }
}

// Unmanaged case
bool MediaPlayerClient::Load(const MEDIA_LOAD_DATA_T* loadData) {
  GMP_DEBUG_PRINT("Load loadData = %p", loadData);
  playerType_ = GMP_PLAYER_TYPE_BUFFER;
  player_ =
      gmp::pf::PlayerFactory::CreatePlayer(loadData);

  if (!player_) {
    GMP_INFO_PRINT("Error: Player not created");
    return false;
  }

  LoadCommon();

  int32_t display_path = DEFAULT_DISPLAY;
  if (resourceRequestor_) {
    display_path = resourceRequestor_->getDisplayPath();
    if (display_path < DEFAULT_DISPLAY) {
      GMP_INFO_PRINT("Error: Failed to get displayPath");
      return false;
    }
  }

  player_->SetDisplayPath(display_path);

  if (player_->Load(loadData)) {
    GMP_DEBUG_PRINT("Loaded Player");
  } else {
    GMP_DEBUG_PRINT("Failed to load player");
    return false;
  }

  isLoaded_ = true;
  return true;
}

// Managed case
bool MediaPlayerClient::Load(const std::string &str) {
  GMP_DEBUG_PRINT("Load loadData = %s", str.c_str());
  player_ = gmp::pf::PlayerFactory::CreatePlayer(str, playerType_);

  if (!player_) {
    GMP_INFO_PRINT("Error: Player not created");
    return false;
  }

  LoadCommon();

  if (player_->Load(str)) {
    GMP_DEBUG_PRINT("Loaded Player");
  } else {
    GMP_DEBUG_PRINT("Failed to load player");
    return false;
  }

  isLoaded_ = true;
  return true;
}

bool MediaPlayerClient::Unload() {
  GMP_DEBUG_PRINT("START");

  if (!isLoaded_) {
    GMP_DEBUG_PRINT("already unloaded");
    return true;
  }

  if (!NotifyBackground())
    GMP_DEBUG_PRINT("NotifyBackground fails");

  if (!ReleaseResources())
    GMP_DEBUG_PRINT("ReleaseResources fails");

  if (!player_ || !player_->Unload())
    GMP_DEBUG_PRINT("fails to unload the player");

  isLoaded_ = false;
  if (playerType_ == GMP_PLAYER_TYPE_BUFFER && resourceRequestor_)
    resourceRequestor_->notifyPipelineStatus(uMediaServer::pipeline_state::UNLOADED);
  GMP_DEBUG_PRINT("END");

  return true;
}

bool MediaPlayerClient::Play() {
  GMP_DEBUG_PRINT("");
  if (!player_ || !isLoaded_) {
    GMP_INFO_PRINT("Invalid MediaPlayerClient state, player should be loaded");
    return false;
  }
  return player_->Play();
}

bool MediaPlayerClient::Pause() {
  GMP_DEBUG_PRINT("");
  if (!player_ || !isLoaded_) {
    GMP_INFO_PRINT("Invalid MediaPlayerClient state, player should be loaded");
    return false;
  }
  return player_->Pause();
}

bool MediaPlayerClient::Seek(int position) {
  GMP_DEBUG_PRINT("");
  if (!player_ || !isLoaded_) {
    GMP_INFO_PRINT("Invalid MediaPlayerClient state, player should be loaded");
    return false;
  }
  return player_->Seek(position);
}

bool MediaPlayerClient::SetPlane(int planeId) {
  GMP_DEBUG_PRINT("SetPlane is not Supported");
  return true;
}

MEDIA_STATUS_T MediaPlayerClient::Feed(const guint8* pBuffer,
                                             guint32 bufferSize,
                                             guint64 pts,
                                             MEDIA_DATA_CHANNEL_T esData) {
  if (!player_ || !isLoaded_) {
    GMP_INFO_PRINT("Invalid state, player(%p) should be loaded", player_.get());
    return MEDIA_NOT_READY;
  }
  return player_->Feed(pBuffer, bufferSize, pts, esData);
}

bool MediaPlayerClient::Flush() {
  GMP_DEBUG_PRINT("");
  if (!player_ || !isLoaded_) {
    GMP_INFO_PRINT("Invalid MediaPlayerClient state, player should be loaded");
    return false;
  }
  return player_->Flush();
}

bool MediaPlayerClient::SetDisplayWindow(const long left,
                                         const long top,
                                         const long width,
                                         const long height,
                                         const bool isFullScreen) {
  GMP_DEBUG_PRINT("Not supported. ignore this.");
  return true;
}

bool MediaPlayerClient::SetCustomDisplayWindow(const long srcLeft,
                                               const long srcTop,
                                               const long srcWidth,
                                               const long srcHeight,
                                               const long destLeft,
                                               const long destTop,
                                               const long destWidth,
                                               const long destHeight,
                                               const bool isFullScreen) {
  GMP_DEBUG_PRINT("Not supported. ignore this.");
  return true;
}

bool MediaPlayerClient::PushEndOfStream() {
  GMP_DEBUG_PRINT("");
  if (!player_) {
    GMP_INFO_PRINT("Invalid MediaPlayerClient state, player should be loaded");
    return false;
  }
  return player_->PushEndOfStream();
}

bool MediaPlayerClient::NotifyForeground() const {
  GMP_DEBUG_PRINT("");
  if (!resourceRequestor_)
    return true;

  return resourceRequestor_->notifyForeground();
}

bool MediaPlayerClient::NotifyBackground() const {
  GMP_DEBUG_PRINT("");
  if (!resourceRequestor_)
    return true;

  return resourceRequestor_->notifyBackground();
}

bool MediaPlayerClient::NotifyActivity() const {
  GMP_DEBUG_PRINT("");
  if (!resourceRequestor_)
    return true;

  return resourceRequestor_->notifyActivity();
}

bool MediaPlayerClient::SetVolume(int volume) {
  GMP_DEBUG_PRINT("");
  if (!player_ || !isLoaded_) {
    GMP_INFO_PRINT("Invalid MediaPlayerClient state, player should be loaded");
    return false;
  }
  return player_->SetVolume(volume);
}

bool MediaPlayerClient::SetExternalContext(GMainContext *context) {
  GMP_DEBUG_PRINT("context = %p", context);
  playerContext_ = context;
  return true;
}

bool MediaPlayerClient::SetPlaybackRate(const double playbackRate) {
  GMP_DEBUG_PRINT("playbackRate = %f", playbackRate);
  if (!player_ || !isLoaded_) {
    GMP_INFO_PRINT("Invalid MediaPlayerClient state, player should be loaded");
    return false;
  }
  return player_->SetPlayRate(playbackRate);
}

const char* MediaPlayerClient::GetMediaID() {
  GMP_DEBUG_PRINT("");
  if (!resourceRequestor_)
    return nullptr;

  return resourceRequestor_->getConnectionId().c_str();
}

void MediaPlayerClient::NotifyFunction(const gint cbType, const gint64 numValue,
  const gchar *strValue, void *udata) {
  GMP_DEBUG_PRINT("type:%d, numValue:%" G_GINT64_FORMAT ", strValue:%p, udata:%p",
                  cbType, numValue, strValue, udata);

  switch (cbType) {
    case NOTIFY_LOAD_COMPLETED: {
      if (playerType_ == GMP_PLAYER_TYPE_BUFFER && resourceRequestor_)
        resourceRequestor_->notifyPipelineStatus(uMediaServer::pipeline_state::LOADED);
      break;
    }
    case NOTIFY_PAUSED : {
      if (playerType_ == GMP_PLAYER_TYPE_BUFFER && resourceRequestor_)
        resourceRequestor_->notifyPipelineStatus(uMediaServer::pipeline_state::PAUSED);
      break;
    }
    case NOTIFY_PLAYING : {
      if (playerType_ == GMP_PLAYER_TYPE_BUFFER && resourceRequestor_)
        resourceRequestor_->notifyPipelineStatus(uMediaServer::pipeline_state::PLAYING);
      break;
    }
    case NOTIFY_ACTIVITY: {
      NotifyActivity();
      break;
    }
    case NOTIFY_ACQUIRE_RESOURCE: {
      ACQUIRE_RESOURCE_INFO_T* info = static_cast<ACQUIRE_RESOURCE_INFO_T*>(udata);
      if (pf::ElementFactory::GetPlatform().find("qemux86") == std::string::npos || 
          pf::ElementFactory::GetPlatform().find("qemux86-64") == std::string::npos)
        info->result = AcquireResources(*(info->sourceInfo), info->displayMode, numValue);
      else
        info->result = true;
      break;
    }
    default: {
      break;
    }
  }

  RunCallback(cbType, numValue, strValue, udata);
}

GstElement* MediaPlayerClient::GetPipeline()
{
  return player_ ? player_->GetPipeline() : NULL;
}

void MediaPlayerClient::RunCallback(const gint type, const gint64 numValue,
  const gchar *strValue, void *udata) {
  if (!userCallback_)
    return;

  if (userData_)
    userCallback_(type, numValue, strValue, userData_);
  else
    userCallback_(type, numValue, strValue, udata);
}
}  // End of namespace player

}  // End of namespace gmp
