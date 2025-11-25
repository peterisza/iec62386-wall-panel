#ifndef DALI_PHY_MAC_H
#define DALI_PHY_MAC_H

#include <stdint.h>

typedef void (*dali_frame_callback_t)(uint32_t frame_data, uint8_t bit_length);

void dali_phy_init(dali_frame_callback_t cb);
void dali_phy_set_callback(dali_frame_callback_t cb);
void dali_phy_enable(void);
void dali_phy_disable(void);
uint8_t dali_phy_get_last_bits(void);
uint32_t dali_phy_get_last_frame(void);

#endif /* DALI_PHY_MAC_H */
