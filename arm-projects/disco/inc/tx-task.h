#ifndef TX_TASK_H_
#define TX_TASK_H_

#include "raging-global.h"
#include "rnet-buf.h"

// for RNET
typedef enum
{
   ID_TX_PACKET_SEND,
   ID_TX_RESTART_TRANSMIT,
} id_tx_t;

// for SSP
typedef enum
{
   ID_TX_SSP_PACKET_SEND,
   ID_TX_SSP_BUFFER_DISCARD,
} id_tx_ssp_t;

void tx_send_packet(rnet_intfc_t intfc, rnet_buf_t *buf, bool is_pcl);

#endif  // TX_TASK_H_