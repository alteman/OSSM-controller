#include "modules/remote.h"

#include <Arduino.h>

#include "ws.h"
#include "mystepper.h"

void process_remote_cmd(const void* _buf, size_t cb) {
    const RemoteCmd *rc = (RemoteCmd*)_buf;
    if (cb != sizeof(RemoteCmd)) {
        Serial.printf("Malformed remote cmd: %u bytes received %u expected\n", cb, sizeof(RemoteCmd));
        return;
    }
    Serial.printf("Remote cmd: %i %i %i/%i\n", rc->enc[0], rc->enc[1], rc->enc[2], rc->cmd);
    switch(rc->cmd) {
        case RC_NONE:
            break;
        case RC_ESTOP:
            mystepper_estop();
            break;
    }
    mystepper_step_adjust(rc->enc[0], &adjScale);
    mystepper_step_adjust(rc->enc[1], &adjOffset);
};
