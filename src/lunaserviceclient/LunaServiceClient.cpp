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

#include <chrono>

#include "LunaServiceClient.h"
#include "log.h"

namespace gmp {

struct AutoLSError : LSError {
  AutoLSError() { LSErrorInit(this); }
  ~AutoLSError() { LSErrorFree(this); }
};

bool handleSubscribe(LSHandle *sh, LSMessage *reply, void *ctx);
bool handleAsync(LSHandle *sh, LSMessage *reply, void *ctx);

std::string kServiceName = "com.webos.pipeline";

// LunaServiceClient implematation
LunaServiceClient::LunaServiceClient()
    : handle(nullptr)
    , context(nullptr) {
  GMP_INFO_PRINT("LunaServiceClient IN");
  AutoLSError error;
  std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

  std::string service_name = kServiceName + std::to_string(ms.count());

  GMP_INFO_PRINT("LunaServiceClient service_name : %s",service_name.c_str());

  if (LSRegister(service_name.c_str(), &handle, &error)) {
    context = g_main_context_ref(g_main_context_default());
    LSGmainContextAttach(handle, context, &error);
  }
  GMP_INFO_PRINT("LunaServiceClient OUT");
}

LunaServiceClient::~LunaServiceClient() {
  GMP_INFO_PRINT("~LunaServiceClient IN");
  AutoLSError error;
  LSUnregister(handle, &error);
  g_main_context_unref(context);
  GMP_INFO_PRINT("~LunaServiceClient OUT");
}

bool LunaServiceClient::subscribe(const char *uri,
                                  const char *param,
                                  unsigned long *subscribeKey,
                                  ResponseHandler handler) {
  GMP_INFO_PRINT("LunaServiceClient subscribe IN");
  AutoLSError error;
  ResponseHandlerWrapper *wrapper = new ResponseHandlerWrapper(handler);
  if (!wrapper) {
    GMP_INFO_PRINT("LunaServiceClient subscribe wrapper NULL");
    return false;
  }

  if (!LSCall(handle, uri, param, handleSubscribe,
               wrapper, subscribeKey, &error)) {
     GMP_INFO_PRINT("LunaServiceClient subscribe LSCall failed");
     delete wrapper;
     return false;
  }

  handlers[*subscribeKey] = std::unique_ptr<ResponseHandlerWrapper>(wrapper);
  GMP_INFO_PRINT("LunaServiceClient subscribe OUT");
  return true;
}

bool LunaServiceClient::unsubscribe(unsigned long subscribeKey) {
  GMP_INFO_PRINT("LunaServiceClient unsubscribe IN");
  AutoLSError error;
  if (!LSCallCancel(handle, subscribeKey, &error)) {
    GMP_INFO_PRINT("LunaServiceClient unsubscribe LSCallCancel failed");
    handlers.erase(subscribeKey);
    return false;
  }

  handlers.erase(subscribeKey);
  GMP_INFO_PRINT("LunaServiceClient unsubscribe OUT");
  return true;
}

bool handleSubscribe(LSHandle *sh, LSMessage *reply, void *ctx) {
  GMP_INFO_PRINT("LunaServiceClient handleSubscribe IN");
  ResponseHandlerWrapper *wrapper = static_cast<ResponseHandlerWrapper *>(ctx);
  if (!wrapper) {
    GMP_INFO_PRINT("LunaServiceClient handleSubscribe wrapper NULL");
    return false;
  }

  LSMessageRef(reply);
  wrapper->handler(LSMessageGetPayload(reply));
  LSMessageUnref(reply);

  GMP_INFO_PRINT("LunaServiceClient handleSubscribe OUT");

  return true;
}

bool LunaServiceClient::CallAsync(const char *uri,
                                  const char *param,
                                  ResponseHandler handler) {
  GMP_INFO_PRINT("LunaServiceClient CallAsync IN");
  AutoLSError error;
  ResponseHandlerWrapper* wrapper = new ResponseHandlerWrapper(handler);
  if (!wrapper) {
    GMP_INFO_PRINT("LunaServiceClient CallAsync wrapper NULL");
    return false;
  }

  if (!LSCallOneReply(handle, uri, param, handleAsync, wrapper,
                      NULL, &error)) {
    GMP_INFO_PRINT("LunaServiceClient CallAsync LSCallOneReply failed");
    delete wrapper;
    return false;
  }
  GMP_INFO_PRINT("LunaServiceClient CallAsync OUT");
  return true;
}

bool handleAsync(LSHandle *sh, LSMessage *reply, void *ctx) {
  GMP_INFO_PRINT("LunaServiceClient handleAsync IN");
  ResponseHandlerWrapper *wrapper = static_cast<ResponseHandlerWrapper *>(ctx);
  if (!wrapper) {
    return false;
  }

  if(wrapper->handler) {
    LSMessageRef(reply);
    wrapper->handler(LSMessageGetPayload(reply));
    LSMessageUnref(reply);
  }

  GMP_INFO_PRINT("LunaServiceClient handleAsync OUT");

  return true;
}

} //gmp
