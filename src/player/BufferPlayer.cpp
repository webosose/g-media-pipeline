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
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0


#include "BufferPlayer.h"

#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <cmath>
#include <cstring>
#include <sstream>
#include <map>
#include <gio/gio.h>

#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>
#include <gst/base/gstbasesrc.h>
#include <log/log.h>
#include <memory>

#include "base/message.h"
#include "parser/composer.h"
#include "util/util.h"
#include "dsi/DSIGeneratorFactory.h"
#include "ElementFactory.h"

#define CURR_TIME_INTERVAL_MS    500
#define LOAD_DONE_TIMEOUT_MS     10

#define MEDIA_VIDEO_MAX      (15 * 1024 * 1024)  // 15MB
#define MEDIA_AUDIO_MAX      (4 * 1024 * 1024)   // 4MB
#define QUEUE_MAX_SIZE       (12 * 1024 * 1024)  // 12MB
#define QUEUE_MAX_TIME       (10 * GST_SECOND)   // 10Secs

#define BUFFER_MIN_PERCENT 50
#define MEDIA_CHANNEL_MAX  2

namespace {

bool IsElementName(const GstElement *element, const char *name) {
  gchar *elementName = gst_element_get_name(element);
  int ret = strcmp(name, elementName);
  g_free(elementName);
  return !ret;
}

static void SetQueueBufferSize(GstElement *pElement,
                               guint maxBufferSize,
                               guint maxBuffer) {
  guint currentSize = 0;
  g_object_get(G_OBJECT(pElement), "max-size-bytes", &currentSize, NULL);

  if (currentSize != maxBufferSize) {
    g_object_set(G_OBJECT(pElement), "max-size-bytes", maxBufferSize,
                                     "max-size-time", QUEUE_MAX_TIME,
                                     "max-size-buffers", maxBuffer,
                 NULL);
  }
}

}  // namespace

namespace gmp {
namespace player {

BufferPlayer::BufferPlayer() {
  GMP_INFO_PRINT("START");

  SetDebugDumpFileName();

  segment_.flags = GST_SEGMENT_FLAG_NONE;
  segment_.rate = 0;
  segment_.applied_rate = 0;
  segment_.format = GST_FORMAT_UNDEFINED;
  segment_.base = 0;
  segment_.offset = 0;
  segment_.start = 0;
  segment_.stop = 0;
  segment_.time = 0;
  segment_.position = 0;
  segment_.duration = 0;

  GMP_INFO_PRINT("END");
}

BufferPlayer::~BufferPlayer() {
  Unload();
}

bool BufferPlayer::UnloadImpl() {
  GMP_INFO_PRINT("START");

  audioSrcInfo_.reset();
  videoSrcInfo_.reset();

  loadData_.reset();

  DisconnectBusCallback();

  if (!detachSurface()) {
    GMP_DEBUG_PRINT("detachSurface() failed");
    return false;
  }

  GMP_INFO_PRINT("END");
  return true;
}

bool BufferPlayer::Play() {
  std::lock_guard<std::recursive_mutex> lock(recursive_mutex_);
  GMP_INFO_PRINT("Start Play");

  if (!pipeline_) {
    GMP_DEBUG_PRINT("pipeline handle is NULL");
    return false;
  }

  if (shouldSetNewBaseTime_) {
    gint64 position = 0;
    gst_element_query_position(pipeline_, GST_FORMAT_TIME, &position);
    GMP_INFO_PRINT("position = %" PRId64, position);

    segment_.start = position;
    segment_.time = position;
    segment_.position = position;

    if (videoSink_) {
      GstEvent *event;
      GstStructure *s;

      s = gst_structure_new ("prepare-seamless-seek",
          "segment-info", GST_TYPE_SEGMENT, &segment_,
          "reset-start-time", G_TYPE_BOOLEAN, TRUE,
          "update-base-time", G_TYPE_BOOLEAN, FALSE,
          "new-base-time", GST_TYPE_CLOCK_TIME, GST_CLOCK_TIME_NONE,
          "propagate", G_TYPE_BOOLEAN, TRUE, NULL);
      event = gst_event_new_custom (GST_EVENT_CUSTOM_UPSTREAM, s);
      GMP_INFO_PRINT("Sending prepare-seamless-seek event to videosink");
      gst_element_send_event (videoSink_, event);
    }
    if (audioSink_) {
      GstEvent *event;
      GstStructure *s;

      s = gst_structure_new ("prepare-seamless-seek",
          "segment-info", GST_TYPE_SEGMENT, &segment_,
          "reset-start-time", G_TYPE_BOOLEAN, TRUE,
          "update-base-time", G_TYPE_BOOLEAN, FALSE,
          "new-base-time", GST_TYPE_CLOCK_TIME, GST_CLOCK_TIME_NONE,
          "propagate", G_TYPE_BOOLEAN, TRUE, NULL);
      event = gst_event_new_custom (GST_EVENT_CUSTOM_UPSTREAM, s);
      GMP_INFO_PRINT("Sending prepare-seamless-seek event to audiosink");
      gst_element_send_event (audioSink_, event);
    }
    shouldSetNewBaseTime_ = false;
  }
  if (load_complete_) {
    feedPossible_ = true;

    GMP_INFO_PRINT("currentState_ [ %d ]", currentState_);
    if (currentState_== PLAYING_STATE)
      return true;

    currentState_ = PLAYING_STATE;
    GstStateChangeReturn retState = gst_element_set_state(pipeline_,
                                                        GST_STATE_PLAYING);
    if (retState == GST_STATE_CHANGE_FAILURE) {
      currentState_ = STOPPED_STATE;
    }

    if (cbFunction_) {
      cbFunction_(NOTIFY_ACTIVITY, 0, nullptr, nullptr);
    }
  }

  return true;
}

bool BufferPlayer::Pause() {
  GMP_INFO_PRINT("Pipeline Pause");

  if (!pipeline_) {
    GMP_DEBUG_PRINT("pipeline handle is NULL");
    return false;
  }

  if (load_complete_) {
    GMP_INFO_PRINT("currentState_ [ %d ]", currentState_);
    if (currentState_ == PAUSING_STATE || currentState_ == PAUSED_STATE)
      return true;

    if (!PauseInternal())
      return false;

    if (cbFunction_) {
      cbFunction_(NOTIFY_ACTIVITY, 0, nullptr, nullptr);
    }

  }

  return true;
}

bool BufferPlayer::SetPlayRate(const double rate) {
  GMP_INFO_PRINT("SetPlayRate: %f", rate);
  std::lock_guard<std::recursive_mutex> lock(recursive_mutex_);
  if (!pipeline_) {
    GMP_DEBUG_PRINT("pipeline handle is NULL");
    return true;
  }

  if (!load_complete_) {
    GMP_DEBUG_PRINT("load_complete_ not ready. SetPlayRate false.");
    return false;
  }

  if (util::Util::CompareDouble(rate, play_rate_)) {
    GMP_DEBUG_PRINT("playRate setting not needed (same)");
    return true;
  }

  play_rate_ = rate;

  gint64 position = 0;
  gst_element_query_position(pipeline_, GST_FORMAT_TIME, &position);
  GMP_INFO_PRINT("rate: %f, position: %" PRId64 "duration: %" PRId64,
                   rate, position, duration_);

  gint64 currentPlayPosition = 0;
  GstState curState = GST_STATE_NULL;
  GstClockTime newBaseTime = GST_CLOCK_TIME_NONE;
  gboolean reset_start_time = FALSE;
  gboolean update_base_time = FALSE;
  gboolean propagate = FALSE;

  if (gst_element_get_state(pipeline_, &curState, NULL, 0) == GST_STATE_CHANGE_FAILURE) {
    GMP_INFO_PRINT("fail to get pipeline state");
  }
  GMP_INFO_PRINT("Current state : %d", curState);

  segment_.start = position;
  segment_.time = position;
  segment_.position = position;
  segment_.applied_rate = segment_.rate;
  segment_.rate = rate;

  if (curState == GST_STATE_PLAYING) {
#ifdef PLATFORM_QEMUX86
    newBaseTime = gst_element_get_base_time (pipeline_);
#else
    newBaseTime = gst_pipeline_get_base_time ((GstPipeline *) pipeline_, /*start-time*/0);
#endif
    reset_start_time = TRUE;
    update_base_time = TRUE;
    propagate = TRUE;
  } else {
    newBaseTime = GST_CLOCK_TIME_NONE;
    reset_start_time = TRUE;
    update_base_time = FALSE;
    propagate = TRUE;
  }

  if (videoSink_) {
    GstEvent *event;
    GstStructure *s;
    s = gst_structure_new ("prepare-seamless-seek",
        "segment-info", GST_TYPE_SEGMENT, &segment_,
        "reset-start-time", G_TYPE_BOOLEAN, reset_start_time,
        "update-base-time", G_TYPE_BOOLEAN, update_base_time,
        "new-base-time", GST_TYPE_CLOCK_TIME, newBaseTime,
        "propagate", G_TYPE_BOOLEAN, propagate, NULL);

    event = gst_event_new_custom (GST_EVENT_CUSTOM_UPSTREAM, s);
    GMP_INFO_PRINT("Sending prepare-seamless-seek event to videosink : %p", pipeline_);

    gst_element_send_event (videoSink_, event);
  }

  if (audioSink_) {
    GstEvent *event;
    GstStructure *s;

    s = gst_structure_new ("prepare-seamless-seek",
        "segment-info", GST_TYPE_SEGMENT, &segment_,
        "reset-start-time", G_TYPE_BOOLEAN, reset_start_time,
        "update-base-time", G_TYPE_BOOLEAN, update_base_time,
        "new-base-time", GST_TYPE_CLOCK_TIME, newBaseTime,
        "propagate", G_TYPE_BOOLEAN, propagate, NULL);

    event = gst_event_new_custom (GST_EVENT_CUSTOM_UPSTREAM, s);
    GMP_INFO_PRINT("Sending prepare-seamless-seek event to audiosink : %p", pipeline_);
    gst_element_send_event (audioSink_, event);
  }
  shouldSetNewBaseTime_ = true;
  return true;
}

bool BufferPlayer::Seek(const int64_t msecond) {
  std::lock_guard<std::recursive_mutex> lock(recursive_mutex_);
  GMP_INFO_PRINT("seek: %" PRId64, msecond);

  if (!pipeline_) {
    GMP_DEBUG_PRINT("pipeline handle is NULL");
    return false;
  }

  if (!load_complete_ ||currentState_ == STOPPED_STATE) {
    GMP_DEBUG_PRINT("not paused or playing state");
    return false;
  }

  if (!SeekInternal(msecond)) {
    GMP_DEBUG_PRINT("fail gstreamer seek");
    return false;
  }
  return true;
}

bool BufferPlayer::SetVolume(int volume) {
  GMP_INFO_PRINT("SetVolume : volume = %d sessionId = %d", volume, display_path_);

  pbnjson::JValue jsonValue = pbnjson::Object();
  jsonValue.put("volume", pbnjson::JValue(volume));
  jsonValue.put("sessionId", pbnjson::JValue((int)display_path_));
  std::string jsonStr = jsonValue.stringify();

  const std::string uri = "luna://com.webos.service.audio/media/setVolume";

  ResponseHandler nullcb = [] (const char *){};
  bool ret = (lsClient_) ? lsClient_->CallAsync(uri.c_str(), jsonStr.c_str(),nullcb)
                   : false;

  return ret;
}

bool BufferPlayer::Load(const MEDIA_LOAD_DATA_T* loadData) {
  GMP_INFO_PRINT("loadData(%p)", loadData);

  if (!UpdateLoadData(loadData))
    return false;

  window_id_ = loadData->windowId ? loadData->windowId : "";
  GMP_INFO_PRINT("window_id_(%s)", window_id_.c_str());
  display_mode_ = "Textured";

  source_info_ = GetSourceInfo(loadData);

  ACQUIRE_RESOURCE_INFO_T resource_info;
  resource_info.sourceInfo = &source_info_;
  resource_info.displayMode = const_cast<char*>(display_mode_.c_str());
  resource_info.result = false;

  if (cbFunction_)
    cbFunction_(NOTIFY_ACQUIRE_RESOURCE, display_path_, nullptr,
                static_cast<void*>(&resource_info));

  if (!resource_info.result) {
    GMP_DEBUG_PRINT("resouce acquire fail!");
    return false;
  }

  if (!attachSurface(loadData_->videoCodec == GMP_VIDEO_CODEC_NONE)) {
    GMP_DEBUG_PRINT("attachSurface() failed");
    return false;
  }

  if (!CreatePipeline()) {
    GMP_DEBUG_PRINT("CreatePipeline Failed");
    return false;
  }
  gst_segment_init(&segment_, GST_FORMAT_TIME);

  if (!PauseInternal()) {
    GMP_INFO_PRINT("Failed to pause !!!");
    return false;
  }

  currentState_ = LOADING_STATE;

  if (loadData_->liveStream) {
    // appsrc element can't produce data in GST_STATE_PAUSED in live case.
    gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    currentState_ = PLAYING_STATE;
    if (cbFunction_)
      cbFunction_(NOTIFY_PLAYING, 0, nullptr, nullptr);
  }

  GMP_INFO_PRINT("END currentState_ = %d", currentState_);

  if (loadData_->ptsToDecode > 0) {
    // By calling seek with ptsToDecode, we handle ReInitialize in seek.
    GMP_DEBUG_PRINT("Seek to (%" G_GINT64_FORMAT ") in Load",
      loadData_->ptsToDecode);
    if (!gst_element_seek(pipeline_, 1.0, GST_FORMAT_TIME,
                          GstSeekFlags(GST_SEEK_FLAG_FLUSH |
                                       GST_SEEK_FLAG_KEY_UNIT),
                          GST_SEEK_TYPE_SET, loadData_->ptsToDecode,
                          GST_SEEK_TYPE_NONE, 0)) {
      GMP_INFO_PRINT("pipeline seek failed");
      return false;
    }
  }
  currPosTimerId_ = g_timeout_add(CURR_TIME_INTERVAL_MS,
                                  (GSourceFunc)NotifyCurrentTime, this);

  feedPossible_ = true;
  return true;
}

bool BufferPlayer::PushEndOfStream() {
  GMP_DEBUG_PRINT("PushEndOfStream");

  if (loadData_->liveStream) {
    GMP_INFO_PRINT("liveStream . So Skip sending EOS to appsrc, \
                   because we may replay media again.");
    return true;
  }

  recEndOfStream_ = true;

  if (audioSrcInfo_ && audioSrcInfo_->pSrcElement) {
    if (GST_FLOW_OK != gst_app_src_end_of_stream(
            GST_APP_SRC(audioSrcInfo_->pSrcElement)))
      return false;
  }

  if (videoSrcInfo_ && videoSrcInfo_->pSrcElement) {
    if (GST_FLOW_OK != gst_app_src_end_of_stream(
            GST_APP_SRC(videoSrcInfo_->pSrcElement)))
      return false;
  }

  return true;
}

bool BufferPlayer::Flush() {
  GMP_DEBUG_PRINT("Flush");

  // check pipeline handle
  if (!pipeline_) {
    GMP_DEBUG_PRINT("Pipeline is null");
    return false;
  }

  if (videoSrcInfo_) {
    videoSrcInfo_->totalFeed = 0;
    videoSrcInfo_->needFeedData = CUSTOM_BUFFER_LOCKED;
  }

  if (audioSrcInfo_) {
    audioSrcInfo_->totalFeed = 0;
    audioSrcInfo_->needFeedData = CUSTOM_BUFFER_LOCKED;
  }

  recEndOfStream_ = false;

  // flush pipeline
  if (!gst_element_seek(pipeline_, (gdouble)(1.0),
                        GST_FORMAT_TIME, GstSeekFlags(GST_SEEK_FLAG_FLUSH |
                                                      GST_SEEK_FLAG_SKIP),
                        GST_SEEK_TYPE_SET, GST_CLOCK_TIME_NONE,
                        GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
    GMP_DEBUG_PRINT("Pipeline Flush ERROR!!!");
    return false;
  }

  return true;
}

MEDIA_STATUS_T BufferPlayer::Feed(const guint8* pBuffer,
    guint32 bufferSize, guint64 pts, MEDIA_DATA_CHANNEL_T esData) {

  if (!pipeline_) {
    GMP_DEBUG_PRINT("Pipeline is null");
    return MEDIA_ERROR;
  }

  if (!feedPossible_) {
    GMP_INFO_PRINT("Pipeline not ready for feed !!!");
    return MEDIA_ERROR;
  }

  MEDIA_SRC_T* pAppSrcInfo = nullptr;
  if (esData == MEDIA_DATA_CH_A) {
    pAppSrcInfo = videoSrcInfo_.get();
  } else if (esData == MEDIA_DATA_CH_B) {
    pAppSrcInfo = audioSrcInfo_.get();
  } else {
    GMP_INFO_PRINT("Wrong media channel type !!!");
    return MEDIA_ERROR;
  }

  if (!pAppSrcInfo) {
    GMP_INFO_PRINT("App SRC not found !!!");
    return MEDIA_ERROR;
  }

  if (!pAppSrcInfo->pSrcElement || bufferSize == 0) {
    GMP_INFO_PRINT("Possible bufferSize (%d) is Zeror!!!", bufferSize);
    return MEDIA_ERROR;
  }

  if (!IsFeedPossible(pAppSrcInfo, bufferSize)) {
     GMP_INFO_PRINT("Feed is not Possible!!!");
     return MEDIA_BUFFER_FULL;
  }

  if (recEndOfStream_) {
    GMP_INFO_PRINT("Already EOS received !!!");
    return MEDIA_ERROR;
  }

  guint8 *feedBuffer = (guint8 *)g_malloc(bufferSize);
  if (feedBuffer == NULL) {
    GMP_DEBUG_PRINT("memory allocation error!!!!!");
    return MEDIA_ERROR;
  }

  memcpy(feedBuffer, pBuffer, bufferSize);

  GstBuffer *appSrcBuffer = gst_buffer_new_wrapped(feedBuffer, bufferSize);
  if (!appSrcBuffer) {
    g_free(feedBuffer);
    GMP_DEBUG_PRINT("can't get app src buffer");
    return MEDIA_ERROR;
  }

  if (esData != MEDIA_DATA_CH_NONE) {// raw data
    GST_BUFFER_TIMESTAMP(appSrcBuffer) = pts;
  }

  GstFlowReturn gstReturn = gst_app_src_push_buffer(
      GST_APP_SRC(pAppSrcInfo->pSrcElement), appSrcBuffer);
  if (gstReturn < GST_FLOW_OK) {
    GMP_INFO_PRINT("gst_app_src_push_buffer errCode[ %d ]", gstReturn);
    return MEDIA_ERROR;
  }

  pAppSrcInfo->totalFeed += bufferSize;  // audio or video

  return MEDIA_OK;
}

std::string StreamStatusName(int streamType) {
  const char* streamStatusName[] = {
    "GST_STREAM_STATUS_TYPE_CREATE",
    "GST_STREAM_STATUS_TYPE_ENTER",
    "GST_STREAM_STATUS_TYPE_LEAVE",
    "GST_STREAM_STATUS_TYPE_DESTROY",
    "GST_STREAM_STATUS_TYPE_START",
    "GST_STREAM_STATUS_TYPE_PAUSE",
    "GST_STREAM_STATUS_TYPE_STOP",
  };
  return std::string(streamStatusName[streamType]);
}

gboolean BufferPlayer::HandleBusMessage(
    GstBus *bus, GstMessage *message, gpointer user_data) {
  GstMessageType messageType = GST_MESSAGE_TYPE(message);
  if (messageType != GST_MESSAGE_QOS && messageType != GST_MESSAGE_TAG) {
    GMP_INFO_PRINT("Element[ %s ][ %d ][ %s ]", GST_MESSAGE_SRC_NAME(message),
                    messageType, gst_message_type_get_name(messageType));
  }

  BufferPlayer *player = static_cast<BufferPlayer*>(user_data);
  if (!player)
    return true;

  switch (messageType) {
    case GST_MESSAGE_ERROR: {
      GError *pError = NULL;
      gchar *pDebug = NULL;
      gst_message_parse_error(message, &pError, &pDebug);
      GMP_INFO_PRINT(" GST_MESSAGE_ERROR : %s pDebug ( %s )",
                     pError->message, pDebug ? pDebug : "null");
      g_error_free(pError);
      g_free(pDebug);
      if (player->cbFunction_)
        player->cbFunction_(NOTIFY_ERROR, 0, nullptr, nullptr);
      break;
    }
    case GST_MESSAGE_WARNING: {
      GError *pWarning = NULL;
      gchar *pDebug = NULL;
      gst_message_parse_warning(message, &pWarning, &pDebug);
      GMP_INFO_PRINT(" GST_MESSAGE_WARNING : %s, pDebug ( %s )",
                     pWarning->message, pDebug ? pDebug : "null");
      g_error_free(pWarning);
      g_free(pDebug);
      break;
    }
    case GST_MESSAGE_EOS: {
      GMP_INFO_PRINT(" GST_MESSAGE_EOS");
      if (player->cbFunction_) {
        player->cbFunction_(NOTIFY_END_OF_STREAM, 0, nullptr, nullptr);
      }
      break;
    }
    case GST_MESSAGE_SEGMENT_START: {
      std::lock_guard<std::recursive_mutex> lock(player->recursive_mutex_);
      GMP_INFO_PRINT(" GST_MESSAGE_SEGMENT_START");
      const GstStructure *posStruct= gst_message_get_structure(message);
      gint64 position =
          g_value_get_int64(gst_structure_get_value(posStruct, "position"));
      player->currentPts_ = position;
      break;
    }
    case GST_MESSAGE_STATE_CHANGED: {
      std::lock_guard<std::recursive_mutex> lock(player->recursive_mutex_);
      GstElement *pipeline = player->pipeline_;
      if (GST_MESSAGE_SRC(message) == GST_OBJECT_CAST(pipeline)) {
        GstState old_state, new_state;
        gst_message_parse_state_changed(message, &old_state, &new_state, NULL);

        // generate dot graph when play start only(READY -> PAUSED)
        if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED) {
          GMP_DEBUG_PRINT("Generate dot graph from %s state to %s state.",
                gst_element_state_get_name(old_state),
                gst_element_state_get_name(new_state));
          std::string dump_name("g-media-pipeline[" + std::to_string(getpid()) + "]");
          GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN (pipeline),
                GST_DEBUG_GRAPH_SHOW_ALL, dump_name.c_str());
        }
      }
      player->HandleBusStateMsg(message);
      break;
    }
    case GST_MESSAGE_ASYNC_DONE: {
      std::lock_guard<std::recursive_mutex> lock(player->recursive_mutex_);
      GMP_INFO_PRINT(" GST_MESSAGE_ASYNC_DONE");
      player->HandleBusAsyncMsg();
      break;
    }
    case GST_MESSAGE_APPLICATION: {
      GMP_INFO_PRINT(" GST_MESSAGE_APPLICATION");
      player->HandleVideoInfoMsg(message);
      break;
    }
    case GST_MESSAGE_STREAM_STATUS: {
      player->HandleStreamStatsMsg(message);
      break;
    }
    default:
      break;
  }

  return true;
}

GstBusSyncReply BufferPlayer::HandleSyncBusMessage(
    GstBus *bus, GstMessage *message, gpointer user_data)
{
  // This handler will be invoked synchronously, don't process any application
  // message handling here
  LSM::Connector *connector = static_cast<LSM::Connector*>(user_data);

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_NEED_CONTEXT:{
      const gchar *type = nullptr;
      gst_message_parse_context_type(message, &type);
      if (g_strcmp0 (type, waylandDisplayHandleContextType) != 0) {
        break;
      }
      GMP_DEBUG_PRINT("Set a wayland display handle : %p", connector->getDisplay());
      GstContext *context = gst_context_new(waylandDisplayHandleContextType, TRUE);
      gst_structure_set(gst_context_writable_structure (context),
          "handle", G_TYPE_POINTER, connector->getDisplay(), nullptr);
      gst_element_set_context(GST_ELEMENT(GST_MESSAGE_SRC(message)), context);
      goto drop;
    }
    case GST_MESSAGE_ELEMENT:{
      if (!gst_is_video_overlay_prepare_window_handle_message(message)) {
        break;
      }
      GMP_DEBUG_PRINT("Set wayland window handle : %p", connector->getSurface());
      if (connector->getSurface()) {
        GstVideoOverlay *videoOverlay = GST_VIDEO_OVERLAY(GST_MESSAGE_SRC(message));
        gst_video_overlay_set_window_handle(videoOverlay,
                                            (guintptr)(connector->getSurface()));

        gint video_disp_height = 0;
        gint video_disp_width = 0;
        connector->getVideoSize(video_disp_width, video_disp_height);
        if (video_disp_width && video_disp_height) {
          gint display_x = (1920 - video_disp_width) / 2;
          gint display_y = (1080 - video_disp_height) / 2;
          GMP_DEBUG_PRINT("Set render rectangle :(%d, %d, %d, %d)",
                          display_x, display_y, video_disp_width, video_disp_height);
          gst_video_overlay_set_render_rectangle(videoOverlay,
              display_x, display_y, video_disp_width, video_disp_height);

          gst_video_overlay_expose(videoOverlay);
        }
      }
      goto drop;
    }
    default:
      break;
  }

  return GST_BUS_PASS;

drop:
  gst_message_unref(message);
  return GST_BUS_DROP;
}


gboolean BufferPlayer::NotifyCurrentTime(gpointer user_data) {
  BufferPlayer *player = static_cast<BufferPlayer*>(user_data);
  std::lock_guard<std::recursive_mutex> lock(player->recursive_mutex_);
  if (player->currentState_ == STOPPED_STATE) {
    player->currentPts_ = 0;
    return true;
  } else if (player->currentState_ != PLAYING_STATE &&
             player->currentState_ != PLAYED_STATE) {
    return true;
  }

  if (!player->pipeline_ || player->seeking_)
    return true;

  gint64 pos = 0;
  if (!gst_element_query_position(player->pipeline_, GST_FORMAT_TIME, &pos))
    return true;

  if (pos < 0)
    return true;

  if (player->cbFunction_ && player->currentPts_ != pos) {
    player->currentPts_ = pos;
    gint64 currentPtsInMs = (gint64)(pos / GST_MSECOND);
    player->cbFunction_(NOTIFY_CURRENT_TIME, currentPtsInMs, nullptr, nullptr);

    gint64 inSecs = currentPtsInMs / 1000;
    gint hours = inSecs / 3600;
    gint minutes = (inSecs - (3600 * hours)) / 60;
    gint seconds = (inSecs - (3600 * hours) - (minutes * 60));
    GMP_DEBUG_PRINT("Current Position: %" G_GINT64_FORMAT " [%02d:%02d:%02d]",
                     currentPtsInMs, hours, minutes, seconds);
  }

  return true;
}

bool BufferPlayer::CreatePipeline() {
  GMP_INFO_PRINT("audioCodec [ %d ], videoCodec [ %d ]",
                  loadData_->audioCodec, loadData_->videoCodec);

  pipeline_ = gst_pipeline_new("custom-player");
  GMP_INFO_PRINT("pipeline_ = %p", pipeline_);
  if (!pipeline_) {
    GMP_INFO_PRINT("Cannot create custom player!");
    return false;
  }

  ConnectBusCallback();

  if (loadData_->audioCodec != GMP_AUDIO_CODEC_NONE) {
     if (!AddAudioPipelineElements()) {
       GMP_INFO_PRINT("Failed to create Audio pipeline path !!!");
       return false;
     }
  }

  if (loadData_->videoCodec != GMP_VIDEO_CODEC_NONE) {
    if (!AddVideoPipelineElements()) {
      GMP_INFO_PRINT("Failed to create Video pipeline path !!!");
      return false;
    }
  }

  GMP_INFO_PRINT("Buffer player pipeline is created !!!");

  SetDecoderSpecificInfomation();

  return true;
}

bool BufferPlayer::AddAudioPipelineElements() {
  GMP_DEBUG_PRINT("Create and add elements for audio pipeline");

  if (!AddAudioSourceElement())  {
    GMP_DEBUG_PRINT("Failed to create audio source element");
    return false;
  }

  if (inputDumpFileName != NULL) {
    if (AddAudioDumpElement()) {
      GMP_DEBUG_PRINT("Audio stream pipeline created successfully!!!");
      return true;
    }
  } else {
    if (AddAudioParserElement() &&
        AddAudioDecoderElement() &&
        AddAudioConverterElement() &&
        AddAudioSinkElement()) {
      GMP_DEBUG_PRINT("Audio stream pipeline created successfully!!!");
      return true;
    }
  }

  FreePipelineElements();
  return false;
}

bool BufferPlayer::AddVideoPipelineElements() {
  GMP_DEBUG_PRINT("Create and add elements for video pipeline");

  if (!AddVideoSourceElement()) {
    GMP_DEBUG_PRINT("Failed to create video source element");
    return false;
  }

  if (inputDumpFileName != NULL) {
    if (AddVideoDumpElement()) {
      GMP_DEBUG_PRINT("Audio stream pipeline created successfully!!!");
      return true;
    }
  } else {
    if (AddVideoParserElement() &&
        AddVideoDecoderElement() &&
        AddVideoConverterElement() &&
        AddVideoSinkElement()) {
      GMP_DEBUG_PRINT("Video stream pipeline created successfully!!!");
      return true;
    }
  }

  FreePipelineElements();
  return false;
}

bool BufferPlayer::AddAudioSourceElement() {
  GMP_DEBUG_PRINT("Create and add audio source element");

  audioSrcInfo_ = std::make_shared<MEDIA_SRC_T>();
  if (!audioSrcInfo_)
    return false;

  audioSrcInfo_->needFeedData = CUSTOM_BUFFER_FEED;
  audioSrcInfo_->totalFeed = 0;
  audioSrcInfo_->elementName = std::string("audio-app-es");

  audioSrcInfo_->pSrcElement =
      gst_element_factory_make("appsrc", "audio-app-es");
  if (!audioSrcInfo_->pSrcElement) {
    GMP_DEBUG_PRINT("audio-app-e element can not be created!!!");
    return false;
  }

  SetAppSrcProperties(audioSrcInfo_.get(), MEDIA_AUDIO_MAX);

  gst_bin_add(GST_BIN(pipeline_), audioSrcInfo_->pSrcElement);
  linkedElement_ = audioSrcInfo_->pSrcElement;

  return true;
}

bool BufferPlayer::AddAudioParserElement() {
  GMP_DEBUG_PRINT("Create and add audio parser element");

  audioPQueue_ = gst_element_factory_make("queue", "audio-parser-queue");
  if (audioPQueue_) {
    GMP_DEBUG_PRINT("Audio parser queue created");
    SetQueueBufferSize(audioPQueue_, 0, 3);
  }

  if (!AddAndLinkElement(audioPQueue_)) {
    GMP_DEBUG_PRINT("Failed to add & link audio parser queue element");
    return false;
  }

  switch (loadData_->audioCodec) {
    case GMP_AUDIO_CODEC_AC3:
    case GMP_AUDIO_CODEC_EAC3:
      GMP_DEBUG_PRINT("AC3 Parser");
      audioParser_ = gst_element_factory_make("ac3parse", "ac3-parse");
      break;
    case GMP_AUDIO_CODEC_PCM:
      GMP_DEBUG_PRINT("PCM Parser");
      audioParser_ = gst_element_factory_make("audioparse", "pcm-parse");
      break;
    case GMP_AUDIO_CODEC_DTS:
      GMP_DEBUG_PRINT("DTS Parser");
      audioParser_ = gst_element_factory_make("dcaparse", "dts-parse");
      break;
    case GMP_AUDIO_CODEC_AAC:
      GMP_DEBUG_PRINT("AAC Parser");
      audioParser_ = gst_element_factory_make("aacparse", "aac-parse");
      break;
    default:
      GMP_DEBUG_PRINT("Audio codec[%d] not supported", loadData_->audioCodec);
      return false;
  }

  if (!AddAndLinkElement(audioParser_)) {
    GMP_DEBUG_PRINT("Failed to add & link audio parser element");
    return false;
  }

  GMP_DEBUG_PRINT("Audio Parser elements are Added!!!");
  return true;
}

bool BufferPlayer::AddAudioDecoderElement() {
  GMP_DEBUG_PRINT("Create and add audio decoder element");

  switch (loadData_->audioCodec) {
    case GMP_AUDIO_CODEC_AC3:
    case GMP_AUDIO_CODEC_EAC3:
      GMP_DEBUG_PRINT("AC3 Decoder");
      audioDecoder_ = pf::ElementFactory::Create("custom", "audio-codec-ac3");
      break;
    case GMP_AUDIO_CODEC_PCM:
      GMP_DEBUG_PRINT("PCM Decoder");
      audioDecoder_ = pf::ElementFactory::Create("custom", "audio-codec-pcm");
      break;
    case GMP_AUDIO_CODEC_DTS:
      GMP_DEBUG_PRINT("DTS Decoder");
      audioDecoder_ = pf::ElementFactory::Create("custom", "audio-codec-dts");
      break;
    case GMP_AUDIO_CODEC_AAC:
      GMP_DEBUG_PRINT("AAC Decoder");
      audioDecoder_ = pf::ElementFactory::Create("custom", "audio-codec-aac");
      break;
    default:
      GMP_DEBUG_PRINT("Audio codec[%d] not supported", loadData_->audioCodec);
      return false;
  }

  if (!AddAndLinkElement(audioDecoder_)) {
    GMP_DEBUG_PRINT("Failed to add & link audio decoder element");
    return false;
  }

  GMP_DEBUG_PRINT("Audio decoder elements are Added!!!");
  return true;
}

bool BufferPlayer::AddAudioConverterElement() {
  GMP_DEBUG_PRINT("Create and add audio converter elements");

  audioConverter_ = gst_element_factory_make("audioconvert", "audio-converter");
  if (!AddAndLinkElement(audioConverter_)) {
    GMP_DEBUG_PRINT("Failed to add & link audio converter element");
    return false;
  }

  aResampler_ = gst_element_factory_make("audioresample", "audio-resampler");
  if (!AddAndLinkElement(aResampler_)) {
    GMP_DEBUG_PRINT("Failed to add & link audio resampler element");
    return false;
  }

  audioVolume_ = gst_element_factory_make("volume", "audio-volume");
  if (!AddAndLinkElement(audioVolume_)) {
    GMP_DEBUG_PRINT("Failed to add & link audio volume element");
    return false;
  }

  GMP_DEBUG_PRINT("Audio converter elements are Added!!!");
  return true;
}

bool BufferPlayer::AddAudioSinkElement() {
  GMP_DEBUG_PRINT("Create and add audio sink element");

  audioQueue_ = gst_element_factory_make("queue", "audiosink-queue");
  if (!AddAndLinkElement(audioQueue_)) {
    GMP_DEBUG_PRINT("Failed to add & link audio sink queue element");
    return false;
  }

  std::string aSinkName = (use_audio ? "audio-sink" : "fake-sink");
  audioSink_ = pf::ElementFactory::Create("custom", aSinkName,
                                          display_path_);
  if (!AddAndLinkElement(audioSink_)) {
    GMP_DEBUG_PRINT("Failed to add & link audio sink element");
    return false;
  }

  linkedElement_ = nullptr;
  GMP_DEBUG_PRINT("Audio sink elements are Added!!!");
  return true;
}

bool BufferPlayer::AddVideoSourceElement() {
  GMP_DEBUG_PRINT("Create and add video source element");

  videoSrcInfo_ = std::make_shared<MEDIA_SRC_T>();
  if (!videoSrcInfo_)
    return false;

  videoSrcInfo_->elementName = "video-app-es";
  videoSrcInfo_->needFeedData = CUSTOM_BUFFER_FEED;
  videoSrcInfo_->totalFeed = 0;

  videoSrcInfo_->pSrcElement =
      gst_element_factory_make("appsrc", "video-app-es");
  if (!videoSrcInfo_->pSrcElement) {
    GMP_DEBUG_PRINT("audio-app-e element can not be created!!!");
    return false;
  }

  SetAppSrcProperties(videoSrcInfo_.get(), MEDIA_VIDEO_MAX);

  if (loadData_->liveStream)
    g_object_set(videoSrcInfo_->pSrcElement, "is-live", true, NULL);

  gst_bin_add(GST_BIN(pipeline_), videoSrcInfo_->pSrcElement);
  linkedElement_ = videoSrcInfo_->pSrcElement;

  return true;
}

bool BufferPlayer::AddVideoParserElement() {
  GMP_DEBUG_PRINT("Create and add video parser elements");

  videoPQueue_ = gst_element_factory_make("queue", "video-parser-queue");
  if (videoPQueue_) {
    SetQueueBufferSize(videoPQueue_, 0, 3);
  }

  if (!AddAndLinkElement(videoPQueue_)) {
    GMP_DEBUG_PRINT("Failed to add and link video parser queue element");
    return false;
  }

  if (loadData_->svpVersion) {
    GMP_DEBUG_PRINT("This is SVP case! Video parser isn't needed!");
    return true;
  }

  switch (loadData_->videoCodec) {
    case GMP_VIDEO_CODEC_VC1:
      GMP_DEBUG_PRINT("VC1 Parser");
      videoParser_ = gst_element_factory_make("vc1parse", "vc1-parse");
      break;
    case GMP_VIDEO_CODEC_H264:
      GMP_DEBUG_PRINT("H264 Parser");
      videoParser_ = gst_element_factory_make("h264parse", "h264-parse");
      break;
    case GMP_VIDEO_CODEC_H265:
      GMP_DEBUG_PRINT("H265 Parser");
      videoParser_ = gst_element_factory_make("h265parse", "h265-parse");
      break;
    case GMP_VIDEO_CODEC_VP8:
    case GMP_VIDEO_CODEC_VP9:
      GMP_DEBUG_PRINT("Parser not needed for video codec[%d]",
                      loadData_->videoCodec);
      return true;
    default:
      GMP_DEBUG_PRINT("Video codec[%d] not supported", loadData_->videoCodec);
      return false;
  }

  if (!AddAndLinkElement(videoParser_)) {
    GMP_DEBUG_PRINT("Failed to add & link video parser element");
    return false;
  }

  GMP_DEBUG_PRINT("Video Parser elements are Added!!!");
  return true;
}

bool BufferPlayer::AddVideoDecoderElement() {
  GMP_DEBUG_PRINT("Create and add video decoder element");

  switch (loadData_->videoCodec) {
    case GMP_VIDEO_CODEC_VC1:
      GMP_DEBUG_PRINT("VC1 Decoder");
      videoDecoder_ = pf::ElementFactory::Create("custom", "video-codec-vc1");
      break;
    case GMP_VIDEO_CODEC_H264:
      GMP_DEBUG_PRINT("H264 Decoder");
      videoDecoder_ = pf::ElementFactory::Create("custom", "video-codec-h264");
      break;
    case GMP_VIDEO_CODEC_H265:
      GMP_DEBUG_PRINT("H265 Decoder");
      videoDecoder_ = pf::ElementFactory::Create("custom", "video-codec-h265");
      break;
    case GMP_VIDEO_CODEC_VP8:
      GMP_DEBUG_PRINT("VP8 Decoder");
      videoDecoder_ = pf::ElementFactory::Create("custom", "video-codec-vp8");
      break;
    case GMP_VIDEO_CODEC_VP9:
      GMP_DEBUG_PRINT("VP9 Decoder");
      videoDecoder_ = pf::ElementFactory::Create("custom", "video-codec-vp9");
      break;
    default:
      GMP_DEBUG_PRINT("Video codec[%d] not supported", loadData_->videoCodec);
      return false;
  }

  if (!AddAndLinkElement(videoDecoder_)) {
    GMP_DEBUG_PRINT("Failed to add & link audio decoder element");
    return false;
  }

  videoPostProc_ = pf::ElementFactory::Create("custom", "vaapi-postproc");
  if (videoPostProc_) {
    // This is only for INTEL based platform (Using gst-vaaapi).
    if (!AddAndLinkElement(videoPostProc_)) {
      GMP_DEBUG_PRINT("Failed to add & link video post processor element");
      return false;
    }
  }

  GMP_DEBUG_PRINT("Video decoder elements are Added!!!");
  return true;
}

bool BufferPlayer::AddVideoConverterElement() {
  GMP_DEBUG_PRINT("Create and add video converter element");

  videoQueue_ = pf::ElementFactory::Create("custom", "video-queue");
  if (videoQueue_) {
    if (!AddAndLinkElement(videoQueue_)) {
      GMP_DEBUG_PRINT("Failed to add & link video queue element");
      return false;
    }
  }

  vConverter_ = pf::ElementFactory::Create("custom", "video-converter");
  if (vConverter_) {
    if (!AddAndLinkElement(vConverter_)) {
      GMP_DEBUG_PRINT("Failed to add & link video converter element");
      return false;
    }
  } else {
    GMP_DEBUG_PRINT("Video converter element not needed!!!");
  }

  return true;
}

bool BufferPlayer::AddVideoSinkElement() {
   GMP_DEBUG_PRINT("Create and add video sink element");

  videoSink_ = pf::ElementFactory::Create("custom", "video-sink");
  if (!AddAndLinkElement(videoSink_)) {
    GMP_DEBUG_PRINT("Failed to add & link video sink element");
    return false;
  }

  linkedElement_ = nullptr;
  GMP_DEBUG_PRINT("Video sink elements are Added!!!");
  return true;
}

void BufferPlayer::FreePipelineElements() {
  audioSrcInfo_.reset();
  videoSrcInfo_.reset();

  gst_object_unref(pipeline_);
  pipeline_ = NULL;
}

bool BufferPlayer::AddAndLinkElement(GstElement* new_element) {
  if (new_element == nullptr) {
    GMP_DEBUG_PRINT("new_element is NULL pointer!");
    return false;
  }

  gchar *elementName = gst_element_get_name(new_element);
  if (elementName) {
    GMP_DEBUG_PRINT("Adding new_element named [ %s]", elementName);
    g_free(elementName);
  }

  gst_bin_add(GST_BIN(pipeline_), new_element);
  if (gst_element_link(linkedElement_, new_element)) {
    linkedElement_ = new_element;
    return true;
  }
  return false;
}

bool BufferPlayer::AddAudioDumpElement() {
  if (!audioSrcInfo_ || !audioSrcInfo_->pSrcElement)
    return true;

  GMP_INFO_PRINT("Create Element for Dumping audio data");

  std::string dumpFileName("/tmp/");
  dumpFileName.append(inputDumpFileName);
  dumpFileName.append(std::string("_audio.data"));

  audiofileDump_= gst_element_factory_make("filesink", "file-sink-audio");
  if (!audiofileDump_) {
    GMP_DEBUG_PRINT("Failed to create file sink element for audio");
    return false;
  }

  g_object_set(G_OBJECT(audiofileDump_),
               "location", dumpFileName.c_str(), NULL);

  if (!AddAndLinkElement(audiofileDump_)) {
    GMP_DEBUG_PRINT("Failed to add & link audio file sink element");
    return false;
  }

  linkedElement_ = nullptr;
  return true;
}

bool BufferPlayer::AddVideoDumpElement() {
  if (!videoSrcInfo_ || !videoSrcInfo_->pSrcElement)
    return true;

  GMP_INFO_PRINT("Create Element for Dumping video data");

  std::string dumpFileName("/tmp/");
  dumpFileName.append(inputDumpFileName);
  dumpFileName.append(std::string("_video.data"));

  videofileDump_= gst_element_factory_make("filesink", "file-sink-video");
  if (!videofileDump_) {
    GMP_DEBUG_PRINT("Failed to create file sink element for video");
    return false;
  }

  g_object_set(G_OBJECT(videofileDump_),
               "location", dumpFileName.c_str(), NULL);

  if (!AddAndLinkElement(videofileDump_)) {
    GMP_DEBUG_PRINT("Failed to add & link video file sink element");
    return false;
  }

  linkedElement_ = nullptr;
  return true;
}

bool BufferPlayer::ConnectBusCallback() {
  GMP_DEBUG_PRINT("ConnectBusCallback");

  busHandler_ = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
  if (!busHandler_) {
    GMP_DEBUG_PRINT("fail!!!! get pipeline bus.. from gstreamer. ");
    return false;
  } else {
    GMP_DEBUG_PRINT("Got pipeline bus. busHandler_ [%p]", busHandler_);
  }

  gst_bus_add_signal_watch(busHandler_);
  gSigBusAsync_ = g_signal_connect(busHandler_, "message",
                                   G_CALLBACK(BufferPlayer::HandleBusMessage),
                                   this);

  if (!gSigBusAsync_) {
    GMP_DEBUG_PRINT("g_signal_connect failed for message");
    return false;
  }
  gst_bus_set_sync_handler(busHandler_, BufferPlayer::HandleSyncBusMessage, &lsm_connector_, NULL);

  return true;
}

bool BufferPlayer::DisconnectBusCallback() {
  GMP_INFO_PRINT("START");

  if (busHandler_) {
    // Drop all bus messages
    gst_bus_set_flushing(busHandler_, true);
    if (gSigBusAsync_)
      g_signal_handler_disconnect(busHandler_, gSigBusAsync_);

    gst_bus_remove_signal_watch(busHandler_);
    gst_object_unref(busHandler_);
    busHandler_ = NULL;
  }

  GMP_INFO_PRINT("END");
  return true;
}

bool BufferPlayer::PauseInternal() {
  GstStateChangeReturn changeStatus = gst_element_set_state(pipeline_,
                                                            GST_STATE_PAUSED);
  GMP_DEBUG_PRINT("changeStatus [ %d ]", changeStatus);

  PIPELINE_STATE prevState = currentState_;
  if (changeStatus == GST_STATE_CHANGE_FAILURE) {
    GMP_DEBUG_PRINT("Failed changing pipeline state to PAUSED");
    currentState_ = STOPPED_STATE;
    return false;
  } else if (changeStatus == GST_STATE_CHANGE_SUCCESS) {
    currentState_ = PAUSED_STATE;

    if (load_complete_ && currentState_ != prevState) {
      if (cbFunction_)
        cbFunction_(NOTIFY_PAUSED, 0, nullptr, nullptr);
    }
    return true;
  }

  currentState_ = PAUSING_STATE;
  return true;
}

bool BufferPlayer::SeekInternal(const int64_t msecond) {
  GMP_DEBUG_PRINT("seek pos: %" PRId64, msecond);

  if (!pipeline_ || currentState_ == STOPPED_STATE || !load_complete_)
    return false;

  if (videoSrcInfo_) {
    videoSrcInfo_->totalFeed = 0;
    videoSrcInfo_->needFeedData = CUSTOM_BUFFER_LOCKED;
  }

  if (audioSrcInfo_) {
    audioSrcInfo_->totalFeed = 0;
    audioSrcInfo_->needFeedData = CUSTOM_BUFFER_LOCKED;
  }

  recEndOfStream_ = false;
  feedPossible_ = false;
  if (!gst_element_seek(pipeline_, play_rate_, GST_FORMAT_TIME,
                        GstSeekFlags(GST_SEEK_FLAG_FLUSH |
                                     GST_SEEK_FLAG_KEY_UNIT),
                        GST_SEEK_TYPE_SET, msecond * GST_MSECOND,
                        GST_SEEK_TYPE_NONE, 0)) {
    GMP_DEBUG_PRINT("Seek failed!!!");
    return false;
  }

  feedPossible_ = true;
  seeking_ = true;
  currentPts_ = msecond * 1000000;

  return true;
}

bool BufferPlayer::IsBufferAvailable(MEDIA_SRC_T* pAppSrcInfo,
                                     guint64 newBufferSize) {
  guint64 maxBufferSize = 0, currBufferSize = 0, availableSize = 0;
  bool bBufferAvailable = false;

  if (pAppSrcInfo->pSrcElement != NULL) {
    g_object_get(G_OBJECT(pAppSrcInfo->pSrcElement),
                 "current-level-bytes", &currBufferSize, NULL);
    g_object_get(G_OBJECT(pAppSrcInfo->pSrcElement),
                 "max-bytes", &maxBufferSize, NULL);
    GMP_DEBUG_PRINT("[%s], maxBufferSize = %" G_GUINT64_FORMAT
                    ", currBufferSize = %" G_GUINT64_FORMAT,
                    pAppSrcInfo->elementName.c_str(),
                    maxBufferSize, currBufferSize);
  }

  if (maxBufferSize <= currBufferSize)
    availableSize = 0;
  else
    availableSize = maxBufferSize - currBufferSize;

  GMP_DEBUG_PRINT("[%s], availableSize = %" G_GUINT64_FORMAT
                  ", newBufferSize = %" G_GUINT64_FORMAT,
                  pAppSrcInfo->elementName.c_str(), availableSize, newBufferSize);
  if (availableSize >= newBufferSize) {
    GMP_DEBUG_PRINT("buffer space is availabe (size:%" G_GUINT64_FORMAT ")",
      availableSize);
    bBufferAvailable = true;
  }

  GMP_DEBUG_PRINT("buffer is %s",
                   (bBufferAvailable ? "Available(0)":"Unavailable(X)"));
  return bBufferAvailable;
}

bool BufferPlayer::IsFeedPossible(MEDIA_SRC_T* pAppSrcInfo,
                                  guint64 newBufferSize) {
  if (pAppSrcInfo->needFeedData == CUSTOM_BUFFER_FULL) {
    if (IsBufferAvailable(pAppSrcInfo, newBufferSize)) {
      pAppSrcInfo->needFeedData = CUSTOM_BUFFER_FEED;
      return true;
    }
    return false;
  }
  pAppSrcInfo->needFeedData = CUSTOM_BUFFER_FEED;

  return true;
}

void BufferPlayer::HandleBusStateMsg(GstMessage *pMessage)  {
  GstState oldState = GST_STATE_NULL;
  GstState newState = GST_STATE_NULL;

  gst_message_parse_state_changed(pMessage, &oldState, &newState, NULL);

  GMP_INFO_PRINT("Element [ %s ] State changed ...%s -> %s",
                  GST_MESSAGE_SRC_NAME(pMessage),
                  gst_element_state_get_name(oldState),
                  gst_element_state_get_name(newState));

  if (GST_MESSAGE_SRC(pMessage) == GST_OBJECT_CAST(pipeline_)) {
    // generate dot graph when play start only(READY -> PAUSED)
    if (oldState == GST_STATE_READY && newState == GST_STATE_PAUSED) {
      GMP_DEBUG_PRINT("Generate dot graph from %s state to %s state.",
            gst_element_state_get_name(oldState),
            gst_element_state_get_name(newState));
      std::string dump_name("g-media-pipeline[" + std::to_string(getpid()) + "]");
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN (pipeline_),
            GST_DEBUG_GRAPH_SHOW_ALL, dump_name.c_str());
    }
  }

  GstElement* gstElement = GST_ELEMENT(pMessage->src);
  switch (newState) {
    case GST_STATE_VOID_PENDING:
    case GST_STATE_NULL: {
      currentState_ = STOPPED_STATE;
      break;
    }
    case GST_STATE_READY: {
      if (currentState_ != STOPPED_STATE && oldState > GST_STATE_READY)
        currentState_ = STOPPED_STATE;
      break;
    }
    case GST_STATE_PAUSED: {
      if (currentState_!= PAUSED_STATE && pipeline_ == gstElement) {
        currentState_ = PAUSED_STATE;

        if (cbFunction_)
          cbFunction_(NOTIFY_PAUSED, 0, nullptr, nullptr);
      }
      if (cbFunction_ && loadData_->liveStream && !load_complete_) {
        load_complete_ = true;
        cbFunction_(NOTIFY_LOAD_COMPLETED, 0, nullptr, nullptr);
      }
      break;
    }
    case GST_STATE_PLAYING: {
      if (currentState_!= PLAYED_STATE && pipeline_ == gstElement) {
        currentState_ = PLAYED_STATE;

        if (cbFunction_)
          cbFunction_(NOTIFY_PLAYING, 0, nullptr, nullptr);
      }
      break;
    }
    default:
      break;
  }
}

void BufferPlayer::HandleBusAsyncMsg() {
  GMP_DEBUG_PRINT("load_complete_ = %d, seeking_ = %d", load_complete_,
      seeking_);

  if (!load_complete_) {
    load_complete_ = true;
    if (cbFunction_)
      cbFunction_(NOTIFY_LOAD_COMPLETED, 0, nullptr, nullptr);

    if (recEndOfStream_)
      return;

    if (videoSrcInfo_ && videoSrcInfo_->needFeedData != CUSTOM_BUFFER_LOW)
      videoSrcInfo_->needFeedData = CUSTOM_BUFFER_LOW;
  } else if (seeking_) {
    seeking_ = false;
    if (cbFunction_)
      cbFunction_(NOTIFY_SEEK_DONE, 0, nullptr, nullptr);
  }
}

void BufferPlayer::HandleVideoInfoMsg(GstMessage *pMessage) {
  GMP_INFO_PRINT("called");

  const GstStructure *pStructure = gst_message_get_structure(pMessage);
  if (gst_structure_has_name(pStructure, "video-info")) {
    GMP_DEBUG_PRINT("new video-info Found: %s",
                     gst_structure_to_string(pStructure));

    gint width = 0, height = 0;
    gst_structure_get_int(pStructure, "width", &width);
    gst_structure_get_int(pStructure, "height", &height);

    gint numerator = 0; gint denominator = 0;
    gst_structure_get_fraction(pStructure, "framerate", &numerator,
                                                        &denominator);

    videoInfo_.width = width;
    videoInfo_.height = height;
    videoInfo_.frame_rate.num = numerator;
    videoInfo_.frame_rate.den = denominator;

    NotifyVideoInfo();
  }
}

void BufferPlayer::HandleStreamStatsMsg(GstMessage *pMessage) {
  GMP_INFO_PRINT("called");

  GstStreamStatusType type;
  GstElement *owner;
  gst_message_parse_stream_status (pMessage, &type, &owner);
  switch (type) {
    case GST_STREAM_STATUS_TYPE_CREATE:
      GMP_INFO_PRINT("Element [ %s ]:GST_STREAM_STATUS_TYPE_CREATE",
                     GST_MESSAGE_SRC_NAME(pMessage));
      break;
    case GST_STREAM_STATUS_TYPE_ENTER:
      GMP_INFO_PRINT("Element [ %s ]:GST_STREAM_STATUS_TYPE_ENTER",
                     GST_MESSAGE_SRC_NAME(pMessage));
      break;
    case GST_STREAM_STATUS_TYPE_LEAVE:
      GMP_INFO_PRINT("Element [ %s ]:GST_STREAM_STATUS_TYPE_LEAVE",
                     GST_MESSAGE_SRC_NAME(pMessage));
      break;
    default:
      break;
  }
}

bool BufferPlayer::UpdateLoadData(const MEDIA_LOAD_DATA_T* loadData) {
  GMP_DEBUG_PRINT("loadData(%p)", loadData);

  loadData_ = std::make_shared<MEDIA_LOAD_DATA_T>();
  if (loadData_) {
    loadData_->maxWidth = loadData->maxWidth;
    loadData_->maxHeight = loadData->maxHeight;
    loadData_->maxFrameRate = loadData->maxFrameRate;
    loadData_->videoCodec = loadData->videoCodec;
    loadData_->audioCodec = loadData->audioCodec;
    loadData_->ptsToDecode = loadData->ptsToDecode;
    loadData_->frameRate = loadData->frameRate;
    loadData_->width = loadData->width;
    loadData_->height = loadData->height;
    loadData_->extraData = loadData->extraData;
    loadData_->extraSize = loadData->extraSize;
    loadData_->channels = loadData->channels;
    loadData_->sampleRate = loadData->sampleRate;
    loadData_->blockAlign = loadData->blockAlign;
    loadData_->bitRate = loadData->bitRate;
    loadData_->bitsPerSample = loadData->bitsPerSample;
    loadData_->format = loadData->format;
    loadData_->audioObjectType = loadData->audioObjectType;
    loadData_->codecData = loadData->codecData;
    loadData_->codecDataSize = loadData->codecDataSize;
    loadData_->drmType = loadData->drmType;
    loadData_->svpVersion = loadData->svpVersion;
    loadData_->displayPath = loadData->displayPath;
    loadData_->liveStream = loadData->liveStream;
    loadData_->sampleFormat = loadData->sampleFormat;
    return true;
  }
  return false;
}

bool BufferPlayer::UpdateVideoResData(
    const gmp::base::source_info_t &sourceInfo) {
  GMP_DEBUG_PRINT("");
  if (sourceInfo.video_streams.empty()) {
    GMP_DEBUG_PRINT("Invalid video stream size error");
    return false;
  }

  gmp::base::video_info_t video_stream_info = sourceInfo.video_streams.front();

  videoInfo_.width = video_stream_info.width;
  videoInfo_.height = video_stream_info.height;
  videoInfo_.codec = static_cast<GMP_VIDEO_CODEC>(video_stream_info.codec);
  videoInfo_.frame_rate.num = video_stream_info.frame_rate.num;
  videoInfo_.frame_rate.den = video_stream_info.frame_rate.den;
  videoInfo_.bit_rate = video_stream_info.bit_rate;

  NotifyVideoInfo();
  return true;
}

void BufferPlayer::NotifyVideoInfo() {
  GMP_INFO_PRINT("new videoSize[ %d, %d ] framerate[%d/%d]",
      videoInfo_.width, videoInfo_.height,
      videoInfo_.frame_rate.num, videoInfo_.frame_rate.den);

  if (cbFunction_) {
    GMP_INFO_PRINT("sink new videoSize[ %d, %d ]",
        videoInfo_.width, videoInfo_.height);

    gmp::parser::Composer composer;
    composer.put("video", videoInfo_);
    cbFunction_(NOTIFY_VIDEO_INFO, 0, composer.result().c_str(),
                static_cast<void*>(&videoInfo_));
  }
}

void BufferPlayer::EnoughData(GstElement *gstAppSrc, gpointer userData) {
  GMP_DEBUG_PRINT("Appsrc signal : EnoughData");

  if (!gstAppSrc)
    return;

  BufferPlayer *player = reinterpret_cast <BufferPlayer*>(userData);

  MEDIA_DATA_CHANNEL_T dataChType = MEDIA_DATA_CH_NONE;
  MEDIA_SRC_T* pAppSrcInfo = nullptr;
  if (IsElementName(gstAppSrc, "video-app-es")) {
    dataChType = MEDIA_DATA_CH_A;
    pAppSrcInfo = player->videoSrcInfo_.get();
  } else  if(IsElementName(gstAppSrc, "audio-app-es")) {
    dataChType = MEDIA_DATA_CH_B;
    pAppSrcInfo = player->audioSrcInfo_.get();
  }

  if (pAppSrcInfo) {
    if ((pAppSrcInfo->needFeedData != CUSTOM_BUFFER_FULL) &&
        (pAppSrcInfo->needFeedData != CUSTOM_BUFFER_LOCKED)) {
      guint64 currBufferSize = 0;
      g_object_get(G_OBJECT(gstAppSrc),
                 "current-level-bytes", &currBufferSize, NULL);
      GMP_DEBUG_PRINT("currBufferSize [ %" G_GUINT64_FORMAT " ]", currBufferSize);

      pAppSrcInfo->needFeedData = CUSTOM_BUFFER_FULL;

      if (pAppSrcInfo == player->videoSrcInfo_.get() && player->cbFunction_)
        player->cbFunction_(NOTIFY_BUFFER_FULL, dataChType, nullptr, nullptr);
    }
  }
}

gboolean BufferPlayer::SeekData(GstElement *gstAppSrc, guint64 position,
                                gpointer userData) {
  GMP_DEBUG_PRINT("Appsrc signal : SeekData");
  return true;
}

void BufferPlayer::SetDecoderSpecificInfomation() {
  GMP_DEBUG_PRINT("");

  // to support aac raw stream for amazon
  if (audioSrcInfo_ && audioSrcInfo_->pSrcElement &&
      !g_strcmp0(loadData_->format, "raw") &&
      loadData_->audioCodec == GMP_AUDIO_CODEC_AAC) {
    gmp::dsi::DSIGeneratorFactory factory(loadData_.get());
    std::shared_ptr<gmp::dsi::DSIGenerator> sp = factory.getDSIGenerator();
    GstCaps* caps = sp->GenerateSpecificInfo();
    if (caps != NULL) {
      GstPad *srcPad =
          gst_element_get_static_pad(audioSrcInfo_->pSrcElement, "src");
      gst_pad_set_caps(srcPad, caps);
      gst_pad_use_fixed_caps(srcPad);
      gst_object_unref(srcPad);
      gst_caps_unref(caps);
    }
  }

  if (videoSrcInfo_ && videoSrcInfo_->pSrcElement) {
    GstCaps* videoSrcCaps = NULL;
    switch (loadData_->videoCodec) {
      case GMP_VIDEO_CODEC_VP8: {
        videoSrcCaps = gst_caps_new_simple("video/x-vp8",
                                           "format", G_TYPE_STRING, "vp8",
                                           "container", G_TYPE_STRING, "ES",
                                           NULL );
        break;
      }
      case GMP_VIDEO_CODEC_VP9: {
        videoSrcCaps = gst_caps_new_simple("video/x-vp9",
                                           "format", G_TYPE_STRING, "vp9",
                                           "container", G_TYPE_STRING, "ES",
                                           NULL );
        break;
      }
      default:
        break;
    }

    if (videoSrcCaps != NULL) {
      // FIXME : After implementing VP9 parser, it will be collected
      gst_caps_set_simple(videoSrcCaps, "width", G_TYPE_INT, 1920,
                                        "height", G_TYPE_INT, 1080,
                          NULL);
      GstPad *srcPad =
          gst_element_get_static_pad(videoSrcInfo_->pSrcElement, "src");
      gst_pad_set_caps(srcPad, videoSrcCaps);
      gst_pad_use_fixed_caps(srcPad);
      gst_object_unref(srcPad);
      gst_caps_unref(videoSrcCaps);
    }
  }
}

base::source_info_t BufferPlayer::GetSourceInfo(
    const MEDIA_LOAD_DATA_T* loadData) {
  GMP_DEBUG_PRINT("loadData = %p", loadData);

  base::source_info_t source_info;

  base::video_info_t video_stream_info;
  base::audio_info_t audio_stream_info;

  video_stream_info.width = loadData->width;
  video_stream_info.height = loadData->height;
  video_stream_info.codec = loadData->videoCodec;
  video_stream_info.frame_rate.num = loadData->frameRate;
  video_stream_info.frame_rate.den = 1;

  audio_stream_info.codec = loadData->audioCodec;
  audio_stream_info.bit_rate = loadData->bitRate;
  audio_stream_info.sample_rate = loadData->bitsPerSample;
  audio_stream_info.channels = loadData->channels;

  base::program_info_t program;
  program.audio_stream = 1;
  program.video_stream = 1;
  source_info.programs.push_back(program);

  source_info.video_streams.push_back(video_stream_info);
  source_info.audio_streams.push_back(audio_stream_info);

  source_info.seekable = true;

  GMP_DEBUG_PRINT("[video info] width: %d, height: %d, frameRate: %d",
                   video_stream_info.width,
                   video_stream_info.height,
                   video_stream_info.frame_rate.num /
                       video_stream_info.frame_rate.den);

  return source_info;
}

void BufferPlayer::SetAppSrcProperties(MEDIA_SRC_T* pAppSrcInfo,
                                       guint64 bufferMaxLevel) {
  guint32 bufferMinPercent = BUFFER_MIN_PERCENT;

  gst_util_set_object_arg(G_OBJECT(pAppSrcInfo->pSrcElement),
                              "stream-type", "seekable");

  g_object_set(G_OBJECT(pAppSrcInfo->pSrcElement),
               "format", GST_FORMAT_TIME,
               "max-bytes", bufferMaxLevel,
               "min-percent", bufferMinPercent,
               NULL);

  g_signal_connect(reinterpret_cast<GstAppSrc*>(pAppSrcInfo->pSrcElement),
                   "enough-data", G_CALLBACK(EnoughData), this);
  g_signal_connect(reinterpret_cast<GstAppSrc*>(pAppSrcInfo->pSrcElement),
                   "seek-data", G_CALLBACK(SeekData), this);

  pAppSrcInfo->bufferMaxByte = bufferMaxLevel;
  pAppSrcInfo->bufferMinPercent = bufferMinPercent;

  gchar * srcReadName = NULL;
  g_object_get(G_OBJECT(pAppSrcInfo->pSrcElement), "name", &srcReadName, NULL);

  if (srcReadName) {
    pAppSrcInfo->elementName = std::string(srcReadName);
    GMP_DEBUG_PRINT("app src properties set for[ %s ]", srcReadName);
    g_free(srcReadName);
  }
}

void BufferPlayer::SetDebugDumpFileName() {
  inputDumpFileName = getenv("GST_DUMP_FILENAME");
}

void BufferPlayer::PrintLoadData(const MEDIA_LOAD_DATA_T* loadData) {
  GMP_DEBUG_PRINT("------------VIDEO information------------");
  GMP_DEBUG_PRINT("video codec[%d]", loadData->videoCodec);
  GMP_DEBUG_PRINT("maxWidth[%d], maxHeight[%d], maxFrameRate[%d]",
                  loadData->maxWidth, loadData->maxHeight, loadData->maxFrameRate);
  GMP_DEBUG_PRINT("width[%d], height[%d], frameRate[%d]",
                  loadData->width, loadData->height, loadData->frameRate);
  GMP_DEBUG_PRINT("ptsToDecode[%" G_GINT64_FORMAT"]", loadData->ptsToDecode);
  GMP_DEBUG_PRINT("extraData[%p], extraSize[%d]", loadData->extraData, loadData->extraSize);
  GMP_DEBUG_PRINT("windowId[%s]", loadData->windowId);
  GMP_DEBUG_PRINT("-----------------------------------------");
  GMP_DEBUG_PRINT("------------AUDIO information------------");
  GMP_DEBUG_PRINT("audio codec[%d]", loadData->audioCodec);
  GMP_DEBUG_PRINT("channels[%d], sampleRate[%d], blockAlign[%d]",
                  loadData->channels, loadData->sampleRate, loadData->blockAlign);
  GMP_DEBUG_PRINT("bitRate[%d], bitsPerSample[%d], format[%s], sampleFormat[%d], audioObjectType[%d]",
                  loadData->bitRate, loadData->bitsPerSample, loadData->format,
                  loadData->sampleFormat, loadData->audioObjectType);
  GMP_DEBUG_PRINT("codecData[%p], codecDataSize[%d]",
                  loadData->codecData, loadData->codecDataSize);
  GMP_DEBUG_PRINT("-----------------------------------------");
  GMP_DEBUG_PRINT("-------------DRM information-------------");
  GMP_DEBUG_PRINT("drmType[%d], svpVersion[%d]", loadData->drmType, loadData->svpVersion);
  GMP_DEBUG_PRINT("-----------------------------------------");
  GMP_DEBUG_PRINT("-------------ETC information-------------");
  GMP_DEBUG_PRINT("liveStream[%s]", loadData->liveStream ? "yes" : "no");
  GMP_DEBUG_PRINT("-----------------------------------------");
}

}  // namespace player
}  // namespace gmp
