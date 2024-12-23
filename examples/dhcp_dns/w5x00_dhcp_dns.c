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

#include "intercom.h"
#include "postgresql.h"

#include "timer.h"

#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include "pico/unique_id.h"

#include "hardware/flash.h"
#include "flash_utils.h"

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
extern pico_unique_board_id_t FlashID;

/* LWIP */
struct netif g_netif;

/* DNS */
static uint8_t g_dns_target_domain[] = "_postgresql._tcp.local";
static uint8_t g_dns_get_ip_flag = 0;
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

extern u8_t crc_error;
extern bool crc_error_sent;

/* MDB */
extern queue_t MDBfifo;

extern const APP MyApplication;

extern CONFIG ConfigMem;

extern unsigned char DirectEntryCounter;
extern uint32_t Uuid[4];

extern bool IntercomConnected;
extern bool rewriteEEPROM;
extern uint16_t oldCOnfigMemCRC;

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

u8_t VarCheck = 1;

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
    char str[256];

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

    pico_get_unique_board_id(&FlashID);    

    printf("starting up...\nFlashID = ");
    hexdump(&FlashID,sizeof(FlashID),8,8);    

    //nvs_print_info();    

    uint16_t chkCRC = CalcCRC2(nvs_get_pagePointer(0),254);
    uint16_t nvsCRC = *(uint16_t*)(nvs_get_pagePointer(0)+254);
    
    if (chkCRC != nvsCRC) {
        printf("nvsCRC=%x\n",nvsCRC);
        printf("calculated CRC=%x\n",chkCRC);

        uint8_t buffer[256];
        memset(&buffer[0],0xff,256);
        memcpy(&buffer[0],(uint8_t*)&MyApplication,sizeof(MyApplication));
        *(unaligned_uint*)&buffer[254] = CalcCRC2(&buffer[0],254);
        nvs_write_page(0,&buffer[0]);        
    }    

    sleep_ms(100); // prevent false key detection

    if (!gpio_get(PROG_PIN_1)) {
        printf("  PROG_KEY1 pressed!\n");
        progPin1State = true;    
    }

    if (!gpio_get(PROG_PIN_2)) {
        printf("  PROG_KEY2 pressed!\n");    
        progPin2State = true;
    }

    unsigned char RTCavail = InitClock();   // init i2C !!!
    printf("rtc chip ds1307 available= %u\n",RTCavail);

    defaultConfigMem();    
    if (ReadEEPROM(&ConfigMem.data[0])) {
        oldCOnfigMemCRC = ConfigMem.CFG.CRC;
        printf("EEPROM content:\n");        
        hexdump((uint8_t*)&ConfigMem.data[0],sizeof(ConfigMem), 16, 8);
        
        uint16_t cfgCRC = CalcCRC2(&ConfigMem.data[2],254);
        
        if ((cfgCRC != ConfigMem.CFG.CRC) || (progPin1State && progPin2State)) {
            printf("EEPROM CRC=%x calculated CRC=%x \n",ConfigMem.CFG.CRC,cfgCRC);
            defaultConfigMem();            
            if (writeEEPROM(&ConfigMem.data[0])) printf("EEPROM Default succesfully written!\n");            
        } else {
            printf("EEPROM checksum OK!\n");
            if (progPin1State) {
                ConfigMem.CFG.dhcpMode = 1;
                calcCRCConfigMem();
                if (writeEEPROM(&ConfigMem.data[0])) printf("EEPROM DHCP ON succesfully written!\n");
            }
            if (progPin2State) {
                ConfigMem.CFG.dhcpMode = 0;
                calcCRCConfigMem();
                if (writeEEPROM(&ConfigMem.data[0])) printf("EEPROM DHCP OFF succesfully written!\n");
            }

            //copy counters to SysVar and calcCoinCRC
            copyConfigMem2SysVar();
        }        
    }
    else printf("No EEPROM found!\n");    

    IlluminationInit();

    queue_init(&KEYfifo, sizeof(KEY_EVENT), 32);    
    
    my_10ms_timer_initialize(repeating_timer_10ms_callback);

    wizchip_1ms_timer_initialize(repeating_timer_1ms_callback);
    
    InitMDB();
    
    sleep_ms(10); // wait a while

    wizchip_spi_initialize();
    wizchip_cris_initialize();

    wizchip_reset();
    wizchip_initialize();
    wizchip_check();

    // Set ethernet chip MAC address
    setSHAR(mac);
    ctlwizchip(CW_RESET_PHY, 0);

    default_ip = ConfigMem.CFG.lwip_ip;
    default_mask = ConfigMem.CFG.lwip_netmask;
    default_gateway = ConfigMem.CFG.lwip_gateway;

    // Initialize LWIP in NO_SYS mode
    lwip_init();

    if (ConfigMem.CFG.dhcpMode==0) {
        netif_add(&g_netif, &default_ip, &default_mask, &default_gateway, NULL, netif_initialize, netif_input);
    } else {
        netif_add(&g_netif, IP4_ADDR_ANY, IP4_ADDR_ANY, IP4_ADDR_ANY, NULL, netif_initialize, netif_input);
    }
    
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
    if (ConfigMem.CFG.dhcpMode!=0) dhcp_start(&g_netif);

    dns_init();
    
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_servermode_dhcp(1);
    sntp_init();

    udp_intercom_init();

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

        if (g_dns_get_ip_flag > 1) {            

            status = dns_gethostbyname(g_dns_target_domain, &g_resolved, NULL, NULL);
            
            if (status == ERR_OK) 
            {
                g_ip = g_resolved.addr;

                printf(" DNS success\n");
                printf(" Target domain : %s\n", g_dns_target_domain);
                printf(" IP of target domain : [%d.%d.%d.%d]\n", g_ip & 0xFF, (g_ip >> 8) & 0xFF, (g_ip >> 16) & 0xFF, (g_ip >> 24) & 0xFF);

                g_dns_get_ip_flag = 1;

                // tcp_postgresql_init(&g_resolved);                
                
            }

            old_status = status;
        }

        /* Cyclic lwIP timers check */
        sys_check_timeouts();

        if (timeRead) {
            timeRead = false;
            ReadTimeFromClock();
            intercomMessage_poll();
            //printf("Current RTC: %02u.%02u.20%02u %02u:%02u:%02u\n", TimeRegDEC[4], TimeRegDEC[5],  TimeRegDEC[6], TimeRegDEC[2], TimeRegDEC[1], TimeRegDEC[0]);

            if (rewriteEEPROM) {
                rewriteEEPROM = false;

                if (oldCOnfigMemCRC != ConfigMem.CFG.CRC) {
                    if (writeEEPROM(&ConfigMem.data[0])) printf("EEPROM update succesfully written!\n");
                } else printf("EEPROM update not necessary!\n");
            }

        }
        
        while(!queue_is_empty(&MDBfifo)) {
            MDB_EVENT ev;
            unsigned int coinval, noteval;

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
                addMessage(ICOM_MESSAGE_CHANGER_ERROR,1,&ev.Data[1]);
                break;
            case EvTypeMDB_ChangerLost:
                printf("Changer %02X lost\n", ev.Data[0]);
                addMessage(ICOM_MESSAGE_CHANGER_ERROR,1,&ev.Data[1]);
                break;
            case EvTypeMDB_ChangerProblem:
                printf("Changer %02X error %02X\n", ev.Data[0], ev.Data[1]);
                if (MDB_Changer1.TubesRead) addMessage(ICOM_MESSAGE_TUBE_ERROR,2,&ev.Data[0]);
                break;
            case EvTypeMDB_ChangerTubeStatusChanged:
				if (ev.Data[1]){ // diff>0
					
					char* strpointer=&str[0];
					
					for (int s=0;s<MDB_MAXCOINS;s++) 
					{
                        int k;
					  	for (k=0;k<MDB_MAXCOINS;k++)
						 if (MDB_Changer1.PhysTubes[k]==s) break;
						 
						if (k<MDB_MAXCOINS) 
						{
							if (SysVar.Tube[k].Deroute)			
							{	
								if (SysVar.Tube[k].Deroute<=MAX_HOPPER)
								 	 *(unaligned_ushort *)strpointer=SysVar.Hopper[SysVar.Tube[k].Deroute-1].Fill;
								else *(unaligned_ushort *)strpointer=0;
							}
							else *(unaligned_ushort*)strpointer=MDB_Changer1.TubeStatus[s];
						}
						else *(unaligned_ushort*)strpointer=MDB_Changer1.TubeStatus[s];
						
						strpointer+=2;									
					}				
					
					if (MDB_Changer1.Problem==6) // cassette removed
					     *(unsigned char *)strpointer++=1;
					else *(unsigned char *)strpointer++=0;
										
					addMessage(ICOM_MESSAGE_COIN_STATUS, MDB_MAXCOINS*2+1, &str[0]);
				}
                break;
            case EvTypeMDB_ChangerDispenseInfo:
                break;
            case EvTypeMDB_ChangerExpandedPayout:
                break;

            case EvTypeMDB_TubesNewFillStatus:
                break;
            
            case EvTypeMDB_CoinInTube:
                coinval=(ev.Data[1] + ev.Data[2]*256);
                printf("Coin %d in Tube - channel %d \n",coinval,ev.Data[5]);
                addMessage(ICOM_MESSAGE_COIN_ACCEPTED,7,&ev.Data[0]);
                break;
            case EvTypeMDB_CoinInCashbox:
                coinval=(ev.Data[1] + ev.Data[2]*256);
                printf("Coin %d in Cashbox - channel %d \n",coinval,ev.Data[5]);
                addMessage(ICOM_MESSAGE_COIN_ACCEPTED,7,&ev.Data[0]);
                break;
            case EvTypeMDB_CoinDispensedManually:
                coinval=(ev.Data[1] + ev.Data[2]*256);
                printf("Coin %d in Cashbox - channel %d \n",coinval,ev.Data[5]);
                addMessage(ICOM_MESSAGE_COIN_DISPENSED,7,&ev.Data[0]);
                break;
            case EvTypeMDB_CoinRejected:
                break;

            case EvTypeMDB_ValidatorStatus:
                break;
            case EvTypeMDB_ValidatorLost:
                break;


            case EvTypeMDB_BillStacked:
                noteval=(ev.Data[1] + ev.Data[2]*0x100 + ev.Data[3]*0x10000 + ev.Data[4]*0x1000000);
                printf("Bill %d accepted - channel %d \n",noteval,ev.Data[7]);
                addMessage(ICOM_MESSAGE_BILL_ACCEPTED,8,&ev.Data[0]);
                break;

            case EvTypeMDB_BillInEscrow:
                noteval=(ev.Data[1] + ev.Data[2]*0x100 + ev.Data[3]*0x10000 + ev.Data[4]*0x1000000);
                printf("Bill %d in escrow - channel %d \n",noteval,ev.Data[6]);                
                break;

            case EvTypeMDB_BillRejected:
            case EvTypeMDB_BillReturned:
                noteval=(ev.Data[1] + ev.Data[2]*0x100 + ev.Data[3]*0x10000 + ev.Data[4]*0x1000000);
                printf("Bill %d rejected - channel %d \n",noteval,ev.Data[7]);
                addMessage(ICOM_MESSAGE_BILL_REJECTED,8,&ev.Data[0]);
                break;
            case EvTypeMDB_BillStackedManually:
                addMessage(ICOM_MESSAGE_BILL_STACKED,7,&ev.Data[0]);
                break;
            case EvTypeMDB_BillEscrowTimeout:
                break;

            case EvTypeMDB_CardreaderReady:
                printf("CardReader ready \n");
                addMessage(ICOM_MESSAGE_CARDREADER_READY,0,&ev.Data[0]);
                break;
            case EvTypeMDB_CardreaderVendApproved:
                noteval=(ev.Data[1] + ev.Data[2]*0x100 + ev.Data[3]*0x10000 + ev.Data[4]*0x1000000);
                printf("CardReader amount %d approved!\n",noteval);
                addMessage(ICOM_MESSAGE_CARDREADER_ACCEPTED,5,&ev.Data[0]);
                break;
            case EvTypeMDB_CardreaderVendDenied:
                printf("CardReader amount %d denied!\n",noteval);
                addMessage(ICOM_MESSAGE_CARDREADER_DENIED,5,&ev.Data[0]);
                break;
            case EvTypeMDB_CardreaderDisplay:
                addMessage(ICOM_MESSAGE_CARDREADER_DISPLAY,strlen(ev.Data),&ev.Data[0]);
                break;
            case EvTypeMDB_CardreaderFunds:            
                addMessage(ICOM_MESSAGE_CARDREADER_FUNDS,9,&ev.Data[1]);
                break;
            case EvTypeMDB_CardreaderLost:
            case EvTypeMDB_CardreaderError:
                addMessage(ICOM_MESSAGE_CARDREADER_ERROR,1,&ev.Data[1]);
                break;
            case EvTypeMDB_CardreaderAgeVerificationStatus:
                break;

            case EvTypeMDB_AVDLost:
                break;

            case EvTypeMDB_ParkIOLost:
                break;

            case EvTypeMDB_HopperInserted:
                addMessage(ICOM_MESSAGE_HOPPER_INSERTED,1,&ev.Data[1]);
                break;
            case EvTypeMDB_HopperStatusChanged:
				for (int s=0;s<MAX_HOPPER;s++)				
				{
					if (SysVar.Hopper[s].Blocked)
						 str[s]=(SysVar.Hopper[s].Status | 0x80);
					else str[s]=SysVar.Hopper[s].Status;
				}				
				addMessage(ICOM_MESSAGE_HOPPER_STATUS, MAX_HOPPER, &str[0]);
                break;
            case EvTypeMDB_HopperCounted:

                SysVar.Hopper[ev.Data[1]].Fill = ((uint16_t)ev.Data[2]<<8) + ev.Data[3];
                SysVar.Hopper[ev.Data[1]].LastFill = ((uint16_t)ev.Data[2]<<8) + ev.Data[3];
                SysVar.Hopper[ev.Data[1]].Ready = 1;
                
                CalcCoinCRC(true,200);

                addMessage(ICOM_MESSAGE_HOPPER_CLEARED,3,&ev.Data[1]);
                break;
            case EvTypeMDB_HopperRemoved:
                addMessage(ICOM_MESSAGE_HOPPER_REMOVED,1,&ev.Data[1]);
                break;
            case EvTypeMDB_HopperPaidOut:
                addMessage(ICOM_MESSAGE_HOPPER_DISPENSED,2,&ev.Data[1]);
                break;
            case EvTypeMDB_HopperTimeout:
                addMessage(ICOM_MESSAGE_PAYOUT_ERROR, 2, &ev.Data[1]);
                            
                SysVar.Hopper[ev.Data[2]].Fill+=ev.Data[1];	//nicht ausgegebene Muenzen wieder auf Bestand !
                SysVar.Hopper[ev.Data[2]].LastFill+=ev.Data[1];
        
                printf("Hopper %s konnte %02u M�nzen nicht ausgeben!\n",ev.Data[2],ev.Data[1]);
                
                CalcCoinCRC(true,201);

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
                // if (evKey.Data[0] == 9) directEntry = !directEntry;
                if (evKey.Data[0] == 10) {
                    if (DirectEntryCounter) DirectEntryCounter=2;
                    uint8_t queueData[16];
                    *(unaligned_ulong*)&queueData[0]=Uuid[0];
                    *(unaligned_ulong*)&queueData[4]=Uuid[1];
                    *(unaligned_ulong*)&queueData[8]=Uuid[2];
                    *(unaligned_ulong*)&queueData[12]=Uuid[3];

                    addMessage(ICOM_MESSAGE_CMD_QUEUE_ACK,16,&queueData[0]);
                    break;
                }
                if (evKey.Data[0] == 11) {
                    if (IntercomConnected) addMessage(ICOM_MESSAGE_HOUSING_OPENED,1,&evKey.Data[0]);
                    break;
                }
                if (evKey.Data[0] == 12) relais4 = !relais4;
                if (evKey.Data[0] == 13) relais5 = !relais5;

                if (IntercomConnected) addMessage(ICOM_MESSAGE_KEY_PRESSED,1,&evKey.Data[0]);
            break;
            case EvTypeKey_Released:
                printf("KEY %d released!\n", evKey.Data[0]);
                if (evKey.Data[0] == 10) break;
                if (evKey.Data[0] == 11) {
                    if (IntercomConnected) addMessage(ICOM_MESSAGE_HOUSING_CLOSED,1,&evKey.Data[0]);
                    break;
                }
                if (IntercomConnected) addMessage(ICOM_MESSAGE_KEY_RELEASED,1,&evKey.Data[0]);
            break;
            default:
                printf("KEY Event %d Length %d\n", evKey.Type, evKey.Length);
            break;
            }
        }

        if (VarCheck) {

            printf("SysVar CRC-Check...\n");

            if ((SysVar.CRC != CalcCRC2((char*)&SysVar, 356))  || crc_error)
            {
                crc_error = 1;
                if (!crc_error_sent)
                {
                    addMessage(ICOM_MESSAGE_CRC_ERROR, 0, &VarCheck);
                    printf("SysVar CRC-Error!\n");
                    crc_error_sent = true;
                }
            } else hexdump((uint8_t*)&SysVar,356, 16, 8);

            VarCheck = 0;
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

    if (!ip4_addr_isany_val(*netif_ip4_addr(netif))) {
        g_dns_get_ip_flag=2;    
        printf(" mDNS getting postgresql-host...\n");
    }
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
    //if (g_dns_get_ip_flag == 1) 
    handleMDBtimer();

    if (++msec_cnt > 19) msec_cnt=0;

}

static void repeating_timer_10ms_callback(void)
{       
    if (++rtc_cnt >= 100) rtc_cnt=0;

    if ((rtc_cnt % 50)==0) {
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
  printf("SNTP dst: %d\n", current_time_val.tm_isdst);
}

