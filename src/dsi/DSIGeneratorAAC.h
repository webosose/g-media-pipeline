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
