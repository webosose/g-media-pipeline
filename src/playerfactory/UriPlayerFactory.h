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

#ifndef SRC_URI_PLAYER_FACTORY_H_
#define SRC_URI_PLAYER_FACTORY_H_

#include <string>
#include <memory>
#include <map>
#include "player/PlayerTypes.h"

namespace gmp { namespace player { class Player; }}

namespace gmp { namespace pf {

typedef std::shared_ptr<gmp::player::Player> (*pFuncHandlerCreator)();
using RegisterUriMap = std::map< std::string, pFuncHandlerCreator > ;
using UriPlayerTypeMap = std::map< std::string, std::shared_ptr<gmp::player::Player> > ;

class UriPlayerFactory {
 private:
  RegisterUriMap mapRegisterUriInfo_;
  UriPlayerTypeMap mapUriPlayerTypeInfo_;
  UriPlayerFactory();

 public:
  UriPlayerFactory(const UriPlayerFactory&) = delete;
  UriPlayerFactory& operator=(const UriPlayerFactory&) = delete;
  ~UriPlayerFactory();
  bool Register(const std::string &handlerName, pFuncHandlerCreator create);
  bool UnRegister(const std::string &handlerName);
  std::shared_ptr<gmp::player::Player> CreateUriTypePlayer(const std::string &handlerName);
  static UriPlayerFactory *getInstance();
  RegisterUriMap getRegisteredHandlers() {
    return mapRegisterUriInfo_;
  }
};

}  // namespace pf
}  // namespace gmp
#endif  // SRC_PLAYER_FACTORY_H_
