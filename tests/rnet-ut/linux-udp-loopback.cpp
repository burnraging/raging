//!
//! @file    linux-udp-loopback.cpp
//!
//! @auther  Bernie Woodland
//! @date    3 Jul 2018
//!
//! @brief   Test program to inject UDP packets from a linux desktop
//! @brief   over a PPP link to a target device which is configured
//! @brief   in loopback mode.
//!
//! @details To build:
//! @details     g++ -std=gnu++11 linux-udp-loopback.cpp -o udp-loopback
//! @details 
//! @details To configure RNET for loopback:
//! @details  - In RNET, create an interface which runs PPP.
//! @details  - Make sure a driver is connected between
//! @details    one of the device's serial ports and the RNET
//! @details    stack.
//! @details  - Assume that we're running IPv6 in a link-local connection.
//! @details     o The linux host will have an IP address of FE80::1
//! @details     o The device will have an IP address of FE80::2
//! @details     o UDP port number of 45000 will be used (chosen at random)
//! @details  - Configure the connecting interface in the device, and
//! @details    configure the RNET stack to run IPv6 with the needed settings.
//! @details    These setting will be rnet-app.c/.h 
//! @details       o Create an interface for the serial connection 
//! @details       o Configure that interface to run PPP (RNET_L2_PPP)
//! @details       o Configure that interface's PPP to IPv6 (RNET_IOPT_PPP_IPV6CP)
//! @details       o Hook up the packet driver callback 
//! @details       o Create an IPv6 link-local subinterface on that interface (RNET_TR_IPV6_LINK_LOCAL) 
//! @details       o Create a hard-coded circuit for the host connection 
//! @details  - Configure the RNET stack for loopback mode.
//! @details    In rnet-compile-switches.h set this flag:
//! @details         RNET_SERVER_MODE_LOOPBACK     1
//! @details  - Run a USB-serial cable between the device and the
//! @details    linux host.
//! @details  - Assume that when the serial cable is plugged into
//! @details    the linux host, the new interface "/dev/ttyUSB0"
//! @details    is created.
//! @details  - Power up the device.
//! @details  - Launch pppd on the linux host.
//! @details       sudo pppd -detach lcp-echo-interval 0 debug noauth nopcomp noaccomp nocrtscts noip ipv6 ::1,::2 /dev/ttyUSB1 115200
//! @details    ...this will create the networking interface "ppp0"
//! @details    pppd will log the netotiation session.
//! @details  - With PPP up, verify the connection before using it
//! @details        o Verify that 'ppp0' exists with 'ifconfig'
//! @details        o Ping the device:
//! @details            ping6 -I ppp0 FE80::2
//! @details  - Run this app. Here is an example which uses the above config.
//! @details    This sends 1 packet of a 100 byte payload:
//! @details       ./udp-loopback -i ppp0 -d 'FE80::2' -s 'FE80::1' -p 45000 -n 1 -l 100

#include <cstdint>
#include <string>
#include <string.h>
#include <vector>
#include <stdio.h>

// Linux socket and networking headers
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <net/if.h>

#include <unistd.h>

#define VERSION   "1.0"
#define ENABLE_DEBUG_PRINTS            1

#define MTU       1000

std::string          g_self_intfc_name;
std::string          g_device_ip_addr;
std::string          g_self_ip_addr;
uint16_t             g_udp_port_number;
unsigned             g_num_packets;
unsigned             g_packet_length;
bool                 g_using_ipv6;

std::vector<uint8_t> g_packet_data;
uint8_t              g_rx_buffer[MTU];

void print_help_screen(const char *app_name);


// Socket handle is return value. Handle must be >= 0 if successful.
// Returns < 0 if error.
int open_socket(uint16_t    my_port,
                const char *interface_name,
                const char *self_ip_address,
                bool        is_ipv6)
{
    struct sockaddr_in  sock_addr_ipv4;
    struct sockaddr_in6 sock_addr_ipv6;
    int    socket_handle;
    int    bind_result;

#if ENABLE_DEBUG_PRINTS == 1
    printf("%s: interface <%s>, self IP address <%s>, is_ipv6<%u>\n",
           __FUNCTION__, interface_name, self_ip_address, is_ipv6);
#endif

    if (!is_ipv6)
    {
        socket_handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    }
    else
    {
        socket_handle = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    }


    if (socket_handle != -1)
    {
        if (!is_ipv6)
        {
            memset((char *) &sock_addr_ipv4, 0, sizeof(sock_addr_ipv4));
            sock_addr_ipv4.sin_family = AF_INET;
            sock_addr_ipv4.sin_port = htons(my_port);    //tried zero and it didn't work

            // Interface not needed for IPv4
            //sock_addr_ipv4.sin_scope_id = if_nametoindex(interface_name); 
            inet_pton(AF_INET, self_ip_address, &(sock_addr_ipv4.sin_addr));    // text to binary

            bind_result = bind(socket_handle, (struct sockaddr*)&sock_addr_ipv4, sizeof(sock_addr_ipv4));
            if (bind_result != -1)
            {
            #if ENABLE_DEBUG_PRINTS == 1
                printf("%s: New socket handle <%d>\n", __FUNCTION__, socket_handle);
            #endif    
                return socket_handle;
            }
            else
            {
                char addr_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(sock_addr_ipv4.sin_addr), addr_str, INET_ADDRSTRLEN);   // converts binary to text
                printf("%s: bind failed\n", __FUNCTION__);
    
                close(socket_handle);
            }
        }
        else
        {
            memset((char *) &sock_addr_ipv6, 0, sizeof(sock_addr_ipv6));
            sock_addr_ipv6.sin6_family = AF_INET6;
            sock_addr_ipv6.sin6_port = htons(my_port);            //tried zero and it didn't work
    
            sock_addr_ipv6.sin6_scope_id = if_nametoindex(interface_name); 
            inet_pton(AF_INET6, self_ip_address, &(sock_addr_ipv6.sin6_addr));    // text to binary
    
            if (sock_addr_ipv6.sin6_scope_id == 0)
            {
                printf("%s: if_nametoindex() failed\n", __FUNCTION__);
                return -1;
            }
    
            bind_result = bind(socket_handle, (struct sockaddr*)&sock_addr_ipv6, sizeof(sock_addr_ipv6));
            if (bind_result != -1)
            {
            #if ENABLE_DEBUG_PRINTS == 1
                printf("%s: New socket handle <%d>\n", __FUNCTION__, socket_handle);
            #endif
    
                return socket_handle;
            }
            else
            {
                char addr_str[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &(sock_addr_ipv6.sin6_addr), addr_str, INET6_ADDRSTRLEN);  //binary to text
                printf("%s: bind failed\n", __FUNCTION__);
    
                close(socket_handle);
            }
        }
    }

    printf("%s: -1 on socket_handle\n", __FUNCTION__);

    return -1;
}

// Send 'buffer' of length 'length' out socket 'socket_handle'
void packet_send(int         socket_handle,
                 uint8_t    *buffer,
                 int         length,
                 bool        is_ipv6,
                 uint16_t    to_port,
                 const char *to_ip_address,
                 const char *interface_name)
{
    struct sockaddr_in  device_addr_ipv4;
    struct sockaddr_in6 device_addr_ipv6;
    int                 send_status;

    if (!is_ipv6)
    {
        memset((char *) &device_addr_ipv4, 0, sizeof(device_addr_ipv4));
        device_addr_ipv4.sin_family = AF_INET;
        device_addr_ipv4.sin_port = htons(to_port);
        //device_addr_ipv4.sin_scope_id = if_nametoindex(interface_name);     // not needed for IPv4
        inet_pton(AF_INET, to_ip_address, &(device_addr_ipv4.sin_addr));   //converts text to binary
    
        send_status = sendto(socket_handle, buffer, length, 0, (struct sockaddr *)&device_addr_ipv4, sizeof(device_addr_ipv4));
    }
    else
    {
        memset((char *) &device_addr_ipv6, 0, sizeof(device_addr_ipv6));
        device_addr_ipv6.sin6_family = AF_INET6;
        device_addr_ipv6.sin6_port = htons(to_port);
        device_addr_ipv6.sin6_scope_id = if_nametoindex(interface_name); 
        inet_pton(AF_INET6, to_ip_address, &(device_addr_ipv6.sin6_addr));    //text to binary
    
        send_status = sendto(socket_handle, buffer, length, 0, (struct sockaddr *)&device_addr_ipv6, sizeof(device_addr_ipv6));
    }

    if (send_status > 0)
    {
    #if ENABLE_DEBUG_PRINTS == 1
        printf("%s: Sent %d bytes\n", __FUNCTION__, send_status);
    #endif
    }
}

// Check if 'socket_handle' has a UDP datagram in it, fetch it,
//   copying it to 'buffer'.
// Return fetched buffer's length if found one; returns -1 if no rx packet
int packet_poll_receive(int         socket_handle,
                        uint8_t    *buffer,
                        int         maxBuffer,
                        bool        is_ipv6,
                        uint8_t    *src_ip_address,
                        uint16_t   *src_port,
                        unsigned   *intfc)
{
    struct sockaddr_in  device_addr_ipv4;
    struct sockaddr_in6 device_addr_ipv6;
    socklen_t           addr_len;
    int                 rx_length;

    if (!is_ipv6)
    {
        memset((char *) &device_addr_ipv4, 0, sizeof(device_addr_ipv4));
        device_addr_ipv4.sin_family = AF_INET;
        device_addr_ipv4.sin_port = htons(g_udp_port_number);
        //device_addr_ipv4.sin6_scope_id = if_nametoindex(g_self_intfc_name.c_str());
    
        addr_len = sizeof(device_addr_ipv4);
    
        rx_length = recvfrom(socket_handle, buffer, maxBuffer, MSG_DONTWAIT,
                            (struct sockaddr *)&device_addr_ipv4, &addr_len);

        if (NULL != src_ip_address)
        {
            memcpy(src_ip_address, &device_addr_ipv4.sin_addr, sizeof(device_addr_ipv4.sin_addr));
        }
        if (NULL != src_port)
        {
            *src_port = htons(device_addr_ipv4.sin_port);
        }
        if (NULL != intfc)
        {
            *intfc = 0;
        }
    }
    else
    {
        memset((char *) &device_addr_ipv6, 0, sizeof(device_addr_ipv6));
        device_addr_ipv6.sin6_family = AF_INET6;
        device_addr_ipv6.sin6_port = htons(g_udp_port_number);
        device_addr_ipv6.sin6_scope_id = if_nametoindex(g_self_intfc_name.c_str());
    
        addr_len = sizeof(device_addr_ipv6);
    
        rx_length = recvfrom(socket_handle, buffer, maxBuffer, MSG_DONTWAIT,
                            (struct sockaddr *)&device_addr_ipv6, &addr_len);

        if (NULL != src_ip_address)
        {
            memcpy(src_ip_address, &device_addr_ipv6.sin6_addr, sizeof(device_addr_ipv6.sin6_addr));
        }
        if (NULL != src_port)
        {
            *src_port = htons(device_addr_ipv6.sin6_port);
        }

        // In case socket bound to multiple interfaces
        if (NULL != intfc)
        {
            *intfc = device_addr_ipv6.sin6_scope_id;
        }
    }

    if (rx_length > 0)
    {
    #if ENABLE_DEBUG_PRINTS == 1
        printf("%s: recvfrom() fetched %d bytes\n", __FUNCTION__, rx_length);
    #endif
    }

    return rx_length;
}

// Work routine.
// It sends a packet at a time, waiting for a response.
// When the packet is received, its payload is verified against the test
//   pattern which was sent.
// Failures printed. Ever so many successful trials printer (every 100)
void engine(void)
{
    int      socket_handle;
    unsigned i;
    int      rx_data_length;
    bool     same;

    socket_handle = open_socket(g_udp_port_number,
                                g_self_intfc_name.c_str(),
                                g_self_ip_addr.c_str(),
                                g_using_ipv6);

    if (socket_handle < 0)
    {
        return;
    }

    // Load packet data
    for (i = 0; i < g_packet_length; i++)
    {
        g_packet_data.push_back((uint8_t)(i % 256));
    }

    // Send and receive packets
    for (i = 0; i < g_num_packets; i++)
    {

        packet_send(socket_handle,
                    g_packet_data.data(),
                    g_packet_data.size(),
                    g_using_ipv6,
                    g_udp_port_number,
                    g_device_ip_addr.c_str(),
                    g_self_intfc_name.c_str());

        memset(g_rx_buffer, 0, sizeof(g_rx_buffer));

        rx_data_length = packet_poll_receive(socket_handle,
                                  g_rx_buffer,
                                  sizeof(g_rx_buffer),
                                  g_using_ipv6,
                                  NULL,
                                  NULL,
                                  NULL);

        if (rx_data_length > 0)
        {
            same = !memcmp(g_rx_buffer, g_packet_data.data(), rx_data_length);
        }
        else
        {
            same = false;
        }


        if (rx_data_length < 0)
        {
            printf("%u: Error receiving packet!\n", i);
        }
        else if ((unsigned)rx_data_length != g_packet_data.size())
        {
            printf("%u: Only got %i bytes!\n", i, rx_data_length);
        }
        else if (!same)
        {
            printf("%u: data mismatch!\n", i);
        }
        else
        {
            // Just print every 10 packets an informational
            if (((i+1) % 100) == 0)
            {
                printf("%u: packets processed\n", i+1);
            }
            // Last packet informational
            else if (i == g_num_packets - 1)
            {
                printf("%u: packets processed (last one)\n", i+1);
            }
        }
    }
}

int main(int argc, char **argv)
{
    int          c;
    extern char *optarg;
    unsigned     x32;
    bool         no_args_set = true;

    // First ':' means suppress error
    // A ':' following a letter means the option requires a parameter
    while ((c = getopt(argc, argv, ":i:d:s:p:n:l:hvf")) != -1)
    {
        switch (c)
        {
        // interface name
        case 'i':
            g_self_intfc_name.assign(optarg);
            no_args_set = false; 
            break;
        // device ip address
        case 'd':
            g_device_ip_addr.assign(optarg);
            no_args_set = false; 
            break;
        // self ip address
        case 's':
            g_self_ip_addr.assign(optarg);
            no_args_set = false; 
            break;
        // UDP port number
        case 'p':
            if (1 != sscanf(optarg, "%u", &x32))
            {
                printf("Bad UDP port number!! <%s>\n", optarg);
                return(0);
            }
            g_udp_port_number = (uint16_t)x32;
            no_args_set = false; 
            break;

        // number of packets to send
        case 'n':
            if (1 != sscanf(optarg, "%u", &g_num_packets))
            {
                printf("Bad number of packets to send!! <%s>\n", optarg);
                return(0);
            }
            no_args_set = false; 
            break;
        // packet length
        case 'l':
            if (1 != sscanf(optarg, "%u", &g_packet_length))
            {
                printf("Bad packet length!! <%s>\n", optarg);
                return(0);
            }
            no_args_set = false; 
            break;
        // Print version
        case 'v':
            printf("%s: Version: %s\n", argv[0], VERSION);
            return(0);
            break;
        // using IPv6, not IPv4
        case 'f':
            g_using_ipv6 = true;
            no_args_set = false; 
            break;
        // Print help
        case 'h':
            print_help_screen(argv[0]);
            return(0);
            break;
        default:
            printf("Unknown argument: <%s>.\n", optarg);
            print_help_screen(argv[0]);
            return(0);
            break;
        }
    }

    if (no_args_set)
    {
        print_help_screen(argv[0]);
        return 0;
    }

    // Sanity check parms
    if (g_num_packets == 0)
    {
        printf("Number of packets is incorrect\n");
        return(0);
    }
    if ((g_packet_length ==0) || (g_packet_length > MTU))
    {
        printf("Invalid packet length\n");
        return(0);
    }
    if (g_udp_port_number == 0)
    {
        printf("Invalid UDP port number\n");
        return(0);
    }

    // Let's do it
    engine();

    return 0;
}

void print_help_screen(const char *app_name)
{
    printf("Usage: %s -i <intfc-name> -d <device-ip-addr> -s <self-ip-address> ", app_name);
    printf(" -p <port-number> -n <num-packets> -l <packet-length>\n");
    printf("     -h    help\n");
    printf("     -v    print version number\n");
    printf("     -f    use IPv6 instead of IPv4\n");
}
