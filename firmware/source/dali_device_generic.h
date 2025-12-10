#ifndef DALI_DEVICE_GENERIC_H
#define DALI_DEVICE_GENERIC_H

#include <stdint.h>
#include <stdbool.h>
#include "dali_phy_mac.h"

void process_dali_frame(uint32_t frame, uint8_t frame_length, bool is_valid, bool received_twice);

#endif