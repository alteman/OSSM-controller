#include "modules/timesync.h"

#include "proto.h"
#include "ws.h"

static uint16_t sync_epoch = 0;
static uint16_t prec = (uint16_t)-1u;
static bool last_sync_step = false;
uint64_t timedelta_lo = 0;
uint64_t timedelta_hi = -1ull;
static uint64_t ts[3] = {0}; 
static uint64_t sv_ts[2] = {0};

void process_timesync_cmd(const void* data, size_t cb, AsyncWebSocketClient* client) {
    if (cb != sizeof(timesync_in_pkt)) {
        Serial.printf("sync_time_handler: got %u bytes, but expected %u\n", cb, sizeof(timesync_in_pkt));
        return;
    }
    const timesync_in_pkt* pkt = (timesync_in_pkt*)data;
    if (pkt->epoch != sync_epoch) {
        return;
    }
    ts[last_sync_step ? 2 : 1] = esp_timer_get_time();
    memset(&sv_ts[last_sync_step ? 1 : 0], 0, sizeof(*sv_ts));
    memcpy(&sv_ts[last_sync_step ? 1 : 0], pkt->ts6, sizeof(pkt->ts6));
    if (!last_sync_step) {
        last_sync_step = true;
        sync_epoch++;
        sync_msg_2 msg = {.epoch = sync_epoch, .prec = prec};
        ws_typed_response(client, MSG_SYNC, &msg, sizeof(msg));
    } else {
        for (uint32_t i = 0; i < 2; ++i) {
            auto dhi = sv_ts[i] - ts[i] / 1000;
            auto dlo = sv_ts[i] - ts[i + 1] / 1000;
            if (timedelta_hi > dhi) {
                timedelta_hi = dhi;
            }
            if (timedelta_lo < dlo) {
                timedelta_lo = dlo;
            }
            // skew correction
            if (timedelta_hi < timedelta_lo) {
                bool fix_lo = (timedelta_hi == dhi);
                if (fix_lo) {
                    timedelta_lo = dlo;
                } else {
                    timedelta_hi = dhi;
                }
                Serial.printf("UNSKEW: td_low=%llu td_high=%llu dlo=%llu dhi=%llu\n", timedelta_lo, timedelta_hi, dlo, dhi);
            }
        }
        auto prec64 = timedelta_hi - timedelta_lo;
        prec = (uint16_t)prec64;
        if (prec64 > (uint16_t)-1u) {
            prec = (uint16_t)-1u;
        }
        Serial.printf("td_low=%llu td_high=%llu prec64=%llu\n", timedelta_lo, timedelta_hi, prec64);
    }
}

void timesync_sync_time1(AsyncWebSocket * server) {
    sync_msg_1 msg;
    bool messageReady = false;
    for(const auto& c: server->getClients()) {
        if (ws_check_client_type(c, MSG_TYPE::MSG_TIMED)) {
            if (!messageReady) {
                messageReady = true;
                last_sync_step = false;
                sync_epoch++;
                msg.epoch = sync_epoch;
                ts[0] = esp_timer_get_time();
            }
            ws_typed_response(c, MSG_SYNC, &msg, sizeof(msg));            
        }
    }
//    Serial.printf("sync_time1 sent to %i (%p)\n", lastClientId, client);

}