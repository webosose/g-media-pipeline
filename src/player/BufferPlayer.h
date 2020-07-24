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

  protected:
    BufferPlayer();
    virtual bool CreatePipeline();

    bool AddAudioPipelineElements();
    bool AddVideoPipelineElements();

    /* for audio */
    bool AddAudioSourceElement();
    bool AddAudioParserElement();
    bool AddAudioDecoderElement();
    bool AddAudioConverterElement();
    bool AddAudioSinkElement();

    /* for video */
    bool AddVideoSourceElement();
    bool AddVideoParserElement();
    bool AddVideoDecoderElement();
    bool AddVideoConverterElement();
    bool AddVideoSinkElement();

    void FreePipelineElements();

    bool AddAndLinkElement(GstElement * target_element);

    /* for debugging */
    bool AddAudioDumpElement();
    bool AddVideoDumpElement();

    bool ConnectBusCallback();
    bool DisconnectBusCallback();

    bool PauseInternal();
    bool SeekInternal(const int64_t msecond);

    bool IsBufferAvailable(MEDIA_SRC_T* pAppSrcInfo, guint64 newBufferSize);
    bool IsFeedPossible(MEDIA_SRC_T* pAppSrcInfo, guint64 bufferSize);

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
    void SetAppSrcProperties(MEDIA_SRC_T* pAppSrcInfo, guint64 bufferMaxLevel);
    void SetDebugDumpFileName();

    /* for debugging */
    void PrintLoadData(const MEDIA_LOAD_DATA_T* loadData);

    /*Video Pipeline elements*/
    std::shared_ptr<MEDIA_SRC_T> videoSrcInfo_ = nullptr;
    GstElement* videoPQueue_ = nullptr;
    GstElement* videoParser_ = nullptr;
    GstElement* videoDecoder_ = nullptr;
    GstElement* videoPostProc_ = nullptr;
    GstElement* videoQueue_ = nullptr;
    GstElement* vConverter_ = nullptr;
    GstElement* videoSink_ = nullptr;
    GstElement* videofileDump_ = nullptr;

    /*Audio Pipeline elements*/
    std::shared_ptr<MEDIA_SRC_T> audioSrcInfo_ = nullptr;
    GstElement* audioPQueue_ = nullptr;
    GstElement* audioParser_ = nullptr;
    GstElement* audioDecoder_ = nullptr;
    GstElement* audioQueue_ = nullptr;
    GstElement* audioConverter_ = nullptr;
    GstElement* audioSink_ = nullptr;
    GstElement* aResampler_ = nullptr;
    GstElement* audioVolume_ = nullptr;
    GstElement* audiofileDump_ = nullptr;

    GstElement* linkedElement_ = nullptr;

    GstBus * busHandler_ = nullptr;
    gulong gSigBusAsync_ = 0;

    bool planeIdSet_ = false;

    bool recEndOfStream_ = false;
    bool feedPossible_ = false;
    bool shouldSetNewBaseTime_ = false;

    guint64 currentPts_ = 0;
    PIPELINE_STATE currentState_ = STOPPED_STATE;

    std::shared_ptr<MEDIA_LOAD_DATA_T> loadData_ = nullptr;

    gmp::base::video_info_t videoInfo_;
    gmp::base::source_info_t sourceInfo_;
    gchar* inputDumpFileName = nullptr;

    GstSegment segment_;
};

}  // namespace player
}  // namespace gmp

#endif  // SRC_PLAYER_BUFFER_PLAYER_H_
