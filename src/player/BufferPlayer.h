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

#ifndef SRC_PLAYER_BUFFER_PLAYER_H_
#define SRC_PLAYER_BUFFER_PLAYER_H_

#include "AbstractPlayer.h"
#include "PlayerTypes.h"
#include "mediaplayerclient/MediaPlayerClient.h"

namespace gmp { namespace base { struct source_info_t; }}

namespace gmp {
namespace player {

class BufferPlayer : public AbstractPlayer {
  public:
    ~BufferPlayer();

    bool Load(const MEDIA_LOAD_DATA_T* loadData) override;
    bool UnloadImpl() override;
    bool Play() override;
    bool Pause() override;
    bool SetPlayRate(const double rate) override;
    bool Seek(const int64_t position) override;
    bool SetVolume(int volume) override;

    bool UpdateVideoResData(const gmp::base::source_info_t &sourceInfo) override;
    bool PushEndOfStream() override;
    bool Flush() override;
    MEDIA_STATUS_T Feed(const guint8* pBuffer, guint32 bufferSize,
                        guint64 pts, MEDIA_DATA_CHANNEL_T esData) override;

    static gboolean HandleBusMessage(GstBus* bus,
                                     GstMessage* message,
                                     gpointer user_data);
    static GstBusSyncReply HandleSyncBusMessage(GstBus * bus,
                                     GstMessage * msg, gpointer data);

    static gboolean NotifyCurrentTime(gpointer user_data);
    bool SetSourceInfo(const gmp::base::source_info_t &sourceInfo);

  protected:
    BufferPlayer();
    virtual bool CreatePipeline() = 0;

    bool AddSourceElements();
    bool AddParserElements();
    bool AddDecoderElements();
    bool AddConverterElements();
    bool AddSinkElements();
    bool AddDumpElements();

    bool AddAudioParserElement();
    bool AddVideoParserElement();

    bool AddAudioDecoderElement();
    bool AddVideoDecoderElement();

    bool AddAudioConverterElement();
    bool AddVideoConverterElement();

    bool AddAudioSinkElement();
    bool AddVideoSinkElement();

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
    void HandleStreamStatsMsg(GstMessage* pMessage);

    bool UpdateLoadData(const MEDIA_LOAD_DATA_T* loadData);

    void NotifyVideoInfo();
    bool NotifyActivity();

    static void EnoughData(GstElement* gstAppSrc, gpointer userData);
    static gboolean SeekData(GstElement* gstAppSrc, guint64 position,
                             gpointer userData);

    void SetDecoderSpecificInfomation();

    base::source_info_t GetSourceInfo(const MEDIA_LOAD_DATA_T* loadData);
    void SetDebugDumpFileName();

    /*Pipeline elements*/
    GstElement* videoPQueue_ = nullptr;
    GstElement* videoParser_ = nullptr;

    GstElement* audioPQueue_ = nullptr;
    GstElement* audioParser_ = nullptr;

    GstElement* videoDecoder_ = nullptr;
    GstElement* videoPostProc_ = nullptr;

    GstElement* vConverter_ = nullptr;
    GstElement* videoQueue_ = nullptr;
    GstElement* videoSink_ = nullptr;

    GstElement* audioDecoder_ = nullptr;
    GstElement* audioQueue_ = nullptr;
    GstElement* audioConverter_ = nullptr;
    GstElement* audioSink_ = nullptr;
    GstElement* aResampler_ = nullptr;
    GstElement* audioVolume_ = nullptr;
    GstElement* audiofileDump_ = nullptr;
    GstElement* videofileDump_ = nullptr;

    GstElement* linkElement_ = nullptr;

    GstBus * busHandler_ = nullptr;
    gulong gSigBusAsync_ = 0;

    bool planeIdSet_ = false;

    bool recEndOfStream_ = false;
    bool feedPossible_ = false;

    guint64 currentPts_ = 0;
    PIPELINE_STATE currentState_ = STOPPED_STATE;

    MEDIA_LOAD_DATA_T* loadData_ = nullptr;

    std::vector<MEDIA_SRC_T*> sourceList_;
    CUSTOM_BUFFERING_STATE_T needFeedData_[IDX_MAX];

    guint64 totalFeed_[IDX_MAX];

    gmp::base::video_info_t videoInfo_;
    gmp::base::source_info_t sourceInfo_;
    gchar* inputDumpFileName = nullptr;
};

}  // namespace player
}  // namespace gmp

#endif  // SRC_PLAYER_BUFFER_PLAYER_H_
