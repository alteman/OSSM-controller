#include "modules/cuff.h"

#include "proto.h"
#include "ws.h"
#include "mystepper.h"


void process_cuff_cmd(const void* _buf, size_t cb, AsyncWebSocket * server, AsyncWebSocketClient * client) {
    const CuffPktIn *cp = (CuffPktIn*)_buf;
    if (cb != sizeof(CuffPktIn)) {
        Serial.printf("Malformed cuff packet: %u bytes received %u expected\n", cb, sizeof(CuffPktIn));
        return;
    }
    Serial.printf("Cuff cmd: %i \n", cp->state);
    switch(cp->state) {
        case CUFF_IDLE:
            mystepper_estop();
            for(const auto& c: server->getClients()) {
                if (ws_check_client_type(c, MSG_TYPE::MSG_TIMED)) {
                    TimedPktOut pkt = {.cmd = Pause};
                    ws_typed_response(c, MSG_TIMED, &pkt, sizeof(pkt));
                }
            }
        break;
        case CUFF_READY:
        {
            AsyncWebSocketClient * other = NULL;
            bool allReady = true;
            for(const auto& c: server->getClients()) {
                if (c == client) {
                    continue;
                }
                if (ws_check_client_type(c, MSG_TYPE::MSG_CUFF)) {
                    if (!other) {
                        other = c;
                    } else {
                        // More than two clients..
                        allReady = false;
                    }
                    client_context_t *ctx = (client_context_t*)c->_tempObject;   
                    if (ctx->cuff.state != CUFF_READY) {
                        allReady = false;
                    }
                }
            }
            if (allReady && other) {
                CuffPktOut pkt = {.cmd=CUFF_LOCK};
                ws_typed_response(client, MSG_CUFF, &pkt, sizeof(pkt));
                ws_typed_response(other, MSG_CUFF, &pkt, sizeof(pkt));
            }
        }
        break;
        case CUFF_LOCKED:
            for(const auto& c: server->getClients()) {
                if (ws_check_client_type(c, MSG_TIMED)) {
                    TimedPktOut pkt = {.cmd = Play};
                    ws_typed_response(c, MSG_TIMED, &pkt, sizeof(pkt));
                }
            }
        break;
    }
    // Send pings if not locked..
    //TODO: write something here
    // if state1 == ready && state2 == ready
    // send 'lock' to both connections 
    // send 'play' to UI
}