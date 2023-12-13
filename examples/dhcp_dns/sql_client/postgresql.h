#ifndef _POSTGRESQL_H_
#define _POSTGRESQL_H_

#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>

#include "lwip/tcp.h"

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
#define PORT_POSTGRESQL 5432

#define POSTBUFFERSIZE 2048
#define COMPLETEBUFFERSIZE 64
#define NOTIFYBUFFERSIZE 2048

/**
 * ----------------------------------------------------------------------------------------------------
 * Variables
 * ----------------------------------------------------------------------------------------------------
 */
extern struct netif g_netif;
static struct tcp_pcb *tcp_postgresql_pcb;

enum sql_conn_state{
	SQL_NONE,
    SQL_STARTUP_PACKET,
    SQL_STARTUP_PACKET_SENT,
    SQL_CONNECTED,
    SQL_AUTH_MD5,
    SQL_AUTH_MD5_SENT,	
	SQL_READY_FOR_QUERY,
	SQL_AUTH_SCRAMSHA256,
	SQL_AUTH_SCRAMSHA256_SENT,
	SQL_PENDING_QUERY,
	SQL_ACTIVE_QUERY,
	SQL_QUERY_ACKED,
	SQL_CLOSING,
	SQL_CLOSED,
    SQL_ERROR = 255
};

struct tcp_postgresql_struct {
    enum sql_conn_state state;

    struct tcp_pcb *pcb; /* pointer on the current tcp_pcb */
    struct pbuf *p;      /* pointer on the received/to be transmitted pbuf */

    uint8_t handle, count;
    uint8_t cmd,start,subcmd,ecode;
    
    uint16_t sentlen, textlen;
	uint16_t msentlen, mtextlen;
    uint16_t sendptr,msendptr;
    uint16_t pollcount;
    uint16_t cols;
  
    uint32_t filelen, scriptlen, auth, PID, key;
    uint32_t len,sublen,cmdlen;
  
    char salt[4];
    unsigned char lenbuf[8];
  
    char *query;
    char *param;
    char *pval;
	char *n_channel;
	char *n_payload;
    
    char PostBuf[POSTBUFFERSIZE]; // Post buffer
    char *postptr;
	char *completeptr;
	char CompleteBuf[COMPLETEBUFFERSIZE]; // Post buffer
	char NotifyBuf[NOTIFYBUFFERSIZE];
};

struct __attribute__((packed)) sql_row_desc {
	uint32_t	tid;
	uint16_t	tatt;
	uint32_t	oid;
	uint16_t	size;
	uint32_t	tmod;
	uint16_t	fcod;
};

#define POSTGRESQL_NAMEDATALEN	64	//maximale Stringlaenge bei Spaltennamen
#define POSTGRESQL_MAXCOLS	   128	//maximal unterstuetze Spaltenanzahl pro Tabelle

#define SQL_SAVENAMES			1

struct sql_column{
	#if (SQL_SAVENAMES==1)
	char			name[POSTGRESQL_NAMEDATALEN];
	#endif
	uint16_t	index;	// 
	uint16_t	size;	// bytes
	uint32_t	oid; 
};

struct sql_column table_desc[POSTGRESQL_MAXCOLS];

#define SQL_SUBCMD_STRING		0
#define SQL_SUBCMD_PARBLOCK		1
#define	SQL_SUBCMD_LENGTH		2
#define	SQL_SUBCMD_TYPE			3
#define	SQL_SUBCMD_COLS			4
#define	SQL_SUBCMD_DLEN			5
#define	SQL_SUBCMD_PID			6
#define	SQL_SUBCMD_CHANNEL		7
#define	SQL_SUBCMD_UNDEFINED	10
#define	SQL_SUBCMD_CSTRING		11

#define SQLAuthenticationOk					0
#define SQLAuthenticationKerberosV5			2
#define SQLAuthenticationCleartextPassword	3
#define SQLAuthenticationMD5Password		5	//4-byte salt
#define SQLAuthenticationSCMCredential		6
#define SQLAuthenticationGSS				7
#define SQLAuthenticationGSSContinue		8	//n-byte GSSAPI or SSPI data
#define SQLAuthenticationSSPI				9

/*
const char STR_SQL_PARAM_SERVER_ENCODING[]	=	"server_encoding";
const char STR_SQL_PARAM_CLIENT_ENCODING[]	=	"client_encoding";

#define SQLPARAMCOUNT	2

const char *const SQLParams[SQLPARAMCOUNT]
			 		={	STR_SQL_PARAM_SERVER_ENCODING,
						STR_SQL_PARAM_CLIENT_ENCODING};

const char STR_SQL_CMD_BEGIN[]	=	"BEGIN";
const char STR_SQL_CMD_INSERT[]	=	"INSERT";
const char STR_SQL_CMD_UPDATE[]	=	"UPDATE";
const char STR_SQL_CMD_DELETE[]	=	"DELETE";
const char STR_SQL_CMD_SELECT[]	=	"SELECT";
const char STR_SQL_CMD_CREATE[]	=	"CREATE";
const char STR_SQL_CMD_MOVE[]	=	"MOVE";
const char STR_SQL_CMD_FETCH[]	=	"FETCH";
const char STR_SQL_CMD_COPY[]	=	"COPY";

#define SQLCMDCOUNT	9

const char *const SQLCommands[SQLCMDCOUNT]
			 		={	STR_SQL_CMD_BEGIN,
						STR_SQL_CMD_INSERT,
						STR_SQL_CMD_UPDATE,
						STR_SQL_CMD_SELECT,
						STR_SQL_CMD_CREATE,
						STR_SQL_CMD_MOVE,
						STR_SQL_CMD_FETCH,
						STR_SQL_CMD_COPY,
						STR_SQL_CMD_DELETE};
*/

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
void tcp_postgresql_init(ip_addr_t *server_addr);
static err_t tcp_postgresql_connected(void *arg, struct tcp_pcb *pcb_new, err_t err);
static err_t tcp_postgresql_acked(void *arg, struct tcp_pcb *tpcb, u16_t len);
static err_t tcp_postgresql_received(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static err_t tcp_postgresql_poll(void *arg, struct tcp_pcb *tpcb);
static void tcp_postgresql_send(struct tcp_pcb *tpcb, struct tcp_postgresql_struct *sql);
static void tcp_postgresql_error(void *arg, err_t err);
static void tcp_postgresql_connection_close(struct tcp_pcb *tpcb, struct tcp_postgresql_struct *sql);

void tcp_postgresql_senddata(struct tcp_pcb *tpcb, struct tcp_postgresql_struct *sql);

#ifndef un_I2cval
typedef union _un_l2cval {
	uint32_t	lVal;
	uint8_t		cVal[4];
}un_l2cval;
#endif

//uint8_t postgresqlc_run(uint8_t * dbuf);
//char proc_postgresqlc(char * buf);

#endif // _POSTGRESQL_H_
