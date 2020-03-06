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

#ifndef SRC_PLAYER_PLAYERTYPES_H_
#define SRC_PLAYER_PLAYERTYPES_H_

#include <gst/gst.h>
#include <functional>
#include "types.h"

using CALLBACK_T = std::function<void(const gint type, const gint64 numValue,
    const gchar *strValue, void *udata)>;

typedef enum {
  NOTIFY_LOAD_COMPLETED = 0,
  NOTIFY_UNLOAD_COMPLETED,
  NOTIFY_SOURCE_INFO,
  NOTIFY_END_OF_STREAM,
  NOTIFY_CURRENT_TIME,
  NOTIFY_SEEK_DONE,
  NOTIFY_PLAYING,
  NOTIFY_PAUSED,
  NOTIFY_NEED_DATA,
  NOTIFY_ENOUGH_DATA,
  NOTIFY_SEEK_DATA,
  NOTIFY_ERROR,
  NOTIFY_VIDEO_INFO,
  NOTIFY_AUDIO_INFO,
  NOTIFY_BUFFER_FULL,  // NOTIFY_BUFFERING_END? need to check the chromium media backend
  NOTIFY_BUFFER_NEED,  // NOTIFY_BUFFERING_START?
  NOTIFY_BUFFER_RANGE,
  NOTIFY_BUFFERING_START,
  NOTIFY_BUFFERING_END,
  NOTIFY_ACTIVITY,
  NOTIFY_ACQUIRE_RESOURCE,
  NOTIFY_MAX
} NOTIFY_TYPE_T;

typedef enum {
  GMP_PLAYER_TYPE_URI = 0,
  GMP_PLAYER_TYPE_BUFFER,
  GMP_PLAYER_TYPE_EXT,
  GMP_PLAYER_TYPE_HDMI
} GMP_PLAYER_TYPE;

typedef enum {
  GMP_ERROR_NONE,
  GMP_ERROR_STREAM,
  GMP_ERROR_ASYNC,
  GMP_ERROR_RES_ALLOC,
  GMP_ERROR_MAX
} GMP_ERROR_CODE;

typedef enum {
  CUSTOM_BUFFER_FEED = 0,
  CUSTOM_BUFFER_LOW,
  CUSTOM_BUFFER_FULL,
  CUSTOM_BUFFER_LOCKED,
} CUSTOM_BUFFERING_STATE_T;

/**
 * video codec
 */
typedef enum {
  GMP_VIDEO_CODEC_NONE,
  GMP_VIDEO_CODEC_H264,
  GMP_VIDEO_CODEC_VC1,
  GMP_VIDEO_CODEC_MPEG2,
  GMP_VIDEO_CODEC_MPEG4,
  GMP_VIDEO_CODEC_THEORA,
  GMP_VIDEO_CODEC_VP8,
  GMP_VIDEO_CODEC_VP9,
  GMP_VIDEO_CODEC_H265,
  GMP_VIDEO_CODEC_MJPEG,
  GMP_VIDEO_CODEC_MAX = GMP_VIDEO_CODEC_MJPEG,
} GMP_VIDEO_CODEC;

/**
 * audio codec
 */
typedef enum {
  GMP_AUDIO_CODEC_NONE,
  GMP_AUDIO_CODEC_AAC,
  GMP_AUDIO_CODEC_MP3,
  GMP_AUDIO_CODEC_PCM,
  GMP_AUDIO_CODEC_VORBIS,
  GMP_AUDIO_CODEC_FLAC,
  GMP_AUDIO_CODEC_AMR_NB,
  GMP_AUDIO_CODEC_AMR_WB,
  GMP_AUDIO_CODEC_PCM_MULAW,
  GMP_AUDIO_CODEC_GSM_MS,
  GMP_AUDIO_CODEC_PCM_S16BE,
  GMP_AUDIO_CODEC_PCM_S24BE,
  GMP_AUDIO_CODEC_OPUS,
  GMP_AUDIO_CODEC_EAC3,
  GMP_AUDIO_CODEC_PCM_ALAW,
  GMP_AUDIO_CODEC_ALAC,
  GMP_AUDIO_CODEC_AC3,
  GMP_AUDIO_CODEC_DTS,
  GMP_AUDIO_CODEC_MAX = GMP_AUDIO_CODEC_DTS,
} GMP_AUDIO_CODEC;

/**
 * sample format type
 */
typedef enum {
  GMP_AUDIO_FORMAT_UNKNOWN,
  GMP_AUDIO_FORMAT_ENCODED,
  GMP_AUDIO_FORMAT_S8,
  GMP_AUDIO_FORMAT_U8,
  GMP_AUDIO_FORMAT_S16LE,
  GMP_AUDIO_FORMAT_S16BE,
  GMP_AUDIO_FORMAT_U16LE,
  GMP_AUDIO_FORMAT_U16BE,
  GMP_AUDIO_FORMAT_S24_32LE,
  GMP_AUDIO_FORMAT_S24_32BE,
  GMP_AUDIO_FORMAT_U24_32LE,
  GMP_AUDIO_FORMAT_U24_32BE,
  GMP_AUDIO_FORMAT_S32LE,
  GMP_AUDIO_FORMAT_S32BE,
  GMP_AUDIO_FORMAT_MAX = GMP_AUDIO_FORMAT_S32BE,
} GMP_AUDIO_SAMPLE_FORMAT;

/**
 * drm type
 */
typedef enum {
  DRM_UNKNOWN = 0,
  DRM_PLAYREADY,
  DRM_WIDEVINE_MODULAR,
  DRM_TYPE_MAX
} MEDIA_DRM_TYPE_T;

/**
 * HDMI input port number type
 */
typedef enum {
  PORT_MIN = 0,
  PORT_1,
  PORT_2,
  PORT_MAX
} HDMI_PORT_NUM_T;


/**
 * Load data structure for Buffer Player
 */
typedef struct MEDIA_LOAD_DATA {
  guint32 maxWidth;
  guint32 maxHeight;
  guint32 maxFrameRate;

  GMP_VIDEO_CODEC videoCodec;

  GMP_AUDIO_CODEC audioCodec;

  gint64 ptsToDecode;

  /* config for video */
  guint32 frameRate;
  guint32 width;
  guint32 height;
  void*   extraData;
  guint32 extraSize;
  guint32 displayPath;  /* to support multi display, 0(default) for primary, 1 for secondary display */
  char*   windowId;     /* For GAV support */

  /* config for audio */
  guint32 channels;
  guint32 sampleRate;
  guint32 blockAlign;
  guint32 bitRate;
  guint32 bitsPerSample;
  gchar* format;
  guint32 audioObjectType;
  guint8* codecData;
  guint32 codecDataSize;
  MEDIA_DRM_TYPE_T drmType;
  guint32 svpVersion;
  GMP_AUDIO_SAMPLE_FORMAT sampleFormat;
  gboolean liveStream;

  public:
    MEDIA_LOAD_DATA() : maxWidth(0), maxHeight(0), maxFrameRate(0),
                        videoCodec(GMP_VIDEO_CODEC_NONE), audioCodec(GMP_AUDIO_CODEC_NONE),
                        ptsToDecode(0), frameRate(0), width(0), height(0), extraData(NULL),
                        extraSize(0), displayPath(0), windowId(NULL), channels(0), sampleRate(0),
                        blockAlign(0), bitRate(0), bitsPerSample(0), format(NULL),
                        audioObjectType(0), codecData(NULL), codecDataSize(0),
                        drmType(DRM_UNKNOWN), svpVersion(0), liveStream(false),
                        sampleFormat(GMP_AUDIO_FORMAT_UNKNOWN) {
    }
    MEDIA_LOAD_DATA(guint32 maxWidth_, guint32 maxHeight_, guint32 maxFrameRate_,
                    GMP_VIDEO_CODEC videoCodec_, GMP_AUDIO_CODEC audioCodec_,
                    gint64 ptsToDecode_, guint32 frameRate_, guint32 width_, guint32 height_,
                    void* extraData_, guint32 extraSize_, guint32 displayPath_,
                    guint32 channels_, guint32 sampleRate_, guint32 blockAligh_,
                    guint32 bitRate_, guint32 bitsPerSample_,
                    gchar* format_, guint32 audioObjectType_,
                    guint8* codecData_, guint32 codecdataSize_,
                    MEDIA_DRM_TYPE_T drmType_ = DRM_UNKNOWN, guint32 svpVersion_ = 0,
                    gboolean liveStream_ = false,
                    GMP_AUDIO_SAMPLE_FORMAT sampleFormat_ = GMP_AUDIO_FORMAT_UNKNOWN) {

      // TODO: add drmType, svpVersion

      maxWidth = maxWidth_;
      maxHeight = maxHeight_;
      maxFrameRate = maxFrameRate_;
      videoCodec = videoCodec_;
      audioCodec = audioCodec_;
      ptsToDecode = ptsToDecode_;
      frameRate = frameRate_;
      width = width_;
      height = height_;
      extraData = extraData_;
      extraSize = extraSize_;
      displayPath = displayPath_;
      channels = channels_;
      sampleRate = sampleRate_;
      blockAlign = blockAligh_;
      bitRate = bitRate_;
      bitsPerSample = bitsPerSample_;
      format = format_;
      audioObjectType = audioObjectType_;
      codecData = codecData_;
      codecDataSize = codecdataSize_;
      windowId = NULL;
      drmType = drmType_;
      svpVersion = svpVersion_;
      liveStream = liveStream_;
      sampleFormat = sampleFormat_;
    }
} MEDIA_LOAD_DATA_T;

/**
 * Flash ES Channel Info
 */
typedef enum {
  MEDIA_DATA_CH_NONE = 0,
  MEDIA_DATA_CH_A,                                                              /** For H264 data */
  MEDIA_DATA_CH_B,                                                              /** For AAC data */
  MEDIA_DATA_CH_MAX = MEDIA_DATA_CH_B,
} MEDIA_DATA_CHANNEL_T;

typedef struct MEDIA_VIDEO_DATA_INFO {
  guint vcodec;

  guint bufferMinLevel;
  guint bufferMaxLevel;
  guint userQBufferLevel;       // set Queue buffer size

  guint8 *codec_data;
  gint codec_data_size;
  gint videoWidth;
  gint videoHeight;
  gint videoFpsScale;
  gint videoFpsValue;
} MEDIA_VIDEO_DATA_INFO_T;

typedef struct MEDIA_AUDIO_DATA_INFO {
  // TODO: codec º° data type define
  guint acodec;                 // audio codec  // from CP
  guint languageIDX;            // audio track index
  char *languageCode;           // audio track code

  guint bufferMinLevel;         // from CP
  guint bufferMaxLevel;         // from CP
  guint userQBufferLevel;       // set Queue buffer siz
} MEDIA_AUDIO_DATA_INFO_T;

typedef struct MEDIA_CUSTOM_CONTENT_INFO {
  guint32 mediaTransportType;
  guint8 mediaSourceType;
  guint32 container;
  guint64 size;
  guint32 vcodec;                // video codec
  guint32 acodec;                // audio codec

  MEDIA_DATA_CHANNEL_T esCh;

  gint64 ptsToDecode;           // to support seek
  gint64 pipelineStartTime;
  gboolean bRestartStreaming;    // when underflow is detected - VUDU
  gboolean bSeperatedPTS;        // TRUE : pts is sent by parameter (ES case only)
  guint8 svpVersion;             // 0:none, 10:buffer based, 20:address based

  gint32 preBufferTime;

  gboolean bUseBufferCtrl;       // if it is TRUE, the below time value is meaningful.
  gboolean userBufferCtrl;
  gint32 bufferingMinTime;
  gint32 bufferingMaxTime;
  guint8 bufferMinPercent;
  guint8 bufferMaxPercent;

  MEDIA_VIDEO_DATA_INFO_T videoDataInfo;
  MEDIA_AUDIO_DATA_INFO_T audioDataInfo;

  guint16 inPort;                /* to support wifi display */
  guint32 delayOffset;           /* to support wifi display */

  char *pDrmClientID;

  guint32 start_bps;
  guint32 thumbnailOnPause_Width;                /**< thumbnail size define. for thumbnail on pause status func. */
  guint32 thumbnailOnPause_Hight;                /**< thumbnail size define. for thumbnail on pause status func. */
  gboolean bVdecChannelReserved;               /**< for external VDEC resource info (from DVR) */
  gboolean bIsNetflix;                   /**< for netflix specific case> */

  guint32 startTime;
  gboolean isDashTsEncrypted;
} MEDIA_CUSTOM_CONTENT_INFO_T;

typedef enum {
  IDX_MULTI = 0,
  IDX_VIDEO = IDX_MULTI,
  IDX_AUDIO,
  IDX_MAX,
} MEDIA_SRC_ELEM_IDX_T;

typedef enum {
  MEDIA_OK = 0,
  MEDIA_ERROR = -1,
  MEDIA_NOT_IMPLEMENTED = -2,
  MEDIA_NOT_SUPPORTED = -6,
  MEDIA_BUFFER_FULL = -7,                         /**< function doesn't works cause buffer is full */
  MEDIA_INVALID_PARAMS = -3,                      /**< Invalid parameters */
  MEDIA_NOT_READY = -11,                          /**< API's resource is not ready */
} MEDIA_STATUS_T;

typedef enum {
  DEFAULT_DISPLAY = 0,
  PRIMARY_DISPLAY = 0,
  SECONDARY_DISPLAY,
} DISPLAY_PATH;

typedef enum {
  PLAYBIN_PIPELINE = 0,
  CUSTOM_PLAYER_PIPELINE,
  HDMI_PIPELINE
} PIPELINE_TYPE;

typedef struct {
  GstElement *pSrcElement;
  guint bufferMaxByte;
  guint bufferMinPercent;
  std::string elementName;
  CUSTOM_BUFFERING_STATE_T needFeedData;
  guint64 totalFeed;
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

typedef struct ACQUIRE_RESOURCE_INFO {
  gmp::base::source_info_t* sourceInfo;
  char *displayMode;
  gboolean result;
} ACQUIRE_RESOURCE_INFO_T;

#endif  // SRC_PLAYER_PLAYERTYPES_H_
