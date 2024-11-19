#ifndef _INTERCOM_H_
#define _INTERCOM_H_

#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include "pico/util/queue.h"
#include "pico/unique_id.h"

#include "lwip/udp.h"

#include "mdb.h"
#include "illumination.h"

/**
 * ----------------------------------------------------------------------------------------------------
 * Includes
 * ----------------------------------------------------------------------------------------------------
 */

/**
 * ----------------------------------------------------------------------------------------------------
 * Macros
 * ----------------------------------------------------------------------------------------------------
 */
/* Port */
#define INTERCOM_CLIENT_PORT		1001
#define INTERCOM_MESSAGE_PORT		1010
#define SERVER_IDENTIFICATION_PORT	1003
#define INTERCOM_INFO_PORT			1004
#define	OFFLINE_CLIENT_PORT			11111

#define MAX_RELAIS                  4

#define ICOM_TIMEOUT				4
#define ICOM_SEND_REPEAT			3

#define ICOM_COMMAND_END 					0xFF00

#define ICOM_COMMAND_GENERAL_STATUS			0x5347
#define ICOM_COMMAND_CLEAR_MESSAGES			0x4C43
#define ICOM_COMMAND_BILL_STATUS			0x5342
#define ICOM_COMMAND_TUBE_STATUS			0x5354
#define ICOM_COMMAND_COIN_STATUS			0x4F43
#define ICOM_COMMAND_HOPPER_STATUS			0x5348
#define ICOM_COMMAND_CASHBOX_STATUS			0x5358
#define ICOM_COMMAND_BILL_FILL				0x4642
#define ICOM_COMMAND_TUBE_FILL				0x4654
#define ICOM_COMMAND_HOPPER_FILL			0x4648
#define ICOM_COMMAND_HOPPER_VALUES			0x5648
#define ICOM_COMMAND_CASHBOX_FILL			0x4658
#define ICOM_COMMAND_ENABLE_COINS			0x4345
#define ICOM_COMMAND_ENABLE_DISPENSE		0x4445
#define ICOM_COMMAND_ENABLE_BILLS			0x4245
#define ICOM_COMMAND_DISPENSE_AMOUNT		0x4144
#define ICOM_COMMAND_DISPENSE_COIN			0x4344
#define	ICOM_COMMAND_MDB_COIN_CONFIG		0x4343
#define ICOM_COMMAND_MDB_BILL_CONFIG		0x4342
#define	ICOM_COMMAND_MDB_HOPPER_CONFIG		0x4348
#define	ICOM_COMMAND_MDB_READER_CONFIG		0x4352
#define ICOM_COMMAND_RGBANI					0x4E41
#define ICOM_COMMAND_CARDREADER_PAYMENT		0x5043
#define ICOM_COMMAND_CARDREADER_ENABLE		0x4543
#define ICOM_COMMAND_CARDREADER_DENY		0x4443
#define ICOM_COMMAND_GET_PRIORIETIES		0x5047
#define ICOM_COMMAND_SET_PRIORIETIES		0x5053
#define ICOM_COMMAND_CLEAR_HOPPER			0x4843
#define ICOM_COMMAND_CHANGER_TOTAL			0x5443
#define ICOM_COMMAND_SYSTEM_CONFIG			0x4353
#define ICOM_COMMAND_WRITE_CONFIG			0x4357
#define ICOM_COMMAND_GET_TIME				0x5447
#define ICOM_COMMAND_SET_TIME				0x5453
#define ICOM_COMMAND_GET_TUBE_ROUTING		0x5247  //GR
#define ICOM_COMMAND_SET_TUBE_ROUTING		0x5253  //SR
#define ICOM_COMMAND_CMD_SET_OUTPUTS  		0x4F53  //SO
#define ICOM_COMMAND_CMD_SET_LIGHT  		0x4C53  //SL
#define ICOM_COMMAND_CMD_SET_DIRECT  		0x4453  //SD
#define ICOM_COMMAND_CMD_SET_EXTRA  		0x4553  //SE
#define ICOM_COMMAND_CMD_ADD_QUEUE  		0x5141  //AQ

//------------------------------------------MESSAGES
#define ICOM_MESSAGE_BILL_ACCEPTED			0x4142
#define ICOM_MESSAGE_BILL_REJECTED			0x5242
#define ICOM_MESSAGE_BILL_STACKED			0x5342
#define ICOM_MESSAGE_COIN_ACCEPTED			0x4143
#define ICOM_MESSAGE_COIN_DISPENSED			0x4443
#define ICOM_MESSAGE_COIN_STATUS			0x5343
#define ICOM_MESSAGE_CHANGER_ERROR			0x4543
#define ICOM_MESSAGE_CFCARD					0x4643
#define ICOM_MESSAGE_DISPENSER_STATUS		0x5344
#define ICOM_MESSAGE_CARDREADER_ACCEPTED	0x4147
#define ICOM_MESSAGE_CARDREADER_DENIED      0x4456
#define ICOM_MESSAGE_CARDREADER_DISPLAY		0x4447
#define ICOM_MESSAGE_CARDREADER_FUNDS		0x4647
#define ICOM_MESSAGE_CARDREADER_ERROR		0x4547
#define ICOM_MESSAGE_CARDREADER_READY 		0x5247
#define ICOM_MESSAGE_CARDREADER_RCV 		0x474C
#define ICOM_MESSAGE_CARDREADER_SEND 		0x534C
#define ICOM_MESSAGE_HOUSING_OPENED			0x4F48
#define ICOM_MESSAGE_HOUSING_CLOSED			0x4348
#define ICOM_MESSAGE_HOPPER_REMOVED			0x5248
#define ICOM_MESSAGE_HOPPER_INSERTED		0x4948
#define ICOM_MESSAGE_HOPPER_DISPENSED		0x4448
#define ICOM_MESSAGE_HOPPER_ERROR			0x4548
#define ICOM_MESSAGE_HOPPER_CLEARED			0x5948
#define ICOM_MESSAGE_HOPPER_STATUS			0x5348
#define ICOM_MESSAGE_KEY_PRESSED			0x504B
#define ICOM_MESSAGE_KEY_RELEASED			0x524B
#define ICOM_MESSAGE_PAYOUT_ERROR			0x4550
#define ICOM_MESSAGE_TUBE_ERROR				0x4554
#define ICOM_MESSAGE_TUBE_STATUS			0x5354
#define ICOM_MESSAGE_VALIDATOR_ERROR		0x4556
#define ICOM_MESSAGE_EC_DISPLAY				0x4445
#define ICOM_MESSAGE_EMP_ERROR				0x4545
#define ICOM_MESSAGE_EXT_INPUT				0x4945
#define ICOM_MESSAGE_EC_RESULT				0x5245
#define ICOM_MESSAGE_EC_STATUS				0x5345
#define ICOM_MESSAGE_EC_ERRORCODE			0x4345
#define ICOM_MESSAGE_EC_RECEIPT				0x4552
#define ICOM_MESSAGE_EC_ADMIN				0x4145
#define ICOM_MESSAGE_CRC_ERROR				0x5243
#define ICOM_MESSAGE_CMD_QUEUE_ACK  		0x4151  //QA

#define MAX_MESSAGE_LEN 119

typedef struct __attribute__((packed)) ICOMmessage{
    uint8_t  status;
	uint16_t id;
	uint16_t len;
    uint32_t index;
	uint8_t  Data[MAX_MESSAGE_LEN];
}ICOM_MESSAGE;

typedef __attribute__((aligned(1))) unsigned long unaligned_ulong;
typedef __attribute__((aligned(1))) unsigned int unaligned_uint;
typedef __attribute__((aligned(1))) unsigned short unaligned_ushort;

typedef struct __attribute__((packed)) APPtag{
	unsigned char ID[3];
    unsigned char Name[16];
    unsigned char VerInfo[20];
	unsigned int  Version;
}APP;

/**
 * ----------------------------------------------------------------------------------------------------
 * Variables
 * ----------------------------------------------------------------------------------------------------
 */
extern struct SAVEVARtag SysVar;

extern struct COIN_OVERRIDEtag	CoinOver[16];
extern struct BILL_OVERRIDEtag BillOver[16];

extern unsigned char CoinIllumAni;
extern unsigned char BillIllumAni;
extern unsigned char CardIllumAni;

extern EmpTag MDB_Emp;

extern ChangerTag MDB_Changer1;
extern ChangerTag MDB_Changer2;

extern CardreaderTag MDB_Cardreader;
extern ValidatorTag MDB_Validator;
extern AVDTag MDB_AVD;
extern PARKIOTag MDB_ParkIO;

extern TubeCfg PayOutTubes[32];

extern unsigned char ValidatorNotesEN[MDB_MAXNOTES];
extern unsigned char EmpCoinsEN[MDB_MAXCOINS];

extern unsigned long MDB_ChangerTotal;

extern uint16_t SystemConfig;

extern unsigned char FRONTRGB_Ani[MAX_PCA];

//extern struct netif g_netif;
struct udp_pcb *upcb;
struct udp_pcb *mpcb;

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
void udp_intercom_init(void);
static void udp_intercom_received(void *passed_data, struct udp_pcb *upcb, struct pbuf *p, const struct ip4_addr *addr, u16_t port);
static void udp_message_received(void *passed_data, struct udp_pcb *upcb, struct pbuf *p, const struct ip4_addr *addr, u16_t port);

void intercomMessage_poll(void);
void addMessage(uint16_t id, uint16_t len, uint8_t* data);

#endif // _INTERCOM_H_
