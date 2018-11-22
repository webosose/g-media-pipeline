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

#include "MediaPlayerClient.h"

#include "BufferPlayer.h"
#include "log.h"

namespace gmp { namespace player {

MediaPlayerClient::MediaPlayerClient(const std::string& appId)
    : playerContext_(NULL),
      isStopCalled_(false) {
  player_.reset(new gmp::player::BufferPlayer(appId));
  GMP_INFO_PRINT(" Player Created [%p]", player_.get());
}

MediaPlayerClient::~MediaPlayerClient() {
  GMP_INFO_PRINT(" START");
  Stop();
  GMP_INFO_PRINT("END");
}

bool MediaPlayerClient::Load(const MEDIA_LOAD_DATA_T* loadData) {
  GMP_DEBUG_PRINT("Load loadData = %p", loadData);

  if (!player_) {
    GMP_DEBUG_PRINT("Bufferplayer not created");
    return false;
  }

  player_->Initialize(NULL);

  uint32_t display_path = (loadData->displayPath >= MAX_NUM_DISPLAY) ? 0 : loadData->displayPath;
  base::source_info_t source_info = GetSourceInfo(loadData);
  if (!player_->AcquireResources(source_info, display_path)) {
    GMP_DEBUG_PRINT("resouce acquire fail!");
    return false;
  }

  if (player_->Load(loadData)) {
    GMP_DEBUG_PRINT("Loaded Bufferplayer");
  } else {
    GMP_DEBUG_PRINT("Failed to load player");
    return false;
  }

  return true;
}

bool MediaPlayerClient::Play() {
  GMP_DEBUG_PRINT("Play");

  if (!player_) {
    GMP_DEBUG_PRINT("Bufferplayer not created");
    return false;
  }

  return player_->Play();
}

bool MediaPlayerClient::Pause() {
  GMP_DEBUG_PRINT("Pause");

  if (!player_) {
    GMP_DEBUG_PRINT("Bufferplayer not created");
    return false;
  }

  return player_->Pause();
}

bool MediaPlayerClient::Stop() {
  GMP_INFO_PRINT("START");

  if (isStopCalled_)
    return true;

  if (!player_) {
    GMP_DEBUG_PRINT("Bufferplayer not created");
    return false;
  }

  player_->NotifyBackground();
  player_->ReleaseResources();

  player_->Unload();

  isStopCalled_ = true;
  GMP_INFO_PRINT("END");
  return true;
}

bool MediaPlayerClient::Seek(int position) {
  GMP_DEBUG_PRINT("Seek");

  if (!player_) {
    GMP_DEBUG_PRINT("Bufferplayer not created");
    return false;
  }

  return player_->Seek(position);
}

bool MediaPlayerClient::SetPlane(int planeId) {
  GMP_DEBUG_PRINT("SetPlane");

  if (!player_) {
    GMP_DEBUG_PRINT("Bufferplayer not created");
    return false;
  }

  return player_->SetPlane(planeId);
}

MEDIA_STATUS_T MediaPlayerClient::Feed(const guint8* pBuffer,
                                             guint32 bufferSize,
                                             guint64 pts,
                                             MEDIA_DATA_CHANNEL_T esData) {
  if (!player_) {
    GMP_DEBUG_PRINT("Bufferplayer not created");
    return MEDIA_ERROR;
  }

  return player_->Feed(pBuffer, bufferSize, pts, esData);
}

bool MediaPlayerClient::Flush() {
  GMP_DEBUG_PRINT("Flush");

  if (!player_) {
    GMP_DEBUG_PRINT("Bufferplayer not created");
    return false;
  }

  return player_->Flush();
}

bool MediaPlayerClient::SetDisplayWindow(const long left,
                                         const long top,
                                         const long width,
                                         const long height,
                                         const bool isFullScreen) {
  GMP_DEBUG_PRINT("SetDisplayWindow");

  if (!player_) {
    GMP_DEBUG_PRINT("Bufferplayer not created");
    return false;
  }

  return player_->SetDisplayWindow(left, top, width, height, isFullScreen);
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
  GMP_DEBUG_PRINT("SetCustomDisplayWindow");

  if (!player_) {
    GMP_DEBUG_PRINT("Bufferplayer not created");
    return false;
  }

  return player_->SetCustomDisplayWindow(srcLeft,
                                         srcTop,
                                         srcWidth,
                                         srcHeight,
                                         destLeft,
                                         destTop,
                                         destWidth,
                                         destHeight,
                                         isFullScreen);
}

bool MediaPlayerClient::RegisterCallback(
    GMP_CALLBACK_FUNCTION_T cbFunction, void *userData) {
  GMP_DEBUG_PRINT("RegisterCallback");

  if (!player_) {
    GMP_DEBUG_PRINT("Bufferplayer not created");
    return false;
  }

  player_->RegisterCbFunction(cbFunction, userData);
  return true;
}

bool MediaPlayerClient::PushEndOfStream() {
  GMP_DEBUG_PRINT("PushEndOfStream");

  if (!player_) {
    GMP_DEBUG_PRINT("Bufferplayer not created");
    return false;
  }

  return player_->PushEndOfStream();
}

bool MediaPlayerClient ::NotifyForeground() {
  GMP_DEBUG_PRINT("NotifyForeground");

  if (!player_) {
    GMP_DEBUG_PRINT("Bufferplayer not created");
    return false;
  }

  return player_->NotifyForeground();
}

bool MediaPlayerClient::NotifyBackground() {
  GMP_DEBUG_PRINT("NotifyBackground");

  if (!player_) {
    GMP_DEBUG_PRINT("Bufferplayer not created");
    return false;
  }

  return player_->NotifyBackground();
}

bool MediaPlayerClient::SetVolume(int volume) {
  GMP_DEBUG_PRINT("SetVolume");

  if (!player_) {
    GMP_DEBUG_PRINT("Bufferplayer not created");
    return false;
  }

  player_->SetVolume(volume);
  return true;
}

bool MediaPlayerClient::SetExternalContext(GMainContext *context) {
  GMP_DEBUG_PRINT("context = %p", context);
  playerContext_ = context;
}

bool MediaPlayerClient::SetPlaybackRate(const double playbackRate) {
  GMP_DEBUG_PRINT("playbackRate = %f", playbackRate);
  if (player_)
    return player_->SetPlayRate(playbackRate);
  return false;
}

const char* MediaPlayerClient::GetMediaID() {
  if (player_)
    return player_->GetMediaID().c_str();
  return NULL;
}

base::source_info_t MediaPlayerClient::GetSourceInfo(
    const MEDIA_LOAD_DATA_T* loadData) {
  GMP_DEBUG_PRINT("loadData = %p", loadData);

  base::source_info_t source_info = {};

  base::video_info_t video_stream_info = {};
  base::audio_info_t audio_stream_info = {};

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

}  // End of namespace player

}  // End of namespace gmp
