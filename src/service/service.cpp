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

#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <UMSConnector.h>
#include "log/log.h"
#include "base/types.h"
#include "base/message.h"
#include "parser/parser.h"
#include "parser/composer.h"
#include "player/Player.h"
#include "mediaresource/requestor.h"
#include "service/service.h"
#include "playerfactory/PlayerFactory.h"
#include <memory>

namespace gmp { namespace service {
Service *Service::instance_ = nullptr;

Service::Service(const std::string& service_name) {
  umc_ = std::make_unique<UMSConnector>(service_name, nullptr, nullptr, UMS_CONNECTOR_PRIVATE_BUS);

  static UMSConnectorEventHandler event_handlers[] = {
    // uMediaserver public API
    {"load", Service::LoadEvent},
    {"attach", Service::AttachEvent},
    {"unload", Service::UnloadEvent},

    // media operations
    {"play", Service::PlayEvent},
    {"pause", Service::PauseEvent},
    {"seek", Service::SeekEvent},
    {"stateChange", Service::StateChangeEvent},
    {"unsubscribe", Service::UnsubscribeEvent},
    {"setUri", Service::SetUriEvent},
    {"setPlayRate", Service::SetPlayRateEvent},
    {"selectTrack", Service::SelectTrackEvent},
    {"setUpdateInterval", Service::SetUpdateIntervalEvent},
    {"setUpdateIntervalKV", Service::SetUpdateIntervalKVEvent},
    {"changeResolution", Service::ChangeResolutionEvent},
    {"setStreamQuality", Service::SetStreamQualityEvent},
    {"setProperty", Service::SetPropertyEvent},
    {"setVolume", Service::SetVolumeEvent},
    {"setPlane", Service::SetPlaneEvent},

    // pipeline state query API
    {"getPipelineState", Service::GetPipelineStateEvent},
    {"logPipelineState", Service::LogPipelineStateEvent},
    {"getActivePipelines", Service::GetActivePipelinesEvent},
    {"setPipelineDebugState", Service::SetPipelineDebugStateEvent},

    // exit
    {"exit", Service::ExitEvent},
    {nullptr, nullptr}
  };

  umc_->addEventHandlers(reinterpret_cast<UMSConnectorEventHandler *>(event_handlers));
}

Service* Service::GetInstance(const std::string& service_name) {
  if (!instance_)
    instance_ = new Service(service_name);
  return instance_;
}

Service::~Service() {}

void Service::Notify(const gint notification, const gint64 numValue, const gchar *strValue, void *payload) {
  gmp::parser::Composer composer;
  gmp::base::media_info_t mediaInfo = { media_id_ };
  switch (notification) {
    case NOTIFY_CURRENT_TIME: {
      gmp::base::time_t current_position  = *static_cast<gmp::base::time_t *>(payload);
      composer.put("currentTime", current_position);
      break;
    }
    case NOTIFY_SOURCE_INFO: {
      base::source_info_t info  = *static_cast<base::source_info_t *>(payload);
      composer.put("sourceInfo", info);
      break;
    }
    case NOTIFY_VIDEO_INFO: {
      base::video_info_t info = *static_cast<base::video_info_t*>(payload);
      composer.put("videoInfo", info);
      GMP_INFO_PRINT("videoInfo: width %d, height %d", info.width, info.height);
      break;
    }
    case NOTIFY_AUDIO_INFO: {
      base::audio_info_t info = *static_cast<base::audio_info_t*>(payload);
      composer.put("audioInfo", info);
      break;
    }
    case NOTIFY_ERROR: {
      base::error_t error = *static_cast<base::error_t *>(payload);
      error.mediaId = media_id_;
      composer.put("error", error);

      if (numValue == GMP_ERROR_RES_ALLOC) {
        GMP_DEBUG_PRINT("policy action occured!");
        // TODO(nakyeon) : Check unload event from ums is coming after this notification
        this->media_player_client_.reset();
      }
      break;
    }
    case NOTIFY_BUFFER_RANGE: {
      base::buffer_range_t range = *static_cast<base::buffer_range_t *>(payload);
      composer.put("bufferRange", range);
      break;
    }
    case NOTIFY_LOAD_COMPLETED: {
      composer.put("loadCompleted", mediaInfo);
      break;
    }
    case NOTIFY_UNLOAD_COMPLETED: {
      composer.put("unloadCompleted", mediaInfo);
      break;
    }
    case NOTIFY_END_OF_STREAM: {
      composer.put("endOfStream", mediaInfo);
      break;
    }
    case NOTIFY_SEEK_DONE: {
      composer.put("seekDone", mediaInfo);
      break;
    }
    case NOTIFY_PLAYING: {
      composer.put("playing", mediaInfo);
      break;
    }
    case NOTIFY_PAUSED: {
      composer.put("paused", mediaInfo);
      break;
    }
    case NOTIFY_BUFFERING_START: {
      composer.put("bufferingStart", mediaInfo);
      break;
    }
    case NOTIFY_BUFFERING_END: {
      composer.put("bufferingEnd", mediaInfo);
      break;
    }
    default: {
      GMP_DEBUG_PRINT("This notification(%d) can't be handled here!", notification);
      break;
    }
  }
  umc_->sendChangeNotificationJsonString(composer.result());
}

bool Service::Wait() {
  return umc_->wait();
}

bool Service::Stop() {
  return umc_->stop();
}

// uMediaserver public API
bool Service::LoadEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  std::string msg = instance_->umc_->getMessageText(message);
  GMP_DEBUG_PRINT("%s", msg.c_str());

  pbnjson::JDomParser parser;
  if (!parser.parse(msg, pbnjson::JSchema::AllSchema())) {
      GMP_DEBUG_PRINT("ERROR JDomParser.parse. msg=%s", msg.c_str());
      return false;
  }
  pbnjson::JValue parsed = parser.getDom();
  if (!parsed.hasKey("id") && parsed["id"].isString()) {
    GMP_DEBUG_PRINT("id is invalid");
    return false;
  }
  instance_->media_id_ = parsed["id"].asString();
  instance_->app_id_ = parsed["options"]["option"]["appId"].asString();

  instance_->media_player_client_ =
    std::make_unique<gmp::player::MediaPlayerClient>(instance_->app_id_, instance_->media_id_);

  instance_->media_player_client_->RegisterCallback(
    std::bind(&Service::Notify, instance_,
      std::placeholders::_1, std::placeholders::_2,
      std::placeholders::_3, std::placeholders::_4));

  bool ret;
  ret = instance_->media_player_client_->Load(msg);
  if (!ret) {
    base::error_t error;
    error.errorCode = MEDIA_MSG_ERR_LOAD;
    error.errorText = "Load Failed";
    instance_->Notify(NOTIFY_ERROR, 0, nullptr, static_cast<void*>(&error));
  }
  return ret;
}

bool Service::AttachEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
    return true;
}

bool Service::UnloadEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  bool ret;
  ret = instance_->media_player_client_->Unload();
  if (!ret) {
    base::error_t error;
    error.errorCode = MEDIA_MSG_ERR_LOAD;
    error.errorText = "Unload Failed";
    instance_->Notify(NOTIFY_ERROR, 0, nullptr, static_cast<void*>(&error));
  }
  return ret;
}

// media operations
bool Service::PlayEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  GMP_DEBUG_PRINT("PlayEvent");
  return instance_->media_player_client_->Play();
}

bool Service::PauseEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  GMP_DEBUG_PRINT("PauseEvent");
  return instance_->media_player_client_->Pause();
}

bool Service::SeekEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  GMP_DEBUG_PRINT("SeekEvent");
  int64_t position = std::stoll(instance_->umc_->getMessageText(message));
  return instance_->media_player_client_->Seek(position);
}

bool Service::StateChangeEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  return instance_->umc_->addSubscriber(handle, message);
}

bool Service::UnsubscribeEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  return true;
}

bool Service::SetUriEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  return true;
}

bool Service::SetPlayRateEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  GMP_DEBUG_PRINT("SetPlayRateEvent");
  gmp::parser::Parser parser(instance_->umc_->getMessageText(message));
  return instance_->media_player_client_->SetPlaybackRate(parser.get<double>("playRate"));
}

bool Service::SelectTrackEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  return true;
}

bool Service::SetUpdateIntervalEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  return true;
}

bool Service::SetUpdateIntervalKVEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  return true;
}

bool Service::ChangeResolutionEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  return true;
}

bool Service::SetStreamQualityEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  return true;
}

bool Service::SetPropertyEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  return true;
}

bool Service::SetVolumeEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  GMP_DEBUG_PRINT("SetVolumeEvent");
  gmp::parser::Parser parser(instance_->umc_->getMessageText(message));
  return instance_->media_player_client_->SetVolume(parser.get<int>("volume"));
}

bool Service::SetPlaneEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  return true;
}

// pipeline state query API
bool Service::GetPipelineStateEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  return true;
}

bool Service::LogPipelineStateEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  return true;
}

bool Service::GetActivePipelinesEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  return true;
}

bool Service::SetPipelineDebugStateEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  return true;
}

// exit
bool Service::ExitEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  return instance_->umc_->stop();
}
}  // namespace service
}  // namespace gmp
