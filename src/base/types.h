// Copyright (c) 2018-2020 LG Electronics, Inc.
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

#include <cstdint>
#include <string>
#include <vector>

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

  rational_t()
  :num(0), den(1) {}
};

struct stream_info_t {
  int32_t codec;
  uint64_t bit_rate;

  stream_info_t()
  :codec(0), bit_rate(0) {}
};

struct video_info_t : stream_info_t {
  uint32_t width;
  uint32_t height;
  rational_t frame_rate;

  video_info_t()
  :width(0), height(0),
   frame_rate() {}
};

struct audio_info_t : stream_info_t {
  int32_t channels;
  uint32_t sample_rate;

  audio_info_t()
  :channels(0), sample_rate(0) {}
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

  source_info_t()
  :container(""), duration(-1),
   seekable(true), programs(),
   video_streams(),audio_streams() {}
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

struct media_info_t {
  std::string mediaId;
};

struct load_param_t {
  std::string videoDisplayMode;
  int32_t displayPath;
  std::string windowId;
  std::string uri;
};

}  // namespace base
}  // namespace gmp

#endif  // SRC_BASE_TYPES_H_
