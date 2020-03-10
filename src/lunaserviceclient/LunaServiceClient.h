// @@@LICENSE
//
// Copyright (C) 2020, LG Electronics, All Right Reserved.
//
// No part of this source code may be communicated, distributed, reproduced
// or transmitted in any form or by any means, electronic or mechanical or
// otherwise, for any purpose, without the prior written permission of
// LG Electronics.
//
// LICENSE@@@


#ifndef SRC_LUNA_CLIENT_H_
#define SRC_LUNA_CLIENT_H_

#include <lunaservice.h>
#include <glib.h>

#include <map>
#include <thread>
#include <functional>

namespace gmp {
typedef std::function<void (const char *)> ResponseHandler;
    struct ResponseHandlerWrapper {
          ResponseHandler handler;
          explicit ResponseHandlerWrapper(ResponseHandler rh) : handler(rh) { }
    };

class LunaServiceClient {
public:
   LunaServiceClient();
   ~LunaServiceClient();

   bool CallAsync(const char *uri,
                  const char *param,
                  ResponseHandler handler);
   bool subscribe(const char *uri,
                  const char *param,
                  unsigned long *subscribeKey,
                  ResponseHandler handler);
   bool unsubscribe(unsigned long subscribeKey);

private:
  LSHandle* handle;
  GMainContext* context;
  std::map<unsigned long, std::unique_ptr<ResponseHandlerWrapper>> handlers;
};//LunaServiceClient

} //gmp

#endif  // SRC_LUNA_CLIENT_H_
