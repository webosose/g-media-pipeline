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

#include <map>
#include <string>
#include <cstring>
#include "api_call_checker.h"

std::map<std::string, bool> apiCallMap;

void clearCallLog(void)
{
    apiCallMap.clear();
}

bool isAPICalled(const char *apiName)
{
    auto item = apiCallMap.find(apiName);
    if (item != apiCallMap.end()) {
        return true;
    } else {
        return false;
    }
}

void callAPI(const char *apiName)
{
    apiCallMap[apiName] = true;
}
