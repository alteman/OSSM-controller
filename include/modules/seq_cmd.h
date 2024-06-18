#pragma once

#include<stdint.h>
#include<stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void process_seq_cmd(const uint8_t* data, size_t cb);

#ifdef __cplusplus
}
#endif