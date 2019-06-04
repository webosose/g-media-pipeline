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

#ifndef SRC_DECODING_SPECIFIC_INFO_GENERATOR_H_
#define SRC_DECODING_SPECIFIC_INFO_GENERATOR_H_

#include <gst/gst.h>

namespace gmp { namespace dsi {

class DSIGenerator {
    public:
        DSIGenerator() : caps_(NULL), codec_buf_(NULL) {}
        ~DSIGenerator() {}
        virtual GstCaps* GenerateSpecificInfo() = 0;

    protected:
        GstCaps *caps_;
        GstBuffer *codec_buf_;
};
}  // namespace dsi
}  // namespace gmp
#endif  // SRC_DECODING_SPECIFIC_INFO_GENERATOR_H_
