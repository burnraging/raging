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

//! @file     rnet-intfc.c
//! @authors  Bernie Woodland
//! @date     17Mar18
//!
//! @brief    Interface Definitions
//!

#include "rnet-intfc.h"
#include "rnet-app.h"
#include "rnet-dispatch.h"
#include "rnet-ip-utils.h"
#include "nsvc-api.h"

#include "raging-utils-mem.h"
#include "raging-contract.h"

static rnet_intfc_ram_t rnet_intfc[RNET_NUM_INTFC];
static rnet_subi_ram_t  rnet_subi[RNET_NUM_SUBI];
static rnet_cir_ram_t   rnet_cir[RNET_NUM_CIR];

extern const rnet_intfc_rom_t rnet_static_intfc[RNET_NUM_INTFC];
extern const rnet_subi_rom_t rnet_static_subi[RNET_NUM_SUBI];
extern const rnet_cir_rom_t rnet_static_cir[RNET_NUM_PCIR];


//!
//! @name      rnet_intfc_init
//!
//! @brief     Init RNET subsystem
//!
void rnet_intfc_init(void)
{
    unsigned                i;
    int                     rv;
    const rnet_intfc_rom_t *intfc_rom_ptr;
    rnet_intfc_ram_t       *intfc_ram_ptr;
    const rnet_subi_rom_t  *subi_rom_ptr;
    rnet_subi_ram_t        *subi_ram_ptr;
    const rnet_cir_rom_t   *cir_rom_ptr;
    rnet_cir_ram_t         *cir_ram_ptr;

    rutils_memset(&rnet_intfc, 0, sizeof(rnet_intfc));
    rutils_memset(&rnet_subi, 0, sizeof(rnet_subi));
    rutils_memset(&rnet_cir, 0, sizeof(rnet_cir));

    // Interfaces
    for (i = 0; i < RNET_NUM_INTFC; i++)
    {
        intfc_rom_ptr = &rnet_static_intfc[i];
        intfc_ram_ptr = &rnet_intfc[i];

        // Timer not previously allocated from a restart?
        if (NULL == *(intfc_rom_ptr->timer_ptr))
        {
           *(intfc_rom_ptr->timer_ptr) = nsvc_timer_alloc();
        }
        else
        {
            // In case rnet reset happened when timer was running.
           nsvc_timer_kill(*(intfc_rom_ptr->timer_ptr));
        }

        if (RNET_L2_PPP == intfc_rom_ptr->l2_type)
        {
            // clear everything (counters, etc)
            rutils_memset(intfc_ram_ptr, 0, sizeof(rnet_intfc_ram_t));

            rnet_intfc_start_or_restart_l2((rnet_intfc_t) (i +  1));
        }
    }

    // Subinterfaces
    for (i = 0; i < RNET_NUM_SUBI; i++)
    {
        subi_rom_ptr = &rnet_static_subi[i];
        subi_ram_ptr = &rnet_subi[i];

        subi_ram_ptr->prefix_length = subi_rom_ptr->prefix_length;

        if (RNET_IPACQ_HARD_CODED == subi_rom_ptr->acquisition_method)
        {
            rv = RFAIL_ERROR;

            if (rnet_ip_is_ipv6_traffic_type(subi_rom_ptr->type))
            {
                rv = rnet_ipv6_ascii_to_binary((rnet_ip_addr_union_t *)
                                                   subi_ram_ptr->ip_addr.ipv6_addr,
                                               subi_rom_ptr->ip_addr,
                                               true);
            }
            else
            {
                rv = rnet_ipv4_ascii_to_binary((rnet_ip_addr_union_t *)
                                                   subi_ram_ptr->ip_addr.ipv4_addr,
                                               subi_rom_ptr->ip_addr,
                                               true);
            }

            if (rv < 0)
            {
                // fatal error
            }
        }
    }

    // Circuits
    for (i = 0; i < RNET_NUM_PCIR; i++)
    {
        cir_ram_ptr = &rnet_cir[i];
        cir_rom_ptr = &rnet_static_cir[i];

        cir_ram_ptr->is_active = true;
        cir_ram_ptr->type = cir_rom_ptr->type;
        cir_ram_ptr->protocol = cir_rom_ptr->protocol;
        cir_ram_ptr->self_port = cir_rom_ptr->self_port;
        cir_ram_ptr->peer_port = cir_rom_ptr->peer_port;
        cir_ram_ptr->subi = cir_rom_ptr->subi;
        cir_ram_ptr->buf_listener_msg = cir_rom_ptr->buf_listener_msg;
        cir_ram_ptr->pcl_listener_msg = cir_rom_ptr->pcl_listener_msg;
        cir_ram_ptr->listener_task = cir_rom_ptr->listener_task;

        if (!rnet_ip_is_ipv6_traffic_type(cir_rom_ptr->type))
        {
            (void)rnet_ipv4_ascii_to_binary((rnet_ip_addr_union_t *)
                                                &(cir_ram_ptr->peer_ip_addr),
                                            cir_rom_ptr->peer_ip_addr,
                                            true);
        }
        else
        {
            (void)rnet_ipv6_ascii_to_binary((rnet_ip_addr_union_t *)
                                                &(cir_ram_ptr->peer_ip_addr),
                                            cir_rom_ptr->peer_ip_addr,
                                            true);
        }
    }

    rnet_send_msgs_to_event_list(RNET_NOTIF_INIT_COMPLETE, 0);
}

//!
//! @name      rnet_intfc_restart_l2
//!
//! @brief     Init RNET subsystem
//!
void rnet_intfc_start_or_restart_l2(rnet_intfc_t intfc)
{
    const rnet_intfc_rom_t *intfc_rom_ptr;

    intfc_rom_ptr = rnet_intfc_get_rom(intfc);

    if (RNET_L2_PPP == intfc_rom_ptr->l2_type)
    {
        // send message to init this interface's PPP
        // hack: compilers and lint-like tools will go ballistic over
        //       the (void *) casting here
        rnet_msg_send(RNET_ID_PPP_INIT, (void *)(intfc));
    }
}

//!
//! @name      rnet_intfc_is_valid
//!
//! @brief     Init RNET subsystem
//!
//! @param[in] 'intfc'--
//!
//! @return    'true' if 'intfc' is valid
//!
bool rnet_intfc_is_valid(rnet_intfc_t intfc)
{
    unsigned index = (unsigned)intfc;
    bool is_valid;

    is_valid = (index > RNET_INTFC_null) &&
                ((index < RNET_INTFC_max));

    return is_valid;
}

//!
//! @name      rnet_intfc_get_rom
//!
//! @brief     Retrieve read-only settings for given interface
//!
//! @param[in] 'intfc'--
//!
//! @return    ptr to settings
//!
const rnet_intfc_rom_t *rnet_intfc_get_rom(rnet_intfc_t intfc)
{
    SL_REQUIRE_API(rnet_intfc_is_valid(intfc));

    return &rnet_static_intfc[intfc - 1];
}

//!
//! @name      rnet_intfc_get_type
//!
//! @brief     Retrieve this interface's L2 type
//!
//! @param[in] 'intfc'--
//!
//! @return    L2 type
//!
rnet_l2_t rnet_intfc_get_type(rnet_intfc_t intfc)
{
    SL_REQUIRE_API(rnet_intfc_is_valid(intfc));

    return rnet_static_intfc[intfc - 1].l2_type;
}

//!
//! @name      rnet_intfc_get_options
//!
//! @brief     Retrieve this interface's option word
//!
//! @param[in] 'intfc'--
//!
//! @return    option word
//!
uint16_t rnet_intfc_get_options(rnet_intfc_t intfc)
{
    SL_REQUIRE_API(rnet_intfc_is_valid(intfc));

    return rnet_static_intfc[intfc - 1].option_flags;
}

//!
//! @name      rnet_intfc_get_timer
//!
//! @brief     Retrieve this interface's timer
//!
//! @param[in] 'intfc'--
//!
//! @return    ptr to timer
//!
nsvc_timer_t *rnet_intfc_get_timer(rnet_intfc_t intfc)
{
    SL_REQUIRE_API(rnet_intfc_is_valid(intfc));

    return *(rnet_static_intfc[intfc - 1].timer_ptr);
}

//!
//! @name      rnet_intfc_get_counters
//!
//! @brief     Retrieve this interface's counter struct
//!
//! @param[in] 'intfc'--
//! @param[out] 'counter_ptr'-- couter struct assigned to ptr passed in
//!
//! @return    size of counter struct (bytes)
//!
unsigned rnet_intfc_get_counters(rnet_intfc_t intfc, void **counter_ptr)
{
    SL_REQUIRE_API(rnet_intfc_is_valid(intfc));

    *counter_ptr = rnet_static_intfc[intfc - 1].counters;

    return rnet_static_intfc[intfc - 1].counters_size;
}

//!
//! @name      rnet_intfc_get_ram
//!
//! @brief     Retrieve this interface's read-write settings
//!
//! @param[in] 'intfc'--
//!
//! @return    ptr to read-write settings
//!
rnet_intfc_ram_t *rnet_intfc_get_ram(rnet_intfc_t intfc)
{
    SL_REQUIRE_API(rnet_intfc_is_valid(intfc));

    return &rnet_intfc[intfc - 1];
}

//!
//! @name      rnet_intfc_find_intfc_from_timer
//!
//! @brief     Given a pointer to a timer, look up which
//! @brief     interface it belongs to.
//!
//! @param[in] 'timer'--
//!
//! @return    interface timer belongs to; 'RNET_INTFC_null' if none found.
//!
rnet_intfc_t rnet_intfc_find_intfc_from_timer(nsvc_timer_t *timer)
{
    unsigned i;

    for (i = RNET_INTFC_null; i < RNET_INTFC_max; i++)
    {
        if (*(rnet_static_intfc[i].timer_ptr) == timer)
        {
            return (rnet_intfc_t)(i + 1);
        }
    }

    return RNET_INTFC_null;
}

//!
//! @name      rnet_subi_get_rom
//!
//! @brief     Retrieve this sub-interface's read-only settings
//!
//! @param[in] 'subi'--
//!
//! @return    ptr to read-only settings
//!
const rnet_subi_rom_t *rnet_subi_get_rom(rnet_subi_t subi)
{
    unsigned index = (unsigned)subi;

    SL_REQUIRE_API((index > RNET_SUBI_null) &&
                (index < RNET_SUBI_max));

    return &rnet_static_subi[index - 1];
}

//!
//! @name      rnet_subi_get_ram
//!
//! @brief     Retrieve this sub-interface's read-write settings
//!
//! @param[in] 'subi'--
//!
//! @return    ptr to read-write settings
//!
rnet_subi_ram_t *rnet_subi_get_ram(rnet_subi_t subi)
{
    unsigned index = (unsigned)subi;

    SL_REQUIRE_API((index > RNET_SUBI_null) &&
                (index < RNET_SUBI_max));

    return &rnet_subi[index - 1];
}

//!
//! @name      rnet_subi_lookup
//!
//! @brief     Find subinterface that this IP address matches
//!
//! @param[in] 'intfc'--
//! @param[in] 'ip_addr'--
//! @param[in] 'is_ipv6'--
//! @param[out] 'subi_value_ptr'--
//!
//! @return    'true' subinterface match found
//!
bool rnet_subi_lookup(rnet_intfc_t                intfc,
                      rnet_ip_addr_union_t       *ip_addr,
                      bool                        is_ipv6,
                      uint8_t                    *subi_value_ptr)
{
    const rnet_subi_rom_t *subi_rom_ptr;
    rnet_subi_ram_t       *subi_ram_ptr;
    rnet_subi_t            subi;
    unsigned               i;

    UNUSED(intfc);                         // suppress warning

    // find subinterface...
    for (i = 0; i < RNET_NUM_SUBI; i++)
    {
        subi = (rnet_subi_t)(i + 1);

        subi_rom_ptr = rnet_subi_get_rom(subi);
        subi_ram_ptr = rnet_subi_get_ram(subi);

        if ((!is_ipv6 && (RNET_TR_IPV4_UNICAST == subi_rom_ptr->type))
                       ||      is_ipv6)
        {
            if (rnet_ip_match_is_exact_match(is_ipv6,
                                             ip_addr,
                                             &subi_ram_ptr->ip_addr))
            {
                *subi_value_ptr = subi;
                return true;
            }
        }
    }

    return false;
}

//!
//! @name      rnet_subi_attempt_and_learn_address
//!
//! @brief     If subinterface has a null address, learn
//! @brief     the address passed in.
//!
//! @param[in] 'intfc'--
//! @param[in] 'ip_addr'--
//! @param[in] 'is_ipv6'--
//!
//! @return    'RNET_SUBI_null' if no address learned;
//! @return    the 'rnet_subi_t' enum if address learned
//!
rnet_subi_t rnet_subi_attempt_and_learn_address(rnet_intfc_t          intfc,
                                                rnet_ip_addr_union_t *ip_addr,
                                                bool                  is_ipv6)
{
    const rnet_subi_rom_t *subi_rom_ptr;
    rnet_subi_ram_t       *subi_ram_ptr;
    rnet_subi_t            subi;
    unsigned               i;

    // find subinterface...
    for (i = 0; i < RNET_NUM_SUBI; i++)
    {
        subi = (rnet_subi_t)(i + 1);

        subi_rom_ptr = rnet_subi_get_rom(subi);
        subi_ram_ptr = rnet_subi_get_ram(subi);

        if (intfc == subi_rom_ptr->parent)
        {
            if (!is_ipv6 && (RNET_TR_IPV4_UNICAST == subi_rom_ptr->type))
            {
                if ((RNET_IPACQ_LEARNED == subi_rom_ptr->acquisition_method)
                                   &&
                    (rnet_ip_is_null_address(is_ipv6, &subi_ram_ptr->ip_addr)))
                {
                    rutils_memcpy(subi_ram_ptr->ip_addr.ipv4_addr,
                                  ip_addr,
                                  IPV4_ADDR_SIZE);

                    return subi;
                }
            }
            else if (is_ipv6 && (RNET_TR_IPV6_GLOBAL == subi_rom_ptr->type))
            {
                if ((RNET_IPACQ_LEARNED == subi_rom_ptr->acquisition_method)
                                   &&
                    (rnet_ip_is_null_address(is_ipv6, &subi_ram_ptr->ip_addr)))
                {
                    rutils_memcpy(subi_ram_ptr->ip_addr.ipv4_addr,
                                  ip_addr,
                                  IPV4_ADDR_SIZE);

                    return subi;
                }
            }
        }
    }

    return RNET_SUBI_null;
}

//!
//! @name      rnet_circuit_get
//!
//! @brief     Retrieve this circuit's settings
//!
//! @param[in] 'index'-- circuit identifier
//!
//! @return    ptr to settings
//!
rnet_cir_ram_t *rnet_circuit_get(unsigned circuit_index)
{
    SL_REQUIRE_API(circuit_index < RNET_NUM_CIR);

    return &rnet_cir[circuit_index];
}

//!
//! @name      rnet_circuit_index_lookup
//!
//! @brief     Retrieve this circuit's settings
//!
//! @param[in] 'subi'-- subinterface that this circuit is attached to
//! @param[in] 'l4_protocol'-- UDP or TCP
//! @param[in] 'self_port'-- UDP/TCP port number on 'subi'
//! @param[in]               '0' to ignore match
//! @param[in] 'peer_port'-- UDP/TCP port number of device connected to
//! @param[in]               '0' to ignore match
//! @param[in] 'peer_ip_addr'-- IPv4/IPv6 address of device connected to
//!
//! @return    success: circuit's index (must cast to unsigned).
//! @return    fail: RFAIL_NOT_FOUND
//!
int rnet_circuit_index_lookup(rnet_subi_t           subi,
                              rnet_ip_protocol_t    l4_protocol,
                              uint16_t              self_port,
                              uint16_t              peer_port,
                              rnet_ip_addr_union_t *peer_ip_addr)
{
    rnet_cir_ram_t *cir_ptr;
    unsigned        i;
    bool            is_ipv6;
    bool            is_match;
    bool            is_null;

    UNUSED(subi);                           // suppress warning

    for (i = 0; i < RNET_NUM_CIR; i++)
    {
        cir_ptr = rnet_circuit_get(i);

        if (cir_ptr->is_active)
        {
            // Set to zero on tx
            bool match_self_port = (self_port == cir_ptr->self_port) ||
                                   (0 == self_port);
            // Set to zero on rx
            bool match_peer_port = (peer_port == cir_ptr->peer_port) ||
                                   (0 == peer_port);

            if (match_self_port && match_peer_port &&
                (l4_protocol == cir_ptr->protocol))
            {
                is_ipv6 = rnet_ip_is_ipv6_traffic_type(cir_ptr->type);

                is_match = rnet_ip_match_is_exact_match(is_ipv6,
                                             &cir_ptr->peer_ip_addr,
                                             peer_ip_addr);

                is_null = rnet_ip_is_null_address(is_ipv6,
                                                  &cir_ptr->peer_ip_addr);

                if (is_match || is_null)
                {
                    return (int)i;
                }
            }
        }
    }

    return RFAIL_NOT_FOUND;
}

//!
//! @name      rnet_circuit_add
//!
//! @brief     Add a new circuitl
//!
//! @details   fixme: put mutual exclusion on add
//!
//! @param[in] 'new_circuit'-- 
//!
//! @return    'true' if success
//!
bool rnet_circuit_add(rnet_cir_ram_t *new_circuit)
{
    rnet_cir_ram_t *cir_ptr;
    unsigned        i;

    for (i = 0; i < RNET_NUM_CIR; i++)
    {
        cir_ptr = rnet_circuit_get(i);

        if (!cir_ptr->is_active)
        {
            rutils_memcpy(cir_ptr, new_circuit, sizeof(rnet_cir_ram_t));
            cir_ptr->is_active = true;

            return true;
        }
    }

    return false;
}

//!
//! @name      rnet_circuit_delete
//!
//! @brief     Delete circuit
//!
//! @details   fixme: put mutual exclusion on add
//!
//! @param[in] 'index'-- 
//!
void rnet_circuit_delete(unsigned index)
{
    rnet_cir_ram_t *cir_ptr;

    cir_ptr = rnet_circuit_get(index);

    cir_ptr->is_active = false;
}

//!
//! @name      rnet_subi_is_ipv6
//!
//! @brief     Is this subinterface an IPv6 circuit?
//!
//! @param[in] 'subi'--
//!
//! @return    'true' if circuit is IPv6
//!
bool rnet_subi_is_ipv6(rnet_subi_t subi)
{
    const rnet_subi_rom_t *subi_rom_ptr = rnet_subi_get_rom(subi);

    return rnet_ip_is_ipv6_traffic_type(subi_rom_ptr->type);
}

//!
//! @name      rnet_subi_is_ipv4
//!
//! @brief     Is this circuit an IPv4 circuit?
//!
//! @param[in] 'circuit_index'-- circuit identifier
//!
//! @return    'true' if circuit is IPv4
//!
bool rnet_circuit_is_ipv4(unsigned circuit_index)
{
    rnet_cir_ram_t *cir_ptr;

    cir_ptr = rnet_circuit_get(circuit_index);

    // Make sure 'circuit_index' points to a circuit in use;
    // This API can be used to validate a circuit index.
    if (!cir_ptr->is_active)
    {
        return false;
    }

    return !rnet_ip_is_ipv6_traffic_type(cir_ptr->type);
}

//!
//! @name      rnet_subi_is_ipv6
//!
//! @brief     Is this circuit an IPv6 circuit?
//!
//! @param[in] 'circuit_index'-- circuit identifier
//!
//! @return    'true' if circuit is IPv6
//!
bool rnet_circuit_is_ipv6(unsigned circuit_index)
{
    rnet_cir_ram_t *cir_ptr;

    cir_ptr = rnet_circuit_get(circuit_index);

    if (!cir_ptr->is_active)
    {
        return false;
    }

    return rnet_ip_is_ipv6_traffic_type(cir_ptr->type);
}