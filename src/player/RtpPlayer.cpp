// Copyright (c) 2022 LG Electronics, Inc.
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

#include "RtpPlayer.h"
#include "ElementFactory.h"

namespace gmp { namespace player {

bool UriRtpPlayer::mIsObjRegistered = UriRtpPlayer::RegisterObject();


UriRtpPlayer::UriRtpPlayer() {}

UriRtpPlayer::~UriRtpPlayer() {}

bool UriRtpPlayer::LoadPipeline() {

  pipeline_ = gst_pipeline_new("rtp-player");
  if (!pipeline_) {
    GMP_INFO_PRINT("ERROR : Cannot create rtp pipeline!");
    return false;
  }
  /*1. create rtpsrc*/
  pSrcElement_ = gst_element_factory_make("rtpsrc", "streamingrtpsrc");

  if(!pSrcElement_) {
    GMP_INFO_PRINT("ERROR : No rtpsrc element !");
    return false;
  }
  /*2. create decodebin*/
  pDecElement_ = gst_element_factory_make("decodebin","decodebin");

  if(!pDecElement_) {
    GMP_INFO_PRINT("ERROR : No decodebin element !");
    return false;
  }
  auto addDecoderCB = +[] (GstElement *decodebin, GstPad *pad, UriRtpPlayer *player) -> void {
    GstPad *dec_sinkpad;
    GstPadLinkReturn pad_return;

    dec_sinkpad = gst_element_get_static_pad(player->pDecElement_,"sink");
    if (GST_PAD_IS_LINKED (dec_sinkpad)) {
      g_object_unref (dec_sinkpad);
      return;
    }
    /* link'n'play srcelement to decodebin */
    pad_return = gst_pad_link (pad, dec_sinkpad);
    if(pad_return != GST_PAD_LINK_OK) {
      GMP_INFO_PRINT("ERROR : Linking failed for the pads !!");
      return;
    }
    gst_element_sync_state_with_parent(player->pDecElement_);
    g_object_unref (dec_sinkpad);
  };

  auto addSinkCB = +[] (GstElement *decodebin, GstPad *pad, UriRtpPlayer *player) -> void {
    GstCaps *caps;
    GstStructure *str;

    caps = gst_pad_query_caps (pad, NULL);
    str = gst_caps_get_structure (caps, 0);
    gst_caps_unref(caps);

    if (g_strrstr (gst_structure_get_name (str), "video")) {
      GstPad *video_sinkpad;
      GstPadLinkReturn pad_return;

      video_sinkpad = gst_element_get_static_pad (player->pVSinkElement_, "sink");
      if (GST_PAD_IS_LINKED (video_sinkpad)) {
        g_object_unref (video_sinkpad);
        return;
      }

    /* link'n'play */
    pad_return = gst_pad_link (pad, video_sinkpad);
    if(pad_return != GST_PAD_LINK_OK) {
      GMP_INFO_PRINT("ERROR :  Linking failed for the pads !!");
      return;
    }
    gst_element_sync_state_with_parent(player->pVSinkElement_);
    g_object_unref (video_sinkpad);
  } else if(g_strrstr (gst_structure_get_name (str), "audio")) {
    GstPad *audio_sinkpad;
    GstPadLinkReturn pad_return;

    /* only link once */
    audio_sinkpad = gst_element_get_static_pad (player->pAConvertElement_, "sink");
    if (GST_PAD_IS_LINKED (audio_sinkpad)) {
      g_object_unref (audio_sinkpad);
      return;
    }
    /* link'n'play */
    pad_return = gst_pad_link (pad, audio_sinkpad);
    if(pad_return != GST_PAD_LINK_OK) {
      GMP_INFO_PRINT("ERROR :  Linking failed for the pads !!");
      return;
    }
    gst_element_sync_state_with_parent(player->pAConvertElement_);
    g_object_unref (audio_sinkpad);
    }
  };

  g_signal_connect(pSrcElement_,"pad-added", G_CALLBACK(addDecoderCB),this);
  g_signal_connect(pDecElement_,"pad-added", G_CALLBACK(addSinkCB),this);

  pVSinkElement_ = gst_element_factory_make("waylandsink", "waylandsink");
  if(!pVSinkElement_) {
    GMP_INFO_PRINT("ERROR : No waylandsink element !");
    return false;
  }
  std::string aSinkName = "audio-sink";
  aSink = pf::ElementFactory::Create("custom", aSinkName, display_path_);
  if(!aSink) {
    GMP_INFO_PRINT("ERROR : No pulsesink element !");
    return false;
  }

  //set stream-properties for pulsesink
  if (!RegisterTrack()) {
    GMP_INFO_PRINT("RegisterTrack failed ");
  }

  pAConvertElement_ = gst_element_factory_make("audioconvert", "audioconvert");
  if(!pAConvertElement_) {
    GMP_INFO_PRINT("ERROR : No audioconvert element !");
    return false;
  }
  gst_bin_add_many(GST_BIN(pipeline_),pSrcElement_, pDecElement_, pAConvertElement_, aSink, pVSinkElement_, NULL);
  if (!gst_element_link(pAConvertElement_, aSink)) {
    GMP_INFO_PRINT("ERROR : Linking failed pAConvertElement_ -> aSink !" );
    return false;
  }

  g_object_set(G_OBJECT(pSrcElement_), "uri", uri_.c_str(), NULL);
  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
  gst_bus_add_watch(bus, UriPlayer::HandleBusMessage, this);
  gst_bus_set_sync_handler(bus, UriPlayer::HandleSyncBusMessage,
                           &lsm_connector_, NULL);

  gst_object_unref(bus);
  GMP_INFO_PRINT("LoadPipeline and Play");
  gst_element_set_state(pipeline_, GST_STATE_PLAYING);
  return true;
}

}  // namespace player
}  // namespace gmp
