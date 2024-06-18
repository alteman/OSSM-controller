#pragma once

#include <stdint.h>
#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif

void process_remote_cmd(const void* _buf, size_t cb);

#ifdef __cplusplus
}
#endif