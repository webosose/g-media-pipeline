// Copyright (c) 2018-2020 LG Electronics, Inc.
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

#ifndef SRC_MEDIAPLAYERCLIENT_MEDIAPLAYERCLIENT_H_
#define SRC_MEDIAPLAYERCLIENT_MEDIAPLAYERCLIENT_H_

#include <glib.h>
#include <vector>
#include <string>
#include <memory>

#include "types.h"
#include "PlayerTypes.h"

namespace gmp { namespace resource { class ResourceRequestor; }}
namespace gmp { namespace player {

class Player;

class MediaPlayerClient {
 public:
    static bool IsCodecSupported(GMP_VIDEO_CODEC videoCodec);

    MediaPlayerClient(const std::string& appId = "", const std::string& connectionId = "");
    ~MediaPlayerClient();

    bool Load(const MEDIA_LOAD_DATA_T* loadData);
    bool Load(const std::string &str);
    bool Unload();
    bool Play();
    bool Pause();
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
    void RegisterCallback(CALLBACK_T cbFunction) { userCallback_ = std::move(cbFunction); }
    void RegisterCallback(CALLBACK_T cbFunction, void* user_data)
      { RegisterCallback(std::move(cbFunction)); userData_ = user_data; }
    bool PushEndOfStream();
    bool NotifyForeground() const;
    bool NotifyBackground() const;
    bool NotifyActivity() const;
    bool SetVolume(int volume);
    bool SetExternalContext(GMainContext *context);
    bool SetPlaybackRate(const double playbackRate);
    bool AcquireResources(base::source_info_t &sourceInfo,
                            const std::string &display_mode = "Default", uint32_t display_path = 0);
    bool ReacquireResources(base::source_info_t &sourceInfo,
                            const std::string &display_mode = "Default", uint32_t display_path = 0);
    bool ReleaseResources();
    const char* GetMediaID();
    void NotifyFunction(const gint type, const gint64 numValue, const gchar *strValue, void *udata);
    GstElement* GetPipeline();

 private:
    void LoadCommon();
    void RunCallback(const gint type, const gint64 numValue, const gchar *strValue, void *udata);

    std::shared_ptr<gmp::player::Player> player_;
    GMainContext *playerContext_ = nullptr;

    bool isLoaded_ = false;
    std::unique_ptr<gmp::resource::ResourceRequestor> resourceRequestor_;

    std::string appId_;
    std::string connectionId_;

    CALLBACK_T userCallback_ = nullptr;
    void* userData_ = nullptr;

    GMP_PLAYER_TYPE playerType_;
};

}  // namespace player
}  // namespace gmp

#endif  // SRC_MEDIAPLAYERCLIENT_MEDIAPLAYERCLIENT_H_
