// Copyright (c) 2018 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include <glib.h>

#include <vector>
#include <string>
#include <boost/property_tree/ptree.hpp>
#include <boost/shared_ptr.hpp>

#include "PlayerTypes.h"
#include "types.h"

namespace gmp { namespace resource { class ResourceRequestor; }}
namespace gmp { namespace player {

class BufferPlayer;

class MediaPlayerClient {

  public:
    MediaPlayerClient(const std::string& appId);
    ~MediaPlayerClient();

    bool Load(const MEDIA_LOAD_DATA_T* loadData);
    bool Play();
    bool Pause();
    bool Stop();
    bool Seek(int position);
    bool SetPlane(int planeId);
    MEDIA_STATUS_T Feed(const guint8* pBuffer,
                        guint32 bufferSize,
                        guint64 pts,
                        MEDIA_DATA_CHANNEL_T esData);
    bool Flush();
    bool SetDisplayWindow(const long left,
                          const long top,
                          const long width,
                          const long height,
                          const bool isFullScreen);
    bool SetCustomDisplayWindow(const long srcLeft,
                                const long srcTop,
                                const long srcWidth,
                                const long srcHeight,
                                const long destLeft,
                                const long destTop,
                                const long destWidth,
                                const long destHeight,
                                const bool isFullScreen);
    bool RegisterCallback(GMP_CALLBACK_FUNCTION_T cbFunction, void* userData);
    bool PushEndOfStream();
    bool NotifyForeground();
    bool NotifyBackground();
    bool SetVolume(int volume);
    bool SetExternalContext(GMainContext *context);

  private:
    base::source_info_t GetSourceInfo(const MEDIA_LOAD_DATA_T* loadData);

    std::unique_ptr<gmp::player::BufferPlayer> player_;
    GMainContext *playerContext_;

    bool isStopCalled_;
};

}}
