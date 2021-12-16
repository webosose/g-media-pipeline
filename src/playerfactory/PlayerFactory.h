// Copyright (c) 2018-2021 LG Electronics, Inc.
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

#ifndef SRC_PLAYER_FACTORY_H_
#define SRC_PLAYER_FACTORY_H_

#include <string>
#include <memory>
#include "player/PlayerTypes.h"

namespace gmp { namespace player { class Player; }}

namespace gmp { namespace pf {

class PlayerFactory {
 public:
  PlayerFactory();
  ~PlayerFactory();

  static std::shared_ptr<gmp::player::Player> CreatePlayer(const std::string &str, GMP_PLAYER_TYPE &playerType);
  static std::shared_ptr<gmp::player::Player> CreatePlayer(const MEDIA_LOAD_DATA_T*);
};

}  // namespace pf
}  // namespace gmp
#endif  // SRC_PLAYER_FACTORY_H_
