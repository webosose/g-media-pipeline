// @@@LICENSE
//
// Copyright (C) 2019, LG Electronics, All Right Reserved.
//
// No part of this source code may be communicated, distributed, reproduced
// or transmitted in any form or by any means, electronic or mechanical or
// otherwise, for any purpose, without the prior written permission of
// LG Electronics.
//
// LICENSE@@@


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
