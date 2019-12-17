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
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef SRC_BASE_MESSAGE_H_
#define SRC_BASE_MESSAGE_H_

typedef enum MEDIA_MESSAGE {
  MEDIA_MSG_MSG_NONE = 0x00,                         /**< no message */
  MEDIA_MSG_ERR_LOAD = 0x0068,
  MEDIA_MSG_ERR_PLAYING = 0xf000,

  /* gstrreamer core error msg */
  MEDIA_MSG_START_GST_MSG = 0xf100,              // d16
  MEDIA_MSG__GST_CORE_ERROR_FAILED,              // a general error which doesn't fit in any other category. Make sure you add a custom message to the error call.
  MEDIA_MSG__GST_CORE_ERROR_TOO_LAZY,            // do not use this except as a placeholder for deciding where to go while developing code.
  MEDIA_MSG__GST_CORE_ERROR_NOT_IMPLEMENTED,     // use this when you do not want to implement this functionality yet.
  MEDIA_MSG__GST_CORE_ERROR_STATE_CHANGE,        // used for state change errors.
  MEDIA_MSG__GST_CORE_ERROR_PAD,                 // used for pad-related errors.
  MEDIA_MSG__GST_CORE_ERROR_THREAD,              // used for thread-related errors.
  MEDIA_MSG__GST_CORE_ERROR_NEGOTIATION,         // used for negotiation-related errors.
  MEDIA_MSG__GST_CORE_ERROR_EVENT,               // used for event-related errors.
  MEDIA_MSG__GST_CORE_ERROR_SEEK,                // used for seek-related errors.
  MEDIA_MSG__GST_CORE_ERROR_CAPS,                // used for caps-related errors.
  MEDIA_MSG__GST_CORE_ERROR_TAG,                 // used for negotiation-related errors.
  MEDIA_MSG__GST_CORE_ERROR_MISSING_PLUGIN,      // used if a plugin is missing.
  MEDIA_MSG__GST_CORE_ERROR_CLOCK,               // used for clock related errors.
  MEDIA_MSG__GST_CORE_ERROR_DISABLED,            // d30 // used if functionality has been disabled at compile time (Since: 0.10.13).
  /* gstreamer library error msg */
  MEDIA_MSG__GST_LIBRARY_ERROR_FAILED,           // a general error which doesn't fit in any other category. Make sure you add a custom message to the error call.
  MEDIA_MSG__GST_LIBRARY_ERROR_TOO_LAZY,         // do not use this except as a placeholder for deciding where to go while developing code.
  MEDIA_MSG__GST_LIBRARY_ERROR_INIT,             // used when the library could not be opened.
  MEDIA_MSG__GST_LIBRARY_ERROR_SHUTDOWN,         // used when the library could not be closed.
  MEDIA_MSG__GST_LIBRARY_ERROR_SETTINGS,         // used when the library doesn't accept settings.
  MEDIA_MSG__GST_LIBRARY_ERROR_ENCODE,           // used when the library generated an encoding error.
  /* gstreamer resource error msg */
  MEDIA_MSG__GST_RESOURCE_ERROR_FAILED,          // a general error which doesn't fit in any other category. Make sure you add a custom message to the error call.
  MEDIA_MSG__GST_RESOURCE_ERROR_TOO_LAZY,        // do not use this except as a placeholder for deciding where to go while developing code.
  MEDIA_MSG__GST_RESOURCE_ERROR_NOT_FOUND,       // used when the resource could not be found.
  MEDIA_MSG__GST_RESOURCE_ERROR_BUSY,            // d40 // used when resource is busy.
  MEDIA_MSG__GST_RESOURCE_ERROR_OPEN_READ,       // used when resource fails to open for reading.
  MEDIA_MSG__GST_RESOURCE_ERROR_OPEN_WRITE,      // used when resource fails to open for writing.
  MEDIA_MSG__GST_RESOURCE_ERROR_OPEN_READ_WRITE,  // used when resource cannot be opened for both reading and writing, or either (but unspecified which).
  MEDIA_MSG__GST_RESOURCE_ERROR_CLOSE,           // used when the resource can't be closed.
  MEDIA_MSG__GST_RESOURCE_ERROR_READ,            // used when the resource can't be read from.
  MEDIA_MSG__GST_RESOURCE_ERROR_WRITE,           // used when the resource can't be written to.
  MEDIA_MSG__GST_RESOURCE_ERROR_SEEK,            // used when a seek on the resource fails.
  MEDIA_MSG__GST_RESOURCE_ERROR_SYNC,            // used when a synchronize on the resource fails.
  MEDIA_MSG__GST_RESOURCE_ERROR_SETTINGS,        // used when settings can't be manipulated on.
  MEDIA_MSG__GST_RESOURCE_ERROR_NO_SPACE_LEFT,   // d50 // used when the resource has no space left.
  /* gstreamer stream error msg */
  MEDIA_MSG__GST_STREAM_ERROR_FAILED,            // a general error which doesn't fit in any other category. Make sure you add a custom message to the error call.
  MEDIA_MSG__GST_STREAM_ERROR_TOO_LAZY,          // do not use this except as a placeholder for deciding where to go while developing code.
  MEDIA_MSG__GST_STREAM_ERROR_NOT_IMPLEMENTED,   // use this when you do not want to implement this functionality yet.
  MEDIA_MSG__GST_STREAM_ERROR_TYPE_NOT_FOUND,    // used when the element doesn't know the stream's type.
  MEDIA_MSG__GST_STREAM_ERROR_WRONG_TYPE,        // used when the element doesn't handle this type of stream.
  MEDIA_MSG__GST_STREAM_ERROR_CODEC_NOT_FOUND,   // used when there's no codec to handle the stream's type.
  MEDIA_MSG__GST_STREAM_ERROR_DECODE,            // used when decoding fails.
  MEDIA_MSG__GST_STREAM_ERROR_ENCODE,            // used when encoding fails.
  MEDIA_MSG__GST_STREAM_ERROR_DEMUX,             // used when demuxing fails.
  MEDIA_MSG__GST_STREAM_ERROR_MUX,               // used when muxing fails.
  MEDIA_MSG__GST_STREAM_ERROR_FORMAT,            // used when the stream is of the wrong format (for example, wrong caps).
  MEDIA_MSG__GST_STREAM_ERROR_DECRYPT,           // used when the stream is encrypted and can't be decrypted because this is not supported by the element. (Since: 0.10.20)
  MEDIA_MSG__GST_STREAM_ERROR_DECRYPT_NOKEY,     // used when the stream is encrypted and can't be decrypted because no suitable key is available. (Since: 0.10.20)
  MEDIA_MSG_END_GST_MSG,

  MEDIA_CB_MSG_LAST
} MEDIA_MESSAGE_T;

#endif  // SRC_BASE_MESSAGE_H_
