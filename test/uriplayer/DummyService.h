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

#include <service/service.h>

class DummyService {
  public:
    DummyService();
    ~DummyService();
    bool Load(const std::string&);
    bool Play();
    bool Pause();
    bool Seek(const std::string&);
    bool Unload();
    void Notify(const gint notification, const gint64 numValue, const gchar* strValue = nullptr, void* payload = nullptr);
  private:
    std::unique_ptr<gmp::player::MediaPlayerClient> media_player_client_;
    std::string media_id_;
    std::string app_id_;
};
