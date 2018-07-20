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

#ifndef SRC_PLAYER_BUFFER_PLAYER_H_
#define SRC_PLAYER_BUFFER_PLAYER_H_

#include "Player.h"
#include "PlayerTypes.h"

#include "mediaresource/requestor.h"

typedef struct {
  GstElement *pSrcElement;
  MEDIA_SRC_ELEM_IDX_T srcIdx;
  guint bufferMinByte;
  guint bufferMaxByte;
  guint bufferMinPercent;
} MEDIA_SRC_T;

/* player status enum type */
typedef enum {
  LOADING_STATE,
  STOPPED_STATE,
  PAUSING_STATE,
  PAUSED_STATE,
  PLAYING_STATE,
  PLAYED_STATE,
} PIPELINE_STATE;

namespace gmp { namespace base { struct error_t; }}
namespace gmp { namespace base { struct source_info_t; }}

namespace gmp {
namespace player {

class BufferPlayer : public Player {
  public:
    BufferPlayer(const std::string& appId);
    ~BufferPlayer();

    bool Load(const std::string &uri) override;
    bool Unload() override;
    bool Play() override;
    bool Pause() override;
    bool SetPlayRate(const double rate) override;
    bool Seek(const int64_t position) override;
    bool SetVolume(int volume) override;
    bool SetPlane(int planeId) override;
    void Initialize(gmp::service::IService* service) override;

    bool AcquireResources(gmp::base::source_info_t &sourceInfo);
    bool ReleaseResources();

    bool Load(const MEDIA_LOAD_DATA_T* loadData);

    void RegisterCbFunction(GMP_CALLBACK_FUNCTION_T cbFunction, void* udata);
    bool PushEndOfStream();

    bool Flush();
    MEDIA_STATUS_T Feed(const guint8* pBuffer, guint32 bufferSize,
                        guint64 pts, MEDIA_DATA_CHANNEL_T esData);

    bool NotifyForeground();
    bool NotifyBackground();

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

    static gboolean HandleBusMessage(GstBus* bus,
                                     GstMessage* message,
                                     gpointer user_data);

    static gboolean NotifyCurrentTime(gpointer user_data);
    static gboolean NotifyLoadComplete(gpointer user_data);

  private:
    bool CreatePipeline();

    bool AddSourceElements();
    bool AddParserElements();
    bool AddDecoderElements();
    bool AddSinkElements();
    void AddDecoderCapsProbe();

    bool ConnectBusCallback();
    bool DisconnectBusCallback();

    bool PauseInternal();
    bool SeekInternal(const int64_t msecond);

    void SetAppSrcBufferLevel(MEDIA_SRC_ELEM_IDX_T srcIdx,
                              MEDIA_SRC_T* pSrcInfo);
    bool IsBufferAvailable(MEDIA_SRC_ELEM_IDX_T srcIdx, guint64 newBufferSize);
    bool IsFeedPossible(MEDIA_SRC_ELEM_IDX_T chIdx, guint64 bufferSize);

    void HandleBusStateMsg(GstMessage* pMessage);
    void HandleBusAsyncMsg();
    void HandleVideoInfoMsg(GstMessage* pMessage);

    bool UpdateLoadData(const MEDIA_LOAD_DATA_T* loadData);
    bool UpdateVideoResData(const gmp::base::source_info_t &sourceInfo);

    void NotifyVideoInfo();

    static void EnoughData(GstElement* gstappsrc, gpointer user_data);
    static void SeekData(GstElement* appsrc, guint64 position, gpointer data);

    void SetGstreamerDebug();

    /*Pipeline elements*/
    GstElement* videoPQueue_;
    GstElement* videoParser_;

    GstElement* audioPQueue_;
    GstElement* audioParser_;

    GstElement* videoDecoder_;
    GstElement* vSinkQueue_;
    GstElement* videoSink_;

    GstElement* audioDecoder_;
    GstElement* audioConverter_;
    GstElement* aResampler_;
    GstElement* aSinkQueue_;
    GstElement* audioVolume_;
    GstElement* audioSink_;

    GstBus * busHandler_;
    gulong gSigBusAsync_;

    int32_t planeId_;
    bool planeIdSet_;

    bool isUnloaded_;
    bool recEndOfStream_;
    bool feedPossible_;

    bool flushDisabled_;

    guint loadDoneTimerId_;
    guint currPosTimerId_;

    guint64 currentPts_;
    PIPELINE_STATE currentState_;

    MEDIA_LOAD_DATA_T* loadData_;
    void *userData_;

    GMP_CALLBACK_FUNCTION_T notifyFunction_;

    std::shared_ptr<gmp::resource::ResourceRequestor> resourceRequestor_;

    std::vector<MEDIA_SRC_T*> sourceInfo_;
    CUSTOM_BUFFERING_STATE_T needFeedData_[IDX_MAX];

    guint64 totalFeed_[IDX_MAX];

    gmp::base::video_info_t videoInfo_;
};

}  // namespace player
}  // namespace gmp

#endif  // SRC_PLAYER_BUFFER_PLAYER_H_
