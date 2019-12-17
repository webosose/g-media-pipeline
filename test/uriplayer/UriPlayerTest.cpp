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
#include <memory>
#include <string>
#include <functional>
#include <map>
#include <player/Player.h>
#include <parser/composer.h>
#include "DummyService.h"
#include <unistd.h>

#define UNUSED(expr) do { (void)(expr); } while(0)

using functionPtr = std::function<bool(DummyService&, const std::string&)>;
using mapIterator = std::map<std::string, functionPtr>::iterator;

static bool doLoad(DummyService& service,
                   const std::string& param = std::string());
static bool doPlay(DummyService& service,
                   const std::string& param = std::string());
static bool doPause(DummyService& service,
                    const std::string& param = std::string());
static bool doSeek(DummyService& service,
                   const std::string& param = std::string());
static bool doUnload(DummyService& service,
                     const std::string& param = std::string());

std::string window_id;
std::string test_uri("http://10.178.84.247/mp4/1080p.mp4");

enum class UriPlayerAPI : int {
  LOAD = 1,
  PLAY,
  PAUSE,
  SEEK,
  UNLOAD
};

#define MAX_MENU_COUNT 5
std::string menu_table[MAX_MENU_COUNT] = {
  "load",
  "play",
  "pause",
  "seek",
  "unload"
};

std::map<std::string, functionPtr> input_table = {
  { "1", &doLoad },
  { "2", &doPlay },
  { "3", &doPause },
  { "4", &doSeek },
  { "5", &doUnload },
};

static bool doLoad(DummyService& service, const std::string& param)
{
  return service.Load(param);
}

static bool doPlay(DummyService& service, const std::string& param)
{
  UNUSED(param);
  return service.Play();
}

static bool doPause(DummyService& service, const std::string& param)
{
  UNUSED(param);
  return service.Pause();
}

static bool doSeek(DummyService& service, const std::string& param)
{
  return service.Seek(param);
}

static bool doUnload(DummyService& service, const std::string& param)
{
  UNUSED(param);
  return service.Unload();
}

static void printMenu(void)
{
  std::cout << std::string("*** URI Player Test Command ***") << std::endl;
  for (int i = 0; i < MAX_MENU_COUNT; i++)
      std::cout << "\t" << i+1 << ": " << menu_table[i] << std::endl;
  std::cout << std::string("Enter a number(q to quit): ");
}

static bool processCommand(DummyService& service, const std::string& cmd)
{
  if (cmd.compare("q") == 0) {
    return false;
  }

  auto iter = input_table.find(cmd);

  if (iter == input_table.end()) {
    std::cout << std::string("Wrong command. Please enter existing menu!") << std::endl;
    return true;
  }

  bool ret = false;
  UriPlayerAPI api = static_cast<UriPlayerAPI>(std::stoi(iter->first));

  switch(api) {
    case UriPlayerAPI::LOAD: {
      std::cout << std::string("Load command!") << std::endl;
      gmp::parser::Composer composer;
      gmp::base::load_param_t param = { "Textured", 0, window_id, test_uri };

      std::string uri;
      std::cout << "Enter uri(default->http://10.178.84.247/mp4/1080p.mp4): " << std::endl;
      std::getline(std::cin, uri);
      if (!uri.empty())
        param.uri = uri;

      composer.put(param);
      ret = iter->second(service, composer.result());
      break;
    }
    case UriPlayerAPI::PLAY: {
      std::cout << std::string("Play command!") << std::endl;
      ret = iter->second(service, std::string());
      break;
    }
    case UriPlayerAPI::PAUSE: {
      std::cout << std::string("Pause command!") << std::endl;
      ret = iter->second(service, std::string());
      break;
    }
    case UriPlayerAPI::SEEK: {
      std::cout << std::string("Seek command!") << std::endl;
      std::string seekPos;
      std::cout << "Enter seek position(Sec): " << std::endl;
      std::getline(std::cin, seekPos);
      ret = iter->second(service, seekPos);
      break;
    }
    case UriPlayerAPI::UNLOAD: {
      std::cout << std::string("Unload command!") << std::endl;
      ret = iter->second(service, std::string());
      break;
    }
    default:
      std::cout << std::string("Wrong command!") << std::endl;
      break;
  }

  return ret;
}

static void runMainLoop(DummyService& service)
{
  while(1) {
    printMenu();
    std::string cmd;
    std::getline(std::cin, cmd);

    bool ret = processCommand(service, cmd);
    if (!ret) {
        std::cout << std::string("Exit main loop") << std::endl;
        break;
    }
  }
}

int main(int argc, char* argv[])
{
  /* TODO: apply getopt, lsm_exporter configuration */
  if (argc < 2) {
    std::cout << std::string("Please run 'lsm_connector_exporter 0 0 1920 1080 &' to get WindowID on shell") << std::endl;
    return 0;
  }

  window_id = std::string(argv[1]);

  DummyService service;

  runMainLoop(service);

  return 0;
}
