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

#ifndef STREAM_SOURCE_H_
#define STREAM_SOURCE_H_

#include <string>
#include <vector>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <mediaplayerclient/MediaPlayerClient.h>

namespace gmp { namespace test {

class StreamSource {
  public:
    StreamSource(const std::string& windowID);
    ~StreamSource();
    void SetURI(const std::string& uri);
    std::string GetURI() const;
    void SetWindowID(const std::string& windowID);
    std::string GetWindowID() const;
    bool CreatePipeLine();
    bool Load(const std::string& uri);
    void ClearResources();
    bool MakeLoadData(int64_t start_time, MEDIA_LOAD_DATA_T* load_data);
    bool Play();
    bool Pause();
    bool Seek(const std::string& pos);
    bool Unload();
    void Notify(const gint notification, const gint64 numValue,
                const gchar* strValue, void* payload);
    static GstFlowReturn VideoAppSinkSampleAddedCB(GstElement *element, gpointer userData);
    static GstFlowReturn AudioAppSinkSampleAddedCB(GstElement *element, gpointer userData);
    static void ParseBinPadAddedCB(GstElement* element, GstPad* pad, gpointer userData);
    static void MultiQueuePadAddedCB(GstElement* element, GstPad* pad, gpointer userData);

    StreamSource(const StreamSource &) = delete;
    StreamSource& operator=(const StreamSource &) = delete;
    StreamSource& operator=(StreamSource &&) = delete;
  private:
    std::string uri_;
    std::string windowID_;
    std::string appID_;
    std::string mediaID_;

    /* pipeline elements */
    GstElement *pipeline_;
    GstElement *source_;
    GstElement *parseBin_;
    GstElement *multiQueue_;
    GstElement *videoAppSink_;
    GstElement *audioAppSink_;

    /* signal handler ID */
    gulong parseBinCB_id_;
    gulong multiQueueCB_id_;
    gulong videoAppSinkCB_id_;
    gulong audioAppSinkCB_id_;

    bool firstVideoSample;
    bool firstAudioSample;

    std::vector<GstPad*> multiQueueSinkPads_;
    std::unique_ptr<gmp::player::MediaPlayerClient> media_player_client_;
};

}  // namespace test
}  // namespace gmp
#endif  // STREAM_SOURCE_H_
