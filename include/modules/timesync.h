#pragma once

#include <stdint.h>
#include <stddef.h>

#include <ESPAsyncWebServer.h>

extern uint64_t timedelta_lo;
extern uint64_t timedelta_hi;

#ifdef __cplusplus
extern "C" {
#endif

void process_timesync_cmd(const void* _buf, size_t cb, AsyncWebSocketClient* client);
void timesync_sync_time1(AsyncWebSocket * server);

#ifdef __cplusplus
}
#endif