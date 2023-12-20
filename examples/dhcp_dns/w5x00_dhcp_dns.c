/**
 * Copyright (c) 2022 WIZnet Co.,Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * ----------------------------------------------------------------------------------------------------
 * Includes
 * ----------------------------------------------------------------------------------------------------
 */
#include <stdio.h>

#include "port_common.h"

#include "wizchip_conf.h"
#include "socket.h"
#include "w5x00_spi.h"
#include "w5x00_lwip.h"

#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"

#include "lwip/apps/lwiperf.h"
#include "lwip/etharp.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"

#include "postgresql.h"

#include "timer.h"

#include "pico/stdlib.h"
#include "pico/util/queue.h"

#include "hardware/flash.h"

#include "mdb.h"

/**
 * ----------------------------------------------------------------------------------------------------
 * Macros
 * ----------------------------------------------------------------------------------------------------
 */
/* Clock */
#define PLL_SYS_KHZ (133 * 1000)

/* Socket */
#define SOCKET_MACRAW 0

/* Port */
#define PORT_LWIPERF 5001

/**
 * ----------------------------------------------------------------------------------------------------
 * Variables
 * ----------------------------------------------------------------------------------------------------
 */
/* Network */
extern uint8_t mac[6];

/* LWIP */
struct netif g_netif;

/* DNS */
static uint8_t g_dns_target_domain[] = "_postgresql._tcp.local";
static uint8_t g_dns_get_ip_flag = 1;
static uint32_t g_ip;

static ip_addr_t default_ip;
static ip_addr_t default_mask;
static ip_addr_t default_gateway;

static ip_addr_t g_resolved;

/* Timer */
static volatile uint16_t mdb_msec_cnt = 0;

/* MDB */
extern queue_t MDBfifo;

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
/* Clock */
static void set_clock_khz(void);
static void my_netif_status_callback(struct netif *netif);
/* Timer */
static void repeating_timer_callback(void);

/**
 * ----------------------------------------------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------------------------------------------
 */
int main()
{
    /* Initialize */
    int8_t retval = 0;
    uint8_t *pack = malloc(ETHERNET_MTU);
    uint16_t pack_len = 0;
    struct pbuf *p = NULL;
    err_t status = ERR_VAL;
    err_t old_status = ERR_OK;

    // Initialize network configuration
    //IP4_ADDR(&default_ip, 192, 168, 0, 80);
    //IP4_ADDR(&default_mask, 255, 255, 255, 0);
    //IP4_ADDR(&default_gateway, 192, 168, 0, 1);

    set_clock_khz();

    // Initialize stdio after the clock change
    stdio_init_all();

    wizchip_1ms_timer_initialize(repeating_timer_callback);

    InitMDB();
    
    printf("starting up...\n",status);

    sleep_ms(100); // wait a while

    wizchip_spi_initialize();
    wizchip_cris_initialize();

    wizchip_reset();
    wizchip_initialize();
    wizchip_check();

    // Set ethernet chip MAC address
    setSHAR(mac);
    ctlwizchip(CW_RESET_PHY, 0);

    // Initialize LWIP in NO_SYS mode
    lwip_init();

    netif_add(&g_netif, IP4_ADDR_ANY, IP4_ADDR_ANY, IP4_ADDR_ANY, NULL, netif_initialize, netif_input);
    //netif_add(&g_netif, &default_ip, &default_mask, &default_gateway, NULL, netif_initialize, netif_input);
    g_netif.name[0] = 'e';
    g_netif.name[1] = '0';

    // Assign callbacks for link and status
    netif_set_link_callback(&g_netif, netif_link_callback);
    netif_set_status_callback(&g_netif, my_netif_status_callback);

    // MACRAW socket open
    retval = socket(SOCKET_MACRAW, Sn_MR_MACRAW, PORT_LWIPERF, 0x00);

    if (retval < 0)
    {
        printf(" MACRAW socket open failed\n");
    }

    // Set the default interface and bring it up
    netif_set_default(&g_netif);
    netif_set_link_up(&g_netif);
    netif_set_up(&g_netif);

    // Start DHCP configuration for an interface
    dhcp_start(&g_netif);

    dns_init();
    
    /* Infinite loop */
    while (1)
    {
        getsockopt(SOCKET_MACRAW, SO_RECVBUF, &pack_len);

        if (pack_len > 0)
        {
            pack_len = recv_lwip(SOCKET_MACRAW, (uint8_t *)pack, pack_len);

            if (pack_len)
            {
                p = pbuf_alloc(PBUF_RAW, pack_len, PBUF_POOL);
                pbuf_take(p, pack, pack_len);
                //free(pack);

                //pack = malloc(ETHERNET_MTU);
            }
            else
            {
                printf(" No packet received\n");
            }

            if (pack_len && p != NULL)
            {
                LINK_STATS_INC(link.recv);

                if (g_netif.input(p, &g_netif) != ERR_OK)
                {
                    pbuf_free(p);
                }
            }
        }

        if (g_dns_get_ip_flag == 0) {            

            status = dns_gethostbyname(g_dns_target_domain, &g_resolved, NULL, NULL);

            if (status == ERR_OK)
            {
                g_ip = g_resolved.addr;

                printf(" DNS success\n");
                printf(" Target domain : %s\n", g_dns_target_domain);
                printf(" IP of target domain : [%d.%d.%d.%d]\n", g_ip & 0xFF, (g_ip >> 8) & 0xFF, (g_ip >> 16) & 0xFF, (g_ip >> 24) & 0xFF);

                g_dns_get_ip_flag = 1;

                tcp_postgresql_init(&g_resolved);
            }

            old_status = status;
        }

        /* Cyclic lwIP timers check */
        sys_check_timeouts();
        
        while(!queue_is_empty(&MDBfifo)) {
            uint16_t c;
            if (!queue_try_remove(&MDBfifo, &c)) {
                printf("fifo empty");
            }                
            printf("%03X ", c); // Display character in the console
        }        
    }
}

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
/* Clock */
static void set_clock_khz(void)
{
    // set a system clock frequency in khz
    set_sys_clock_khz(PLL_SYS_KHZ, true);

    // configure the specified clock
    clock_configure(
        clk_peri,
        0,                                                // No glitchless mux
        CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, // System PLL on AUX mux
        PLL_SYS_KHZ * 1000,                               // Input frequency
        PLL_SYS_KHZ * 1000                                // Output (must be same as no divider)
    );
}

/// @brief 
/// @param netif 
void my_netif_status_callback(struct netif *netif)
{
    printf("netif status changed %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));

    g_dns_get_ip_flag=0;
}

/* Timer */
static void repeating_timer_callback(void)
{            
    if(g_dns_get_ip_flag) handleMDBtimer();
}