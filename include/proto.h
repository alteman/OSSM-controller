#pragma once

#include <stdint.h>
#include <sys/cdefs.h>

typedef enum {
    MSG_SYNC = 0, 
    MSG_CMD = 1,
    MSG_TIMED = 2,
    MSG_REMOTE = 3,
    MSG_CUFF = 4,
} MSG_TYPE;

typedef enum {
    Play,
    Pause,
} TimedCmdOut;

typedef struct {
    uint32_t ts;
    uint16_t pos;
} __packed TimedOp;

typedef struct {
    TimedCmdOut cmd;
} __packed TimedPktOut;


typedef struct {
    uint16_t epoch;
    uint8_t ts6[6];
} __packed timesync_in_pkt;

typedef struct {
    uint16_t epoch;
} __packed sync_msg_1;

typedef struct {
    uint16_t epoch;
    uint16_t prec;
} __packed sync_msg_2;

// Remote defs

typedef enum {
    RC_NONE = 0, 
    RC_ESTOP = 1,
} RC_CMD;

typedef struct {
    int8_t enc[3] = {0}; // Encoder increments
    uint8_t cmd = 0; // special functions?
} __packed RemoteCmd;

// CUFFS

typedef enum {
  CUFF_IDLE,
  CUFF_READY,
  CUFF_LOCKED,
} CuffState;

typedef enum {
  CUFF_LOCK,
  CUFF_UNLOCK,
} CuffCmd;

typedef struct {
    CuffState state;
} __packed CuffPktIn;

typedef struct {
    CuffCmd cmd;
} __packed CuffPktOut;