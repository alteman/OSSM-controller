#pragma once

#include <stdint.h>
#include <stddef.h>

#include<ESPAsyncWebServer.h>

#include "proto.h"

typedef struct {
  uint8_t typeFlags; // could be multiple types?
  struct {
    CuffState state;
  } cuff;
} client_context_t;

#ifdef __cplusplus
extern "C" {
#endif


bool ws_check_client_type(AsyncWebSocketClient * client, MSG_TYPE msgType);
void ws_typed_response(AsyncWebSocketClient * client, MSG_TYPE msgType, const void* buf, size_t cb);
void ws_10hz_task();
void ws_start_server(void);

#ifdef __cplusplus
}
#endif