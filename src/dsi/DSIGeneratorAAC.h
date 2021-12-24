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

#include "DSIGenerator.h"
#include "PlayerTypes.h"

namespace gmp { namespace dsi {

class DSIGeneratorAAC : public DSIGenerator {
    public:
        DSIGeneratorAAC(const MEDIA_LOAD_DATA_T *loadData);
        ~DSIGeneratorAAC() {}
        GstCaps* GenerateSpecificInfo() override;
    private:

        bool SetGstCodecData();

        /* Gets the Sampling Frequency Index value, as defined by table 1.18 in ISO/IEC 14496-3 */
        guint8 GetFrequencyIndex(const guint32 &sampleRate);


    private:
        guint8 channels_;
        guint8 audioObjectType_;
        guint32 sampleRate_;
        guint8* audioCodecData_;
        guint32 audioCodecDataSize_;
};
}  // namespace dsi
}  // namespace gmp
