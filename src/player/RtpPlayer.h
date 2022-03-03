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

#ifndef SRC_PLAYER_URI_RTP_PLAYER_H_
#define SRC_PLAYER_URI_RTP_PLAYER_H_

#include "UriPlayer.h"
#include "../playerfactory/UriPlayerFactory.h"

namespace gmp { namespace player {

class UriRtpPlayer : public UriPlayer {
 public:
  UriRtpPlayer();
  ~UriRtpPlayer();
  virtual bool LoadPipeline();

  static bool RegisterObject() {
    return (gmp::pf::UriPlayerFactory::getInstance()->Register("rtp",&UriRtpPlayer::CreateObject));
  }

  static std::shared_ptr<gmp::player::Player> CreateObject() {
    std::shared_ptr<gmp::player::Player> player = nullptr;

    if (mIsObjRegistered) {
      player = std::make_shared<gmp::player::UriRtpPlayer>();
    }
    return player;
  }

 private:
 static bool mIsObjRegistered;
 GstElement *pSrcElement_ = nullptr;
 GstElement *pDecElement_ = nullptr;
 GstElement *pVSinkElement_ = nullptr;
 GstElement *pASinkElement = nullptr;
 GstElement *pAConvertElement_ = nullptr;

};

}  // namespace player
}  // namespace gmp
#endif  // SRC_PLAYER_URI_RTP_PLAYER_H_
