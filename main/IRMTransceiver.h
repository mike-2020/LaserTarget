#pragma once
#include "driver/rmt_tx.h"

class IRMTransceiver{
private:
    rmt_channel_handle_t m_TxChannel;
    rmt_encoder_handle_t m_NecEncoder;
    int prepare();
public:
    int init();
    int send(uint8_t addr, uint8_t cmd);
};

