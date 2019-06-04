#ifndef ESCADA_CORE_FUNCTION_H
#define ESCADA_CORE_FUNCTION_H

#include <stdint.h>
#include "dbase.h"

uint32_t baudrate(uint32_t  baud);
bool UpdateThreads(DBase dBase, int thread_id, uint8_t type, uint8_t status);
uint8_t BCD (uint8_t dat);

#endif //ESCADA_CORE_FUNCTION_H
