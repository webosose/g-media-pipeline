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

#include <stdlib.h>
#include <unistd.h>
#include <UMSConnector.h>
#include <string>
#include "log/log.h"
#include "base/types.h"
#include "parser/parser.h"
#include "parser/composer.h"
#include "player/Player.h"
#include "mediaresource/requestor.h"
#include "service/service.h"

namespace gmp { namespace service {
Service *Service::instance_ = NULL;

Service::Service(const char *service_name)
  : umc_(NULL)
  , media_id_("")
  , player_(NULL) {
  umc_ = new UMSConnector(service_name, NULL, NULL, UMS_CONNECTOR_PRIVATE_BUS);
}

Service *Service::GetInstance(const char *service_name) {
  if (!instance_)
    instance_ = new Service(service_name);
  return instance_;
}

Service::~Service() {
  if (umc_)
    delete umc_;

  if (res_requestor_) {
    res_requestor_->releaseResource();
  }
}

void Service::Notify(const NOTIFY_TYPE_T notification) {
  switch (notification) {
    case NOTIFY_LOAD_COMPLETED: {
        gchar *message = g_strdup_printf("{\"loadCompleted\":{\"mediaId\":\"%s\"}}", media_id_.c_str());
        umc_->sendChangeNotificationJsonString(message);
        g_free(message);
        // TODO(someone) : check the need
        // res_requestor_->mediaContentReady(true);

        if (instance_->player_->reloading_) {
          gint64 start_time = instance_->player_->reload_seek_position_;
          instance_->player_->reloading_ = false;
          instance_->player_->reload_seek_position_ = 0;
          instance_->player_->Seek(start_time);
        }

        break;
    }

    case NOTIFY_UNLOAD_COMPLETED: {
        gchar *message = g_strdup_printf("{\"unloadCompleted\":{\"mediaId\":\"%s\"}}", media_id_.c_str());
        umc_->sendChangeNotificationJsonString(message);
        g_free(message);
        break;
    }

    case NOTIFY_END_OF_STREAM: {
        gchar *message = g_strdup_printf("{\"endOfStream\":{\"mediaId\":\"%s\"}}", media_id_.c_str());
        umc_->sendChangeNotificationJsonString(message);
        g_free(message);
        break;
    }

    case NOTIFY_SEEK_DONE: {
      gchar *message = g_strdup_printf("{\"seekDone\":{\"mediaId\":\"%s\"}}", media_id_.c_str());
      umc_->sendChangeNotificationJsonString(message);
      g_free(message);
      break;
    }

    case NOTIFY_PLAYING: {
      gchar *message = g_strdup_printf("{\"playing\":{\"mediaId\":\"%s\"}}", media_id_.c_str());
      umc_->sendChangeNotificationJsonString(message);
      g_free(message);
      break;
    }

    case NOTIFY_PAUSED: {
      gchar *message = g_strdup_printf("{\"paused\":{\"mediaId\":\"%s\"}}", media_id_.c_str());
      umc_->sendChangeNotificationJsonString(message);
      g_free(message);
      break;
    }

    case NOTIFY_BUFFERING_START: {
      gchar *message = g_strdup_printf("{\"bufferingStart\":{\"mediaId\":\"%s\"}}", media_id_.c_str());
      umc_->sendChangeNotificationJsonString(message);
      g_free(message);
      break;
    }

    case NOTIFY_BUFFERING_END: {
      gchar *message = g_strdup_printf("{\"bufferingEnd\":{\"mediaId\":\"%s\"}}", media_id_.c_str());
      umc_->sendChangeNotificationJsonString(message);
      g_free(message);
      break;
    }

    default: {
      GMP_DEBUG_PRINT("This notification can't be handled here!");
      break;
    }
  }
}

void Service::Notify(const NOTIFY_TYPE_T notification, const void *payload) {
  if (!payload)
    return;

  switch (notification) {
    case NOTIFY_CURRENT_TIME: {
        gmp::parser::Composer composer;
        gmp::base::time_t *current_position  = (gmp::base::time_t *)payload;
        composer.put("currentTime", *current_position);
        umc_->sendChangeNotificationJsonString(composer.result());
        break;
    }

    case NOTIFY_SOURCE_INFO: {
        gmp::parser::Composer composer;
        base::source_info_t *info  = (base::source_info_t *)payload;
        composer.put("sourceInfo", *info);
        umc_->sendChangeNotificationJsonString(composer.result());

        base::video_info_t videoInfo;
        memset(&videoInfo, 0, sizeof(base::video_info_t));
        videoInfo.width = info->video_streams.front().width;
        videoInfo.height = info->video_streams.front().height;
        videoInfo.frame_rate.num = info->video_streams.front().frame_rate.num;
        videoInfo.frame_rate.den = info->video_streams.front().frame_rate.den;

        res_requestor_->setVideoInfo(videoInfo);

        break;
    }

    case NOTIFY_VIDEO_INFO: {
        gmp::parser::Composer composer;
        base::video_info_t *info = (base::video_info_t*)payload;
        composer.put("videoInfo", *info);
        GMP_INFO_PRINT("%s : info->width[%d], info->height[%d]", __func__, info->width, info->height);
        umc_->sendChangeNotificationJsonString(composer.result());
        res_requestor_->setVideoInfo(*info);
        break;
    }

    case NOTIFY_AUDIO_INFO: {
        gmp::parser::Composer composer;
        base::audio_info_t *info = (base::audio_info_t*)payload;
        composer.put("audioInfo", *info);
        umc_->sendChangeNotificationJsonString(composer.result());
        break;
    }

    case NOTIFY_ERROR: {
        gmp::parser::Composer composer;
        base::error_t *error = (base::error_t *)payload;
        error->mediaId = media_id_;
        composer.put("error", *error);
        umc_->sendChangeNotificationJsonString(composer.result());
        break;
    }

    case NOTIFY_BUFFER_RANGE: {
        gmp::parser::Composer composer;
        base::buffer_range_t *range = (base::buffer_range_t *)payload;
        composer.put("bufferRange", *range);
        umc_->sendChangeNotificationJsonString(composer.result());
        break;
    }

    default: {
        GMP_DEBUG_PRINT("This notification can't be handled here!");
        break;
    }
  }
}

void Service::Initialize(gmp::player::Player *player) {
  if (!player || !umc_)
    return;

  player_ = player;
  player_->Initialize(this);

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

    // Resource Manager API
    {"registerPipeline", Service::RegisterPipelineEvent},
    {"unregisterPipeline", Service::UnregisterPipelineEvent},
    {"acquire", Service::AcquireEvent},
    {"tryAcquire", Service::TryAcquireEvent},
    {"release", Service::ReleaseEvent},
    {"notifyForeground", Service::NotifyForegroundEvent},
    {"notifyBackground", Service::NotifyBackgroundEvent},
    {"notifyActivity", Service::NotifyActivityEvent},
    {"trackAppProcesses", Service::TrackAppProcessesEvent},

    // pipeline state query API
    {"getPipelineState", Service::GetPipelineStateEvent},
    {"logPipelineState", Service::LogPipelineStateEvent},
    {"getActivePipelines", Service::GetActivePipelinesEvent},
    {"setPipelineDebugState", Service::SetPipelineDebugStateEvent},

    // exit
    {"exit", Service::ExitEvent},
    {NULL, NULL}};

  umc_->addEventHandlers(reinterpret_cast<UMSConnectorEventHandler *>(event_handlers));
}

bool Service::Wait() {
  return umc_->wait();
}

bool Service::Stop() {
  return umc_->stop();
}

bool Service::acquire(gmp::base::source_info_t &source_info, const int32_t display_path) {
  gmp::resource::PortResource_t resourceMMap;
  gmp::base::disp_res_t dispRes = {-1,-1,-1};

  res_requestor_->setSourceInfo(source_info);

  if (!res_requestor_->acquireResources(NULL, resourceMMap, dispRes, display_path)) {
    GMP_DEBUG_PRINT("resource acquisition failed");
    return false;
  }

  for (auto it : resourceMMap) {
    GMP_DEBUG_PRINT("Got Resource - name:%s, index:%d", it.first.c_str(), it.second);
  }

  if (dispRes.plane_id > 0 && dispRes.crtc_id > 0 && dispRes.conn_id > 0)
    //player_->SetPlane(dispRes.plane_id);
    player_->SetDisplayResource(dispRes);
  else {
    GMP_DEBUG_PRINT("ERROR : Failed to get displayResource(%d,%d,%d)", dispRes.plane_id, dispRes.crtc_id, dispRes.conn_id);
    return false;
  }
/*
  if (plane_id > 0)
    player_->SetPlane(plane_id);
*/
  GMP_DEBUG_PRINT("resource acquired!!!, plane_id: %d, crtc_id: %d, conn_id: %d", dispRes.plane_id, dispRes.crtc_id, dispRes.conn_id);
  return true;
}

// uMediaserver public API
bool Service::LoadEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
    bool ret = false;
    gmp::parser::Parser parser(instance_->umc_->getMessageText(message));
    instance_->media_id_ = parser.get<std::string>("id");
    gint64 start_time = parser.get_start_time();
    instance_->res_requestor_ = std::make_shared<gmp::resource::ResourceRequestor>("media", instance_->media_id_);
    // TODO(someone) : check the need
    // instance_->res_requestor_->notifyForeground();

    // TODO(jaehoon) : need to implement
    instance_->res_requestor_->registerUMSPolicyActionCallback([=] () {
            GMP_DEBUG_PRINT("registerUMSPolicyActionCallback");
            instance_->res_requestor_->notifyBackground();
            instance_->player_->Unload();
            });

/*
    instance_->res_requestor_->registerPlaneIdCallback([=] (int32_t planeId) -> bool {
            GMP_DEBUG_PRINT("registerPlaneIdCallback planeId:%d", planeId);
            instance_->player_->SetPlane(planeId);
            return true;
            });
*/
    std::string msg = instance_->umc_->getMessageText(message);
    GMP_DEBUG_PRINT("LoadEvent: [%s]", msg.c_str());

    ret = instance_->player_->Load(msg);


    if (ret && (start_time>0)) {
      GMP_DEBUG_PRINT("Reloading. Seek to [%lld]", start_time);
      instance_->player_->reloading_ = true;
      instance_->player_->reload_seek_position_ = start_time;
    }

    return ret;
}

bool Service::AttachEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
    return true;
}

bool Service::UnloadEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
    instance_->res_requestor_->notifyBackground();
    instance_->res_requestor_->releaseResource();
    return instance_->player_->Unload();
}

// media operations
bool Service::PlayEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  GMP_DEBUG_PRINT("PlayEvent");
  return instance_->player_->Play();
}

bool Service::PauseEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  GMP_DEBUG_PRINT("PauseEvent");
  return instance_->player_->Pause();
}

bool Service::SeekEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  GMP_DEBUG_PRINT("SeekEvent");
  int64_t position = std::stoll(instance_->umc_->getMessageText(message));
  return instance_->player_->Seek(position);
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
  return instance_->player_->SetPlayRate(parser.get<double>("playRate"));
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
  return instance_->player_->SetVolume(parser.get<int>("volume"));
}

bool Service::SetPlaneEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
/*
  GMP_DEBUG_PRINT("SetPlaneEvent");
  int planeID = -1;
  gmp::parser::Parser parser(instance_->umc_->getMessageText(message));
  planeID = parser.get<int>("planeID");
  GMP_DEBUG_PRINT("setPlaneEvent player:%p, planeId:%d", instance_->player_, planeID);
  return instance_->player_->SetPlane(planeID);
*/
  return true;
}

// Resource Manager API
bool Service::RegisterPipelineEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  return true;
}

bool Service::UnregisterPipelineEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  return true;
}

bool Service::AcquireEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  return true;
}

bool Service::TryAcquireEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  return true;
}

bool Service::ReleaseEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  return true;
}

bool Service::NotifyForegroundEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  return true;
}

bool Service::NotifyBackgroundEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  return true;
}

bool Service::NotifyActivityEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
  return true;
}

bool Service::TrackAppProcessesEvent(UMSConnectorHandle *handle, UMSConnectorMessage *message, void *ctxt) {
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
