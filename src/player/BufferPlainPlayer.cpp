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

#include "BufferPlainPlayer.h"

namespace gmp {
namespace player {

BufferPlainPlayer::BufferPlainPlayer()
{
}

BufferPlainPlayer::~BufferPlainPlayer() {
}

bool BufferPlainPlayer::CreatePipeline() {
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

  if(inputDumpFileName != NULL) {
    GMP_INFO_PRINT("Create Element Dumping data");

    if (!AddDumpElements()) {
      GMP_INFO_PRINT("Failed to Create and Add filesink !!!");
      return false;
    }
  } else {
    GMP_INFO_PRINT("Create Elements for playback");

    if (!AddParserElements()) {
       GMP_INFO_PRINT("Failed to Create and Add Parser !!!");
       return false;
    }

    if (!AddDecoderElements()) {
      GMP_INFO_PRINT("Failed to Add Decoder Elements!!!");
      return false;
    }

    if (!AddConverterElements()) {
      GMP_INFO_PRINT("Failed to Add Converter Elements!!!");
      return false;
    }

    if (!AddSinkElements()) {
      GMP_INFO_PRINT("Failed to Add sink elements!!!");
      return false;
    }
  }

  if (!PauseInternal()) {
    GMP_INFO_PRINT("CreatePipeline -> failed to pause !!!");
    return false;
  }

  SetDecoderSpecificInfomation();

  currentState_ = LOADING_STATE;
  GMP_INFO_PRINT("END currentState_ = %d", currentState_);
  return true;
}

}  // namespace player
}  // namespace gmp
