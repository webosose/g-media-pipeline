// Copyright (c) 2018-2022 LG Electronics, Inc.
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

#include <glib.h>
#include <glib-unix.h>
#include <gst/gst.h>
#include <pbnjson.hpp>
#include <regex>

#include "PlayerFactory.h"
#include "UriPlayerFactory.h"
#include "log/log.h"

#include "player/Player.h"
#include "player/UriPlainPlayer.h"
#include "player/BufferPlainPlayer.h"

namespace gmp { namespace pf {

PlayerFactory::PlayerFactory() {}

PlayerFactory::~PlayerFactory() {}

std::shared_ptr<gmp::player::Player> PlayerFactory::CreatePlayer(const std::string &str, GMP_PLAYER_TYPE &playerType) {
  std::shared_ptr<gmp::player::Player> player = nullptr;

  pbnjson::JDomParser jdparser;
  if (!jdparser.parse(str, pbnjson::JSchema::AllSchema())) {
    GMP_DEBUG_PRINT("ERROR JDomParser.parse. msg: %s ", str.c_str());
    return nullptr;
  }
  pbnjson::JValue parsed = jdparser.getDom();

  if (parsed.hasKey("uri")) {
    const std::string uri = parsed["uri"].asString();
    GMP_INFO_PRINT("uri = %s", uri.c_str());

    playerType = GMP_PLAYER_TYPE_URI;

    std::string protocol = GetProtocolType(uri);
    player = UriPlayerFactory::getInstance()->CreateUriTypePlayer(protocol);

    if (!player) {
      GMP_INFO_PRINT("Create UriPlainPlayer");
      player = std::make_shared<gmp::player::UriPlainPlayer>();
    }
  } else {
    GMP_INFO_PRINT("ERROR from CreatePlayer");
    return nullptr;
  }

  return player;
}

std::shared_ptr<gmp::player::Player> PlayerFactory::CreatePlayer(const MEDIA_LOAD_DATA_T* loadData) {

  std::shared_ptr<gmp::player::Player> player;
  player = std::make_shared<gmp::player::BufferPlainPlayer>();
  return player;
}

std::string PlayerFactory::GetProtocolType(const std::string &uri) {
  std::string protocolType;
  const std::string prot_end("://");
  std::string::const_iterator prot_i = search(uri.begin(), uri.end(),
                                           prot_end.begin(), prot_end.end());
  protocolType.reserve(distance(uri.begin(), prot_i));
  transform(uri.begin(), prot_i,std::back_inserter(protocolType),
              std::ptr_fun<int,int>(tolower));
  return protocolType;
}

}  // namespace pf
}  // namespace gmp
