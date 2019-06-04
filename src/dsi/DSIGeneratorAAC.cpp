// @@@LICENSE
//
// Copyright (C) 2018-2019, LG Electronics, All Right Reserved.
//
// No part of this source code may be communicated, distributed, reproduced
// or transmitted in any form or by any means, electronic or mechanical or
// otherwise, for any purpose, without the prior written permission of
// LG Electronics.
//
// LICENSE@@@

#include <cstring>
#include "DSIGeneratorAAC.h"
#include "log/log.h"

namespace gmp { namespace dsi {

DSIGeneratorAAC::DSIGeneratorAAC(const MEDIA_LOAD_DATA_T* loadData)
    : channels_(0),
      audioObjectType_(0),
      sampleRate_(0),
      audioCodecData_(NULL),
      audioCodecDataSize_(0)
{
    if (loadData) {
        channels_ = loadData->channels;
        audioObjectType_ = loadData->audioObjectType;
        sampleRate_ = loadData->sampleRate;
        audioCodecData_ = loadData->codecData;
        audioCodecDataSize_ = loadData->codecDataSize;
    }
}

GstCaps* DSIGeneratorAAC::GenerateSpecificInfo()
{
    GMP_DEBUG_PRINT("");
    guint8 audioObjectTypeExt = 0;
    guint8 codec_data[6] = {0, };
    caps_ = gst_caps_new_simple("audio/mpeg", "mpegversion", G_TYPE_INT, 4, NULL);
    if (caps_ == NULL) {
        GMP_INFO_PRINT("caps is NULL! DSI generation fails!");
        return NULL;
    }

    // in case of received decoding specific information from application.
    // if audioCodecData doesn't initialized from application, playback is not working.
    if (audioCodecData_ != NULL && audioCodecDataSize_ > 0) {
        bool ret = SetGstCodecData();
        if (ret) {
            GMP_DEBUG_PRINT("DSI generation success!");
            return caps_;
        } else {
            GMP_INFO_PRINT("SetGstCodecData fails!");
            gst_caps_unref(caps_);
            return NULL;
        }
    }

    guint8 frequencyIdx = GetFrequencyIndex(sampleRate_);

    codec_data[0] = (audioObjectType_ & 0x1F) << 3;
    if (audioObjectType_ == 31) {
        //set audioObjectTypeExt 6bit(case of audioObjectType : 31)
        codec_data[0] |= (audioObjectTypeExt & 0x38) >> 3;
        codec_data[1] = (audioObjectTypeExt & 0x07) << 5;

        //set frequencyIdx 4bit(frequency index)
        codec_data[1] |= (frequencyIdx & 0x0F) << 1;

        if(frequencyIdx == 15) {
            //set frequency 24bit(case of frequency index : 15(0xF))
            codec_data[1] |= (sampleRate_ & 0x800000) >> 23; // 1 bit
            codec_data[2] = (sampleRate_ & 0x7F8000) >> 15;  // 8 bit
            codec_data[3] = (sampleRate_ & 0x7F80) >> 7;     // 8 bit
            codec_data[4] = (sampleRate_ & 0x7F);            // 7 bit (total 24 bit)

            //set channels 4bit
            codec_data[4] |= (channels_ & 0x08) << 3;
            codec_data[5] = (channels_ & 0x07) << 5;
        }
        else {
            //set channels 4bit
            codec_data[1] |= (channels_ & 0x08);
            codec_data[2] = (channels_ & 0x07) << 5;
        }
    } else {
        //set freqIdx 4bit(frequency index)
        codec_data[0] |= (frequencyIdx & 0x0E) >> 1;
        codec_data[1] = (frequencyIdx & 0x01) << 7;

        if(frequencyIdx == 15) {
            //set frequency 24bit(case of frequency index : 15(0xF))
            codec_data[1] |= (sampleRate_ & 0xFE0000) >> 17; // 7bit
            codec_data[2] = (sampleRate_ & 0x01FE00) >> 9;   // 8 bit
            codec_data[3] = (sampleRate_ & 0x01FE) >> 1;     // 8 bit
            codec_data[4] = (sampleRate_ & 0x01);            // 1 bit (total 24 bit)

            //set channels 4bit
            codec_data[4] |= (channels_ & 0x0F) << 3;
        }
        else {
            //set channels 4bit
            codec_data[1] |= (channels_ & 0x0F) << 3;
        }
    }

    if ((codec_data[0] != 0x00) || (codec_data[1] != 0x00)) {
        audioCodecData_ = codec_data;
        audioCodecDataSize_ = sizeof(codec_data);

        bool ret = SetGstCodecData();
        if (!ret) {
            GMP_INFO_PRINT("SetGstCodecData fails!");
            gst_caps_unref(caps_);
            return NULL;
        }
    }

    GMP_INFO_PRINT("DSI generation success!");
    return caps_;
}

bool DSIGeneratorAAC::SetGstCodecData()
{
    GstMapInfo info;

    codec_buf_ = gst_buffer_new_and_alloc(audioCodecDataSize_);
    if (codec_buf_ == NULL) {
        GMP_INFO_PRINT("gst_buffer_new_and_alloc fails!");
        return false;
    }
    gst_buffer_map(codec_buf_, &info, GST_MAP_READ);
    memcpy(info.data, audioCodecData_, audioCodecDataSize_);
    gst_buffer_unmap(codec_buf_, &info);
    gst_caps_set_simple(caps_, "codec_data", GST_TYPE_BUFFER, codec_buf_, NULL);
    gst_buffer_unref(codec_buf_);

    return true;
}

/* Gets the Sampling Frequency Index value, as defined by table 1.18 in ISO/IEC 14496-3 */
guint8 DSIGeneratorAAC::GetFrequencyIndex(const guint32 &sampleRate) {
  GMP_DEBUG_PRINT("");
  guint8 ret = 0;
  switch (sampleRate) {
    case 96000: {
      ret = 0x0U;
      break;
    }
    case 88200: {
      ret = 0x1U;
      break;
    }
    case 64000: {
      ret = 0x2U;
      break;
    }
    case 48000: {
      ret = 0x3U;
      break;
    }
    case 44100: {
      ret = 0x4U;
      break;
    }
    case 32000: {
      ret = 0x5U;
      break;
    }
    case 24000: {
      ret = 0x6U;
      break;
    }
    case 22050: {
      ret = 0x7U;
      break;
    }
    case 16000: {
      ret = 0x8U;
      break;
    }
    case 12000: {
      ret = 0x9U;
      break;
    }
    case 11025: {
      ret = 0xAU;
      break;
    }
    case 8000: {
      ret = 0xBU;
      break;
    }
    case 7350: {
      ret = 0xCU;
      break;
    }
    default: {
      ret = 0xFU;
      break;
    }
  }

  return ret;
}

}  // namespace dsi
}  // namespace gmp


