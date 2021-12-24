// Copyright (c) 2018-2021 LG Electronics, Inc.
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

#include "DSIGeneratorFactory.h"
/* TODO: need to add WMA and H264 generator for raw stream */
#include "DSIGeneratorAAC.h"

namespace gmp { namespace dsi {

DSIGeneratorFactory::DSIGeneratorFactory(const MEDIA_LOAD_DATA_T* loadData)
    : dsiPtr_(NULL)
{
    dsiPtr_ = CreateDSIGenerator(loadData);
}

std::shared_ptr<gmp::dsi::DSIGenerator> DSIGeneratorFactory::CreateDSIGenerator(const MEDIA_LOAD_DATA_T* loadData)
{
    switch(loadData->audioCodec) {
       case GMP_AUDIO_CODEC_AAC: {
          dsiPtr_ = std::make_shared<gmp::dsi::DSIGeneratorAAC>(loadData); //audioObjectType, Channel, sampleRate
          break;
       }
       default: {
          break;
       }
    }

    return dsiPtr_;
}


}  // namespace dsi
}  // namespace gmp
