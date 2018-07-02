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

#include "Player.h"
#include <log/log.h>
#include <util/util.h>
#include <base/types.h>
#include <base/message.h>
#include <service/service.h>
#include <gst/pbutils/pbutils.h>
#include <pbnjson.hpp>
#include <sys/types.h>
#include <unistd.h>

#define DISCOVER_EXPIRE_TIME (10 * GST_SECOND)
#define UPDATE_INTERVAL_MS 200

namespace gmp { namespace player {
UriPlayer::UriPlayer()
    : uri_(""),
    positionTimer_id_(0),
    bufferingTimer_id_(0),
    planeId_(-1),
    queue2_(NULL),
    buffering_(false),
    buffered_time_(-1),
    current_state_(base::playback_state_t::STOPPED) {
}

UriPlayer::~UriPlayer() {
  Unload();
  gst_deinit ();
}

bool UriPlayer::Load(const std::string &uri) {
  if (uri.empty()) {
    GMP_DEBUG_PRINT("Uri is empty!");
    return false;
  }
  GMP_DEBUG_PRINT("load: %s", uri.c_str());
  uri_ = uri;
  SetGstreamerDebug();
  gst_init(NULL, NULL);
  gst_pb_utils_init();
  if (!GetSourceInfo()) {
    GMP_DEBUG_PRINT("get source information failed!");
    return false;
  }
  // TODO(someone): check return value of acquire.
  // Should use wxh value for acquire.
  if (!service_->acquire(source_info_)) {
    GMP_DEBUG_PRINT("resouce acquire fail!");
    return false;
  }

  if (!LoadPipeline()) {
    GMP_DEBUG_PRINT("pipeline load failed!");
    return false;
  }

  positionTimer_id_ = g_timeout_add(UPDATE_INTERVAL_MS,
                            (GSourceFunc)NotifyCurrentTime, this);

  bufferingTimer_id_ = g_timeout_add(UPDATE_INTERVAL_MS,
                            (GSourceFunc)NotifyBufferingTime, this);

  SetPlayerState(base::playback_state_t::LOADED);

  GMP_DEBUG_PRINT("Load Done: %s", uri_.c_str());
  return true;
}

bool UriPlayer::Unload() {
  GMP_DEBUG_PRINT("unload");
  if (!pipeline_) {
    GMP_DEBUG_PRINT("pipeline is null");
    return false;
  }

  gst_element_set_state(pipeline_, GST_STATE_NULL);
  gst_object_unref(GST_OBJECT(pipeline_));
  pipeline_ = NULL;

  if (positionTimer_id_) {
    g_source_remove(positionTimer_id_);
    positionTimer_id_ = 0;
  }

  if (bufferingTimer_id_) {
    g_source_remove(bufferingTimer_id_);
    bufferingTimer_id_ = 0;
  }

  SetPlayerState(base::playback_state_t::STOPPED);

  service_->Notify(NOTIFY_UNLOAD_COMPLETED);
  return service_->Stop();
}

bool UriPlayer::Play() {
  GMP_DEBUG_PRINT("play");
  if (!pipeline_) {
    GMP_DEBUG_PRINT("pipeline is null");
    return false;
  }

  if (!gst_element_set_state(pipeline_, GST_STATE_PLAYING))
    return false;

  SetPlayerState(base::playback_state_t::PLAYING);

  service_->Notify(NOTIFY_PLAYING);
  return true;
}

bool UriPlayer::Pause() {
  GMP_DEBUG_PRINT("pause");
  if (!pipeline_) {
    GMP_DEBUG_PRINT("pipeline is null");
    return false;
  }

  if (!gst_element_set_state(pipeline_, GST_STATE_PAUSED))
    return false;

  SetPlayerState(base::playback_state_t::PAUSED);

  service_->Notify(NOTIFY_PAUSED);
  return true;
}

bool UriPlayer::SetPlayRate(const double rate) {
  GMP_DEBUG_PRINT("SetPlayRate: %lf", rate);

  if (!pipeline_) {
    GMP_DEBUG_PRINT("pipeline is null");
    return false;
  }

  if (util::Util::CompareDouble(rate, play_rate_)) {
    GMP_DEBUG_PRINT("playRate setting not needed (same)");
    return true;
  }

  play_rate_ = rate;
  gmp::base::time_t position = 0;
  seeking_ = true;

  gst_element_query_position(pipeline_, GST_FORMAT_TIME, &position);
  GMP_DEBUG_PRINT("rate: %lf, position: %lld, duration: %lld",
                  rate, position, duration_);

  if (rate > 0.0) {
    return gst_element_seek(pipeline_, (gdouble)rate, GST_FORMAT_TIME,
                     GstSeekFlags(GST_SEEK_FLAG_FLUSH |
                                  GST_SEEK_FLAG_KEY_UNIT |
                                  GST_SEEK_FLAG_TRICKMODE),
                     GST_SEEK_TYPE_SET, position, GST_SEEK_TYPE_SET, duration_);
  } else {
// reverse playback might be unsupported with some demuxer(e.g. qtdemxer)
    return gst_element_seek(pipeline_, (gdouble)rate, GST_FORMAT_TIME,
                     GstSeekFlags(GST_SEEK_FLAG_FLUSH |
                                  GST_SEEK_FLAG_KEY_UNIT |
                                  GST_SEEK_FLAG_TRICKMODE |
                                  GST_SEEK_FLAG_TRICKMODE_KEY_UNITS |
                                  GST_SEEK_FLAG_TRICKMODE_NO_AUDIO),
                     GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_SET, position);
  }
}

bool UriPlayer::Seek(const int64_t msecond) {
  GMP_DEBUG_PRINT("Seek: %lld", msecond);

  if (!pipeline_) {
    GMP_DEBUG_PRINT("pipeline is null");
    return false;
  }

  seeking_ = true;
  buffered_time_ = msecond;
  // TODO(anonymous): Support reverse playback
  return gst_element_seek(pipeline_, (gdouble)play_rate_, GST_FORMAT_TIME,
                   GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                   GST_SEEK_TYPE_SET, msecond * GST_MSECOND,
                   GST_SEEK_TYPE_NONE, 0);
}

bool UriPlayer::SetVolume(int volume) {
  GMP_DEBUG_PRINT("SetVolume: volume(%d)", volume);

  if (!pipeline_) {
    GMP_DEBUG_PRINT("pipeline is null");
    return false;
  }

  if (volume < 0)
    volume = 0;

  g_object_set(G_OBJECT(pipeline_), "volume", (gdouble) (volume / 100.0), NULL);
  return true;
}

bool UriPlayer::SetPlane(int planeId) {
  GMP_DEBUG_PRINT("setPlane planeId:(%d)", planeId);
  planeId_ = planeId;

  return true;
}

void UriPlayer::Initialize(gmp::service::IService *service) {
  if (!service)
    return;

  service_ = service;
}

bool UriPlayer::GetSourceInfo() {
  GError *err = NULL;
  GstDiscoverer* discoverer = NULL;
  GstDiscovererInfo* info = NULL;
  uint64_t duration = 0;
  discoverer = gst_discoverer_new((GstClockTime)DISCOVER_EXPIRE_TIME, &err);
  info = gst_discoverer_discover_uri(discoverer, uri_.c_str(), &err);
  duration = gst_discoverer_info_get_duration(info);

  GList *video_info = gst_discoverer_info_get_video_streams(info);
  GList *audio_info = gst_discoverer_info_get_audio_streams(info);

  base::video_info_t video_stream_info;
  base::audio_info_t audio_stream_info;

  if (!video_info && !audio_info) {
    GMP_DEBUG_PRINT("Failed to get A/V info from stream");
    return false;
  }

  if (video_info) {
    GList *first = g_list_first(video_info);
    GstDiscovererVideoInfo *video
      = reinterpret_cast<GstDiscovererVideoInfo *>(first->data);

    video_stream_info.width = gst_discoverer_video_info_get_width(video);
    video_stream_info.height = gst_discoverer_video_info_get_height(video);
    video_stream_info.codec = 0;
    video_stream_info.bit_rate = gst_discoverer_video_info_get_bitrate(video);
    video_stream_info.frame_rate.num = gst_discoverer_video_info_get_framerate_num(video);
    video_stream_info.frame_rate.den = gst_discoverer_video_info_get_framerate_denom(video);

    GMP_DEBUG_PRINT("[video info] width: %d, height: %d, bitRate: %lld, frameRate: %d/%d",
                   video_stream_info.width,
                   video_stream_info.height,
                   video_stream_info.bit_rate,
                   video_stream_info.frame_rate.num,
                   video_stream_info.frame_rate.den
                   );
  } else {
    GMP_DEBUG_PRINT("Failed to get video info from stream");
  }

  if (audio_info) {
    GList *first = g_list_first(audio_info);
    GstDiscovererAudioInfo *audio
      = reinterpret_cast<GstDiscovererAudioInfo *>(first->data);
    audio_stream_info.codec = 0;
    audio_stream_info.bit_rate = gst_discoverer_audio_info_get_bitrate(audio);
    audio_stream_info.sample_rate
      = gst_discoverer_audio_info_get_sample_rate(audio);
    GMP_DEBUG_PRINT("[audio info] bitRate: %lld, sampleRate: %d",
                   audio_stream_info.bit_rate,
                   audio_stream_info.sample_rate
                   );
  } else {
    GMP_DEBUG_PRINT("Failed to get audio info from stream");
  }
  duration_ = duration;
  source_info_.duration = GST_TIME_AS_MSECONDS(duration);
  source_info_.seekable = true;

  // TODO(anonymous): Support multi-track media case.
  base::program_info_t program;
  program.audio_stream = 1;
  program.video_stream = 1;
  source_info_.programs.push_back(program);

  source_info_.video_streams.push_back(video_stream_info);
  source_info_.audio_streams.push_back(audio_stream_info);

  GMP_DEBUG_PRINT("Width: : %d", video_stream_info.width);
  GMP_DEBUG_PRINT("Height: : %d", video_stream_info.height);
  GMP_DEBUG_PRINT("Duration: : %lld", duration);

  g_clear_error(&err);
  gst_discoverer_stream_info_list_free(video_info);
  gst_discoverer_stream_info_list_free(audio_info);
  g_object_unref(discoverer);
  g_object_unref(info);
  return true;
}

void UriPlayer::NotifySourceInfo() {
  //TODO(anonymous): Support multiple video/audio stream case
  service_->Notify(NOTIFY_SOURCE_INFO, &source_info_);
}

gboolean UriPlayer::HandleBusMessage(GstBus *bus,
                                     GstMessage *message, gpointer user_data) {
  // TODO(anonymous): handle more bus message
  UriPlayer *player = reinterpret_cast<UriPlayer *>(user_data);

  switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR: {
      base::error_t error = player->HandleErrorMessage(message);
      player->service_->Notify(NOTIFY_ERROR, &error);
      break;
    }

    case GST_MESSAGE_EOS: {
      GMP_DEBUG_PRINT("Got endOfStream");
      player->service_->Notify(NOTIFY_END_OF_STREAM);
      break;
    }

    case GST_MESSAGE_ASYNC_DONE: {
      GMP_DEBUG_PRINT("ASYNC DONE");

      if (!player->load_complete_) {
        player->service_->Notify(NOTIFY_LOAD_COMPLETED);
        player->load_complete_ = true;
      } else if (player->seeking_) {
        player->service_->Notify(NOTIFY_SEEK_DONE);
        player->seeking_ = false;
      }

      break;
    }

    case GST_STATE_PAUSED: {
      GMP_DEBUG_PRINT("PAUSED");
      player->service_->Notify(NOTIFY_PAUSED);
      break;
    }

    case GST_STATE_PLAYING: {
      GMP_DEBUG_PRINT("PLAYING");
      player->service_->Notify(NOTIFY_PLAYING);
      break;
    }

    case GST_MESSAGE_STATE_CHANGED: {
      GstElement *pipeline = player->pipeline_;
      if (GST_MESSAGE_SRC (message) == GST_OBJECT_CAST (pipeline)) {
        GstState old_state, new_state;
        gst_message_parse_state_changed (message, &old_state, &new_state, NULL);

        // generate dot graph when play start only(READY -> PAUSED)
        if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED) {
          GMP_DEBUG_PRINT("Generate dot graph from %s state to %s state.",
                gst_element_state_get_name (old_state),
                gst_element_state_get_name (new_state));
          std::string dump_name("g-media-pipeline[" + std::to_string(getpid()) + "]");
          GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
                GST_DEBUG_GRAPH_SHOW_ALL, dump_name.c_str());
        }
      }
      break;
    }

    case GST_MESSAGE_APPLICATION: {
      const GstStructure *gStruct = gst_message_get_structure(message);

      /* video-info message comes from sink element */
      if (gst_structure_has_name(gStruct, "video-info")) {
        GMP_INFO_PRINT("got video-info message");
        base::video_info_t video_info;
        memset(&video_info, 0, sizeof(base::video_info_t));
        gint width, height, fps_n, fps_d, par_n, par_d;
        gst_structure_get_int(gStruct, "width", &width);
        gst_structure_get_int(gStruct, "height", &height);
        gst_structure_get_fraction(gStruct, "framerate", &fps_n, &fps_d);
        gst_structure_get_int(gStruct, "par_n", &par_n);
        gst_structure_get_int(gStruct, "par_d", &par_d);

        GMP_INFO_PRINT("width[%d], height[%d], framerate[%d/%d], pixel_aspect_ratio[%d/%d]",
                width, height, fps_n, fps_d, par_n, par_d);

        video_info.width = width;
        video_info.height = height;
        video_info.frame_rate.num = fps_n;
        video_info.frame_rate.den = fps_d;
        // TODO: we already know this info. but it's not used now.
        video_info.bit_rate = 0;
        video_info.codec = 0;

        player->service_->Notify(NOTIFY_VIDEO_INFO, &video_info);
      }
      break;
    }

    default:  break;
  }

  return true;
}

bool UriPlayer::LoadPipeline() {
  GMP_DEBUG_PRINT("LoadPipeline planeId:%d", planeId_);

  NotifySourceInfo();

  pipeline_ = gst_element_factory_make("playbin", "playbin");

  if (!pipeline_) {
    GMP_DEBUG_PRINT("Cannot create pipeline!");
    return false;
  }

  auto elementSetupCB = +[] (GstElement *playbin, GstElement *element, UriPlayer *player) -> void {
    GMP_DEBUG_PRINT("%s element deployed!", GST_ELEMENT_NAME(element));
    if (g_strrstr(GST_ELEMENT_NAME(element), "queue2")) {
      // FIXME: should increase reference count for queue2?
      player->queue2_ = element;

      /* The default queue size limits are 100 buffers, 2MB of data, or two seconds worth of data.
       * This is too small value to support streaming playback.
       * updated from 2MB to 24MB and ten seconds temporarily.
       */
      g_object_set(element, "max-size-bytes", player->queue2MaxSizeBytes,
                            "max-size-time" , player->queue2MaxSizeTime, NULL);
    }
  };

  g_signal_connect(pipeline_, "element-setup", G_CALLBACK(elementSetupCB), this);

  GstElement *aSink = gst_element_factory_make("alsasink", NULL);
  GstElement *vSink = gst_element_factory_make("kmssink", NULL);

  if (!aSink || !vSink) {
    GMP_DEBUG_PRINT("Cannot create sink element!");
    return false;
  }
  g_object_set(G_OBJECT(vSink), "driver-name", "vc4", NULL);
  g_object_set(G_OBJECT(aSink), "device", "hw:0,0", NULL);

  g_object_set(G_OBJECT(pipeline_), "uri", uri_.c_str(),
                                    "video-sink", vSink,
                                    "audio-sink", aSink, NULL);

  if (planeId_ > 0)
    g_object_set(G_OBJECT(vSink), "plane-id", planeId_, NULL);

  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
  gst_bus_add_watch(bus, UriPlayer::HandleBusMessage, this);
  gst_object_unref(bus);

  return gst_element_set_state(pipeline_, GST_STATE_PAUSED);
}

gboolean UriPlayer::NotifyCurrentTime(gpointer user_data) {
  UriPlayer *player = reinterpret_cast<UriPlayer *>(user_data);
  gint64 pos = 0;

  if (!player->pipeline_ || player->seeking_ || !player->load_complete_)
    return true;

  if (!gst_element_query_position(player->pipeline_, GST_FORMAT_TIME, &pos)) {
    GMP_DEBUG_PRINT("gst_element_query_position fail!");
    return true;
  }

  pos = GST_TIME_AS_MSECONDS(pos);
  player->service_->Notify(NOTIFY_CURRENT_TIME, &pos);
  GMP_DEBUG_PRINT("Current position: [%" G_GINT64_FORMAT " ms]", pos);

  return true;
}

gboolean UriPlayer::NotifyBufferingTime(gpointer user_data) {
  UriPlayer *player = reinterpret_cast<UriPlayer *>(user_data);
  // TODO: no need to send buffer notification in case of file src

  gint percent;
  gint64 position, duration, buffered_time, buffering_left;
  gboolean is_buffering, fully_buffered;

  if (!player->pipeline_ || player->seeking_ || !player->load_complete_)
    return true;

  if (!gst_element_query_position (player->pipeline_, GST_FORMAT_TIME, &position)) {
    GMP_DEBUG_PRINT("gst_element_query_position fail!");
    return true;
  }

  GstQuery *query = gst_query_new_buffering (GST_FORMAT_TIME);

  if (!gst_element_query (player->pipeline_, query)) {
    GMP_DEBUG_PRINT("gst_element_query fail!");
    gst_query_unref (query);
    return true;
  }

  // percent means percentage of buffered data. This is a value between 0 and 100.
  // The is_buffering indicator is TRUE when the buffering is in progress.
  gst_query_parse_buffering_percent (query, &is_buffering, &percent);
  GMP_DEBUG_PRINT("is_buffering[%d] percent[%d]", is_buffering, percent);

  // amount of buffering time left in milliseconds.
  gst_query_parse_buffering_stats (query, NULL, NULL, NULL, &buffering_left);
  GMP_DEBUG_PRINT("buffering_left[%" G_GINT64_FORMAT " ms] -> [%d sec]",
            buffering_left, buffering_left / 1000);

  position = GST_TIME_AS_MSECONDS (position);
  duration = GST_TIME_AS_MSECONDS (player->duration_);

  buffered_time = player->queue2MaxSizeMsec - buffering_left + position;
  GMP_DEBUG_PRINT("preBuffered_time[%" G_GINT64_FORMAT " ms] buffered_time[%" G_GINT64_FORMAT " ms]",
              player->buffered_time_, buffered_time);

  // FIXME: it's inacuurate. sometimes buffered_time is reduced during buffering.
  if (player->buffered_time_ > buffered_time)
    buffered_time = player->buffered_time_;

  if (buffered_time > duration) {
    fully_buffered = true;
    buffered_time = duration;
  }

  GMP_DEBUG_PRINT("buffered_time[%" G_GINT64_FORMAT " ms] -> [%d sec]",
                    buffered_time, buffered_time / 1000);

  /*
   * beginTime = ms
   * endTime = sec
   * remainingTime = ms
   * percent = int
   */
  base::buffer_range_t bufferRange;
  bufferRange.beginTime = position;
  bufferRange.endTime = buffered_time / 1000;
  bufferRange.remainingTime = buffered_time - position;
  bufferRange.percent = percent;

  GMP_DEBUG_PRINT("beginTime[%" G_GINT64_FORMAT " ms] bufferEndTime[%d sec] buffered_time[%" G_GINT64_FORMAT " ms] percent[%d]", position, buffered_time / 1000, buffered_time - position, percent);

  player->buffered_time_ = buffered_time;
  player->service_->Notify(NOTIFY_BUFFER_RANGE, &bufferRange);
  gst_query_unref (query);

  //FIXME: need to check buffering condition for more details.
  if (percent == 100 && !is_buffering && buffering_left == 0) {
    GMP_DEBUG_PRINT("Buffering done!");
    base::playback_state_t state = player->GetPlayerState();
    if ( state == base::playback_state_t::PLAYING )
      gst_element_set_state(player->pipeline_, GST_STATE_PLAYING);
    else
      gst_element_set_state(player->pipeline_, GST_STATE_PAUSED);

    player->service_->Notify(NOTIFY_BUFFERING_END);
  } else { // else if (percent == 0 && is_buffering && buffering_left == kqueue2MaxSizeMsec)
    GMP_DEBUG_PRINT("Buffering...");
    base::playback_state_t state = player->GetPlayerState();
    if ( state != base::playback_state_t::PAUSED ) {
      GstStateChangeReturn ret = gst_element_set_state(player->pipeline_, GST_STATE_PAUSED);
      if (ret == GST_STATE_CHANGE_FAILURE) {
         GMP_DEBUG_PRINT("gst_elmenet_set_state failed!");
         return true;
      }
      player->service_->Notify(NOTIFY_BUFFERING_START);
    }
  }

  return true;
}

base::error_t UriPlayer::HandleErrorMessage(GstMessage *message) {
  GError *err = NULL;
  gchar *debug_info;
  gst_message_parse_error(message, &err, &debug_info);
  GQuark domain = err->domain;

  base::error_t error;
  error.errorCode = ConvertErrorCode(domain, (gint)err->code);
  error.errorText = g_strdup(err->message);

  GMP_DEBUG_PRINT("[GST_MESSAGE_ERROR][domain:%s][from:%s][code:%d][converted:%d][msg:%s]",
                   g_quark_to_string(domain), (GST_OBJECT_NAME(GST_MESSAGE_SRC(message))),
                   err->code, error.errorCode, err->message);
  GMP_DEBUG_PRINT("Debugging information: %s", debug_info ? debug_info : "none");

  g_clear_error(&err);
  g_free(debug_info);

  return error;
}

int32_t UriPlayer::ConvertErrorCode(GQuark domain, gint code) {
  int32_t converted = MEDIA_MSG_ERR_PLAYING;

  if (domain == GST_CORE_ERROR) {
    switch (code) {
      case GST_CORE_ERROR_EVENT:
        converted = MEDIA_MSG__GST_CORE_ERROR_EVENT;
        break;
      default:
        break;
    }
  } else if (domain == GST_LIBRARY_ERROR) {
    // do nothing
  } else if (domain == GST_RESOURCE_ERROR) {
    switch (code) {
      case GST_RESOURCE_ERROR_SETTINGS:
        converted = MEDIA_MSG__GST_RESOURCE_ERROR_SETTINGS;
        break;
      case GST_RESOURCE_ERROR_NOT_FOUND:
        converted = MEDIA_MSG__GST_RESOURCE_ERROR_NOT_FOUND;
        break;
      case GST_RESOURCE_ERROR_OPEN_READ:
        converted = MEDIA_MSG__GST_RESOURCE_ERROR_OPEN_READ;
        break;
      case GST_RESOURCE_ERROR_READ:
        converted = MEDIA_MSG__GST_RESOURCE_ERROR_READ;
        break;
      default:
        break;
    }
  } else if (domain == GST_STREAM_ERROR) {
    switch (code) {
      case GST_STREAM_ERROR_TYPE_NOT_FOUND:
        converted = MEDIA_MSG__GST_STREAM_ERROR_TYPE_NOT_FOUND;
        break;
      case GST_STREAM_ERROR_DEMUX:
        converted = MEDIA_MSG__GST_STREAM_ERROR_DEMUX;
        break;
      default:
        break;
    }
  }

  return converted;
}

void UriPlayer::SetGstreamerDebug() {
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

bool UriPlayer::FindQueue2Element()
{
  if (!pipeline_) {
    GMP_DEBUG_PRINT("pipeline is NULL!");
    return false;
  }

  auto compare_q2_string = +[] (const GValue *item, const gpointer user_data) -> gint {
    GstElement *element = reinterpret_cast<GstElement *>(g_value_get_object(item));
    GMP_DEBUG_PRINT("%s element deployed!", GST_ELEMENT_NAME(element));

    if (!g_strrstr(GST_ELEMENT_NAME(element), "queue2")) return 1;
    else return 0;
  };

  gboolean found = FALSE;
  GValue item = {0, };
  GstIterator *it = gst_bin_iterate_recurse(GST_BIN_CAST (pipeline_));
  found = gst_iterator_find_custom(it,
          (GCompareFunc) compare_q2_string, &item, &pipeline_);
  gst_iterator_free(it);

  if (found) queue2_ = reinterpret_cast<GstElement *>(g_value_get_object(&item));

  g_value_unset(&item);
  return found;
}

}  // namespace player
}  // namespace gmp
