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
#include <string>
#include <memory>
#include <string>
#include <functional>
#include <map>
#include <thread>
#include <player/Player.h>
#include <unistd.h>

#include "StreamSource.h"
#include <mediaplayerclient/MediaPlayerClient.h>

#define UNUSED(expr) do { (void)(expr); } while(0)

using functionPtr = std::function<bool(gmp::test::StreamSource&, const std::string&)>;

static bool doLoad(gmp::test::StreamSource& source,
                   const std::string& param = std::string());
static bool doPlay(gmp::test::StreamSource& source,
                   const std::string& param = std::string());
static bool doPause(gmp::test::StreamSource& source,
                    const std::string& param = std::string());
static bool doSeek(gmp::test::StreamSource& source,
                   const std::string& param = std::string());
static bool doUnload(gmp::test::StreamSource& source,
                     const std::string& param = std::string());

std::string window_id;
std::string test_uri("http://10.178.84.247/mp4/sintel.mp4");

enum class BufferPlayerAPI : int {
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

static bool doLoad(gmp::test::StreamSource& source, const std::string& param)
{
  return source.Load(param);
}

static bool doPlay(gmp::test::StreamSource& source, const std::string& param)
{
  UNUSED(param);
  return source.Play();
}

static bool doPause(gmp::test::StreamSource& source, const std::string& param)
{
  UNUSED(param);
  return source.Pause();
}

static bool doSeek(gmp::test::StreamSource& source, const std::string& param)
{
  return source.Seek(param);
}

static bool doUnload(gmp::test::StreamSource& source, const std::string& param)
{
  UNUSED(param);
  return source.Unload();
}

static void printMenu(void)
{
  std::cout << std::string("*** Buffer Player Test Command ***") << std::endl;
  for (int i = 0; i < MAX_MENU_COUNT; i++)
      std::cout << "\t" << i+1 << ": " << menu_table[i] << std::endl;
  std::cout << std::string("Enter a number(q to quit): ");
}

static bool processCommand(const std::string& cmd, gmp::test::StreamSource& source, GMainLoop* loop)
{
  if (cmd.compare("q") == 0) {
    g_main_loop_quit(loop);
    return false;
  }

  auto iter = input_table.find(cmd);

  if (iter == input_table.end()) {
    std::cout << std::string("Wrong command. Please enter existing menu!") << std::endl;
    return true;
  }

  bool ret = false;
  BufferPlayerAPI api = static_cast<BufferPlayerAPI>(std::stoi(iter->first));

  switch(api) {
    case BufferPlayerAPI::LOAD: {
      std::string uri;
      std::cout << std::string("Load command!") << std::endl;
      std::cout << "Enter uri(default->http://10.178.84.247/mp4/1080p.mp4): " << std::endl;
      std::getline(std::cin, uri);
      if (uri.empty())
        uri = test_uri;

      ret = iter->second(source, uri);
      break;
    }
    case BufferPlayerAPI::PLAY: {
      std::cout << std::string("Play command!") << std::endl;
      ret = iter->second(source, std::string());
      break;
    }
    case BufferPlayerAPI::PAUSE: {
      std::cout << std::string("Pause command!") << std::endl;
      ret = iter->second(source, std::string());
      break;
    }
    case BufferPlayerAPI::SEEK: {
      std::cout << std::string("Seek command!") << std::endl;
      std::string seekPos;
      std::cout << "Enter seek position(Sec): " << std::endl;
      std::getline(std::cin, seekPos);
      ret = iter->second(source, seekPos);
      break;
    }
    case BufferPlayerAPI::UNLOAD: {
      std::cout << std::string("Unload command!") << std::endl;
      ret = iter->second(source, std::string());
      break;
    }
    default:
      std::cout << std::string("Wrong command!") << std::endl;
      break;
  }

  return true;
}

int main(int argc, char* argv[])
{
  /* TODO: apply getopt, lsm_exporter configuration */
  if (argc < 2) {
    std::cout << std::string("Please run 'lsm_connector_exporter 0 0 1920 1080 &' to get WindowID on shell") << std::endl;
    return 0;
  }

  window_id = std::string(argv[1]);
  GMainLoop *loop = g_main_loop_new(NULL, FALSE);

  std::thread th([](GMainLoop *loop) -> void {
    gmp::test::StreamSource stream_source(window_id);
    while(1) {
      printMenu();
      std::string cmd;
      std::getline(std::cin, cmd);
      bool ret = processCommand(cmd, stream_source, loop);
      if (!ret) {
        std::cout << std::string("Exit thread!") << std::endl;
        break;
      }
    }
  }, loop);

  g_main_loop_run(loop);
  th.join();
  g_main_loop_unref(loop);

  return 0;
}
