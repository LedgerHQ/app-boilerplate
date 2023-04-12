#pragma once

#include <stdint.h>

#include "os.h"

void cdc_mgmt_event_cb(uint32_t event, uint32_t param);
void cdc_mgmt_controller_packet_cb(uint8_t  *packet,
                                   uint16_t length);

int cdc_mgmt_process_req(uint8_t *buffer, uint16_t length);

void cdc_mgmt_init(void);

void cdc_mgmt_tick(void);