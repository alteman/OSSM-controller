
#include "mystepper.h"

#include <stdint.h>
#include <Arduino.h>

#include "ard_spline.h"
#include "ard_stepper.h"
#include "ard_timing.h"

#include "ws.h"

#include "proto.h"
#include "modules/timesync.h"


// Stepper
// -------

#define STEPPER_MIN_PULSE_STEP_SIZE 100U
#define STEPPER_PULSES_PER_REV 800
#define STEPPER_MAX_DEGREES_PER_SEC 360.0 * 6
#define STEPPER_COMMAND_DELAY_STEPS 3

#ifndef STEPPER_PIN_PULSE
#define STEPPER_PIN_PULSE 4
#endif

#ifndef STEPPER_PIN_DIRECTION
#define STEPPER_PIN_DIRECTION 5
#endif

#ifndef STEPPER_PIN_ENABLE
#define STEPPER_PIN_ENABLE 6
#endif


#define ERROR_ALLOCATE_MESSAGE "Failed to allocate memory"
#define ERROR_SPLINE_MESSAGE "Failed to generate spline"
#define ERROR_ENCODER_MESSAGE "Failed to configure encoder"

static void fail_forever(const char *restrict msg)
{
    // prepare packet for write
    Serial.println(msg);
    while (true)
    {
        delay(3000);
        Serial.println(msg);
    }
}

static void fail_alloc() { fail_forever(ERROR_ALLOCATE_MESSAGE); }

ArdStepper g_stepper;

static ArdStepperParameters get_stepper_parameters(void)
{
    ArdStepperParameters stepper_parameters;
    stepper_parameters.buffer_size = 16;
    stepper_parameters.pulses_per_rev = STEPPER_PULSES_PER_REV;
    stepper_parameters.units_per_rev = 360.0;
    stepper_parameters.max_unit_velocity = STEPPER_MAX_DEGREES_PER_SEC;
    stepper_parameters.min_pulse_step_size = STEPPER_MIN_PULSE_STEP_SIZE;
    stepper_parameters.command_step_size = ARD_TIMING_100HZ_STEP_SIZE;
    stepper_parameters.command_delay_steps = STEPPER_COMMAND_DELAY_STEPS;
    stepper_parameters.pins.pulse = STEPPER_PIN_PULSE;
    stepper_parameters.pins.direction = STEPPER_PIN_DIRECTION;
    stepper_parameters.pins.enable = STEPPER_PIN_ENABLE;
    stepper_parameters.pins.direction_polarity = 0;

    return stepper_parameters;
}


// ArdTiming
// ----------

ArdTiming g_timing_10hz;
ArdTiming g_timing_100hz;
ArdTiming g_timing_200hz;

ArdSplineTrajectory g_traj;
ArdSplinePva g_traj_start;
ArdSplinePva g_traj_end;
double g_traj_duration;

#define NEXTOP(x) (((x) + 1) % MAX_OPS_LEN)
uint16_t g_ops_cnt = 0;

Op g_ops[MAX_OPS_LEN];
bool g_ops_dirty = false;
static uint16_t g_ops_step = 0;
static double g_current_pos = 0;
static uint32_t g_current_ts = 0;
static bool g_estop = false;

static bool g_streaming = false;

static TimedOp g_timedOps[MAX_OPS_LEN];
static uint16_t g_timedOps_begin = 0;
static uint16_t g_timedOps_end = 0;
static bool g_preempt_current_op = false;

// timesync stuff
static double g_offset = 0;
const AdjVar adjOffset = {&g_offset, .max=500, -500, 100};

static double g_scale = 1.0 / 120;
const AdjVar adjScale = {&g_scale, .max=1.0 / 60, 1.0 / 600, 100};

__inline int __mod32cmp(uint32_t a, uint32_t b) {
    if (a == b)
        return 0;
    return (a - b) >= 0x80000000 ? -1: 1;
}

#define MOD32CMP(a, b) (__mod32cmp(a, b))

void mystepper_motion_update()
{
    bool reset = false;
    if (g_ops_dirty) {
        g_ops_step = 0;
        g_ops_dirty = false;
        if (g_current_pos != g_offset)
            reset = true;
    }
    Op op = {.pos = 0, .ms_x10 = 100};
    if (!g_estop && !reset && g_ops_cnt != 0) {
        Serial.printf("motion_update: g_ops_step=%u/%u, pos=%u, ms10=%u\n", g_ops_step, g_ops_cnt, g_ops[g_ops_step].pos, g_ops[g_ops_step].ms_x10);
        op = g_ops[g_ops_step++];
        if (g_ops_step >= g_ops_cnt) {
            g_ops_step = 0;
        }
    }
    g_traj_start.position = g_current_pos;
    g_traj_start.velocity = 0;
    g_traj_start.acceleration = 0;

    g_traj_end.position = op.pos * g_scale + g_offset;
    g_traj_end.velocity = 0;
    g_traj_end.acceleration = 0;

    g_current_pos = g_traj_end.position;

    g_traj_duration = op.ms_x10 * 0.01;

    int ret =
        ard_spline_traj_generate(&g_traj, &g_traj_start, &g_traj_end, g_traj_duration,
                                ARD_TIMING_100HZ_STEP_SIZE, EARD_SPLINE_QUINTIC);
    Serial.printf("motion_update: %f to %f, dur=%f\n", g_traj_start.position, g_traj_end.position, g_traj_duration);
    if (ret != 0) {
        fail_forever(ERROR_SPLINE_MESSAGE);
    }
}

void streamed_cmd_update()
{
    uint32_t ts = (uint32_t)(esp_timer_get_time() / 1000);

    TimedOp op;
    for (;;) {
        if (!g_estop && g_timedOps_begin != g_timedOps_end) {
            op = g_timedOps[g_timedOps_begin];
            g_timedOps_begin = NEXTOP(g_timedOps_begin);
            if (MOD32CMP(op.ts, ts + 50) <= 0) {
                continue;
            }
        } else {
            op = {.ts = ts + 500, .pos = 0};
        }
        g_traj_start.position = g_current_pos;
        g_traj_start.velocity = 0;
        g_traj_start.acceleration = 0;

        g_traj_end.position = op.pos * g_scale + g_offset;
        g_traj_end.velocity = 0;
        g_traj_end.acceleration = 0;

        g_traj_duration = (op.ts - ts) * 0.001;

        int ret =
            ard_spline_traj_generate(&g_traj, &g_traj_start, &g_traj_end, g_traj_duration,
                                    ARD_TIMING_100HZ_STEP_SIZE, EARD_SPLINE_QUINTIC);
        if (ret != 0) {
            Serial.printf("spline generation failed (%f to %f, dur=%f), will retry next point\n", 
            g_traj_start.position, g_traj_end.position, g_traj_duration);
            continue;
        }
        digitalWrite(2, g_traj_start.position > g_traj_end.position);
        g_current_pos = g_traj_end.position;
        g_current_ts = op.ts;

        Serial.printf("streamed_cmd_update: %f to %f, dur=%f\n", g_traj_start.position, g_traj_end.position, g_traj_duration);
        break;
    }
}

static void purge_ops_after_ts(uint32_t ts) {
    auto _p = g_timedOps_begin;
    for(; _p != g_timedOps_end; _p = NEXTOP(_p)) {
        TimedOp op = g_timedOps[_p];
        if (MOD32CMP(op.ts, ts) >= 0) {
            // set cbuf end to first op with op.ts >= arg.ts
            g_timedOps_end = _p;
            return;
        }
    }
    if (MOD32CMP(g_current_ts, ts) >=0) {
        // preempt current op
        g_preempt_current_op = true;
    }
}

void mystepper_process_timed_cmd(const void* _buf, size_t cb) {
    const size_t cOps = cb / sizeof(TimedOp);
    if (cOps * sizeof(TimedOp) != cb) {
        Serial.printf("process_timed_cmd: extra data, %u bytes not divisible ty %u\n", cb, sizeof(TimedOp));
        return;
    }
    uint32_t current_ts = (uint32_t)(esp_timer_get_time() / 1000);
    if (!cOps) {
        purge_ops_after_ts(current_ts);
        return;
    }
    uint32_t td = (uint32_t)((timedelta_hi + timedelta_lo) / 2);
    const TimedOp* bufOps = (const TimedOp*)_buf;
    if (timedelta_hi >= timedelta_lo && timedelta_hi - timedelta_lo < 100) {
        g_streaming = true;
    } else {
        // don't add commands before we have time sync
        Serial.printf("No timesync, skipping add\n");
        return;
    }    
    bool did_purge = false;
    // TODO: max cmd duration check?
    // (Seems unnecessary with working preemption)
    // copy streamed commands to ring buffer
    for (auto first_op_index = 0; first_op_index < cOps; ++first_op_index) {
        const TimedOp op = {.ts = bufOps[first_op_index].ts - td, .pos = bufOps[first_op_index].pos};
        if (MOD32CMP(op.ts, current_ts) <= 0) { // remove expired ops from stream
            Serial.printf("Dropping already expired op %u < %u\n", op.ts, current_ts);
            continue;
        }
        
        if (!did_purge) {
            did_purge = true;
            purge_ops_after_ts(op.ts);
        }

        uint16_t _end = NEXTOP(g_timedOps_end);
        if (_end == g_timedOps_begin) {
            Serial.printf("Ring buffer overflow when adding timed ops\n");
            break;
        }

        // insert into ringbuf
        Serial.printf("Add op %u @%u\n", op.pos, op.ts);

        g_timedOps[g_timedOps_end] = op;
        g_timedOps_end = _end;
    }
}

void mystepper_step_adjust(int8_t signedSteps, const AdjVar* av) {
    if (signedSteps == 0) {
        return;
    }
    uint16_t current_step = (uint16_t)(0.5 + (*av->val - av->min) / (av->max - av->min) * av->topStep);
    int32_t newStep = (int32_t)current_step + signedSteps;
    if (newStep < 0) {
        newStep = 0;
    }
    if (newStep > av->topStep) {
        newStep = av->topStep;
    }
    *av->val = av->min + (av->max - av->min) * newStep / av->topStep;
}

void mystepper_estop() {
    g_estop = true;
    uint32_t current_ts = (uint32_t)(esp_timer_get_time() / 1000);
    purge_ops_after_ts(current_ts);
}

void mystepper_reset_state() {
    g_streaming = false;
    timedelta_lo = 0;
    timedelta_hi = -1ull;
    g_timedOps_begin = g_timedOps_end = 0;
    pinMode(2, OUTPUT);
}

bool mystepper_init() {
    // stepper test parameters
    ArdStepperParameters stepper_parameters = get_stepper_parameters();
    Serial.println("stepper_config/0");

    int ret = ard_stepper_alloc(&g_stepper, &stepper_parameters);
    Serial.println("stepper_config/1");

    if (ret != 0) { 
        for(;;) {
            Serial.printf("ard_stepper_alloc error %i\n", ret);
        }
    }

    Serial.println("stepper_config");


    // Timing
    // ------

    const uint32_t now = micros();
    ard_timing_reset(ARD_TIMING_10HZ_STEP_SIZE, now, &g_timing_10hz);
    ard_timing_reset(ARD_TIMING_100HZ_STEP_SIZE, now, &g_timing_100hz);
    ard_timing_reset(ARD_TIMING_200HZ_STEP_SIZE, now, &g_timing_200hz);

    // Finish Setup
    // ------------

    // Reset stepper
    ard_stepper_reset(&g_stepper, 0, true);
    return true;
}

void mystepper_loop() {
   const uint32_t now = micros();
    // 10 Hz
    // ------
    if (ard_timing_step(&g_timing_10hz, now)) {
        ws_10hz_task();
    }

    // // 200 Hz
    // // ------
    
    if (ard_timing_step(&g_timing_100hz, now))
    {
        // spline trajectory
        // -----------------

        // make new reversed traj if end reached
        if (g_traj.index == g_traj.num_steps) {
            if (g_streaming) {
                streamed_cmd_update();
            } else {
                mystepper_motion_update();
            }
        }

        const double command = ard_spline_traj_evaluate_position(&g_traj);
        // produce test commands
        ard_stepper_produce_command(&g_stepper, &command, 1);

        // stepper
        // -------

        ard_stepper_consume_run(&g_stepper, now);
        if (g_preempt_current_op) {
            g_preempt_current_op = false;
            g_traj.index = g_traj.num_steps;
            g_current_pos = command - g_offset;
        }
    }
}