#include "ws.h"

#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>

#include "modules/timesync.h"
#include "modules/seq_cmd.h"
#include "modules/remote.h"
#include "modules/cuff.h"

#include "mystepper.h"


static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

static bool ensure_context(AsyncWebSocketClient * client) {
    if (!client->_tempObject) {
        client->_tempObject = calloc(1, sizeof(client_context_t));
        if (!client->_tempObject) {
            Serial.printf("Cannot alloc %u bytes for client_context_t\n", sizeof(client_context_t));
            return false;
        }
    }
    return true;
}

static void tag_client_type(AsyncWebSocketClient * client, MSG_TYPE msgType) {
    if (!ensure_context(client)) {
        return;
    }
    client_context_t* c = (client_context_t*)client->_tempObject;
    c->typeFlags |= 1 << msgType;
}

static void handle_error_or_disconnect(AsyncWebSocketClient * client) {
    if (ws_check_client_type(client, MSG_TYPE::MSG_TIMED)) {
        mystepper_reset_state();
    }
    // clean up _tempObject
    if (client->_tempObject) {
        free(client->_tempObject);
        client->_tempObject = NULL;
    }
}

static void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
    if(type == WS_EVT_CONNECT) {
        Serial.printf("ws[%s][%u] connect, extra len=%u\n", server->url(), client->id(), len);
        //client->printf("Hello Client %u :)", client->id());
        //client->ping();
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("ws[%s][%u] disconnect\n", server->url(), client->id());
        handle_error_or_disconnect(client);
    } else if(type == WS_EVT_ERROR) {
        Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
        handle_error_or_disconnect(client);
    } else if(type == WS_EVT_PONG) {
        //Serial.printf("ws[%s][%u] pong[%u]: %%s\n", server->url(), client->id(), len/*, (len)?(char*)data:""*/);
    } else if(type == WS_EVT_DATA) {
        AwsFrameInfo * info = (AwsFrameInfo*)arg;
        String msg = "";
        if (info->final && info->index == 0 && info->len == len) {
            //the whole message is in a single frame and we got all of it's data
            //Serial.printf("ws[%s][%u] len=[%llu]\n", server->url(), client->id(), info->len);

            if (info->opcode == WS_BINARY) {
                if (info->len) {
                    auto cb = info->len - 1;
                    MSG_TYPE msgType = (MSG_TYPE)(data[0]);
                    tag_client_type(client, msgType);
                    // FIXME tag connection with types here?
                    // FIXME could keep lastControllerId pointing to stale half-dead connection..
                    // if (data[0] != MSG_REMOTE && lastControllerId == -1) {
                    //     lastControllerId = client->id();
                    // }
                    switch(msgType) {
                        case MSG_SYNC:
                            process_timesync_cmd(data + 1, cb, client);
                            break;
                        case MSG_CMD:
                            process_seq_cmd(data + 1, cb);
                            break;
                        case MSG_TIMED:
                            mystepper_process_timed_cmd(data + 1, cb);
                            break;
                        case MSG_REMOTE:
                            process_remote_cmd(data + 1, cb);
                            break;
                        case MSG_CUFF:
                            process_cuff_cmd(data + 1, cb, server, client);
                            break;
                        default:
                            Serial.printf("Unknown message type: %u\n", msgType);
                            break;
                    }
                }
            }
        }
    }
}

bool ws_check_client_type(AsyncWebSocketClient * client, MSG_TYPE msgType) {
    if (!ensure_context(client)) {
        return false;
    }
    client_context_t* c = (client_context_t*)client->_tempObject;
    return !!(c->typeFlags & (1 << msgType));
}

void ws_typed_response(AsyncWebSocketClient * client, MSG_TYPE msgType, const void* buf, size_t cb) {
    uint8_t* _buf = (uint8_t*)alloca(cb + 1);
    *_buf = msgType;
    memcpy(_buf + 1, buf, cb);
    client->binary(_buf, cb + 1);
}

void ws_10hz_task() { 
    ws.cleanupClients();
    static uint8_t every10 = 0;
    if (++every10 == 10) {
        every10 = 0;
        timesync_sync_time1(&ws);
    }
}


void ws_start_server() {
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
    Serial.println("serve/1");
    server.begin();
}
