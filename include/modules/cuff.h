#pragma once

#include <stdint.h>
#include <stddef.h>

#include <ESPAsyncWebServer.h>

#ifdef __cplusplus
extern "C" {
#endif

void process_cuff_cmd(const void* _buf, size_t cb, AsyncWebSocket * server, AsyncWebSocketClient * client);

#ifdef __cplusplus
}
#endif