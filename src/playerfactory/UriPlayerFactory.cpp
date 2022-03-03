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

#include <glib.h>
#include <glib-unix.h>
#include <gst/gst.h>
#include <pbnjson.hpp>
#include <regex>

#include "UriPlayerFactory.h"
#include "log/log.h"

#include "player/Player.h"

namespace gmp { namespace pf {

UriPlayerFactory::UriPlayerFactory() {}

UriPlayerFactory::~UriPlayerFactory() {}

bool UriPlayerFactory::Register(const std::string &handlerName, pFuncHandlerCreator create) {
  if (mapRegisterUriInfo_.find(handlerName)!= mapRegisterUriInfo_.end())
    return false;
  mapRegisterUriInfo_.insert(std::pair<const std::string,pFuncHandlerCreator>(handlerName,create));
  return true;
}

bool UriPlayerFactory::UnRegister(const std::string &handlerName) {
  std::shared_ptr<gmp::player::Player> handle = nullptr;
  std::map<std::string, std::shared_ptr<gmp::player::Player>>::iterator it = mapUriPlayerTypeInfo_.begin();

  for ( ; it != mapUriPlayerTypeInfo_.end(); ++it) {
    if (handlerName == it->first) {
      handle = it->second;
      mapUriPlayerTypeInfo_.erase(handlerName);
      return true;
    }
  }

  return false;
}

std::shared_ptr<gmp::player::Player> UriPlayerFactory::CreateUriTypePlayer(const std::string &handlerName) {
  std::shared_ptr<gmp::player::Player> handle = nullptr;
  std::map<std::string, pFuncHandlerCreator>::iterator it = mapRegisterUriInfo_.begin();

  for ( ; it != mapRegisterUriInfo_.end(); ++it) {
    if (handlerName == it->first) {
      handle = it->second();
      mapUriPlayerTypeInfo_.insert(std::pair<const std::string,std::shared_ptr<gmp::player::Player>>(handlerName,handle));
    }
  }
  return handle;
}

UriPlayerFactory *UriPlayerFactory::getInstance() {
  static UriPlayerFactory _instance;
  return &_instance;
}


}  // namespace pf
}  // namespace gmp
