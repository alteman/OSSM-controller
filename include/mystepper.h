#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct {
    double* val;
    double max;
    double min;
    uint16_t topStep;
} AdjVar;

typedef struct {
    uint16_t pos;
    uint16_t ms_x10;
} Op;

extern const AdjVar adjScale;
extern const AdjVar adjOffset;

const uint16_t MAX_OPS_LEN = 256;

extern Op g_ops[MAX_OPS_LEN];
extern bool g_ops_dirty;
extern uint16_t g_ops_cnt;


#ifdef __cplusplus
extern "C" {
#endif

void mystepper_process_timed_cmd(const void* _buf, size_t cb);
void mystepper_step_adjust(int8_t signedSteps, const AdjVar* av);
void mystepper_estop(void);
void mystepper_reset_state(void);
void mystepper_motion_update(void);
void mystepper_loop(void);

#ifdef __cplusplus
}
#endif