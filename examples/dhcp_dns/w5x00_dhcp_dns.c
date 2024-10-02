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

#include "lwip/apps/sntp.h"
#include "lwip/etharp.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"

#include "postgresql.h"

#include "timer.h"

#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include "pico/unique_id.h"

#include "hardware/flash.h"

#include "mdb.h"
#include "hexdump.h"
#include "illumination.h"
#include "rtc.h"

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
pico_unique_board_id_t SystemID;

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
static volatile uint16_t msec_cnt = 0;
static volatile uint16_t rtc_cnt = 0;

static struct repeating_timer my_timer;
void (*my_callback_ptr)(void);

static void repeating_timer_1ms_callback(void);
static void repeating_timer_10ms_callback(void);

bool my_10ms_timer_callback(struct repeating_timer *t);

/* Clock */
static void set_clock_khz(void);
static void my_netif_status_callback(struct netif *netif);

/* RTC */
extern unsigned char TimeRegDEC[7];
bool timeRead = false;

extern bool trayLight;
extern bool directEntry;
extern bool directSignal;
extern bool relais4;
extern bool relais5;


/* MDB */
extern queue_t MDBfifo;

/* GPIO */

queue_t KEYfifo;

const uint PROG_PIN_1 = 14;
const uint PROG_PIN_2 = 15;

const uint KEYS_LEFT_1 = 4;
const uint KEYS_LEFT_2 = 5;
const uint KEYS_LEFT_3 = 6;
const uint KEYS_LEFT_4 = 7;

const uint KEYS_RIGHT_1 = 10;
const uint KEYS_RIGHT_2 = 11;
const uint KEYS_RIGHT_3 = 12;
const uint KEYS_RIGHT_4 = 13;

const uint DirectEntryACK = 22;
const uint DOOR = 28;

bool progPin1State = false;
bool progPin2State = false;

bool keysLeft1State = false;
bool keysLeft2State = false;
bool keysLeft3State = false;
bool keysLeft4State = false;

bool keysRight1State = false;
bool keysRight2State = false;
bool keysRight3State = false;
bool keysRight4State = false;

bool directEntryState = false;
bool doorState = false;

bool ext1State = false;
bool ext2State = false;
bool ext3State = false;

typedef struct __attribute__((packed)) KEYevent{
	unsigned int  Type;
	unsigned int  Length;
	unsigned char Data[60];
}KEY_EVENT;

KEY_EVENT KeyEvent;

enum KeyEvent_types{
	EvTypeKey_Released = 1,
	EvTypeKey_Pressed
};

static uint KeyboardDelay = 10;

void set_system_time(u32_t sec);

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */

/* Timer */
void my_10ms_timer_initialize(void (*callback)(void))
{
    my_callback_ptr = callback;
    add_repeating_timer_us(-10000, my_10ms_timer_callback, NULL, &my_timer);
}

bool my_10ms_timer_callback(struct repeating_timer *t)
{
    if (my_callback_ptr != NULL)
    {
        my_callback_ptr();
    }

    return true;
}

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

    gpio_init(PROG_PIN_1);
    gpio_set_dir(PROG_PIN_1, GPIO_IN);
    gpio_init(PROG_PIN_2);
    gpio_set_dir(PROG_PIN_2, GPIO_IN);
    gpio_init(KEYS_LEFT_1);
    gpio_init(KEYS_LEFT_2);
    gpio_init(KEYS_LEFT_3);
    gpio_init(KEYS_LEFT_4);
    gpio_set_dir(KEYS_LEFT_1, GPIO_IN);
    gpio_set_dir(KEYS_LEFT_2, GPIO_IN);
    gpio_set_dir(KEYS_LEFT_3, GPIO_IN);
    gpio_set_dir(KEYS_LEFT_4, GPIO_IN);
    gpio_init(KEYS_RIGHT_1);
    gpio_init(KEYS_RIGHT_2);
    gpio_init(KEYS_RIGHT_3);
    gpio_init(KEYS_RIGHT_4);
    gpio_set_dir(KEYS_RIGHT_1, GPIO_IN);
    gpio_set_dir(KEYS_RIGHT_2, GPIO_IN);
    gpio_set_dir(KEYS_RIGHT_3, GPIO_IN);
    gpio_set_dir(KEYS_RIGHT_4, GPIO_IN);
    gpio_init(DirectEntryACK);
    gpio_set_dir(DirectEntryACK, GPIO_IN);
    gpio_init(DOOR);
    gpio_set_dir(DOOR, GPIO_IN);

    unsigned char RTCavail = InitClock();

    printf("rtc chip ds1307 available= %u \n",RTCavail);

    IlluminationInit();

    pico_get_unique_board_id(&SystemID);    

    queue_init(&KEYfifo, sizeof(KEY_EVENT), 32);    
    
    my_10ms_timer_initialize(repeating_timer_10ms_callback);

    wizchip_1ms_timer_initialize(repeating_timer_1ms_callback);
    
    InitMDB();
    
    printf("starting up...\nSystemID = ");
    hexdump(&SystemID,sizeof(SystemID),8,8);    

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

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_servermode_dhcp(1);
    sntp_init();

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

        if (timeRead) {
            //printf("Current RTC: %02u.%02u.20%02u %02u:%02u:%02u\n", TimeRegDEC[4], TimeRegDEC[5],  TimeRegDEC[6], TimeRegDEC[2], TimeRegDEC[1], TimeRegDEC[0]);
            timeRead = false;
        }
        
        while(!queue_is_empty(&MDBfifo)) {
            MDB_EVENT ev;
            unsigned int coinval;

            if (!queue_try_remove(&MDBfifo, &ev)) {
                printf("MDB fifo empty");
            }   
            
            switch (ev.Type)
            {
            case EvTypeMDB_ChangerReady:
                printf("Enabling coins...\n");
                EnableCoins(0xFFFF);
                IlluminateCoinInsert(1);                
                IlluminateBillInsert(1);
                IlluminateCardInsert(1);
                break;
            case EvTypeMDB_ChangerStatus:
                printf("Changer %02X Status %02X\n", ev.Data[0],ev.Data[1]); // Display character in the console
                break;
            
            case EvTypeMDB_CoinInTube:
                coinval=(ev.Data[0] + ev.Data[1]*256);
                printf("Coin %d in Tube - channel %d \n",coinval,ev.Data[2]);
                break;

            case EvTypeMDB_ChangerLost:
                printf("Changer %02X lost\n", ev.Data[0]);
                break;

            default:
                printf("MDB Event %d Length %d\n", ev.Type, ev.Length);
                break;
            }            
        }    

        while(!queue_is_empty(&KEYfifo)) {
            KEY_EVENT evKey;

            if (!queue_try_remove(&KEYfifo, &evKey)) {
                printf("KEY fifo empty");
            }   
            
            switch (evKey.Type)
            {
            case EvTypeKey_Pressed:
                printf("KEY %d pressed!\n", evKey.Data[0]);
                if (evKey.Data[0] == 8) trayLight = !trayLight;
                if (evKey.Data[0] == 9) directEntry = !directEntry;
                if (evKey.Data[0] == 10) directSignal = !directSignal;
                if (evKey.Data[0] == 12) relais4 = !relais4;
                if (evKey.Data[0] == 13) relais5 = !relais5;
            break;
            case EvTypeKey_Released:
                printf("KEY %d released!\n", evKey.Data[0]);
            break;
            default:
                printf("KEY Event %d Length %d\n", evKey.Type, evKey.Length);
            break;
            }
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

void putKeyEvent(KEY_EVENT *event ) {
	if (!queue_try_add(&KEYfifo, event)) {
		printf("KEY fifo full");
	}
    KeyboardDelay = 5;			      			
}


/* Timer */
static void repeating_timer_1ms_callback(void)
{            
    if (g_dns_get_ip_flag) handleMDBtimer();

    if (++msec_cnt > 19) msec_cnt=0;

}

static void repeating_timer_10ms_callback(void)
{       
    if (++rtc_cnt >= 100) rtc_cnt=0;

    if ((rtc_cnt % 50)==0) {
        ReadTimeFromClock();
        timeRead = true;
    }

    if (rtc_cnt & 0x01) {
        ScanFRONTRGB(0);             
        ScanFRONTRGB(2);
    }
    else {
        ScanFRONTRGB(1);
        writeIllum();
    }

    //scan keys

    if (KeyboardDelay) {
        KeyboardDelay--;
        return;
    }

    if (gpio_get(PROG_PIN_1)) {
        if (progPin1State) {
            KeyEvent.Type    = EvTypeKey_Released;
            KeyEvent.Data[0] = 8;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        progPin1State = false;
    }
    else {
        if (!progPin1State) {
            KeyEvent.Type    = EvTypeKey_Pressed;
            KeyEvent.Data[0] = 8;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);            
        }
        progPin1State = true;
    }

    if (gpio_get(PROG_PIN_2)) {
        if (progPin2State) {
            KeyEvent.Type    = EvTypeKey_Released;
            KeyEvent.Data[0] = 9;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        progPin2State = false;
    }
    else {
        if (!progPin2State) {
            KeyEvent.Type    = EvTypeKey_Pressed;
            KeyEvent.Data[0] = 9;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        progPin2State = true;
    }

    if (gpio_get(KEYS_LEFT_1)) {
        if (keysLeft1State) {
            KeyEvent.Type    = EvTypeKey_Released;
            KeyEvent.Data[0] = 0;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        keysLeft1State = false;
    }
    else {
        if (!keysLeft1State) {
            KeyEvent.Type    = EvTypeKey_Pressed;
            KeyEvent.Data[0] = 0;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        keysLeft1State = true;
    }

    if (gpio_get(KEYS_LEFT_2)) {
        if (keysLeft2State) {
            KeyEvent.Type    = EvTypeKey_Released;
            KeyEvent.Data[0] = 1;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        keysLeft2State = false;
    }
    else {
        if (!keysLeft2State) {
            KeyEvent.Type    = EvTypeKey_Pressed;
            KeyEvent.Data[0] = 1;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        keysLeft2State = true;
    }

    if (gpio_get(KEYS_LEFT_3)) {
        if (keysLeft3State) {
            KeyEvent.Type    = EvTypeKey_Released;
            KeyEvent.Data[0] = 2;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        keysLeft3State = false;
    }
    else {
        if (!keysLeft3State) {
            KeyEvent.Type    = EvTypeKey_Pressed;
            KeyEvent.Data[0] = 2;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        keysLeft3State = true;
    }

    if (gpio_get(KEYS_LEFT_4)) {
        if (keysLeft4State) {
            KeyEvent.Type    = EvTypeKey_Released;
            KeyEvent.Data[0] = 3;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        keysLeft4State = false;
    }
    else {
        if (!keysLeft4State) {
            KeyEvent.Type    = EvTypeKey_Pressed;
            KeyEvent.Data[0] = 3;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        keysLeft4State = true;
    }

    if (gpio_get(KEYS_RIGHT_1)) {
        if (keysRight1State) {
            KeyEvent.Type    = EvTypeKey_Released;
            KeyEvent.Data[0] = 4;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        keysRight1State = false;
    }
    else {
        if (!keysRight1State) {
            KeyEvent.Type    = EvTypeKey_Pressed;
            KeyEvent.Data[0] = 4;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        keysRight1State = true;
    }

    if (gpio_get(KEYS_RIGHT_2)) {
        if (keysRight2State) {
            KeyEvent.Type    = EvTypeKey_Released;
            KeyEvent.Data[0] = 5;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        keysRight2State = false;
    }
    else {
        if (!keysRight2State) {
            KeyEvent.Type    = EvTypeKey_Pressed;
            KeyEvent.Data[0] = 5;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        keysRight2State = true;
    }

    if (gpio_get(KEYS_RIGHT_3)) {
        if (keysRight3State) {
            KeyEvent.Type    = EvTypeKey_Released;
            KeyEvent.Data[0] = 6;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        keysRight3State = false;
    }
    else {
        if (!keysRight3State) {
            KeyEvent.Type    = EvTypeKey_Pressed;
            KeyEvent.Data[0] = 6;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        keysRight3State = true;
    }

    if (gpio_get(KEYS_RIGHT_4)) {
        if (keysRight4State) {
            KeyEvent.Type    = EvTypeKey_Released;
            KeyEvent.Data[0] = 7;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        keysRight4State = false;
    }
    else {
        if (!keysRight4State) {
            KeyEvent.Type    = EvTypeKey_Pressed;
            KeyEvent.Data[0] = 7;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        keysRight4State = true;
    }

    if (gpio_get(DirectEntryACK)) {
        if (directEntryState) {
            KeyEvent.Type    = EvTypeKey_Released;
            KeyEvent.Data[0] = 10;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        directEntryState = false;
    }
    else {
        if (!directEntryState) {
            KeyEvent.Type    = EvTypeKey_Pressed;
            KeyEvent.Data[0] = 10;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        directEntryState = true;
    }

    if (gpio_get(DOOR)) {
        if (doorState) {
            KeyEvent.Type    = EvTypeKey_Released;
            KeyEvent.Data[0] = 11;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        doorState = false;
    }
    else {
        if (!doorState) {
            KeyEvent.Type    = EvTypeKey_Pressed;
            KeyEvent.Data[0] = 11;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        doorState = true;
    }

    uint8_t scan = scan_pcf();

    if (scan & 0x20) {
        if (ext1State) {
            KeyEvent.Type    = EvTypeKey_Released;
            KeyEvent.Data[0] = 12;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        ext1State = false;
    }
    else {
        if (!ext1State) {
            KeyEvent.Type    = EvTypeKey_Pressed;
            KeyEvent.Data[0] = 12;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        ext1State = true;
    }

    if (scan & 0x80) {
        if (ext2State) {
            KeyEvent.Type    = EvTypeKey_Released;
            KeyEvent.Data[0] = 13;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        ext2State = false;
    }
    else {
        if (!ext2State) {
            KeyEvent.Type    = EvTypeKey_Pressed;
            KeyEvent.Data[0] = 13;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        ext2State = true;
    }

    if (scan & 0x40) {
        if (ext3State) {
            KeyEvent.Type    = EvTypeKey_Released;
            KeyEvent.Data[0] = 14;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        ext3State = false;
    }
    else {
        if (!ext3State) {
            KeyEvent.Type    = EvTypeKey_Pressed;
            KeyEvent.Data[0] = 14;
  			KeyEvent.Length  = 1;
  			putKeyEvent(&KeyEvent);
        }
        ext3State = true;
    }


}
/*
void set_system_time(uint32_t sec)
{
    time_t epoch = sec;
    struct tm *time = gmtime(&epoch);

    datetime_t datetime = {
            .year = (int16_t) (1900 + time->tm_year),
            .month = (int8_t) (time->tm_mon + 1),
            .day = (int8_t) time->tm_mday,
            .hour = (int8_t) time->tm_hour,
            .min = (int8_t) time->tm_min,
            .sec = (int8_t) time->tm_sec,
            .dotw = (int8_t) time->tm_wday,
    };

    // rtc_set_datetime(&datetime);
    printf("Got Time from NTP! %u, %02u:%02u:%02u %02u:%02u:%02u" ,datetime.dotw, datetime.day, datetime.month, datetime.year, datetime.hour, datetime.min, datetime.sec);

}*/

void set_system_time(u32_t sec)
{
  char buf[32];
  struct tm current_time_val;
  time_t current_time = (time_t)sec;

#if defined(_WIN32) || defined(WIN32)
  localtime_s(&current_time_val, &current_time);
#else
  localtime_r(&current_time, &current_time_val);
#endif

  strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", &current_time_val);
  printf("SNTP time: %s\n", buf);
}

