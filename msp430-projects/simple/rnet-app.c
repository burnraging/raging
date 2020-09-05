/*
Copyright (c) 2018, Bernie Woodland
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

//! @file     rnet-app.c
//! @authors  Bernie Woodland
//! @date     10Mar18
//!
//! @brief   Application settings for Raging Networking (RNET)
//!

#include "rnet-intfc.h"
#include "tx-task.h"

// Per-interface counter definitions
rnet_ppp_counters_t rnet_counters_usb_serial1;
nsvc_timer_t        rnet_timer_usb_serial1;


//!
//! @name      rnet_event_lists
//!
//! @brief     Event notification lists
//!
//! @details   WHEN ADDING OR DELETING MEMBERS FROM THIS LIST,
//! @details   YOU *MUST* CHANGE THE LIST SIZES IN rnet-app.h TO MATCH!
//! @details   LIST SIZES:
//! @details       RNET_EVENT_LIST_SIZE_INIT_COMPLETE
//! @details       RNET_EVENT_LIST_SIZE_INTFC_UP
//! @details       RNET_EVENT_LIST_SIZE_INTFC_DOWN
//!
const rnet_notif_list_t rnet_event_list_init_complete[RNET_EVENT_LIST_SIZE_INIT_COMPLETE] =
{
    {RNET_LISTENER_MSG_DISABLED, NUFR_TID_null},
};

const rnet_notif_list_t rnet_event_list_intfc_up[RNET_EVENT_LIST_SIZE_INTFC_UP] =
{
    {RNET_LISTENER_MSG_DISABLED, NUFR_TID_null},
};

const rnet_notif_list_t rnet_event_list_intfc_down[RNET_EVENT_LIST_SIZE_INTFC_DOWN] =
{
    {RNET_LISTENER_MSG_DISABLED, NUFR_TID_null},
};

//!
//! @name      rnet_static_intfc
//!
//! @brief     Interface descriptors
//!
const rnet_intfc_rom_t rnet_static_intfc[RNET_NUM_INTFC] =  {
           // RNET_INTFC_USB_SERIAL1
    {RNET_L2_PPP, RNET_SUBI_USB_SERIAL1_LL, RNET_SUBI_USB_SERIAL1_GLOBAL, RNET_SUBI_null,
         &rnet_timer_usb_serial1, &rnet_counters_usb_serial1, sizeof(rnet_counters_usb_serial1),
         tx_send_packet,                                          // packet driver callback
                              RNET_IOPT_PPP_IPV6CP},              // ...options
};

//!
//! @name      rnet_static_subi
//!
//! @brief     Sub-Interface descriptors
//!
//! @details   If IP address is null, it will learn IP address from
//! @details   first peer packet received.
//!
const rnet_subi_rom_t rnet_static_subi[RNET_NUM_SUBI] = {
    {RNET_TR_IPV6_LINK_LOCAL, RNET_IPACQ_HARD_CODED, RNET_INTFC_USB_SERIAL1, 64, "FE80::2"},   // RNET_SUBI_USB_SERIAL1_LL
    {RNET_TR_IPV6_GLOBAL,     RNET_IPACQ_HARD_CODED, RNET_INTFC_USB_SERIAL1, 64, "2000::2"},   // RNET_SUBI_USB_SERIAL1_GLOBAL
};

//!
//! @name      rnet_static_cir
//!
//! @brief     Hard-coded circuits
//!
//! @details   If circuit has a null IP address, then it is a wildcard match
//! @details   on peer source IP address for rx packets.
//!
const rnet_cir_rom_t rnet_static_cir[RNET_NUM_PCIR] = {
    {RNET_TR_IPV6_GLOBAL, RNET_IP_PROTOCOL_UDP, 45000, 45000, RNET_SUBI_USB_SERIAL1_LL, "FE80::1",    // RNET_PCIR_USB_SERIAL1_LL
           // message fields TBD!
        RNET_LISTENER_MSG_DISABLED,
        //NUFR_SET_MSG_FIELDS(0, 0, 0, NUFR_MSG_PRI_MID),   // RNET buffer listener message
        RNET_LISTENER_MSG_DISABLED,
        NUFR_TID_null                                     // listener task
    },
    {RNET_TR_IPV6_GLOBAL, RNET_IP_PROTOCOL_UDP, 45000, 45000, RNET_SUBI_USB_SERIAL1_GLOBAL, "2000::1",    // RNET_PCIR_USB_SERIAL1_GLOBAL
        RNET_LISTENER_MSG_DISABLED,
        RNET_LISTENER_MSG_DISABLED,
        NUFR_TID_null                                     // listener task
    },
};