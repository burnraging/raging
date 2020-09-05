
#include "raging-global.h"
#include "nufr-platform.h"
#include "rnet-buf.h"
#include "rnet-ahdlc.h"
#include "rnet-dispatch.h"
#include "nsvc-api.h"

#define RX_DRIVER_GLOBAL_DEFS

#include "../../arm-projects/disco/inc/rx-driver.h"

#define MIN_FRAME_LENGTH       6      // somewhat arbitrary

#define BUF_START_OFFSET       0      // offset to use for a new buffer


// Number of contiguous chars received between
// 0x7E/frame delimiters
unsigned consecutive_count;

// Info for the buffer for the current rx frame
rnet_buf_t *current_rx_buf;
bool        has_the_buf;
uint8_t    *current_buf_ptr;
unsigned    cannot_exceed_count;

// The pre-queued RNET buffer and NUFR message blocks.
// Pre-queued by task so IRQ handler won't have to suffer
// latency hit for allocations at interrupt level.
// Task needs to maintain spares
rnet_buf_t *rx_queue_head;
rnet_buf_t *rx_queue_tail;

// Pre-formed message info
uint32_t    irq_message_fields;
nufr_tid_t  irq_message_dest_task;

// RNET info
rnet_intfc_t rx_driver_intfc;
bool         rx_handler_init_done;


// Called by UART IRQ handler on rx of 1+ bytes
void rx_handler_for_ahdlc(uint8_t *data, unsigned data_length)
{
    unsigned  i;
    uint8_t   character;

    for (i = 0; i < data_length; i++)
    {
        character = *data++;

        // Is a frame delimiter
        if (RNET_AHDLC_FLAG_SEQUENCE == character)
        {
            // Is this frame big enough to be considered legitimate?
            if (consecutive_count > MIN_FRAME_LENGTH)
            {
                // Sanity check that we've been using a buffer
                // and a message block
                if (has_the_buf)
                {
                    // Finalize buffer header fields
                    current_rx_buf->header.intfc = rx_driver_intfc;
                    current_rx_buf->header.length = consecutive_count;

                    (void)nufr_msg_send(irq_message_fields,
                                        (uint32_t)current_rx_buf,
                                        irq_message_dest_task);

                    // Reset message block and RNET buffer.
                    // Clearing variables notifies subsequent
                    //   rx bytes to reload new block and buffer.
                    current_rx_buf = NULL;
                    has_the_buf = false;
                    current_buf_ptr = NULL;
                }
            }
            // Runt frame.
            // Clear the current buffer and continue to use it.
            else
            {
                if (has_the_buf)
                {
                    current_rx_buf->header.length = 0;

                    cannot_exceed_count = RNET_BUF_SIZE - 
                                           (current_rx_buf->header.offset +
                                              current_rx_buf->header.length);

                    current_buf_ptr = RNET_BUF_FRAME_START_PTR(current_rx_buf);
                }
            }

            // frame delimiter resets 'consecutive_count'
            consecutive_count = 0;
        }

        // Not a frame delimiter character
        else
        {
            // Do we need to allocate a buffer and/or block?
            if (!has_the_buf)
            {
                // Is there a spare buffer on the queue? Dequeue it
                if ((NULL == current_rx_buf) && (NULL != rx_queue_head))
                {
                    current_rx_buf = rx_queue_head;
                    rx_queue_head = rx_queue_head->flink;
                    if (NULL == rx_queue_head)
                    {
                        rx_queue_tail = NULL;
                    }
                    current_rx_buf->flink = NULL;

                    // deleteme:
                    // assume task pre-sets these fields
                    //current_rx_buf->header.offset = BUF_START_OFFSET;
                    //current_rx_buf->length = 0;
                    //current_rx

                    cannot_exceed_count = RNET_BUF_SIZE - 
                                  (current_rx_buf->header.offset +
                                   current_rx_buf->header.length);

                    current_buf_ptr = RNET_BUF_FRAME_START_PTR(current_rx_buf);
                }

                has_the_buf = (current_rx_buf != NULL);
            }

            // Do sanity checks to ensure that we have a buffer and
            // that we're not running over the end-of-buffer,
            // then store character to buffer.
            if ((cannot_exceed_count > 0) && has_the_buf)
            {
                *current_buf_ptr++ = character;

                consecutive_count++;
                cannot_exceed_count--;
            }
        }
    }
}

void rx_handler_init(uint32_t     message_fields,
                     nufr_tid_t   dest_task,
                     rnet_intfc_t intfc)
{
    irq_message_fields = message_fields;
    irq_message_dest_task = dest_task;

    rx_driver_intfc = intfc;

    rx_handler_init_done = true;
}

// Count number of queued buffers
unsigned rx_handler_queued_buf_count(void)
{
    rnet_buf_t       *buf;
    nufr_sr_reg_t     saved_psr;
    unsigned          count = 0;

    buf = rx_queue_head;

    saved_psr = NUFR_LOCK_INTERRUPTS();

    while (NULL != buf)
    {
        buf = buf->flink;
        count++;
    }

    NUFR_UNLOCK_INTERRUPTS(saved_psr);

    return count;
}

// Same as above, but for RNET buffers
void rx_handler_enqueue_buf(unsigned num_bufs)
{
    rnet_buf_t       *new_buf;
    nufr_sr_reg_t     saved_psr;
    unsigned          i;

    for (i = 0; i < num_bufs; i++)
    {
        new_buf = rnet_alloc_bufW();

        // Reserve 1 byte for when a config request gets turned
        // around and sent back out as an ack. Same buffer is
        // used. Frame will add flag char, but rx driver pushes
        // no frame char to begin with
        new_buf->header.offset = AHDLC_FLAG_CHAR_SIZE;

        saved_psr = NUFR_LOCK_INTERRUPTS();

        if (NULL == rx_queue_head)
        {
            rx_queue_head = new_buf;
            rx_queue_tail = new_buf;
        }
        else
        {
            rx_queue_tail->flink = new_buf;
            rx_queue_tail = new_buf;
        }

        NUFR_UNLOCK_INTERRUPTS(saved_psr);
    }
}