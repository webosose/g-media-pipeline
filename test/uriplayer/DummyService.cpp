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

#include <iostream>
#include <regex>
#include "DummyService.h"

DummyService::DummyService()
  : media_id_(std::string("UriPlayerTest"))
{
}

DummyService::~DummyService()
{
  if (media_player_client_)
    media_player_client_.reset();
}

bool DummyService::Load(const std::string& param)
{
  bool ret = false;
  media_player_client_.reset();

  /* appId is empty */
  media_player_client_ = std::make_unique<gmp::player::MediaPlayerClient>(app_id_, media_id_);

  if (!media_player_client_) {
    std::cout << std::string("media player client creation fail!") << std::endl;
    return ret;
  }

  media_player_client_->RegisterCallback(
    std::bind(&DummyService::Notify, this,
      std::placeholders::_1, std::placeholders::_2,
      std::placeholders::_3, std::placeholders::_4));

  ret = media_player_client_->Load(param);
  return ret;
}

bool DummyService::Play()
{
  if (media_player_client_)
    return media_player_client_->Play();
  else {
    std::cout << std::string("Load is not completed") << std::endl;
    return false;
  }
}

bool DummyService::Pause()
{
  if (media_player_client_)
    return media_player_client_->Pause();
  else {
    std::cout << std::string("Player should be in Playing state") << std::endl;
    return false;
  }
}

bool DummyService::Seek(const std::string& param)
{
  if (!param.length()) {
    std::cout << std::string("position is empty in Seek! Return..") << std::endl;
    return false;
  }

  if (!(std::regex_match(param, std::regex("^[0-9]+$")))) {
     std::cout << std::string("Provide Seek position in numeric value") << std::endl;
     return false;
  }

  int64_t msec = std::stoi(param) * 1000;
  if (media_player_client_)
    return media_player_client_->Seek(msec);
  else {
    std::cout << std::string("Player should be in Playing state") << std::endl;
    return false;
  }
}

bool DummyService::Unload()
{
  if (media_player_client_)
    return media_player_client_->Unload();
  else {
    std::cout << std::string("Player is not loaded") << std::endl;
    return false;
  }
}

void DummyService::Notify(const gint notification, const gint64 numValue, const gchar* strValue, void* payload)
{
  switch (notification) {
    case NOTIFY_LOAD_COMPLETED: {
      std::cout << std::string("loadCompleted") << std::endl;
      break;
    }

    case NOTIFY_UNLOAD_COMPLETED: {
      std::cout << std::string("unloadCompleted") << std::endl;
      break;
    }

    case NOTIFY_END_OF_STREAM: {
      std::cout << std::string("endOfStream") << std::endl;
      break;
    }

    case NOTIFY_SEEK_DONE: {
      std::cout << std::string("seekDone") << std::endl;
      break;
    }

    case NOTIFY_PLAYING: {
      std::cout << std::string("playing") << std::endl;
      break;
    }

    case NOTIFY_PAUSED: {
      std::cout << std::string("paused") << std::endl;
      break;
    }

    case NOTIFY_BUFFERING_START: {
      std::cout << std::string("buffering start") << std::endl;
      break;
    }

    case NOTIFY_BUFFERING_END: {
      std::cout << std::string("buffering end") << std::endl;
      break;
    }

    default:
      break;
  }
}

