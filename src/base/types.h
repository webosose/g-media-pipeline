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

#ifndef SRC_BASE_TYPES_H_
#define SRC_BASE_TYPES_H_

#include <string>
#include <vector>

/**
 * video codec
 */
typedef enum {
  GMP_VIDEO_CODEC_NONE,
  GMP_VIDEO_CODEC_H264,
  GMP_VIDEO_CODEC_VC1,
  GMP_VIDEO_CODEC_MPEG2,
  GMP_VIDEO_CODEC_MPEG4,
  GMP_VIDEO_CODEC_THEORA,
  GMP_VIDEO_CODEC_VP8,
  GMP_VIDEO_CODEC_VP9,
  GMP_VIDEO_CODEC_H265,
  GMP_VIDEO_CODEC_MJPEG,
  GMP_VIDEO_CODEC_MAX = GMP_VIDEO_CODEC_MJPEG,
} GMP_VIDEO_CODEC;

/**
 * audio codec
 */
typedef enum {
  GMP_AUDIO_CODEC_NONE,
  GMP_AUDIO_CODEC_AAC,
  GMP_AUDIO_CODEC_MP3,
  GMP_AUDIO_CODEC_PCM,
  GMP_AUDIO_CODEC_VORBIS,
  GMP_AUDIO_CODEC_FLAC,
  GMP_AUDIO_CODEC_AMR_NB,
  GMP_AUDIO_CODEC_AMR_WB,
  GMP_AUDIO_CODEC_PCM_MULAW,
  GMP_AUDIO_CODEC_GSM_MS,
  GMP_AUDIO_CODEC_PCM_S16BE,
  GMP_AUDIO_CODEC_PCM_S24BE,
  GMP_AUDIO_CODEC_OPUS,
  GMP_AUDIO_CODEC_EAC3,
  GMP_AUDIO_CODEC_PCM_ALAW,
  GMP_AUDIO_CODEC_ALAC,
  GMP_AUDIO_CODEC_AC3,
  GMP_AUDIO_CODEC_DTS,
  GMP_AUDIO_CODEC_MAX = GMP_AUDIO_CODEC_DTS,
} GMP_AUDIO_CODEC;


// TODO(anonymous): split this on domains,
// e.g. media_types.h, ipc_types.h, error.h, etc.

namespace gmp { namespace base {

enum class playback_state_t : int {
  STOPPED,
  LOADED,
  PAUSED,
  PLAYING,
};

typedef int64_t time_t;   // time in milliseconds

struct rational_t {
  int32_t num;
  int32_t den;
};

struct stream_info_t {
  uint64_t bit_rate;
};

struct video_info_t : stream_info_t {
  uint32_t width;
  uint32_t height;
  rational_t frame_rate;
  GMP_VIDEO_CODEC codec;
};

struct audio_info_t : stream_info_t {
  int32_t channels;
  uint32_t sample_rate;
  GMP_AUDIO_CODEC codec;
};

struct program_info_t {
  uint32_t video_stream;
  uint32_t audio_stream;
};

struct source_info_t {
  std::string container;
  time_t duration;
  bool seekable;
  std::vector<program_info_t> programs;
  std::vector<video_info_t> video_streams;
  std::vector<audio_info_t> audio_streams;
};

struct result_t {
  bool state;
  std::string mediaId;
};

struct error_t {
  int32_t errorCode;
  std::string errorText;
  std::string mediaId;
};

struct buffer_range_t {
  int64_t beginTime;
  int64_t endTime;
  int64_t remainingTime;
  int64_t percent;
};

struct disp_res_t {
  int32_t plane_id;
  int32_t crtc_id;
  int32_t conn_id;
};

}  // namespace base
}  // namespace gmp

#endif  // SRC_BASE_TYPES_H_
