/*
Copyright (c) 2018-2019, Bernie Woodland
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

//! @file     ssp-gateway.cpp
//! @authors  Bernie Woodland
//! @date     4Dec18
//!
//! @brief    
//!
//! @details  ./ssp-gateway -a 10.1.1.1 [-i eth0] [-p 10.1.1.2] -u 45000 -q 3
//! @details  
//! @details  -a    listening IP address, IPv4 or IPv6
//! @details  -i    If -a is an IPv6 link local address, must specify
//! @details           networking interface this IP address is found on
//! @details  -p    peer server IP address, IPv4 or IPv6
//! @details  -u    base UDP port, for both self and peer.
//! @details           app=1 == base port, app=2 == base port+1, etc
//! @details  -q    Number of UDP ports to listen on
//! @details  
//! @details  Can have multiple -a, -i, -p parameters  
//! @details  Can mix IPv4 and IPv6
//! @details  
//! @details  
//!

#if 0

#include "raging-global.h"
#include "raging-utils.h"
#include "raging-utils-mem.h"
#include "rnet-ip-utils.h"

#include <unistd.h>
#include <cstdlib>       // 'std::exit()'
//#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>
#include <list>

#define  NO_INDEX                      (-1)
#define  MAX_UDP_LISTENING_PORTS        10

struct gw_address_parm_t
{
    std::string          addr_text;
    rnet_ip_addr_union_t addr;
    bool                 is_ipv6;
    bool                 is_link_local;
??    std::string          interface_name;
???    bool                 has_interface_name;
};

struct gw_listen_t
{
    rnet_ip_addr_union_t addr;
    bool                 is_ipv6;
    std::string          interface_name;
    uint16_t             udp_port;
    int                  peer_index;
    ??? socket ???
};

struct gw_remote_peer_t
{
    rnet_ip_addr_union_t addr;
    bool                 is_ipv6;
    uint16_t             udp_port;
    int                  listener_index;
};

struct gw_packet_t
{    
    rnet_ip_addr_union_t addr;
    bool                 is_ipv6;
    uint16_t             src_udp_port;
    uint16_t             dest_udp_port;
    std::vector<uint8_t> payload;
    ??? time born ????
};

// CLI selected parameters
std::vector<gw_address_parm_t> gw_listener_parms;
std::vector<gw_address_parm_t> gw_peer_parms;
std::string                    interface_parms;
std::string                    udp_port_base_text;
std::string                    quantity_of_udp_port_text;

// Operational values
std::vector<gw_listen_t>      gw_listeners;
std::vector<gw_remote_peer_t> remote_peers;
uint16_t                      udp_port_base;
unsigned                      quantity_of_udp_ports;
std::list<gw_packet_t>        packet_list;


void process_cli_parameters(void)
{
    uint32_t    x32 = 0;
??    bool        is_ipv6 = false;
    bool        fail_flag = false;

    // *** Quantity of UDP ports
    quantity_of_udp_port_text.push_back('\0');    // make C compatible string
    (void)rutils_decimal_ascii_to_unsigned32(quantity_of_udp_port_text.c_str(), &x32, &fail_flag);
    if (fail_flag)
    {
        printf("Illegal quantity of UDP listening port value (-q) <%s>\n", quantity_of_udp_port_text.c_str());
        std::exit(0);
    }
    else if (x32 > MAX_UDP_LISTENING_PORTS)
    {
        printf("Max number of UDP listening ports exceeded <%u>\n", x32);
        std::exit(1);
    }
    quantity_of_udp_port = (uint16_t)x32;

    // *** UDP base port
    udp_port_base_text.push_back('\0');    // make C compatible string
    (void)rutils_decimal_ascii_to_unsigned32(udp_port_base_text.c_str(), &x32, &fail_flag);
    if (fail_flag)
    {
        printf("Illegal UDP listening port value <%s>\n", udp_port_base_text.c_str());
        std::exit(0);
    }
    else if (quantity_of_udp_port + x32 > BIT_MASK16)
    {
        printf("Illegal UDP base port (-u) value <%u>\n", x32);
        std::exit(1);
    }
    udp_port_base = (uint16_t)x32;

    // *** Listeners: Make sure there's at least one
    if (gw_listener_parms.size() == 0)
    {
        printf("Must have at least one -a parameter\n");
        std::exit(1);
    }

    // *** Listeners: build list
    for (unsigned i = 0; i < gw_listener_parms.size(); i++)
    {
        gw_address_parm_t &listener = gw_listener_parms.at(i);

        listener.ip_address.push_back('\0');   // C string-ize
        listener.is_ipv6 = false;
        listener.has_interface_name = false;
        listener.peer_index = NO_INDEX;

        if (rnet_ipv4_ascii_to_binary(&listener.addr,
                                      listener.addr_text.c_str(),
                                      true) < 0)
        {
            if (rnet_ipv6_ascii_to_binary(&listener.addr,
                                          listener.addr_text.c_str(),
                                          true) < 0)
            {
                printf("Unrecognizable IP address <%s>\n", listener.addr_text.c_str());
                std::exit(1);
            }

            listener.is_ipv6 = true;
        }

        listener.is_link_local = listener.is_ipv6 && rnet_is_link_local_address(&listener.addr);

        gw_listeners.push_back(listener);

        // Check for duplicate IP addresses
        for (unsigned k = 0; k < i; k++)
        {
            unsigned cmp_size;

            if (listener.is_ipv6 && !listener.is_link_local) 
            {
                cmp_size = IPV6_ADDR_SIZE;
            }
            else if (listener.is_ipv4)
            {
                cmp_size = IPV4_ADDR_SIZE;
            }
            else
            {
                cmp_size = 0;
            }

            gw_listen_t &previous_listener = gw_listeners.at(k);

            if (rutils_memcmp(&listener.addr, &previous_listener.addr, cmp_size)
                    == RFAIL_NOT_FOUND)
            {
                printf("Duplicate IP listening addr <%s>\n", listener.addr_text.c_str());
                std::exit(1);
            }
        }
    }

    // ** Ensure no duplicate interfaces
    for (unsigned i = 0; i < interface_parms.size(); i++)
    {
        for (unsigned k = i + 1; k < interface_parms.size(); k++)
        {
            if (interface_parms.at(i) == interface_parms.at(k))
            {
                printf("Duplicate interface <%s>\n", interface_parms.at(i).c_str());
                std::exit(1);
            }
        }
    }

    unsigned int_index = 0;

    // ** Assign interfaces to listeners
    for (unsigned i = 0; i < gw_listeners.size(); i++)
    {
        gw_listen_t &listener = gw_listeners.at(i);

        if (listener.is_link_local)
        {
            if (int_index == interface_parms.size())
            {
                printf("Not enough -i options\n");
                std::exit(1);
            }

            listener.interface_name = interface_parms.at(int_index);
            int_index++;
        }
    }

    if (int_index != interface_parms.size())
    {
        printf("Too many -i options\n");
        std::exit(1);
    }

    // *** Peer/Server IP addresses
    for (unsigned i = 0; i < gw_peer_parms.size(); i++)
    {
        gw_address_parm_t &peer = gw_peer_parms.at(i);

        peer.ip_address.push_back('\0');   // C string-ize
        peer.is_ipv6 = false;
        peer.listener_index = NO_INDEX;
        if (peer.has_interface_name)
        {
            peer.interface_name.push_back('\0');   // C string-ize
        }

        if (rnet_ipv4_ascii_to_binary(&peer.addr,
                                      peer.addr_text.c_str(),
                                      true) < 0)
        {
            if (rnet_ipv6_ascii_to_binary(&peer.addr,
                                          peer.addr_text.c_str(),
                                          true) < 0)
            {
                printf("Unrecognizable IP address <%s>\n", peer.addr_text.c_str());
                std::exit(1);
            }

            peer.is_ipv6 = true;
        }

        peer.is_link_local = peer.is_ipv6 && rnet_is_link_local_address(&peer.addr);

        if (peer.is_link_local && !peer.has_interface_name)
        {
            printf("Must specify interface with IP address <%s>\n", peer.addr_text.c_str());
            std::exit(1);
        }

        // Check for duplicate IP addresses
        for (unsigned k = 0; k < i; k++)
        {
            unsigned cmp_size;

            if (peer.is_ipv6 && !peer.is_link_local) 
            {
                cmp_size = IPV6_ADDR_SIZE;
            }
            else if (listener.is_ipv4)
            {
                cmp_size = IPV4_ADDR_SIZE;
            }
            else
            {
                cmp_size = 0;
            }

            gw_address_parm_t previous_peer = gw_peer_parm.at(k);

            if (rutils_memcmp(&peer.addr, &previous_peer.addr, cmp_size)
                    == RFAIL_NOT_FOUND)
            {
                printf("Duplicate IP listening addr <%s>\n", peer.addr_text.c_str());
                std::exit(1);
            }
        }
    }

    // *** bind peers to listeners
    for (unsigned i = 0; i < gw_peer_parms.size(); i++)
    {
        gw_address_parm_t &peer = gw_peer_parms.at(i);

        if (peer.is_link_local)
        {
            bool found = false;

            // Walk listeners until you find the next unmatched
            // link local listener and bind it to this peer address
            for (unsigned k = 0; k < gw_listeners.size(); k++)
            {
                gw_listen_t &listener = gw_listeners.at(k);

                if (listener.is_link_local && (listener.peer_index != NO_INDEX))
                {
                    listener.peer_index = i;
                    peer.listener_index = k;
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                printf("Can't find listener for peer <%s>\n", addr_text.c_str());
                std::exit(1);
            }
        }
    }


}


int main(int argc, char **argv)
{
    int         result;
    int         option;
    bool        has_udp_port_base_text = false;
    bool        has_quantity_text = false;

    // Semicolon means preceding opt takes parm

    while ((option = getopt(argc, argv, "a:i:p:u:q:h")) != -1)
    {
        switch (option)
        {
        case 'a':
            {
                gw_address_parm_t listener;
                listener.addr_text.assign(optarg);

#if 0
                if (interface_specified)
                {
                    listener.interface_name.assign(interface_name);
                    listener.has_interface_name = true;
                    interface_specified = false;
                }
                else
                {
                    listener.interface_name.clear();
                    listener.has_interface_name = false;
                }
#endif
                gw_listener_parms.push_back(listener);
            }
            break;

        case 'p':
            {
                gw_address_parm_t peer;
                peer.addr_text.assign(optarg);

                gw_peer_parms.push_back(peer);
            }
            break;


        case 'i':
            {
                if (rutils_strlen(optarg) == 0)
                {
                    printf("Missing parameter after -i\n");
                    std::exit(1);
                }

                std::string this_interface(opt_arg);
                this_interface.append('\0');          // Make C-compatible

                interface_parms.push_back(this_interface);
            }

            break;

        case 'u':

            if (has_udp_port_base_text)
            {
                printf("Multiple -u options\n");
                std::exit(1);
            }
            has_udp_port_base_text = true;
            udp_port_base_text.assign(optarg);

            break;

        case 'q':
            if (has_quantity_text)
            {
                printf("Multiple -q options\n");
                std::exit(1);
            }
            has_quantity_text = true;
            quantity_of_udp_port_text.assign(optarg);

            break;

        case 'h':
            printf("./ssp-gateway -a 10.1.1.1 [-i eth0] [-p 10.1.1.2] -u 45000 -q 3\n");
            std::exit(0);
            break;

        default:
            printf("Unknown option %c\n", option);
            std::exit(1);
            break;
        }
    }

    if (!has_quantity_text)
    {
        printf("Missing -q option\n");
        std::exit(1);
    }
    if (!has_udp_port_base_text)
    {
        printf("Missing -u option\n");
        std::exit(1);
    }

    process_cli_parameters();


    return 0;
}
#else
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return 0;
}
#endif