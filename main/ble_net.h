#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int ble_net_init(void);
void ble_send_data(uint16_t conn_handle, uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

