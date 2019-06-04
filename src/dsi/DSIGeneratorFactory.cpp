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
