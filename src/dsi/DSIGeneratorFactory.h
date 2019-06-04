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
        ~DSIGeneratorFactory() { GMP_INFO_PRINT("dtor dsi count: %d", dsiPtr_.use_count()); }

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
