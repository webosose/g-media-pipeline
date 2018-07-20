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

#include "BufferPlayer.h"

#include <string.h>
#include <glib.h>
#include <pthread.h>

#include <cmath>
#include <cstring>
#include <sstream>
#include <map>

#include <gio/gio.h>
#include <pbnjson.hpp>

#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>
#include <gst/base/gstbasesrc.h>
#include <gst/pbutils/pbutils.h>
#include <log/log.h>

#include "base/types.h"
#include "base/message.h"
#include "mediaresource/requestor.h"
#include "parser/composer.h"
#include "service/service.h"
#include "util/util.h"

#define CURR_TIME_INTERVAL_MS    500
#define LOAD_DONE_TIMEOUT_MS     10

#define MEDIA_VIDEO_MAX      (15 * 1024 * 1024)  // 15MB
#define MEDIA_AUDIO_MAX      (4 * 1024 * 1024)   // 4MB
#define QUEUE_MAX_SIZE       (12 * 1024 * 1024)  // 12MB
#define QUEUE_MAX_TIME       (10 * GST_SECOND)   // 10Secs
#define TIMESTAMP_OFFSET     (1 * GST_SECOND)    // 1Secs

#define BUFFER_MIN_PERCENT 50
#define MEDIA_CHANNEL_MAX  2

#ifdef GMP_DEBUG_PRINT
#undef GMP_DEBUG_PRINT
#endif

#define GMP_DEBUG_PRINT GMP_INFO_PRINT

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

BufferPlayer::BufferPlayer(const std::string& appId)
  : videoPQueue_(NULL),
    videoParser_(NULL),
    audioPQueue_(NULL),
    audioParser_(NULL),
    videoDecoder_(NULL),
    vSinkQueue_(NULL),
    videoSink_(NULL),
    audioDecoder_(NULL),
    audioConverter_(NULL),
    aResampler_(NULL),
    aSinkQueue_(NULL),
    audioVolume_(NULL),
    audioSink_(NULL),
    busHandler_(NULL),
    gSigBusAsync_(0),
    planeId_(-1),
    planeIdSet_(false),
    isUnloaded_(false),
    recEndOfStream_(false),
    feedPossible_(false),
    flushDisabled_(false),
    loadDoneTimerId_(0),
    currPosTimerId_(0),
    currentPts_(0),
    currentState_(STOPPED_STATE),
    loadData_(NULL),
    userData_(NULL),
    notifyFunction_(NULL) {
  GMP_INFO_PRINT("START :: appId [ %s ]", appId.c_str());

  memset(&totalFeed_, 0x00, (sizeof(guint64)*IDX_MAX));
  memset(&needFeedData_, 0x00, (sizeof(CUSTOM_BUFFERING_STATE_T)*IDX_MAX));

  resourceRequestor_ =
      std::make_shared<gmp::resource::ResourceRequestor>(appId);

  GMP_INFO_PRINT("END");
}

BufferPlayer::~BufferPlayer() {
  GMP_INFO_PRINT("START");

  Unload();

  GMP_INFO_PRINT("loadData_ = %p", loadData_);
  if (loadData_) {
    delete loadData_;
    loadData_ = NULL;
  }

  GMP_INFO_PRINT("END");
}

bool BufferPlayer::Load(const std::string &uri) {
  return true;
}

bool BufferPlayer::Unload() {
  GMP_INFO_PRINT("START isUnloaded_ [ %d ]", isUnloaded_);

  if (isUnloaded_)
    return true;

  if (pipeline_) {
    gst_element_set_state(pipeline_, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(pipeline_));
    pipeline_ = NULL;
  }

  if (currPosTimerId_) {
    g_source_remove(currPosTimerId_);
    currPosTimerId_ = 0;
  }

  if (loadDoneTimerId_) {
    g_source_remove(loadDoneTimerId_);
    loadDoneTimerId_ = 0;
  }

  DisconnectBusCallback();

  isUnloaded_ = true;
  GMP_INFO_PRINT("END isUnloaded_ [ %d ]", isUnloaded_);
  return true;
}

bool BufferPlayer::Play() {
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
    GstStateChangeReturn retVal = gst_element_set_state(pipeline_,
                                                        GST_STATE_PLAYING);
    if (retVal == GST_STATE_CHANGE_FAILURE) {
      currentState_ = STOPPED_STATE;
    }

    if (resourceRequestor_)
      resourceRequestor_->notifyActivity();
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

    if (resourceRequestor_)
      resourceRequestor_->notifyActivity();
  }

  return true;
}

bool BufferPlayer::SetPlayRate(const double rate) {
  GMP_INFO_PRINT("SetPlayRate: %lf", rate);

  if (!pipeline_)
    return true;

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
  GMP_DEBUG_PRINT("rate: %lf, position: %lld, duration: %lld",
                   rate, position, duration_);

  if (rate > 0.0) {
    gst_element_seek(pipeline_, (gdouble)rate, GST_FORMAT_TIME,
                     GstSeekFlags(GST_SEEK_FLAG_FLUSH |
                                  GST_SEEK_FLAG_KEY_UNIT |
                                  GST_SEEK_FLAG_TRICKMODE),
                     GST_SEEK_TYPE_SET, position, GST_SEEK_TYPE_SET, duration_);
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
}

bool BufferPlayer::Seek(const int64_t msecond) {
  GMP_INFO_PRINT("Seek: %lld", msecond);

  if (!load_complete_ ||currentState_ == STOPPED_STATE) {
    GMP_DEBUG_PRINT("not paused or playing state");
    return false;
  }

  if (seeking_) {
    GMP_DEBUG_PRINT("Pipeline is in seeking state");
    return true;
  }

  if (!SeekInternal(msecond)) {
    GMP_DEBUG_PRINT("fail gstreamer seek");
    return false;
  }
  return true;
}

bool BufferPlayer::SetVolume(int volume) {
  if (!audioSink_)
    return false;

  gdouble floatVolume = (gdouble) ((gdouble)volume / 100.0);
  GMP_DEBUG_PRINT("volume(%d), floatVolume(%f)", volume, floatVolume);
  g_object_set(audioVolume_, "volume", floatVolume, NULL);
  return true;
}

bool BufferPlayer::SetPlane(int planeId) {
  GMP_DEBUG_PRINT("SetPlane: planeId(%d)", planeId);
  planeId_ = planeId;
  return true;
}

void BufferPlayer::Initialize(gmp::service::IService *service) {
  GMP_DEBUG_PRINT("service(%p)", service);
  SetGstreamerDebug();

  gst_init(NULL, NULL);
  gst_pb_utils_init();

  GMP_DEBUG_PRINT("END");
}

bool BufferPlayer::AcquireResources(gmp::base::source_info_t &sourceInfo) {
  gmp::resource::PortResource_t resourceMMap;
  int planeId = -1;

  resourceRequestor_->notifyForeground();

  resourceRequestor_->registerUMSPolicyActionCallback( [this] () {
          resourceRequestor_->notifyBackground();
          Unload();
      });
  resourceRequestor_->registerPlaneIdCallback( [this] (int32_t planeId)->bool {
       GMP_DEBUG_PRINT("registerPlaneIdCallback PlaneId = %d", planeId);
       planeId_ = planeId;
       return true;
     });

  resourceRequestor_->setSourceInfo(sourceInfo);

  if (!resourceRequestor_->acquireResources(NULL, resourceMMap)) {
    GMP_DEBUG_PRINT("resource acquisition failed");
    return false;
  }

  for (auto it : resourceMMap) {
    GMP_DEBUG_PRINT("Resource::[%s]=>index:%d", it.first.c_str(), it.second);
    if (it.first.substr(0, 4) == "DISP") {
      planeId = kPlaneMap[it.second];
      break;
    }
  }

  if (planeId > 0)
    SetPlane(planeId);

  UpdateVideoResData(sourceInfo);

  GMP_DEBUG_PRINT("resource acquired!!!, planeId: %d", planeId);
  return true;
}

bool BufferPlayer::ReleaseResources() {
  if (!resourceRequestor_)
    return false;
  return resourceRequestor_->releaseResource();
}

bool BufferPlayer::Load(const MEDIA_LOAD_DATA_T* loadData) {
  GMP_INFO_PRINT("loadData(%p)", loadData);

  isUnloaded_ = false;

  if (!UpdateLoadData(loadData))
    return false;

  if (!CreatePipeline()) {
    GMP_DEBUG_PRINT("CreatePipeline Failed");
    return false;
  }

  currPosTimerId_ = g_timeout_add(CURR_TIME_INTERVAL_MS,
                                  (GSourceFunc)NotifyCurrentTime, this);
  feedPossible_ = true;

  return true;
}

void BufferPlayer::RegisterCbFunction(
    GMP_CALLBACK_FUNCTION_T callbackFunction, void *udata) {
  GMP_DEBUG_PRINT("userData(%p)", udata);

  notifyFunction_ = callbackFunction;
  userData_ = udata;
}

bool BufferPlayer::PushEndOfStream() {
  GMP_DEBUG_PRINT("PushEndOfStream");

  recEndOfStream_ = true;
  for (int index = 0; index < sourceInfo_.size(); index++) {
    GstElement* pSrcElement = sourceInfo_[index]->pSrcElement;
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

  if (!sourceInfo_[srcIdx]->pSrcElement || bufferSize == 0)
    return MEDIA_ERROR;

  if (!IsFeedPossible(srcIdx, bufferSize)) {
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
      GST_APP_SRC(sourceInfo_[srcIdx]->pSrcElement), appSrcBuffer);
  if (gstReturn < GST_FLOW_OK) {
    GMP_INFO_PRINT("gst_app_src_push_buffer errCode[ %d ]", gstReturn);
    return MEDIA_ERROR;
  }

  totalFeed_[srcIdx] += bufferSize;  // audio or video

  return MEDIA_OK;
}

bool BufferPlayer ::NotifyForeground() {
  GMP_DEBUG_PRINT("NotifyForeground");

  if (!resourceRequestor_)
    return false;

  return resourceRequestor_->notifyForeground();
}

bool BufferPlayer::NotifyBackground() {
  GMP_DEBUG_PRINT("NotifyBackground");

  if (!resourceRequestor_)
    return false;

  return resourceRequestor_->notifyBackground();
}

bool BufferPlayer::SetDisplayWindow(const long left,
                                    const long top,
                                    const long width,
                                    const long height,
                                    const bool isFullScreen) {
  if (!resourceRequestor_)
    return false;

  GMP_INFO_PRINT("fullscreen[%d], [ %d, %d, %d, %d]",
                  isFullScreen, left, top, width, height);
  return resourceRequestor_->setVideoDisplayWindow(left, top, width,
                                                   height, isFullScreen);
}

bool BufferPlayer::SetCustomDisplayWindow(const long srcLeft,
                                          const long srcTop,
                                          const long srcWidth,
                                          const long srcHeight,
                                          const long destLeft,
                                          const long destTop,
                                          const long destWidth,
                                          const long destHeight,
                                          const bool isFullScreen) {
  if (!resourceRequestor_)
    return false;

  GMP_INFO_PRINT("fullscreen[%d], [ %d, %d, %d, %d] => [ %d, %d, %d, %d]",
                  isFullScreen, srcLeft, srcTop, srcWidth, srcHeight,
                  destLeft, destTop, destWidth, destHeight);
  return resourceRequestor_->setVideoCustomDisplayWindow(srcLeft,
      srcTop, srcWidth, srcHeight, destLeft, destTop, destWidth, destHeight,
      isFullScreen);
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
      if (player->notifyFunction_)
        player->notifyFunction_(NOTIFY_ERROR, 0, NULL, player->userData_);
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
      if (player->notifyFunction_) {
        player->notifyFunction_(NOTIFY_END_OF_STREAM,
                                0, NULL, player->userData_);
      }
      break;
    }
    case GST_MESSAGE_SEGMENT_START: {
      GMP_INFO_PRINT(" GST_MESSAGE_SEGMENT_START");
      const GstStructure *posStruct= gst_message_get_structure(message);
      gint64 position =
          g_value_get_int64(gst_structure_get_value(posStruct, "position"));
      player->currentPts_ = position;
      break;
    }
    case GST_MESSAGE_STATE_CHANGED: {
      player->HandleBusStateMsg(message);
      break;
    }
    case GST_MESSAGE_ASYNC_DONE: {
      GMP_INFO_PRINT(" GST_MESSAGE_ASYNC_DONE");
      player->HandleBusAsyncMsg();
      break;
    }
    case GST_MESSAGE_APPLICATION: {
      GMP_INFO_PRINT(" GST_MESSAGE_APPLICATION");
      player->HandleVideoInfoMsg(message);
      break;
    }
    default:
      break;
  }

  return true;
}

gboolean BufferPlayer::NotifyCurrentTime(gpointer user_data) {
  BufferPlayer *player = static_cast<BufferPlayer*>(user_data);
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

  if (player->notifyFunction_ && player->currentPts_ != pos) {
    player->currentPts_ = pos;
    gint64 currentPtsInMs = (gint64)(pos / GST_MSECOND);
    player->notifyFunction_(NOTIFY_CURRENT_TIME,
                            currentPtsInMs, NULL, player->userData_);

    gint64 inSecs = currentPtsInMs / 1000;
    gint hours = inSecs / 3600;
    gint minutes = (inSecs - (3600 * hours)) / 60;
    gint seconds = (inSecs - (3600 * hours) - (minutes * 60));
    GMP_DEBUG_PRINT("Curr Position: %lld [%d:%d:%d]",
                     currentPtsInMs, hours, minutes, seconds);
  }

  return true;
}

gboolean BufferPlayer::NotifyLoadComplete(gpointer user_data) {
  BufferPlayer *player = static_cast<BufferPlayer*>(user_data);
  if (!player)
    return false;

  player->loadDoneTimerId_ = 0;
  if (!player->load_complete_ && player->notifyFunction_) {
    GMP_DEBUG_PRINT("Notify load complete!!!");
    player->load_complete_ = true;
    player->notifyFunction_(NOTIFY_LOAD_COMPLETED, 0, NULL, player->userData_);
  }

  return false;
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

  if (!AddSourceElements()) {
    GMP_INFO_PRINT("Failed to Create and Add appsrc !!!");
    return false;
  }

  if (!AddParserElements()) {
     GMP_INFO_PRINT("Failed to Create and Add Parser !!!");
     return false;
  }

  if (!AddDecoderElements()) {
    GMP_INFO_PRINT("Failed to Add Decoder Elements!!!");
    return false;
  }

  if (!AddSinkElements()) {
    GMP_INFO_PRINT("Failed to Add sink elements!!!");
    return false;
  }

  if (!PauseInternal()) {
    GMP_INFO_PRINT("CreatePipeline -> failed to pause !!!");
    return false;
  }

  currentState_ = LOADING_STATE;
  GMP_INFO_PRINT("END currentState_ = %d", currentState_);

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

    gst_util_set_object_arg(G_OBJECT(pSrcInfo->pSrcElement),
                            "stream-type", "seekable");

    g_signal_connect(reinterpret_cast<GstAppSrc*>(pSrcInfo->pSrcElement),
                     "enough-data", G_CALLBACK(EnoughData), this);
    g_signal_connect(reinterpret_cast<GstAppSrc*>(pSrcInfo->pSrcElement),
                     "seek-data", G_CALLBACK(SeekData), this);

    gst_bin_add(GST_BIN(pipeline_), pSrcInfo->pSrcElement);

    sourceInfo_.push_back(pSrcInfo);

    guint64 maxBufferSize = 0;
    g_object_get(G_OBJECT(pSrcInfo->pSrcElement),
                 "max-bytes", &maxBufferSize, NULL);
    GMP_DEBUG_PRINT("srcIdx[%d], maxBufferSize[%llu]", srcIdx, maxBufferSize);
  }

  GMP_DEBUG_PRINT("Audio/Video source elements are Added!!!");
  return true;
}

bool BufferPlayer::AddParserElements() {
  GMP_DEBUG_PRINT("Create and add audio/video parser elements");

  videoPQueue_ = gst_element_factory_make("queue", "video-queue");
  if (!videoPQueue_) {
    GMP_DEBUG_PRINT("Failed to create video queue");
    return false;
  }

  SetQueueBufferSize(videoPQueue_, QUEUE_MAX_SIZE, 0);

  audioPQueue_ = gst_element_factory_make("queue", "audio-queue");
  if (!audioPQueue_) {
    GMP_DEBUG_PRINT("Failed to create audio queue");
    return false;
  }

  SetQueueBufferSize(audioPQueue_, QUEUE_MAX_SIZE, 0);

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

  gst_bin_add_many(GST_BIN(pipeline_), videoPQueue_, videoParser_,
                                       audioPQueue_, audioParser_, NULL);
  gst_element_link_many(sourceInfo_[IDX_VIDEO]->pSrcElement,
                        videoPQueue_, videoParser_, NULL);
  gst_element_link_many(sourceInfo_[IDX_AUDIO]->pSrcElement,
                        audioPQueue_, audioParser_, NULL);

  GMP_DEBUG_PRINT("Audio/Video Parser elements are Added!!!");
  return true;
}

bool BufferPlayer::AddDecoderElements() {
  GMP_DEBUG_PRINT("Create and add audio/video decoder element");

  switch (loadData_->videoCodec) {
    case GMP_VIDEO_CODEC_VC1:
      GMP_DEBUG_PRINT("VC1 Decoder");
      videoDecoder_ = gst_element_factory_make("omxvc1dec", "omxvc1-decoder");
      break;
    case GMP_VIDEO_CODEC_H265:
      GMP_DEBUG_PRINT("H265 Decoder");
      videoDecoder_ = gst_element_factory_make("avdec_h265", "avdec_h265-dec");
      break;
    case GMP_VIDEO_CODEC_H264:
    default:
      GMP_DEBUG_PRINT("H264 Decoder");
      videoDecoder_ = gst_element_factory_make("omxh264dec", "omxh264-decoder");
      break;
  }

  if (!videoDecoder_) {
    GMP_DEBUG_PRINT("Failed to create video Decoder");
    return false;
  }

  switch (loadData_->audioCodec) {
    case GMP_AUDIO_CODEC_AC3:
    case GMP_AUDIO_CODEC_EAC3:
      GMP_DEBUG_PRINT("AC3 Decoder");
      audioDecoder_ = gst_element_factory_make("avdec_ac3", "audio-decoder");
      break;
    case GMP_AUDIO_CODEC_PCM:
      GMP_DEBUG_PRINT("PCM Decoder");
      audioDecoder_ = gst_element_factory_make("adpcmdec", "audio-decoder");
      break;
    case GMP_AUDIO_CODEC_DTS:
      GMP_DEBUG_PRINT("DTS Decoder");
      audioDecoder_ = gst_element_factory_make("avdec_dca", "audio-decoder");
      break;
    case GMP_AUDIO_CODEC_AAC:
    default:
      GMP_DEBUG_PRINT("AAC Decoder");
      audioDecoder_ = gst_element_factory_make("avdec_aac", "audio-decoder");
      break;
  }

  if (!audioDecoder_) {
    GMP_DEBUG_PRINT("Unable to create audio Decoder");
    return false;
  }

  gst_bin_add_many(GST_BIN(pipeline_), videoDecoder_, audioDecoder_,  NULL);

  if (!gst_element_link(videoParser_, videoDecoder_)) {
    GMP_DEBUG_PRINT("Unable to link video parser and Decoder");
    return false;
  }

  if (!gst_element_link(audioParser_, audioDecoder_)) {
    GMP_DEBUG_PRINT("Unable to link audio parser and Decoder");
    return false;
  }

  GMP_DEBUG_PRINT("Audio/Video decoder elements are Added!!!");
  return true;
}

bool BufferPlayer::AddSinkElements() {
  GMP_DEBUG_PRINT("Create and add audio/video sink element");

  vSinkQueue_ = gst_element_factory_make("queue", "videosink-queue");
  if (!vSinkQueue_ ) {
    GMP_DEBUG_PRINT("Failed to create video sink Queue");
    return false;
  }

  videoSink_ = gst_element_factory_make("kmssink", "video-sink");
  if (!videoSink_) {
    GMP_DEBUG_PRINT("Failed to create video sink");
    return false;
  }

  audioConverter_ = gst_element_factory_make("audioconvert", "audio-converter");
  if (!audioConverter_ ) {
    GMP_DEBUG_PRINT("Failed to create audio converted");
    return false;
  }

  aResampler_ = gst_element_factory_make("audioresample", "audio-resampler");
  if (!aResampler_ ) {
    GMP_DEBUG_PRINT("Failed to create audio ressampler");
    return false;
  }

  audioVolume_ = gst_element_factory_make("volume", "audio-volume");
  if (!audioVolume_ ) {
    GMP_DEBUG_PRINT("Failed to create audio volume");
    return false;
  }

  aSinkQueue_ =  gst_element_factory_make("queue", "audiosink-queue");
  if (!aSinkQueue_ ) {
    GMP_DEBUG_PRINT("Failed to create audio sink Queue");
    return false;
  }

  audioSink_ = gst_element_factory_make("alsasink", "audio-sink");
  if (!audioSink_ ) {
    GMP_DEBUG_PRINT("Failed to create audio sink");
    return false;
  }

  gst_bin_add_many(GST_BIN(pipeline_), vSinkQueue_, videoSink_,
                   audioConverter_, aResampler_, aSinkQueue_, audioVolume_, audioSink_, NULL);
  if (!gst_element_link_many(videoDecoder_, vSinkQueue_, videoSink_, NULL)) {
    GMP_DEBUG_PRINT("Failed to link video sink elements");
    return false;
  }

  g_object_set(G_OBJECT(videoSink_), "driver-name", "vc4",
                                     "ts-offset", TIMESTAMP_OFFSET, NULL);

  g_object_set(G_OBJECT(audioSink_), "device", "hw:0,0",
                                     "ts-offset", TIMESTAMP_OFFSET, NULL);

  if (!gst_element_link_many(audioDecoder_, aSinkQueue_, audioConverter_,
                             aResampler_, audioVolume_, audioSink_, NULL)) {
    GMP_DEBUG_PRINT("Failed to link audio sink elements");
    return false;
  }

  GMP_DEBUG_PRINT("planeIdSet_ = %d, planeId_ = %d", planeIdSet_, planeId_);
  if (!planeIdSet_ && planeId_ > 0) {
    g_object_set(G_OBJECT(videoSink_), "plane-id", planeId_, NULL);
    planeIdSet_ = true;
  }

  GMP_DEBUG_PRINT("Audio/Video sink elements are Added!!!");
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
      if (notifyFunction_)
        notifyFunction_(NOTIFY_PAUSED, 0, NULL, userData_);
    }
    return true;
  }

  currentState_ = PAUSING_STATE;
  return true;
}

bool BufferPlayer::SeekInternal(const int64_t msecond) {
  GMP_DEBUG_PRINT("seek pos [ %lld ]", msecond);

  if (!pipeline_ || currentState_ == STOPPED_STATE || !load_complete_)
    return false;

  memset(&totalFeed_, 0x00, (sizeof(guint64)*IDX_MAX));
  needFeedData_[IDX_VIDEO] = CUSTOM_BUFFER_LOCKED;
  needFeedData_[IDX_AUDIO] = CUSTOM_BUFFER_LOCKED;

  recEndOfStream_ = false;

  gchar* decoderName = NULL;
  g_object_get(G_OBJECT(videoDecoder_), "name", &decoderName, NULL);
  if (decoderName) {
    if (std::string(decoderName) == std::string("omxh264-decoder")) {
      g_object_set(G_OBJECT(videoDecoder_), "flush-in-drain", false, NULL);
      flushDisabled_ = true;
      GMP_DEBUG_PRINT("video [ %s ] flush-in-drain Disabled", decoderName);
    }
    g_free(decoderName);
  }

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
  GMP_DEBUG_PRINT("srcIdx[%d], maxBufferSize[%llu]", srcIdx, maxBufferSize);
}

bool BufferPlayer::IsBufferAvailable(MEDIA_SRC_ELEM_IDX_T srcIdx,
                                     guint64 newBufferSize){
  guint64 maxBufferSize = 0, currBufferSize = 0, availableSize = 0;
  bool bBufferAvailable = false;

  if (sourceInfo_[srcIdx]->pSrcElement != NULL) {
    g_object_get(G_OBJECT(sourceInfo_[srcIdx]->pSrcElement),
                 "current-level-bytes", &currBufferSize, NULL);
    g_object_get(G_OBJECT(sourceInfo_[srcIdx]->pSrcElement),
                 "max-bytes", &maxBufferSize, NULL);
    GMP_DEBUG_PRINT("srcIdx = %d, maxBufferSize = %llu, currBufferSize = %llu",
                    srcIdx, maxBufferSize, currBufferSize);
  }

  if (maxBufferSize <= currBufferSize)
    availableSize = 0;
  else
    availableSize = maxBufferSize - currBufferSize;

  GMP_DEBUG_PRINT("srcIdx = %d, availableSize = %llu, newBufferSize = %llu",
                  srcIdx, availableSize, newBufferSize);
  if (availableSize >= newBufferSize) {
    GMP_DEBUG_PRINT("buffer space is availabe (size:%llu)", availableSize);
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

  GstElement* gstElement = GST_ELEMENT(pMessage->src);
  switch (newState) {
    case GST_STATE_VOID_PENDING:
    case GST_STATE_NULL: {
      currentState_ = STOPPED_STATE;
      break;
    }
    case GST_STATE_READY: {
      if (!load_complete_ && pipeline_ == gstElement) {
        loadDoneTimerId_ = g_timeout_add(LOAD_DONE_TIMEOUT_MS,
                                         (GSourceFunc)NotifyLoadComplete, this);
      }

      if (currentState_ != STOPPED_STATE && oldState > GST_STATE_READY)
        currentState_ = STOPPED_STATE;
      break;
    }
    case GST_STATE_PAUSED: {
      if (currentState_!= PAUSED_STATE && pipeline_ == gstElement) {
        currentState_ = PAUSED_STATE;

        if (notifyFunction_)
          notifyFunction_(NOTIFY_PAUSED, 0, NULL, userData_);
      }
      break;
    }
    case GST_STATE_PLAYING: {
      if (currentState_!= PLAYED_STATE && pipeline_ == gstElement) {
        currentState_ = PLAYED_STATE;

        if (notifyFunction_)
          notifyFunction_(NOTIFY_PLAYING, 0, NULL, userData_);
      }
      break;
    }
    default:
      break;
  }
}

void BufferPlayer::HandleBusAsyncMsg() {
  GMP_DEBUG_PRINT("seeking_ = %d", seeking_);

  if (!load_complete_) {
    load_complete_ = true;
    notifyFunction_(NOTIFY_LOAD_COMPLETED, 0, NULL, userData_);

    if (recEndOfStream_)
      return;

    if (needFeedData_[IDX_VIDEO] != CUSTOM_BUFFER_LOW)
      needFeedData_[IDX_VIDEO] = CUSTOM_BUFFER_LOW;
  } else if (seeking_) {
    seeking_ = false;
    notifyFunction_(NOTIFY_SEEK_DONE, 0, NULL, userData_);

    if (flushDisabled_) {
      g_object_set(G_OBJECT(videoDecoder_), "flush-in-drain", true, NULL);
      flushDisabled_ = false;
      GMP_DEBUG_PRINT("videoDecoder flush-in-drain Enabled");
    }
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
    return true;
  }
  return false;
}

bool BufferPlayer::UpdateVideoResData(
    const gmp::base::source_info_t &sourceInfo) {
  GMP_DEBUG_PRINT("");

  gmp::base::video_info_t video_stream_info = sourceInfo.video_streams.front();

  videoInfo_.width = video_stream_info.width;
  videoInfo_.height = video_stream_info.height;
  videoInfo_.codec = static_cast<GMP_VIDEO_CODEC>(video_stream_info.codec);
  videoInfo_.frame_rate.num = video_stream_info.frame_rate.num;
  videoInfo_.frame_rate.den = video_stream_info.frame_rate.den;
  videoInfo_.bit_rate = video_stream_info.bit_rate;

  NotifyVideoInfo();
}

void BufferPlayer::NotifyVideoInfo() {
  GMP_INFO_PRINT("new videoSize[ %d, %d ] framerate[%f]",
      videoInfo_.width, videoInfo_.height,
      videoInfo_.frame_rate.num / videoInfo_.frame_rate.den);

  if (resourceRequestor_)
    resourceRequestor_->setVideoInfo(videoInfo_);

  if (notifyFunction_) {
    GMP_INFO_PRINT("sink new videoSize[ %d, %d ]",
                    videoInfo_.width, videoInfo_.height);

    gmp::parser::Composer composer;
    composer.put("videoInfo", videoInfo_);
    notifyFunction_(NOTIFY_VIDEO_INFO,
                    0, composer.result().c_str(), userData_);
  }
}

void BufferPlayer::EnoughData(GstElement *gstAppSrc, gpointer user_data) {
  GMP_DEBUG_PRINT("Appsrc signal : EnoughData");

  if (!gstAppSrc)
    return;

  gint32 srcIdx = 1;
  if (IsElementName(gstAppSrc, "video-app-es")) {
    srcIdx = 0;
  } else  if(IsElementName(gstAppSrc, "audio-app-es")) {
    srcIdx = 1;
  }

  BufferPlayer *player = reinterpret_cast <BufferPlayer*>(user_data);
  if ((player->needFeedData_[srcIdx] != CUSTOM_BUFFER_FULL) &&
      (player->needFeedData_[srcIdx] != CUSTOM_BUFFER_LOCKED)) {
    guint64 currBufferSize = 0;
    g_object_get(G_OBJECT(gstAppSrc),
                 "current-level-bytes", &currBufferSize, NULL);
    GMP_DEBUG_PRINT("currBufferSize [ %llu ]", currBufferSize);

    player->needFeedData_[srcIdx] = CUSTOM_BUFFER_FULL;

    if (srcIdx == IDX_VIDEO && player->notifyFunction_)
      player->notifyFunction_(NOTIFY_BUFFER_FULL, srcIdx, 0, player->userData_);
  }
}

void BufferPlayer::SeekData(GstElement *gstAppSrc, guint64 position,
                            gpointer user_data) {
  GMP_DEBUG_PRINT("Appsrc signal : SeekData");

  if (!gstAppSrc)
    return;

  BufferPlayer *player = reinterpret_cast <BufferPlayer*>(user_data);
  for (gint32 idx = 0; idx < player->sourceInfo_.size(); idx++) {
    if (player->sourceInfo_[idx]->pSrcElement == gstAppSrc) {
      if (player->notifyFunction_)
        player->notifyFunction_(NOTIFY_SEEK_DATA, idx, 0, player->userData_);
      return;
    }
  }
}

void BufferPlayer::SetGstreamerDebug() {
  GMP_DEBUG_PRINT("");
  const char *kDebug = "GST_DEBUG";
  const char *kDebugFile = "GST_DEBUG_FILE";
  const char *kDebugDot = "GST_DEBUG_DUMP_DOT_DIR";

  std::string input_file("/etc/g-media-pipeline/gst_debug.conf");
  pbnjson::JDomParser parser(NULL);

  if (!parser.parseFile(input_file, pbnjson::JSchema::AllSchema(), NULL)) {
    GMP_DEBUG_PRINT("Debug file parsing error");
    return;
  }

  pbnjson::JValue parsed = parser.getDom();
  pbnjson::JValue debug = parsed["gst_debug"];

  for (int i = 0; i < debug.arraySize(); i++) {
    if (debug[i].hasKey(kDebug) && !debug[i][kDebug].asString().empty())
      setenv(kDebug, debug[i][kDebug].asString().c_str(), 1);
    if (debug[i].hasKey(kDebugFile) && !debug[i][kDebugFile].asString().empty())
      setenv(kDebugFile, debug[i][kDebugFile].asString().c_str(), 1);
    if (debug[i].hasKey(kDebugDot) && !debug[i][kDebugDot].asString().empty())
      setenv(kDebugDot, debug[i][kDebugDot].asString().c_str(), 1);
  }
}

}  // namespace player
}  // namespace gmp
