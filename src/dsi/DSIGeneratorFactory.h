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

#ifndef SRC_DECODING_SPECIFIC_INFO_GENERATOR_FACTORY_H_
#define SRC_DECODING_SPECIFIC_INFO_GENERATOR_FACTORY_H_

#include <gst/gst.h>
#include <memory>
#include "PlayerTypes.h"
#include "DSIGenerator.h"
#include "log/log.h"

/*
enum class media_codec_t : int {
  MEDIA_CODEC_AAC,
  MEDIA_CODEC_MAX
}
*/

namespace gmp { namespace dsi {

class DSIGeneratorFactory {
    public:
        DSIGeneratorFactory(const MEDIA_LOAD_DATA_T* loadData);
        ~DSIGeneratorFactory() {}

        std::shared_ptr<gmp::dsi::DSIGenerator> getDSIGenerator(){
            return dsiPtr_;
        }

    private:
        std::shared_ptr<gmp::dsi::DSIGenerator> CreateDSIGenerator(const MEDIA_LOAD_DATA_T* loadData);

    private:
        std::shared_ptr<gmp::dsi::DSIGenerator> dsiPtr_;
};
}  // namespace dsi
}  // namespace gmp
#endif  // SRC_DECODING_SPECIFIC_INFO_GENERATOR_FACTORY_H_
