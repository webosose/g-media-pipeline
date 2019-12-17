/******************************************************************************
 *   LCD TV LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 *   Copyright(c) 2018-2019 by LG Electronics Inc.
 *
 *   All rights reserved. No part of this work may be reproduced, stored in a
 *   retrieval system, or transmitted by any means without prior written
 *   permission of LG Electronics Inc.
 ******************************************************************************/

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
