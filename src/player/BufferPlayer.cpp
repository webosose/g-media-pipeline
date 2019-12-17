// @@@LICENSE
//
// Copyright (C) 2018-2019, LG Electronics, All Right Reserved.
//
// No part of this source code may be communicated, distributed, reproduced
// or transmitted in any form or by any means, electronic or mechanical or
// otherwise, for any purpose, without the prior written permission of
// LG Electronics.
//
// LICENSE@@@


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
  memset(&totalFeed_, 0x00, (sizeof(guint64)*IDX_MAX));
  memset(&needFeedData_, 0x00, (sizeof(CUSTOM_BUFFERING_STATE_T)*IDX_MAX));

  GMP_INFO_PRINT("END");
}

BufferPlayer::~BufferPlayer() {
  Unload();

  GMP_INFO_PRINT("loadData_ = %p", loadData_);
  if (loadData_) {
    delete loadData_;
    loadData_ = nullptr;
  }
}

bool BufferPlayer::UnloadImpl() {
  GMP_INFO_PRINT("START");

  for (auto mediaSrc : sourceList_) {
    if (mediaSrc)
      delete mediaSrc;
  }
  sourceList_.clear();

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

  if (load_complete_) {
    feedPossible_ = true;

    if (seeking_) {
      GMP_DEBUG_PRINT("Pipeline is in seeking state");
      return false;
    }

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
      cbFunction_(NOTIFY_ACTIVITY,
          0, nullptr, nullptr);
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
      cbFunction_(NOTIFY_ACTIVITY,
          0, nullptr, nullptr);
    }

  }

  return true;
}

bool BufferPlayer::SetPlayRate(const double rate) {
  GMP_INFO_PRINT("SetPlayRate: %lf", rate);
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
  seeking_ = true;

  gmp::base::time_t position = 0;
  gst_element_query_position(pipeline_, GST_FORMAT_TIME, &position);
  GMP_DEBUG_PRINT("rate: %lf, position: %ld, duration: %ld",
                   rate, position, duration_);

  if (rate > 0.0) {
    if (!gst_element_seek(pipeline_, (gdouble)rate, GST_FORMAT_TIME,
                     GstSeekFlags(GST_SEEK_FLAG_FLUSH |
                                  GST_SEEK_FLAG_KEY_UNIT |
                                  GST_SEEK_FLAG_TRICKMODE),
                     GST_SEEK_TYPE_SET, position, GST_SEEK_TYPE_END, duration_))
      GMP_INFO_PRINT("Set Playback Rate failed");
  } else {
    // reverse playback might be unsupported with some demuxer(e.g. qtdemxer)
    gst_element_seek(pipeline_, (gdouble)rate, GST_FORMAT_TIME,
                     GstSeekFlags(GST_SEEK_FLAG_FLUSH |
                                  GST_SEEK_FLAG_KEY_UNIT |
                                  GST_SEEK_FLAG_TRICKMODE |
                                  GST_SEEK_FLAG_TRICKMODE_KEY_UNITS |
                                  GST_SEEK_FLAG_TRICKMODE_NO_AUDIO),
                     GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_SET, position);
  }
  return true;
}

bool BufferPlayer::Seek(const int64_t msecond) {
  std::lock_guard<std::recursive_mutex> lock(recursive_mutex_);
  GMP_INFO_PRINT("Seek: %ld", msecond);

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
  GMP_DEBUG_PRINT("Custom pipeline does not have volume setting.");
  return false;
}

bool BufferPlayer::Load(const MEDIA_LOAD_DATA_T* loadData) {
  GMP_INFO_PRINT("loadData(%p)", loadData);

  if (!UpdateLoadData(loadData))
    return false;

  window_id_ = loadData->windowId ? loadData->windowId : "";

  display_mode_ = "Textured";

  source_info_ = GetSourceInfo(loadData);

  ACQUIRE_RESOURCE_INFO_T resource_info;
  resource_info.sourceInfo = &source_info_;
  resource_info.displayMode = const_cast<char*>(display_mode_.c_str());
  resource_info.result = false;

  if (cbFunction_)
    cbFunction_(NOTIFY_ACQUIRE_RESOURCE, display_path_, nullptr, static_cast<void*>(&resource_info));

  if (!resource_info.result) {
    GMP_DEBUG_PRINT("resouce acquire fail!");
    return false;
  }

  if (!attachSurface()) {
    GMP_DEBUG_PRINT("attachSurface() failed");
    return false;
  }

  if (!CreatePipeline()) {
    GMP_DEBUG_PRINT("CreatePipeline Failed");
    return false;
  }

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

  recEndOfStream_ = true;
  for (int index = 0; index < sourceList_.size(); index++) {
    GstElement* pSrcElement = sourceList_[index]->pSrcElement;
    if (pSrcElement) {
      if (GST_FLOW_OK != gst_app_src_end_of_stream(GST_APP_SRC(pSrcElement)))
        return false;
    }
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

  // init values regarding with feeding
  memset(&totalFeed_, 0x00, (sizeof(guint64)*IDX_MAX));

  needFeedData_[IDX_VIDEO] = CUSTOM_BUFFER_LOCKED;
  needFeedData_[IDX_AUDIO] = CUSTOM_BUFFER_LOCKED;

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

  MEDIA_SRC_ELEM_IDX_T srcIdx = IDX_MULTI;
  if (esData == MEDIA_DATA_CH_A)
    srcIdx = IDX_VIDEO;
  else if (esData == MEDIA_DATA_CH_B)
    srcIdx = IDX_AUDIO;

  if (!sourceList_[srcIdx]->pSrcElement || bufferSize == 0)
    return MEDIA_ERROR;

  if (!IsFeedPossible(srcIdx, bufferSize)) {
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
      GST_APP_SRC(sourceList_[srcIdx]->pSrcElement), appSrcBuffer);
  if (gstReturn < GST_FLOW_OK) {
    GMP_INFO_PRINT("gst_app_src_push_buffer errCode[ %d ]", gstReturn);
    return MEDIA_ERROR;
  }

  totalFeed_[srcIdx] += bufferSize;  // audio or video

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
        player->cbFunction_(NOTIFY_END_OF_STREAM,
                                0, nullptr, nullptr);
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
    player->cbFunction_(NOTIFY_CURRENT_TIME,
                            currentPtsInMs, nullptr, nullptr);

    gint64 inSecs = currentPtsInMs / 1000;
    gint hours = inSecs / 3600;
    gint minutes = (inSecs - (3600 * hours)) / 60;
    gint seconds = (inSecs - (3600 * hours) - (minutes * 60));
    GMP_DEBUG_PRINT("Current Position: %" G_GINT64_FORMAT " [%02d:%02d:%02d]",
                     currentPtsInMs, hours, minutes, seconds);
  }

  return true;
}

bool BufferPlayer::AddSourceElements() {
  GMP_DEBUG_PRINT("Create and add audio/video source elements");

  std::string srcElementName = "";
  for (gint32 srcIdx = 0; srcIdx < MEDIA_CHANNEL_MAX; srcIdx++) {
    MEDIA_SRC_T *pSrcInfo = new MEDIA_SRC_T();
    pSrcInfo->bufferMinByte = 0;
    pSrcInfo->bufferMaxByte = 0;
    pSrcInfo->bufferMinPercent = 0;

    pSrcInfo->srcIdx = static_cast<MEDIA_SRC_ELEM_IDX_T>(srcIdx);
    if (srcIdx == IDX_VIDEO)
      srcElementName = "video-app-es";
    else if (srcIdx == IDX_AUDIO)
      srcElementName = "audio-app-es";

    pSrcInfo->pSrcElement =
        gst_element_factory_make("appsrc", srcElementName.c_str());
    if (!pSrcInfo->pSrcElement) {
      GMP_DEBUG_PRINT("appsrc(%d) element can not be created!!!", srcIdx);
      return false;
    }
    GMP_INFO_PRINT("pSrcInfo->pSrcElement = %p", pSrcInfo->pSrcElement);
    gchar * srcReadName = NULL;
    g_object_get(G_OBJECT(pSrcInfo->pSrcElement), "name", &srcReadName, NULL);

    if (srcReadName) {
      GMP_DEBUG_PRINT("=============srcIdx[ %d ],name[ %s ]==============",
                       srcIdx, srcReadName);
      g_free(srcReadName);
    }

    SetAppSrcBufferLevel(static_cast<MEDIA_SRC_ELEM_IDX_T>(srcIdx), pSrcInfo);

    if (pSrcInfo->pSrcElement) {
      gst_util_set_object_arg(G_OBJECT(pSrcInfo->pSrcElement),
                              "stream-type", "seekable");

      g_signal_connect(reinterpret_cast<GstAppSrc*>(pSrcInfo->pSrcElement),
                       "enough-data", G_CALLBACK(EnoughData), this);
      g_signal_connect(reinterpret_cast<GstAppSrc*>(pSrcInfo->pSrcElement),
                       "seek-data", G_CALLBACK(SeekData), this);

      gst_bin_add(GST_BIN(pipeline_), pSrcInfo->pSrcElement);

      sourceList_.push_back(pSrcInfo);

      guint64 maxBufferSize = 0;
      g_object_get(G_OBJECT(pSrcInfo->pSrcElement),
                   "max-bytes", &maxBufferSize, NULL);
      GMP_DEBUG_PRINT("srcIdx[%d], maxBufferSize[%" G_GUINT64_FORMAT "]",
        srcIdx, maxBufferSize);
      GMP_DEBUG_PRINT("Audio/Video source elements are Added!!!");
    }
    else {
      GMP_DEBUG_PRINT("ERROR : Audio/Video source elements Add failed!!!");
    }
  }
  return true;
}

bool BufferPlayer::AddParserElements() {
  if (!AddAudioParserElement())
    return false;

  if (!AddVideoParserElement())
    return false;

  return true;
}

bool BufferPlayer::AddDecoderElements() {
  if (!AddAudioDecoderElement())
    return false;

  if (!AddVideoDecoderElement())
    return false;

  return true;
}

bool BufferPlayer::AddConverterElements() {
  if (!AddAudioConverterElement())
    return false;

  if (!AddVideoConverterElement())
    return false;

  return true;
}

bool BufferPlayer::AddSinkElements() {
  if (!AddAudioSinkElement())
    return false;

  if (!AddVideoSinkElement())
    return false;

  return true;
}

bool BufferPlayer::AddDumpElements() {
  std::string dumpName("/tmp/");
  dumpName.append(inputDumpFileName);
  videofileDump_= gst_element_factory_make("filesink", "file-sink-video");
  if (!videofileDump_) {
    GMP_DEBUG_PRINT("Failed to create file sink for video");
    return false;
  }
  std::string vName = dumpName + "_video.data";
  g_object_set(G_OBJECT(videofileDump_), "location", vName.c_str(), NULL);
  gst_bin_add_many(GST_BIN(pipeline_), videofileDump_, NULL);
  gst_element_link_many(sourceList_[IDX_VIDEO]->pSrcElement,
                        videofileDump_, NULL);

  if (use_audio) {
    std::string aName = dumpName + "_audio.data";
    audiofileDump_= gst_element_factory_make("filesink", "file-sink-audio");
    if (!audiofileDump_) {
      GMP_DEBUG_PRINT("Failed to create file sink for audio");
      return false;
    }
    g_object_set(G_OBJECT(audiofileDump_), "location", aName.c_str(), NULL);
    gst_bin_add_many(GST_BIN(pipeline_), audiofileDump_, NULL);
    gst_element_link_many(sourceList_[IDX_AUDIO]->pSrcElement,
                          audiofileDump_, NULL);
  }
  return true;
}

bool BufferPlayer::AddAudioParserElement() {
  GMP_DEBUG_PRINT("Create and add audio parser element");

  audioPQueue_ = gst_element_factory_make("queue", "audio-parser-queue");
  if (!audioPQueue_) {
    GMP_DEBUG_PRINT("Failed to create audio parser queue");
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
    default:
      GMP_DEBUG_PRINT("AAC Parser");
      audioParser_ = gst_element_factory_make("aacparse", "aac-parse");
      break;
  }

  if (!audioParser_) {
    GMP_DEBUG_PRINT("Unable to create audio parser");
    return false;
  }

  SetQueueBufferSize(audioPQueue_, QUEUE_MAX_SIZE, 0);

  gst_bin_add_many(GST_BIN(pipeline_), audioPQueue_, audioParser_, NULL);
  if (!gst_element_link_many(sourceList_[IDX_AUDIO]->pSrcElement,
                        audioPQueue_, audioParser_, NULL)) {
    GMP_DEBUG_PRINT("Unable to link audio parser element");
    return false;
  }

  GMP_DEBUG_PRINT("Audio Parser elements are Added!!!");
  return true;
}

bool BufferPlayer::AddVideoParserElement() {
  GMP_DEBUG_PRINT("Create and add video parser elements");

  videoPQueue_ = gst_element_factory_make("queue", "video-parser-queue");
  if (!videoPQueue_) {
    GMP_DEBUG_PRINT("Failed to create video parser queue");
    return false;
  }

  switch (loadData_->videoCodec) {
    case GMP_VIDEO_CODEC_VC1:
      GMP_DEBUG_PRINT("VC1 Parser");
      videoParser_ = gst_element_factory_make("vc1parse", "vc1-parse");
      break;
    case GMP_VIDEO_CODEC_H265:
      GMP_DEBUG_PRINT("H265 Parser");
      videoParser_ = gst_element_factory_make("h265parse", "h265-parse");
      break;
    case GMP_VIDEO_CODEC_H264:
    default:
      GMP_DEBUG_PRINT("H264 Parser");
      videoParser_ = gst_element_factory_make("h264parse", "h264-parse");
      break;
  }

  if (!videoParser_) {
    GMP_DEBUG_PRINT("Failed to create video parser");
    return false;
  }

  SetQueueBufferSize(videoPQueue_, QUEUE_MAX_SIZE, 0);

  gst_bin_add_many(GST_BIN(pipeline_), videoPQueue_, videoParser_, NULL);
  if (!gst_element_link_many(sourceList_[IDX_VIDEO]->pSrcElement,
                             videoPQueue_, videoParser_, NULL)) {
    GMP_DEBUG_PRINT("Unable to link video parser element");
    return false;
  }

  GMP_DEBUG_PRINT("Video Parser elements are Added!!!");
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
    default:
      GMP_DEBUG_PRINT("AAC Decoder");
      audioDecoder_ = pf::ElementFactory::Create("custom", "audio-codec-aac");
      break;
  }

  if (!audioDecoder_) {
    GMP_DEBUG_PRINT("Unable to create audio Decoder");
    return false;
  }

  gst_bin_add_many(GST_BIN(pipeline_), audioDecoder_,  NULL);
  if (!gst_element_link_many(audioParser_, audioDecoder_, NULL)) {
    GMP_DEBUG_PRINT("Unable to link audio parser and Decoder");
    return false;
  }

  GMP_DEBUG_PRINT("Audio decoder elements are Added!!!");
  return true;
}

bool BufferPlayer::AddVideoDecoderElement() {
  GMP_DEBUG_PRINT("Create and add video decoder element");

  switch (loadData_->videoCodec) {
    case GMP_VIDEO_CODEC_VC1:
      GMP_DEBUG_PRINT("VC1 Decoder");
      videoDecoder_ = pf::ElementFactory::Create("custom", "video-codec-vc1");
      break;
    case GMP_VIDEO_CODEC_H265:
      GMP_DEBUG_PRINT("H265 Decoder");
      videoDecoder_ = pf::ElementFactory::Create("custom", "video-codec-h265");
      break;
    case GMP_VIDEO_CODEC_H264:
    default:
      GMP_DEBUG_PRINT("H264 Decoder");
      videoDecoder_ = pf::ElementFactory::Create("custom", "video-codec-h264");
      break;
  }

  if (!videoDecoder_) {
    GMP_DEBUG_PRINT("Unable to create video Decoder element");
    return false;
  }

  videoPostProc_ = pf::ElementFactory::Create("custom", "vaapi-postproc");
  if (videoPostProc_) {
    // This is only for INTEL based platform (Using gst-vaaapi).
    gst_bin_add_many(GST_BIN(pipeline_), videoDecoder_, videoPostProc_, NULL);
    if (!gst_element_link_many(videoParser_, videoDecoder_, videoPostProc_,
        NULL)) {
      GMP_DEBUG_PRINT("Unable to link video parser and Decoder");
      return false;
    }
  } else {
    gst_bin_add_many(GST_BIN(pipeline_), videoDecoder_, NULL);
    if (!gst_element_link_many(videoParser_, videoDecoder_, NULL)) {
      GMP_DEBUG_PRINT("Unable to link video parser and Decoder");
      return false;
    }
  }

  GMP_DEBUG_PRINT("Video decoder elements are Added!!!");
  return true;
}

bool BufferPlayer::AddAudioConverterElement() {
  GMP_DEBUG_PRINT("Create and add audio converter element");

  audioQueue_ = gst_element_factory_make("queue", "audio-queue");
  if (!audioQueue_) {
    GMP_DEBUG_PRINT("Failed to create audio queue element");
    return false;
  }

  audioConverter_ = gst_element_factory_make("audioconvert", "audio-converter");
  if (!audioConverter_) {
    GMP_DEBUG_PRINT("Failed to create audio converted element");
    return false;
  }

  aResampler_ = gst_element_factory_make("audioresample", "audio-resampler");
  if (!aResampler_) {
    GMP_DEBUG_PRINT("Failed to create audio ressampler element");
    return false;
  }

  gst_bin_add_many(GST_BIN(pipeline_), audioQueue_, audioConverter_,
                   aResampler_, NULL);
  if (!gst_element_link_many(audioDecoder_, audioQueue_, audioConverter_,
                             aResampler_, NULL)) {
    GMP_DEBUG_PRINT("Failed to link audio conveter elements");
    return false;
  }

  GMP_DEBUG_PRINT("Audio converter elements are Added!!!");
  return true;
}

bool BufferPlayer::AddVideoConverterElement() {
  GMP_DEBUG_PRINT("Create and add video converter element");

  vConverter_ = pf::ElementFactory::Create("custom", "video-converter");
  if (vConverter_) {
    gint scale_width = VIDEO_SCALE_WIDTH;
    gint scale_height = (loadData_->height * scale_width) / loadData_->width;
    lsm_connector_.setVideoSize(scale_width, scale_height);

    videoQueue_ = pf::ElementFactory::Create("custom", "video-queue");
    if (videoQueue_) {
      gst_bin_add_many(GST_BIN(pipeline_), videoQueue_, vConverter_, NULL);
      if (!gst_element_link_many(videoDecoder_, videoQueue_, vConverter_,
          NULL)) {
        GMP_DEBUG_PRINT("Failed to link video converter elements");
        return false;
      }
    } else {
      gst_bin_add_many(GST_BIN(pipeline_), vConverter_, NULL);
      if (!gst_element_link_many(videoDecoder_, vConverter_, NULL)) {
        GMP_DEBUG_PRINT("Failed to link video converter elements");
        return false;
      }
    }
    linkElement_ = vConverter_;
    GMP_DEBUG_PRINT("Video converter element are Added!!!");
  } else {
    GMP_DEBUG_PRINT("Video converter element not needed!!!");
    linkElement_ = videoDecoder_;
  }

  return true;
}

bool BufferPlayer::AddAudioSinkElement() {
  GMP_DEBUG_PRINT("Create and add audio sink element");

  audioVolume_ = gst_element_factory_make("volume", "audio-volume");
  if (!audioVolume_) {
    GMP_DEBUG_PRINT("Failed to create audio volume");
    return false;
  }

  std::string aSinkName = (use_audio ? "audio-sink" : "fake-sink");
  audioSink_ = pf::ElementFactory::Create("custom", aSinkName, display_path_);
  if (!audioSink_) {
    GMP_DEBUG_PRINT("Failed to create audio sink element");
    return false;
  }

  gst_bin_add_many(GST_BIN(pipeline_), audioVolume_, audioSink_, NULL);
  if (!gst_element_link_many(aResampler_, audioVolume_, audioSink_, NULL)) {
    GMP_DEBUG_PRINT("Failed to link audio sink elements");
    return false;
  }

  GMP_DEBUG_PRINT("Audio sink elements are Added!!!");
  return true;
}

bool BufferPlayer::AddVideoSinkElement() {
  GMP_DEBUG_PRINT("Create and add video sink element");

  videoSink_ = pf::ElementFactory::Create("custom", "video-sink");
  if (!videoSink_) {
    GMP_DEBUG_PRINT("Failed to create video sink");
    return false;
  }

  gst_bin_add_many(GST_BIN(pipeline_), videoSink_, NULL);
  if (!gst_element_link_many(linkElement_, videoSink_, NULL)) {
    GMP_DEBUG_PRINT("Failed to link video sink elements");
    return false;
  }
  linkElement_ = nullptr;

  GMP_DEBUG_PRINT("Video sink elements are Added!!!");
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
  GMP_DEBUG_PRINT("seek pos [ %ld ]", msecond);

  if (!pipeline_ || currentState_ == STOPPED_STATE || !load_complete_)
    return false;

  memset(&totalFeed_, 0x00, (sizeof(guint64)*IDX_MAX));
  needFeedData_[IDX_VIDEO] = CUSTOM_BUFFER_LOCKED;
  needFeedData_[IDX_AUDIO] = CUSTOM_BUFFER_LOCKED;

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

void BufferPlayer::SetAppSrcBufferLevel(MEDIA_SRC_ELEM_IDX_T srcIdx,
                                        MEDIA_SRC_T *pSrcInfo) {
  guint64 bufferMaxLevel = 0;
  guint32 bufferMinPercent = BUFFER_MIN_PERCENT;

  if (srcIdx == IDX_AUDIO)
    bufferMaxLevel = MEDIA_AUDIO_MAX;
  else if (srcIdx == IDX_VIDEO)
    bufferMaxLevel = MEDIA_VIDEO_MAX;

  g_object_set(G_OBJECT(pSrcInfo->pSrcElement),
               "format", GST_FORMAT_TIME,
               "max-bytes", bufferMaxLevel,
               "min-percent", bufferMinPercent,
               NULL);

  pSrcInfo->bufferMaxByte = bufferMaxLevel;
  pSrcInfo->bufferMinPercent = bufferMinPercent;

  guint64 maxBufferSize = 0;
  g_object_get(G_OBJECT(pSrcInfo->pSrcElement),
               "max-bytes", &maxBufferSize, NULL);
  GMP_DEBUG_PRINT("srcIdx[%d], maxBufferSize[%" G_GUINT64_FORMAT "]", srcIdx, maxBufferSize);
}

bool BufferPlayer::IsBufferAvailable(MEDIA_SRC_ELEM_IDX_T srcIdx,
                                     guint64 newBufferSize){
  guint64 maxBufferSize = 0, currBufferSize = 0, availableSize = 0;
  bool bBufferAvailable = false;

  if (sourceList_[srcIdx]->pSrcElement != NULL) {
    g_object_get(G_OBJECT(sourceList_[srcIdx]->pSrcElement),
                 "current-level-bytes", &currBufferSize, NULL);
    g_object_get(G_OBJECT(sourceList_[srcIdx]->pSrcElement),
                 "max-bytes", &maxBufferSize, NULL);
    GMP_DEBUG_PRINT("srcIdx = %d, maxBufferSize = %" G_GUINT64_FORMAT
                    ", currBufferSize = %" G_GUINT64_FORMAT,
                    srcIdx, maxBufferSize, currBufferSize);
  }

  if (maxBufferSize <= currBufferSize)
    availableSize = 0;
  else
    availableSize = maxBufferSize - currBufferSize;

  GMP_DEBUG_PRINT("srcIdx = %d, availableSize = %" G_GUINT64_FORMAT
                  ", newBufferSize = %" G_GUINT64_FORMAT,
                  srcIdx, availableSize, newBufferSize);
  if (availableSize >= newBufferSize) {
    GMP_DEBUG_PRINT("buffer space is availabe (size:%" G_GUINT64_FORMAT ")",
      availableSize);
    bBufferAvailable = true;
  }

  GMP_DEBUG_PRINT("buffer is %s",
                   (bBufferAvailable ? "Available(0)":"Unavailable(X)"));
  return bBufferAvailable;
}

bool BufferPlayer::IsFeedPossible(MEDIA_SRC_ELEM_IDX_T srcIdx,
                                  guint64 newBufferSize) {
  if (needFeedData_[srcIdx] == CUSTOM_BUFFER_FULL) {
    if (IsBufferAvailable(srcIdx, newBufferSize)) {
      needFeedData_[srcIdx] = CUSTOM_BUFFER_FEED;
      return true;
    }
    return false;
  }
  needFeedData_[srcIdx] = CUSTOM_BUFFER_FEED;

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

    if (needFeedData_[IDX_VIDEO] != CUSTOM_BUFFER_LOW)
      needFeedData_[IDX_VIDEO] = CUSTOM_BUFFER_LOW;
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

  loadData_ = new MEDIA_LOAD_DATA_T();
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
    composer.put("videoInfo", videoInfo_);
    cbFunction_(NOTIFY_VIDEO_INFO,
        0, composer.result().c_str(),
        static_cast<void*>(&videoInfo_));
  }
}

void BufferPlayer::EnoughData(GstElement *gstAppSrc, gpointer userData) {
  GMP_DEBUG_PRINT("Appsrc signal : EnoughData");

  if (!gstAppSrc)
    return;

  gint32 srcIdx = 1;
  if (IsElementName(gstAppSrc, "video-app-es")) {
    srcIdx = 0;
  } else  if(IsElementName(gstAppSrc, "audio-app-es")) {
    srcIdx = 1;
  }

  BufferPlayer *player = reinterpret_cast <BufferPlayer*>(userData);
  if ((player->needFeedData_[srcIdx] != CUSTOM_BUFFER_FULL) &&
      (player->needFeedData_[srcIdx] != CUSTOM_BUFFER_LOCKED)) {
    guint64 currBufferSize = 0;
    g_object_get(G_OBJECT(gstAppSrc),
                 "current-level-bytes", &currBufferSize, NULL);
    GMP_DEBUG_PRINT("currBufferSize [ %" G_GUINT64_FORMAT " ]", currBufferSize);

    player->needFeedData_[srcIdx] = CUSTOM_BUFFER_FULL;

    if (srcIdx == IDX_VIDEO && player->cbFunction_)
      player->cbFunction_(NOTIFY_BUFFER_FULL, srcIdx, nullptr, nullptr);
  }
}

gboolean BufferPlayer::SeekData(GstElement *gstAppSrc, guint64 position,
                                gpointer userData) {
  GMP_DEBUG_PRINT("Appsrc signal : SeekData");
  return true;
}

void BufferPlayer::SetDecoderSpecificInfomation() {
  GMP_DEBUG_PRINT("");

  for (auto mediaSource : sourceList_) {
    if (mediaSource->pSrcElement == NULL)
      return;
  }
/*
  // amazon test
  loadData_->channels = 2;
  loadData_->sampleRate = 48000;
  loadData_->audioObjectType = 2;
  loadData_->format = "raw";
*/

  // to support aac raw stream for amazon
  if (!g_strcmp0(loadData_->format, "raw") && loadData_->audioCodec == GMP_AUDIO_CODEC_AAC) {
    gmp::dsi::DSIGeneratorFactory factory(loadData_);
    std::shared_ptr<gmp::dsi::DSIGenerator> sp = factory.getDSIGenerator();
    GstCaps* caps = sp->GenerateSpecificInfo();
    if (caps != NULL) {
      GstPad *srcPad = gst_element_get_static_pad(sourceList_[IDX_AUDIO]->pSrcElement, "src");
      gst_pad_set_caps(srcPad, caps);
      gst_pad_use_fixed_caps(srcPad);
      gst_object_unref(srcPad);
      gst_caps_unref(caps);
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

void BufferPlayer::SetDebugDumpFileName() {
  inputDumpFileName = getenv("GST_DUMP_FILENAME");
}

}  // namespace player
}  // namespace gmp
