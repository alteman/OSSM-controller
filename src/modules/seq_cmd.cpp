#include "modules/seq_cmd.h"

#include <Arduino.h>

#include "mystepper.h"

void process_seq_cmd(const uint8_t* data, size_t cb) {
    if ((cb % sizeof(Op)) == 0 && cb <= sizeof(g_ops)) {
        memcpy(&g_ops, data + 1, cb);
        g_ops_cnt = cb / sizeof(Op);
        g_ops_dirty = true;
        Serial.printf("New cmd len: %u\n", cb);
    }
}