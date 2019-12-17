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
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// SPDX-License-Identifier: Apache-2.0

#include <iostream>
#include <string>
#include "StreamSource.h"

namespace gmp { namespace test {

static gboolean print_field(GQuark field, const GValue* value, gpointer pfx)
{
  gchar* str = gst_value_serialize(value);
  g_print("%s %15s: %s\n", (gchar*)pfx, g_quark_to_string(field), str);
  g_free(str);
  return TRUE;
}

static void print_caps(const GstCaps* caps, const gchar* pfx)
{
  g_return_if_fail (caps != NULL);

  if (gst_caps_is_any (caps)) {
    g_print ("%sANY\n", pfx);
    return;
  }
  if (gst_caps_is_empty (caps)) {
    g_print ("%sEMPTY\n", pfx);
    return;
  }

  for (guint i = 0; i < gst_caps_get_size (caps); i++) {
    GstStructure *structure = gst_caps_get_structure (caps, i);

    g_print ("%s%s\n", pfx, gst_structure_get_name (structure));
    gst_structure_foreach (structure, print_field, (gpointer) pfx);
  }
}

/* stream source structure
 * +------------------------------------------------------------------------------------------+
 * | stream-source                                                                            |
 * |                                         +--------------------+    +-------------------+  |
 * |                                         |     multiqueue     |    |    videoAppSink   |  |
 * |                                      ┌-sink                 src--sink                 |  |
 * |      +----------+   +-------------+  |  |                    |    |                   |  |
 * |      |  source  |   |   parseBin src--  |                    |    +-------------------+  |
 * |      |         src-sink  .....    |     |                    |                           |
 * |      |          |   |            src--  |                    |    +-------------------+  |
 * |      +----------+   +-------------+  |  |                    |    |    audioAppSink   |  |
 * |                                      └-sink                 src--sink                 |  |
 * |                                         |                    |    |                   |  |
 * |                                         +--------------------+    +-------------------+  |
 * +------------------------------------------------------------------------------------------+
 */

StreamSource::StreamSource(const std::string& windowID)
  : windowID_(windowID)
  , mediaID_(std::string("BufferPlayerTest"))
  , pipeline_(nullptr)
  , source_(nullptr)
  , parseBin_(nullptr)
  , multiQueue_(nullptr)
  , videoAppSink_(nullptr)
  , audioAppSink_(nullptr)
  , media_player_client_(nullptr)
  , firstVideoSample(false)
  , firstAudioSample(false)
{
}

StreamSource::~StreamSource()
{
  windowID_.clear();
  mediaID_.clear();

  ClearResources();

  if (media_player_client_) {
    media_player_client_.reset();
  }
  std::cout << "dtor!" << std::endl;
}

void StreamSource::SetURI(const std::string& uri)
{
  uri_ = uri;
}

std::string StreamSource::GetURI() const
{
  return uri_;
}

void StreamSource::SetWindowID(const std::string& windowID)
{
  windowID_ = windowID;
}

std::string StreamSource::GetWindowID() const
{
  return windowID_;
}

bool StreamSource::Load(const std::string& uri)
{
  bool ret = false;
  if (!uri.empty())
    uri_ = uri;
  else
    return ret;

  ClearResources();

  if (media_player_client_) {
    media_player_client_->Unload();
    media_player_client_.reset();
  }

  /* appID is empty */
  media_player_client_ = std::make_unique<gmp::player::MediaPlayerClient>(appID_, mediaID_);

  if (!media_player_client_) {
    std::cout << std::string("media player client creation fail!") << std::endl;
    return ret;
  }

  media_player_client_->RegisterCallback(
    std::bind(&StreamSource::Notify, this,
      std::placeholders::_1, std::placeholders::_2,
      std::placeholders::_3, std::placeholders::_4));

  MEDIA_LOAD_DATA load_data;
  if (!MakeLoadData(0, &load_data)) {
    std::cout << std::string("Making load data info failed!") << std::endl;
    return ret;
  }

  /* media player client should be set PLAYING state before feeding */
  ret = media_player_client_->Load(&load_data);
  if (!ret) {
    std::cout << std::string("Load failed!") << std::endl;
    return ret;
  }

  /* change Play state */
  ret = media_player_client_->Play();
  if (!ret) {
    std::cout << std::string("Play failed for media player!") << std::endl;
    return ret;
  }

  ret = CreatePipeLine();
  gst_element_set_state(pipeline_, GST_STATE_PLAYING);

  return ret;
}

bool StreamSource::Play()
{
  if (pipeline_ == NULL) {
    std::cout << std::string("pipeline is NULL!") << std::endl;
    return false;
  }

  gst_element_set_state(pipeline_, GST_STATE_PLAYING);

  return media_player_client_->Play();
}

bool StreamSource::Pause()
{
  if (pipeline_ == NULL) {
    std::cout << std::string("pipeline is NULL!") << std::endl;
    return false;
  }

  gst_element_set_state(pipeline_, GST_STATE_PAUSED);

  return media_player_client_->Pause();
}

bool StreamSource::Seek(const std::string& pos)
{
  if (pipeline_ == NULL) {
    std::cout << std::string("pipeline is NULL!") << std::endl;
    return false;
  }

  int64_t sec = std::stoi(pos);
  if (sec < 0) {
    std::cout << std::string("position is wrong! position = ") << sec << std::endl;
    return false;
  }

  if (!gst_element_seek(pipeline_, 1.0, GST_FORMAT_TIME,
          GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
          GST_SEEK_TYPE_SET, GST_TIME_AS_NSECONDS(sec),
          GST_SEEK_TYPE_NONE, 0)) {
    std::cout << std::string("Seek failed!") << std::endl;
    return false;
  }

  if (media_player_client_)
    return media_player_client_->Seek(GST_TIME_AS_MSECONDS(sec));
  else
    return false;
}

bool StreamSource::Unload()
{
  ClearResources();

  return media_player_client_->Unload();
}

bool StreamSource::MakeLoadData(int64_t start_time, MEDIA_LOAD_DATA_T* load_data)
{
  /* TODO: need to consider all media contents */
  load_data->maxWidth = 1920;
  load_data->maxHeight = 1080;
  load_data->maxFrameRate = 30;
  load_data->videoCodec = GMP_VIDEO_CODEC_H264;
  load_data->audioCodec = GMP_AUDIO_CODEC_AAC;
  load_data->ptsToDecode = start_time;
  load_data->windowId = const_cast<char*>(windowID_.c_str());
  return true;
}

void StreamSource::ParseBinPadAddedCB(GstElement* element, GstPad* pad, gpointer userData)
{
  StreamSource *streamSrc = reinterpret_cast<StreamSource *>(userData);

  gchar* parseBinPadName = gst_pad_get_name(pad);
  std::cout << "[ParseBinCB] parseBin pad name -> " << parseBinPadName << std::endl;

  GstPadTemplate* mqSinkTemplate =
    gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(streamSrc->multiQueue_), "sink_%u");

  if (mqSinkTemplate == NULL) {
    std::cout << "multiQueue sink template NULL!" << std::endl;
    g_free(parseBinPadName);
    return;
  }

  // request pad should be release using gst_element_release_request_pad API
  GstPad* mqSinkPad = gst_element_request_pad(streamSrc->multiQueue_, mqSinkTemplate, NULL, NULL);
  if (mqSinkPad == NULL) {
    std::cout << "multiQueue request pad is NULL!" << std::endl;
    g_free(parseBinPadName);
    return;
  }

  streamSrc->multiQueueSinkPads_.push_back(mqSinkPad);

  gchar* mqPadName = gst_pad_get_name(mqSinkPad);
  std::cout << "multiQueue pad name -> " << mqPadName << std::endl;

  GstPadLinkReturn ret = gst_pad_link(pad, mqSinkPad);
  if (ret != GST_PAD_LINK_OK)
    std::cout << "link fail!" << std::endl;

  g_free(parseBinPadName);
  g_free(mqPadName);
}

void StreamSource::MultiQueuePadAddedCB(GstElement* element, GstPad* pad, gpointer userData)
{
  StreamSource *streamSrc = reinterpret_cast<StreamSource *>(userData);
  gchar* mqPadName = gst_pad_get_name(pad);
  std::cout << "[MultiQueueCB] multiQueue pad name -> " << mqPadName << std::endl;

  if (!g_strcmp0(mqPadName, "src_0")) { // video
    GstPad* appSinkPad = gst_element_get_static_pad(streamSrc->videoAppSink_, "sink");
    if (appSinkPad != NULL) {
      GstPadLinkReturn ret = gst_pad_link(pad, appSinkPad);
      if (ret != GST_PAD_LINK_OK)
        std::cout << "link fail!" << std::endl;

      gst_object_unref(appSinkPad);
    }
  }

  if (!g_strcmp0(mqPadName, "src_1")) { // audio
    GstPad* appSinkPad = gst_element_get_static_pad(streamSrc->audioAppSink_, "sink");
    if (appSinkPad != NULL) {
      GstPadLinkReturn ret = gst_pad_link(pad, appSinkPad);
      if (ret != GST_PAD_LINK_OK)
        std::cout << "link fail!" << std::endl;

      gst_object_unref(appSinkPad);
    }
  }

  // need to support subtitle case?

  g_free(mqPadName);
}

GstFlowReturn StreamSource::VideoAppSinkSampleAddedCB(GstElement *element, gpointer userData)
{
  StreamSource *streamSrc = reinterpret_cast<StreamSource *>(userData);
  gchar* elementName = gst_element_get_name(element);
  MEDIA_STATUS_T ret;
  GstElement* pipeline = streamSrc->media_player_client_->GetPipeline();
  if (!pipeline)
    return GST_FLOW_ERROR;

  GstElement* source = gst_bin_get_by_name(GST_BIN(pipeline), "video-app-es");

  /* get the sample from appsink */
  GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(element));
  GstBuffer* buffer = gst_sample_get_buffer(sample);

  /* make a copy */
  GstBuffer* appBuffer = gst_buffer_copy(buffer);
  GST_BUFFER_PTS(appBuffer) = GST_CLOCK_TIME_NONE;

  /* we don't need the appsink sample anymore */
  gst_sample_unref(sample);

  // can't use static variable after unload. so added firstVideoSample variable.
  if (!streamSrc->firstVideoSample) {
    GstPad* srcPad = gst_element_get_static_pad(source, "src");
    GstPad* sinkPad = gst_element_get_static_pad(element, "sink");
    GstCaps* caps = gst_pad_get_current_caps(sinkPad);
    gst_app_src_set_caps(GST_APP_SRC(source), caps);
    streamSrc->firstVideoSample = true;
//    print_caps(caps, "\t");
    gst_object_unref(srcPad);
    gst_object_unref(sinkPad);
  }

  /* get buffer and size from Gstbuffer */
  GstMapInfo mapInfo;
  gst_buffer_map(appBuffer, &mapInfo, GST_MAP_READ);

  guint8* bufferData = reinterpret_cast<guint8 *>(mapInfo.data);
  guint32 bufferSize = mapInfo.size; // gst_buffer_get_size(AppBuffer)

  ret = streamSrc->media_player_client_->Feed(bufferData, bufferSize, GST_CLOCK_TIME_NONE, MEDIA_DATA_CH_A);

  gst_buffer_unmap(appBuffer, &mapInfo);
  g_free(elementName);
  gst_object_unref(source);

  if (ret == MEDIA_OK)
    return GST_FLOW_OK;
  else
    return GST_FLOW_ERROR;
}

GstFlowReturn StreamSource::AudioAppSinkSampleAddedCB(GstElement *element, gpointer userData)
{
  StreamSource *streamSrc = reinterpret_cast<StreamSource *>(userData);
  gchar* elementName = gst_element_get_name(element);
  MEDIA_STATUS_T ret;
  GstElement* pipeline = streamSrc->media_player_client_->GetPipeline();
  if (!pipeline)
    return GST_FLOW_ERROR;

  GstElement* source = gst_bin_get_by_name(GST_BIN(pipeline), "audio-app-es");

  /* get the sample from appsink */
  GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(element));
  GstBuffer* buffer = gst_sample_get_buffer(sample);

  /* make a copy */
  GstBuffer* appBuffer = gst_buffer_copy(buffer);
  GST_BUFFER_PTS(appBuffer) = GST_CLOCK_TIME_NONE;

  /* we don't need the appsink sample anymore */
  gst_sample_unref(sample);

  // can't use static variable after unload. so added firstAudioSample variable.
  if (!streamSrc->firstAudioSample) {
    GstPad* srcPad = gst_element_get_static_pad(source, "src");
    GstPad* sinkPad = gst_element_get_static_pad(element, "sink");
    GstCaps* caps = gst_pad_get_current_caps(sinkPad);
    gst_app_src_set_caps(GST_APP_SRC(source), caps);
    streamSrc->firstAudioSample = true;
//    print_caps(caps, "\t");
    gst_object_unref(srcPad);
    gst_object_unref(sinkPad);
  }

  /* get buffer and size from Gstbuffer */
  GstMapInfo mapInfo;
  gst_buffer_map(appBuffer, &mapInfo, GST_MAP_READ);

  guint8* bufferData = reinterpret_cast<guint8 *>(mapInfo.data);
  guint32 bufferSize = mapInfo.size;

  ret = streamSrc->media_player_client_->Feed(bufferData, bufferSize, GST_CLOCK_TIME_NONE, MEDIA_DATA_CH_B);

  gst_buffer_unmap(appBuffer, &mapInfo);
  g_free(elementName);
  gst_object_unref(source);

  if (ret == MEDIA_OK)
    return GST_FLOW_OK;
  else
    return GST_FLOW_ERROR;
}

bool StreamSource::CreatePipeLine()
{
  bool ret = false;

  /* create stream-source pipeline */
  pipeline_ = gst_pipeline_new("stream-source");
  if (!pipeline_) {
    std::cout << "stream-source pipeline creation fail!" << std::endl;
    return ret;
  }

  /* create source element. don't need to set uri property. it will be set internally. */
  source_ = gst_element_make_from_uri(GST_URI_SRC, uri_.c_str(), "source", NULL);
  if (!source_) {
    std::cout << "source creation fail!" << std::endl;
    return ret;
  }

  /* create parsebin element and pad-added signal connect */
  parseBin_ = gst_element_factory_make("parsebin", "parseBin");
  if (!parseBin_) {
    std::cout << "parsebin creation fail!" << std::endl;
    return ret;
  }

  parseBinCB_id_ = g_signal_connect(G_OBJECT(parseBin_), "pad-added", G_CALLBACK(ParseBinPadAddedCB), this);

  /* create multiqueue element and pad-added signal connect */
  multiQueue_ = gst_element_factory_make("multiqueue", "multiQueue");
  if (!multiQueue_) {
    std::cout << "multiqueue creation fail!" << std::endl;
    return ret;
  }

  multiQueueCB_id_ = g_signal_connect(G_OBJECT(multiQueue_), "pad-added", G_CALLBACK(MultiQueuePadAddedCB), this);

  /* create appsink element for video and new-sample signal connect */
  videoAppSink_ = gst_element_factory_make("appsink", "videoAppSink");
  if (!videoAppSink_) {
    std::cout << "videoappsink creation fail!" << std::endl;
    return ret;
  }

  g_object_set(G_OBJECT(videoAppSink_), "emit-signals", TRUE, NULL);
  videoAppSinkCB_id_ = g_signal_connect(G_OBJECT(videoAppSink_), "new-sample", G_CALLBACK(VideoAppSinkSampleAddedCB), this);

  /* create appsink element for audio and new-sample signal connect */
  audioAppSink_ = gst_element_factory_make("appsink", "audioAppSink");
  if (!audioAppSink_) {
    std::cout << "audioappsink creation fail!" << std::endl;
    return ret;
  }

  g_object_set(G_OBJECT(audioAppSink_), "emit-signals", TRUE, NULL);
  audioAppSinkCB_id_ = g_signal_connect(G_OBJECT(audioAppSink_), "new-sample", G_CALLBACK(AudioAppSinkSampleAddedCB), this);

  /* add all element into pipeline */
  gst_bin_add_many(GST_BIN(pipeline_), source_, parseBin_, multiQueue_, videoAppSink_, audioAppSink_, NULL);

  ret = gst_element_link(source_, parseBin_);
  if (!ret) {
    std::cout << std::string("element link fail between source and parsebin!") << std::endl;
    return ret;
  }

  return ret;
}

void StreamSource::ClearResources()
{
  /* release requested pad for multiqueue */
  for(GstPad* pad : multiQueueSinkPads_) {
    if (pad) {
      gchar *padName = gst_pad_get_name(pad);
      std::cout << std::string("multiqueue pad remove! pad name : ") << padName << std::endl;
      if (multiQueue_)
        gst_element_release_request_pad(multiQueue_, pad);
      gst_object_unref(pad);
      g_free(padName);
    }
  }

  multiQueueSinkPads_.clear();

  /* disconnect signal handlers */
  if (parseBinCB_id_)
    g_signal_handler_disconnect(parseBin_, parseBinCB_id_);
  if (multiQueueCB_id_)
    g_signal_handler_disconnect(multiQueue_, multiQueueCB_id_);
  if (videoAppSinkCB_id_)
    g_signal_handler_disconnect(videoAppSink_, videoAppSinkCB_id_);
  if (audioAppSinkCB_id_)
    g_signal_handler_disconnect(audioAppSink_, audioAppSinkCB_id_);

  parseBinCB_id_ =  multiQueueCB_id_ = videoAppSinkCB_id_ = audioAppSinkCB_id_ = 0;

  if (pipeline_) {
    gst_element_set_state(pipeline_, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(pipeline_));
    pipeline_ = NULL;
  }

  firstVideoSample = false;
  firstAudioSample = false;

  std::cout << "clear Done!" << std::endl;
}

void StreamSource::Notify(const gint notification, const gint64 numValue, const gchar* strValue, void* payload)
{
  switch (notification) {
    case NOTIFY_LOAD_COMPLETED: {
      std::cout << std::string("\nloadCompleted!") << std::endl;
      break;
    }

    case NOTIFY_UNLOAD_COMPLETED: {
      std::cout << std::string("\nunloadCompleted!") << std::endl;
      break;
    }

    case NOTIFY_END_OF_STREAM: {
      std::cout << std::string("\nendOfStream!") << std::endl;
      break;
    }

    case NOTIFY_SEEK_DONE: {
      std::cout << std::string("\nseekDone!") << std::endl;
      break;
    }

    case NOTIFY_PLAYING: {
      std::cout << std::string("\nplaying!") << std::endl;
      break;
    }

    case NOTIFY_PAUSED: {
      std::cout << std::string("\npaused!") << std::endl;
      break;
    }

    case NOTIFY_BUFFERING_START: {
      std::cout << std::string("\nbuffering start!") << std::endl;
      break;
    }

    case NOTIFY_BUFFERING_END: {
      std::cout << std::string("\nbuffering end!") << std::endl;
      break;
    }

    default:
      break;
  }
}

}  // namespace test
}  // namespace gmp
