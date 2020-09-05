#ifndef RX_DRIVER_H_
#define RX_DRIVER_H_

#include "raging-global.h"
#include "rnet-intfc.h"

#ifndef RX_DRIVER_GLOBAL_DEFS
    extern bool rx_handler_init_done;
#endif

void rx_handler_for_ahdlc(uint8_t *data, unsigned data_length);
void rx_handler_init(uint32_t     message_fields,
                     nufr_tid_t   dest_task,
                     rnet_intfc_t intfc);
unsigned rx_handler_queued_buf_count(void);
void rx_handler_enqueue_buf(unsigned num_bufs);

#endif // RX_DRIVER_H_