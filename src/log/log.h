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

#ifndef SRC_LOG_LOG_H_
#define SRC_LOG_LOG_H_

#include <PmLogLib.h>
#include <assert.h>

PmLogContext GetPmLogContext();
#define GMP_LOG_CRITICAL(...) PmLogCritical(GetPmLogContext(), ##__VA_ARGS__)
#define GMP_LOG_ERROR(...)    PmLogError(GetPmLogContext(), ##__VA_ARGS__)
#define GMP_LOG_WARNING(...)  PmLogWarning(GetPmLogContext(), ##__VA_ARGS__)

#define GMP_LOG_INFO(FORMAT__, ...) \
    PmLogInfo(GetPmLogContext(), \
    "gmp", 0, "[%s:%d] " FORMAT__, __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__)

#define GMP_LOG_DEBUG(FORMAT__, ...) \
    PmLogDebug(GetPmLogContext(), \
    "[%s:%d]" FORMAT__, __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__)

#define GMP_LOG_OBJ_SET(OBJ__)           PmLogContext GetPmLogContext_##OBJ__()
#define GMP_LOG_OBJ_CRITICAL(OBJ__, ...) \
    PmLogCritical(GetPmLogContext_##OBJ__(), ##__VA_ARGS__)
#define GMP_LOG_OBJ_ERROR(OBJ__, ...) \
    PmLogError(GetPmLogContext_##OBJ__(), ##__VA_ARGS__)
#define GMP_LOG_OBJ_WARNING(OBJ__, ...) \
    PmLogWarning(GetPmLogContext_##OBJ__(), ##__VA_ARGS__)
#define GMP_LOG_OBJ_INFO(OBJ__, ...) \
    PmLogInfo(GetPmLogContext_##OBJ__(), ##__VA_ARGS__)
#define GMP_LOG_OBJ_DEBUG(OBJ__, FORMAT__, ...) \
    PmLogDebug(GetPmLogContext_##OBJ__(), \
    "[%s:%d]" FORMAT__, __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__)

/* Info Print */
#define GMP_INFO_PRINT GMP_LOG_INFO

/* Debug Print */
#define GMP_DEBUG_PRINT GMP_LOG_DEBUG

/* Assert print */
#define GMPASSERT(cond) { \
    if (!(cond)) { \
        GMP_DEBUG_PRINT("ASSERT FAILED : %s:%d:%s: %s", \
                __FILE__, __LINE__, __func__, #cond); \
        assert(0); \
    } \
}

#endif  // SRC_LOG_LOG_H_
