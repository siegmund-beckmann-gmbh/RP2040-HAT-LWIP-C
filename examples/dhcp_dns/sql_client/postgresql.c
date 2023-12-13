#include "postgresql.h"

#include "md5.h"
#include "hexdump.h"

//uint16_t  local_port;
static uint8_t connect_state_data_postgresql = 0;

static uint8_t SQL_destip[4] = {192, 168, 0, 82};	// For SQL client examples; destination network info
static uint16_t SQL_destport = 5432;				// For SQL client examples; destination network info

static char sqlDATABASE[]="postgres";
static char sqlUSER[]="bootloader";
static char sqlPASS[]="phoenixbootloader";
static bool gotSysinfo=false;
static bool listenLogs=false;
static bool gotLog=true;

/*-----------------------------------------------------------------------------------
uint16_t
htons(uint16_t val)
{
  return (((val & 0x00ff) << 8) | ((val >> 8) & 0x00ff) );
}
uint32_t
htonl(uint32_t n)
{
  return ((n & 0xff) << 24) |
    ((n & 0xff00) << 8) |
    ((n & 0xff0000) >> 8) |
    ((n & 0xff000000) >> 24);
}
*/

void md5_hex(char *hexdigest, unsigned char *text, unsigned int text_len, char *salt)
{
    unsigned char digest[20];
    int cnt;	
    MD5_CTX context;

    MD5Init(&context);
    MD5Update(&context, text, text_len);
    MD5Final(digest, &context);

    for(cnt=0; cnt<16; cnt++) sprintf(hexdigest + 2 * cnt, "%02x", digest[cnt]); 
	//jetzt salt dazu	
    for(cnt=0; cnt<4; cnt++) hexdigest[32+cnt]=salt[cnt];
	hexdigest[36]=0;
	
    MD5Init(&context);
    MD5Update(&context, hexdigest, 36);
    MD5Final(digest, &context);
	
    for(cnt=0; cnt<16; cnt++) sprintf(hexdigest + 2 * cnt, "%02x", digest[cnt]); 
}

/**
 * @brief  Initializes the tcp postgresql client
 * @param  None
 * @retval None
 */
void tcp_postgresql_init(ip_addr_t *server_addr)
{
    /* create new tcp pcb */
    tcp_postgresql_pcb = tcp_new();

    if (tcp_postgresql_pcb != NULL)
    {
        err_t err;

        err = tcp_connect(tcp_postgresql_pcb, server_addr, SQL_destport, tcp_postgresql_connected);
        if (err == ERR_OK)
        {            
            printf("[LWIP POSTGRESQL Client] connect to : [%d.%d.%d.%d]\n",  server_addr->addr & 0xFF, (server_addr->addr >> 8) & 0xFF, (server_addr->addr >> 16) & 0xFF, (server_addr->addr >> 24) & 0xFF);
        }
    }
    else
    {
        memp_free(MEMP_TCP_PCB, tcp_postgresql_pcb);
        tcp_postgresql_pcb = NULL;
    }
}

/**
 * @brief  This function is the implementation of tcp_connected LwIP callback
 * @param  arg: not used
 * @param  newpcb: pointer on tcp_pcb struct for the newly created tcp connection
 * @param  err: not used
 * @retval err_t: error status
 */
err_t tcp_postgresql_connected(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    err_t ret_err;
    struct tcp_postgresql_struct *sql;

    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(err);

    /* error when connect to the server */
    if (err != ERR_OK) {

        printf("[LWIP POSTGRESQL Client] connection ERROR !\n");
        return err;
    }
    
    /* set priority for the client pcb */
    tcp_setprio(newpcb, TCP_PRIO_NORMAL);

    /* allocate structure es to maintain tcp connection informations */
    sql = (struct tcp_postgresql_struct *)mem_malloc(sizeof(struct tcp_postgresql_struct));
    if (sql != NULL)
    {
        sql->state = SQL_NONE;
        sql->sentlen 	= sql->sendptr = sql->textlen = 0;
        sql->pcb = newpcb;
        sql->p = NULL;

        printf("[LWIP POSTGRESQL Client] connected !\n");

        /* pass newly allocated es structure as argument to newpcb */
        tcp_arg (newpcb, sql);

        /* initialize lwip tcp_err callback function for newpcb  */
        tcp_err (newpcb, tcp_postgresql_error);

        /* initialize lwip tcp_sent callback function for newpcb  */
        tcp_sent(newpcb, tcp_postgresql_acked);

        /* initialize lwip tcp_recv callback function for newpcb  */
        tcp_recv(newpcb, tcp_postgresql_received); 

        /* initialize lwip tcp_poll callback function for newpcb */
        tcp_poll(newpcb, tcp_postgresql_poll, 0); 

        sql->state = SQL_STARTUP_PACKET;
        tcp_postgresql_senddata(newpcb, sql);
        ret_err = ERR_OK;
    }
    else
    {
        printf("[LWIP POSTGRESQL Client] MEMORY error !\n");
        /*  close tcp connection */
        tcp_postgresql_connection_close(newpcb, sql);
        /* return memory error */
        ret_err = ERR_MEM;
    }
    
    return ret_err;
}

/**
 * @brief  This function is the implementation for tcp_recv LwIP callback
 * @param  arg: pointer on a argument for the tcp_pcb connection
 * @param  tpcb: pointer on the tcp_pcb connection
 * @param  p: pointer on the received pbuf
 * @param  err: error information regarding the reveived pbuf
 * @retval err_t: error code
 */
err_t tcp_postgresql_received(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    struct tcp_postgresql_struct *sql;
    err_t ret_err;

    LWIP_ASSERT("arg != NULL", arg != NULL);

    sql = (struct tcp_postgresql_struct *)arg;

    /* if we receive an empty tcp frame from client => close connection */
    if (p == NULL)
    {
        /* remote host closed connection */
        printf("Received empty frame.\n");
        sql->state = SQL_CLOSING;
        if (sql->p == NULL)
        {
            /* we're done sending, close connection */
            tcp_postgresql_connection_close(tpcb, sql);
        }

        ret_err = ERR_OK;
    }
    /* else : a non empty frame was received from client but for some reason err != ERR_OK */
    else if (err != ERR_OK)
    {
        /* free received pbuf*/
        printf("Ignoring data due to receive error.\n");
        if (p != NULL)
        {
            sql->p = NULL;
            pbuf_free(p);
        }
        ret_err = err;
    }
    else if (sql->state == SQL_CLOSING)
    {
        printf("Ignoring data due to connection closing.\n");

        tcp_recved(tpcb, p->tot_len);
        sql->p = NULL;
        pbuf_free(p);
        ret_err = ERR_OK;
    }
    else 
    {
        // Data available
        
        char *rxpointer,*cpos,*datapointer,*endp;
        char str[128];	
        uint32_t lval;
        uint32_t lgth,oid,tid,tmod;
        uint16_t cnt,tatt,fcod;
        struct sql_row_desc *rdesc;

		printf("Received data ! len=%d\r\nsql->len=%u\r\nsql->cmd=%c\r\nsql->sublen=%u\r\n", p->len, sql->len, sql->cmd, sql->sublen);
        
        rxpointer	= p->payload;
        endp = rxpointer+p->len;

        hexdump(rxpointer, p->len, 16, 8);

        while (rxpointer<endp)
        {		
            if (!sql->len && (sql->subcmd!=SQL_SUBCMD_DLEN))		//wenn keine Länge dann beginnt hier ein SQL CMD (len-4)
            {
                sql->cmd=*rxpointer++;					//erstes Kommando
                sql->sublen=4;
                sql->param=sql->lenbuf;
                sql->subcmd=SQL_SUBCMD_DLEN;
            }

            if(rxpointer>=endp) break;
                
            if(!sql->len && (sql->subcmd==SQL_SUBCMD_DLEN))
            {
                while(sql->sublen)
                {
                    if(rxpointer>=endp) break;
                    *sql->param++=*rxpointer++;
                    sql->sublen--;
                    if(!sql->sublen)
                    {
                        sql->len=sql->lenbuf[0] << 24;
                        sql->len+=sql->lenbuf[1] << 16;
                        sql->len+=sql->lenbuf[2] << 8;
                        sql->len+=sql->lenbuf[3];
                        sql->len-=4;

                        sql->subcmd=SQL_SUBCMD_UNDEFINED;
                        sql->start=1;							//start der eines Kommandos kennzeichen

                        printf("Command: %c length %u\r\n",sql->cmd, sql->len);
                    }
                }
            }

            if(rxpointer>=endp) break;		
            if(!sql->len) break;

            datapointer=rxpointer;
                                                                            
            switch(sql->cmd)
            {			
            case '1':	//parse complete
            break;
            case '2':	//bind complete
            break;
            case '3':	//Close complete
            break;
            case 'c':	//Copy Done
            break;
            case 'A':	//Asynchronous Notifications
                if (sql->start)
                {
                    sql->subcmd=SQL_SUBCMD_PID;
                    sql->sublen=4;
                    sql->param=sql->lenbuf;
                    sql->start=0;
                }

                while((sql->subcmd==SQL_SUBCMD_PID) && sql->sublen)
                {
                    if(datapointer>=endp) break;
                    *sql->param++=*datapointer++;
                    sql->sublen--;
                    if(!sql->sublen)
                    {
                        sql->PID=sql->lenbuf[0] << 24;
                        sql->PID+=sql->lenbuf[1] << 16;
                        sql->PID+=sql->lenbuf[2] << 8;
                        sql->PID+=sql->lenbuf[3];

                        sql->subcmd=SQL_SUBCMD_CHANNEL;
                        sql->postptr=sql->NotifyBuf;			//alle Notifications Daten dort speichern
                        sql->n_channel=sql->postptr;			//Channel Pointer sichern
                    }
                }
                if(sql->subcmd==SQL_SUBCMD_CHANNEL)
                {
                    while(datapointer<endp)		//Block kopieren
                    {
                        if ((sql->postptr-sql->NotifyBuf)<NOTIFYBUFFERSIZE) 
                            *sql->postptr++=*datapointer++;
                        else 
                            datapointer++;
                        if(!*datapointer) break;
                    }
                    
                    if(!*datapointer) 
                    {
                        sql->subcmd=SQL_SUBCMD_STRING;
                        *sql->postptr++=*datapointer++;
                        sql->n_payload=sql->postptr;			//Payload Pointer sichern
                    }
                    if(datapointer>=endp) break;	
                }				
                if(sql->subcmd==SQL_SUBCMD_STRING)
                {
                    while(datapointer<endp)		//Block kopieren
                    {
                        if ((sql->postptr-sql->NotifyBuf)<NOTIFYBUFFERSIZE) 
                            *sql->postptr++=*datapointer++;
                        else 
                            datapointer++;
                        if(!*datapointer) break;	//nächstes Zeichen 0
                    }
                    if (!*datapointer)
                    {
                        *sql->postptr++=*datapointer++;	//null mitkopieren
                        printf("Notification: %s Payload: %s \r\n",sql->NotifyBuf, sql->n_payload);
                        gotLog=false;
                        //SQL_Notification(sql, CurrentSqlQuery);
                    }
                    if(datapointer>=endp) break;	
                }
            break;
            case 'C':	//Command complete
                if (sql->start)
                {
                    sql->start=0;
                    sql->subcmd=SQL_SUBCMD_CSTRING;
                    sql->cmdlen=sql->len;
                    sql->completeptr=sql->CompleteBuf;
                }

                if (sql->subcmd==SQL_SUBCMD_CSTRING) 
                {
                    while (sql->cmdlen)
                    {
                        if (datapointer>=endp) break;
                        if ((sql->completeptr-sql->CompleteBuf)<COMPLETEBUFFERSIZE) 
                            *sql->completeptr++=*datapointer++;
                        else datapointer++;
                        sql->cmdlen--;
                        if (!sql->cmdlen) {
                            sql->subcmd=SQL_SUBCMD_UNDEFINED;
                            *sql->completeptr++=0;
                            printf("%s\r\n",sql->CompleteBuf);
                        }
                    }
                }
            break;
            case 'D':	//Data Row
                if (sql->start)
                {
                    sql->cols=0;                   
                    sql->start=0;
                    sql->subcmd=SQL_SUBCMD_COLS;
                    sql->sublen=2;
                    sql->param=sql->lenbuf;
                    sql->postptr=sql->PostBuf;
                }

                while((sql->subcmd==SQL_SUBCMD_COLS) && sql->sublen) 
                {
                    if (datapointer>=endp) break;
                    *sql->param++=*datapointer++;
                    sql->sublen--;
                    if (!sql->sublen) {
                        sql->cols=sql->lenbuf[0] << 8;
                        sql->cols+=sql->lenbuf[1];
                        sql->subcmd=SQL_SUBCMD_LENGTH;
                        sql->sublen=4;
                        sql->param=sql->lenbuf;
                        sql->postptr=sql->PostBuf;

                        printf("Spaltenanzahl: %u\r\n",sql->cols);
                    }
                }
                
                while((sql->subcmd!=SQL_SUBCMD_COLS) && sql->cols && (datapointer<endp))
                {                            
                    switch(sql->subcmd)
                    {
                    case SQL_SUBCMD_LENGTH:
                        while(sql->sublen) {
                            if (datapointer>=endp) break;
                            *sql->param++=*datapointer++;
                            sql->sublen--;
                        }

                        if (datapointer<endp)
                        {						
                            sql->sublen=sql->lenbuf[0] << 24;
                            sql->sublen+=sql->lenbuf[1] << 16;
                            sql->sublen+=sql->lenbuf[2] << 8;
                            sql->sublen+=sql->lenbuf[3];
                            if (sql->sublen==0xFFFFFFFF) sql->sublen=0;
                            sql->subcmd=SQL_SUBCMD_STRING;
                        }
                    break;									
                    case SQL_SUBCMD_STRING:
                        while(sql->sublen ) 
                        {
                            if (datapointer>=endp) break;                                    
                            if ((sql->postptr-sql->PostBuf)<POSTBUFFERSIZE) 
                                *sql->postptr++=*datapointer++;
                            else datapointer++;
                            sql->sublen--;
                        }
                        
                        if (datapointer<endp)
                        {	
                            //Trennzeichen
                            //*sql->postptr++='\r';
                            //*sql->postptr++='\n';
                            //*sql->postptr++='\r';
                            //*sql->postptr++='\n';

                            sql->subcmd=SQL_SUBCMD_LENGTH;
                            sql->sublen=4;
                            sql->param=sql->lenbuf;
                            sql->cols--;

                            if ((sql->postptr-sql->PostBuf)<POSTBUFFERSIZE) *sql->postptr++='\n';
                        }
                    break;
                    }
                }
                
                if (sql->cols==0)
                {
                    if ((sql->postptr-sql->PostBuf)<POSTBUFFERSIZE) *sql->postptr++=0;
                    //Daten komplett ---> Application EVENT

                    printf("%s\r\n",sql->PostBuf);
                }
            
            break;
            case 'E':	//Error			
            case 'N':	//Notice Response
                if (sql->start)
                {
                    if (sql->cmd=='E') 
                    {
                        sql->state=SQL_ERROR; // nicht bei Notice
                        printf("ERROR!:\r\n");
                    }
                    else printf("NOTICE:\r\n");
                    sql->start=0;
                    sql->subcmd=SQL_SUBCMD_TYPE;				
                    sql->ecode=*datapointer;
                }
                
                while (sql->ecode && (datapointer<endp))
                {				
                    //The message body consists of one or more identified fields, followed by a zero byte as a terminator. 
                    //Fields can appear in any order. For each field there is the following:

                    //Byte1     A code identifying the field type; if zero, this is the message terminator and no string follows. 
                    //			The presently defined field types are listed in Section 46.6. Since more field types might be added in future, frontends should silently ignore fields of unrecognized type.
                    //String    The field value.
                    
                    switch(sql->subcmd)
                    {
                    case SQL_SUBCMD_TYPE:
                        sql->ecode=*datapointer++;
                        sql->subcmd=SQL_SUBCMD_STRING;
                        sql->postptr=sql->PostBuf;
                    break;
                    case SQL_SUBCMD_STRING:
                        while(*datapointer && (datapointer<endp)) 
                        {
                            if ((sql->postptr-sql->PostBuf) < POSTBUFFERSIZE) 
                                *sql->postptr++=*datapointer++;
                            else datapointer++;
                        }
                        if (datapointer<endp)
                            sql->subcmd=SQL_SUBCMD_TYPE;
                        {
                            datapointer++;
                            *sql->postptr++=0;
                            
                            switch(sql->ecode)
                            {
                            case 'S':
                            printf("Severity: %s\r\n",sql->PostBuf);
                            break;
                            case 'C':
                            printf("Code: %s\r\n",sql->PostBuf);
                            break;
                            case 'M':
                            printf("Message: %s\r\n",sql->PostBuf);
                            break;
                            case 'P':
                            printf("Position: %s\r\n",sql->PostBuf);
                            break;
                            case 'F':
                            printf("File: %s\r\n",sql->PostBuf);
                            break;
                            case 'L':
                            printf("Line: %s\r\n",sql->PostBuf);
                            break;
                            case 'R':
                            printf("Routine: %s\r\n",sql->PostBuf);
                            break;
                            default:
                            printf("unknown: %s\r\n",sql->PostBuf);
                            break;
                            }				
                        }									
                    break;
                    }												
                }
                
            break;
            case 'f':	//Copy Fail
            break;
            case 'G':	//Copy IN Response
            break;
            case 'H':	//Copy OUT Response
            break;
            case 'I':	//Empty Query Response
            break;
            case 'n':	//No data Response
            break;
            case 'K':	//Backend-Data PID + key
                sql->PID=*datapointer++ << 24;
                sql->PID+=*datapointer++ << 16;
                sql->PID+=*datapointer++ << 8;
                sql->PID+=*datapointer++;

                sql->key=*datapointer++ << 24;
                sql->key+=*datapointer++ << 16;
                sql->key+=*datapointer++ << 8;
                sql->key+=*datapointer++;
                printf("PID=%u KEY=%u\r\n",sql->PID,sql->key);

            break;
            case 'p':	//Password Message
            break;
            case 'R':	//Authentication request
                lval= *datapointer << 24;
                lval+=*(datapointer+1) << 16;
                lval+=*(datapointer+2) << 8;
                lval+=*(datapointer+3);

                printf("lval=%u\r\n",lval);
                
                switch(sql->state)
                {
                case SQL_STARTUP_PACKET_SENT:
                    if (lval==5)
                    {						
                        sql->auth=lval;
                        datapointer+=4;
                        for(cnt=0; cnt<4; cnt++) sql->salt[cnt]=*datapointer++;
                        sql->state=SQL_AUTH_MD5;
                    }
                    else if (lval==10) {
                        sql->auth=lval;
                        datapointer+=4;                      
                        if (memcmp(datapointer,"SCRAM-SHA-256",13)==0)          
                        {
                            sql->state=SQL_AUTH_SCRAMSHA256;
                        }
                    }
                break;
                case SQL_AUTH_MD5_SENT:
                    if (lval==SQLAuthenticationOk) sql->state=SQL_CONNECTED;
                break;
                }
            break;
            case 'S':	//Parameter status
                cpos=datapointer;
                while(*cpos++ && (cpos<endp));*(cpos-1)='=';
                printf(datapointer);
                printf("\r\n");
                
                //Param = getParamNo(datapointer, (char far**)&SQLParams[0],SQLPARAMCOUNT,0, 0);
                
            break;
            case 't':	//Parameter Description
            break;
            case 'T':	//Row Description
                if (sql->start)
                {
                    sql->start=0;
                    sql->subcmd=SQL_SUBCMD_COLS;
                    sql->sublen=2;
                    sql->param=sql->lenbuf;
                    
                }

                while((sql->subcmd==SQL_SUBCMD_COLS) && sql->sublen) 
                {
                    if (datapointer>=endp) break;
                    *sql->param++=*datapointer++;
                    sql->sublen--;
                    if (!sql->sublen) {
                        sql->cols=sql->lenbuf[0] << 8;
                        sql->cols+=sql->lenbuf[1];
                        sql->count=sql->cols;
                        sql->subcmd=SQL_SUBCMD_STRING;
                        sql->param=sql->PostBuf;
                        sql->postptr=sql->PostBuf;

                        printf("Spaltenanzahl: %u\r\n",sql->cols);

                        memset((char*)table_desc, 0x00, sizeof(table_desc)); // Ergebnisse löschen
                    }
                }
                                        
                while((sql->subcmd!=SQL_SUBCMD_COLS) && sql->cols && (datapointer<endp))
                {				
                    switch(sql->subcmd)
                    {
                    case SQL_SUBCMD_STRING:
                        while(*datapointer && (datapointer<endp)) 
                        {
                            if ((sql->postptr-sql->PostBuf)<POSTBUFFERSIZE) 
                                *sql->postptr++=*datapointer++;
                            else datapointer++;
                        }
                        
                        if (datapointer<endp)
                        {
                            if ((sql->postptr-sql->PostBuf)<POSTBUFFERSIZE) 
                                *sql->postptr++=*datapointer++;
                            else datapointer++;
                            sql->pval=sql->postptr;
                            sql->subcmd=SQL_SUBCMD_PARBLOCK;
                            sql->sublen=sizeof(struct sql_row_desc);
                        }
                    break;
                    case SQL_SUBCMD_PARBLOCK:
                        while(sql->sublen && (datapointer<endp))
                        {
                            sql->sublen--;
                            if ((sql->postptr-sql->PostBuf)<POSTBUFFERSIZE) 
                                *sql->postptr++=*datapointer++;
                            else datapointer++;
                        }
                        
                        if (datapointer<endp)
                        {		                                    
                            rdesc=(struct sql_row_desc *)sql->pval;
                            #if (SQL_SAVENAMES==1)
                            strcpy(table_desc[sql->count-sql->cols].name,sql->param);	// Spaltenname
                            #endif
                            table_desc[sql->count-sql->cols].index=htons(rdesc->tatt);			// Spaltenindex
                            table_desc[sql->count-sql->cols].size=htons(rdesc->size);			// Breite in bytes
                            table_desc[sql->count-sql->cols].oid=htonl(rdesc->oid);
                            
                            printf("name: %-24s idx: %2u size: %5u OID: %8u\r\n", 
                                    table_desc[sql->count-sql->cols].name,
                                    table_desc[sql->count-sql->cols].index,
                                    table_desc[sql->count-sql->cols].size,
                                    table_desc[sql->count-sql->cols].oid);

                            sql->subcmd=SQL_SUBCMD_STRING;
                            sql->cols--;
                            sql->sublen=0;
                            sql->param=sql->PostBuf;
                            sql->postptr=sql->PostBuf;
                        }
                    break;
                    }								
                }
                
            break;
            case 'V':	//Function Call Response
            break;
            case 'W':	//Copy Both Response
            break;
            case 'Z':	//Ready for Query (IDLE)
                if (*datapointer=='I')
                {
                    sql->state=SQL_READY_FOR_QUERY;
                    sql->pollcount	= 0;
                    printf("Ready for query!\r\n");
                }
            break;
            }

            //printf("END: sql->len=%u rxpointer=%u endp=%u datapointer=%u \r\n",sql->len,rxpointer,endp,datapointer);

            if (sql->len>(endp-rxpointer))	//nicht vollständig
            {
                sql->len-=(datapointer-rxpointer);
                rxpointer=endp;
                printf("Data missing !!!\r\n");
            }					 
            else 
            {
                rxpointer+=sql->len;
                sql->len=0;
            }                                        
        }

        tcp_recved(tpcb, p->len);
        sql->p = NULL;
        pbuf_free(p);        
        ret_err = ERR_OK;
        
        if (!sql->len) tcp_postgresql_senddata(tpcb,sql);
    }

    return ret_err;
}

/**
 * @brief  This function implements the tcp_sent LwIP callback (called when ACK
 *         is received from remote host for sent data)
 * @param  arg: pointer on a argument for the tcp_pcb connection
 * @param  tpcb: pointer on the tcp_pcb connection
 * @param  len: Length of the transferred data
 * @retval None
 */
err_t tcp_postgresql_acked(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    struct tcp_postgresql_struct *sql;

    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(tpcb);
    LWIP_UNUSED_ARG(len);

    sql = (struct tcp_postgresql_struct *)arg;

    sql->sendptr += len;
    
    if(sql->sendptr >= sql->textlen) sql->sendptr = sql->textlen = 0;
    
    switch(sql->state)
    {
        case SQL_STARTUP_PACKET:
            sql->state=SQL_STARTUP_PACKET_SENT;
            break;
        case SQL_AUTH_MD5:
            sql->state=SQL_AUTH_MD5_SENT;
            break;
        //case SQL_PENDING_QUERY:	
        case SQL_ACTIVE_QUERY:	
            sql->state=SQL_QUERY_ACKED;
            break;
    }	

    tcp_postgresql_senddata(tpcb,sql);

    return ERR_OK;
}

/**
 * @brief  This function is used to send data for tcp connection
 * @param  tpcb: pointer on the tcp_pcb connection
 * @param  sql: pointer on postgresql_state structure
 * @retval None
 */
void tcp_postgresql_send(struct tcp_pcb *tpcb, struct tcp_postgresql_struct *sql)
{
    struct pbuf *ptr;
    err_t wr_err = ERR_OK;

    while ((wr_err == ERR_OK) &&
           (sql->p != NULL) &&
           (sql->p->len <= tcp_sndbuf(tpcb)))
    {        

        /* get pointer on pbuf from sql structure */
        ptr = sql->p;

        printf("TCP_WRITE:\n");
        hexdump(ptr->payload, ptr->len, 16, 8);    

        /* enqueue data for transmission */
        wr_err = tcp_write(tpcb, ptr->payload, ptr->len, TCP_WRITE_FLAG_COPY);

        if (wr_err == ERR_OK)
        {
            u16_t plen;
            u8_t freed;

            plen = ptr->len;

            /* continue with next pbuf in chain (if any) */
            sql->p = ptr->next;

            if (sql->p != NULL)
            {
                /* increment reference count for sql->p */
                pbuf_ref(sql->p);
            }

            /* chop first pbuf from chain */
            do
            {
                /* try hard to free pbuf */
                freed = pbuf_free(ptr);
            } while (freed == 0);
        }
        else if (wr_err == ERR_MEM)
        {
            /* we are low on memory, try later / harder, defer to poll */
            sql->p = ptr;
        }
    }
}

/**
 * @brief  This function implements the tcp_err callback function (called
 *         when a fatal tcp_connection error occurs.
 * @param  arg: pointer on argument parameter
 * @param  err: not used
 * @retval None
 */
void tcp_postgresql_error(void *arg, err_t err)
{
    struct tcp_postgresql_struct *sql;

    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(err);

    sql = (struct tcp_postgresql_struct *)arg;
    if (sql != NULL)
    {
        /*  free sql structure */
        printf("[LWIP POSTGRESQL Client] connection ERROR !\n");
        mem_free(sql);
    }
}

/**
 * @brief  This function implements the tcp_poll LwIP callback function
 * @param  arg: pointer on argument passed to callback
 * @param  tpcb: pointer on the tcp_pcb for the current tcp connection
 * @retval err_t: error code
 */
err_t tcp_postgresql_poll(void *arg, struct tcp_pcb *tpcb)
{
    err_t ret_err;
    struct tcp_postgresql_struct *sql;
    char str[128];

    sql = (struct tcp_postgresql_struct *)arg;
    if (sql != NULL)
    {
        if (sql->p != NULL)
        {
            tcp_sent(tpcb, tcp_postgresql_acked); // set callback function
            /* there is a remaining pbuf (chain) , try to send data */
            printf("TCP_POLL send:\n");
            tcp_postgresql_send(tpcb, sql);
        }
        else
        {
            /* no remaining pbuf (chain)  */
            if (sql->state == SQL_CLOSING)
            {
                /*  close tcp connection */
                tcp_postgresql_connection_close(tpcb, sql);
            }
            else 
            {
                switch (sql->state) {
                    case SQL_READY_FOR_QUERY:
                        if (!gotSysinfo) {
                            sql->query="SELECT * FROM sysinfo;";
                            sql->state=SQL_PENDING_QUERY;
                            gotSysinfo=true;
                        }
                        else
                        if (!listenLogs) {
                            sql->query="LISTEN logs_insert;";
                            sql->state=SQL_PENDING_QUERY;
                            listenLogs=true;
                        }
                        else 
                        if (!gotLog) {
                            sprintf(str,"SELECT * FROM logs WHERE id='%s';",sql->n_payload);
                            sql->query=str;
                            sql->state=SQL_PENDING_QUERY;
                            gotLog=true;
                        }
                    break;
                }

                tcp_postgresql_senddata(tpcb,sql);
            }
        }

        ret_err = ERR_OK;
    }
    else
    {
        /* nothing to be done */
        tcp_abort(tpcb);
        ret_err = ERR_ABRT;
    }
    return ret_err;
}

/*
            switch (sql->state) {
                case SQL_READY_FOR_QUERY:
                    if (!gotSysinfo) {
                        sql->query="SELECT * FROM sysinfo;";
                        sql->state=SQL_PENDING_QUERY;
                        proc_postgresqlc((char *)dbuf);
                        gotSysinfo=true;
                    }
                    else
                    if (!listenLogs) {
                        sql->query="LISTEN logs_insert;";
                        sql->state=SQL_PENDING_QUERY;
                        proc_postgresqlc((char *)dbuf);
                        listenLogs=true;
                    }
                    else 
                    if (!gotLog) {
                        sprintf(str,"SELECT * FROM logs WHERE id='%s';",sql->n_payload);
                        sql->query=str;
                        sql->state=SQL_PENDING_QUERY;
                        proc_postgresqlc((char *)dbuf);
                        gotLog=true;
                    }
                break;
            }

*/


/**
 * @brief  This functions sends the packets depending on the status
 * @param  tcp_pcb: pointer on the tcp connection
 * @param  sql: pointer on postgresql_state structure
 * @retval None
 */
void tcp_postgresql_senddata(struct tcp_pcb *tpcb, struct tcp_postgresql_struct *sql)
{
    char *textptr;
    char * buf;
    unsigned int challengelen,len;
    unsigned char hexdigest[41];
    char str[128];
    unsigned int maxlen;
	
	// maxlen = tcp_mss(tpcb)-10;
    maxlen = tcp_sndbuf(tpcb)-10;

    if(sql->textlen && (sql->textlen == sql->sendptr)) return;

    buf = sql->PostBuf;
    textptr = buf;

    switch(sql->state)
    {
        case SQL_STARTUP_PACKET:
			printf("Sending startup packet > \r\n");
			textptr+=4;
			*(uint32_t *)textptr=htonl(0x00030000);
			textptr+=4;
			textptr+=sprintf(textptr,"user")+1;
			textptr+=sprintf(textptr,(char *)sqlUSER)+1;
			textptr+=sprintf(textptr,"database")+1;
			textptr+=sprintf(textptr,(char *)sqlDATABASE)+1;
			textptr+=sprintf(textptr,"application_name")+1;
			textptr+=sprintf(textptr,"EMS-ENTRY-PICO")+1;
			*textptr++=0;
			len=textptr-buf;	
            buf[0]=(len >> 24) & 0xff;
            buf[1]=(len >> 16) & 0xff;
            buf[2]=(len >> 8) & 0xff;
            buf[3]= len & 0xff;
            sql->state = SQL_STARTUP_PACKET_SENT;
        break;

        case SQL_AUTH_MD5:        
            printf("Sending password auth md5 > \r\n");
            challengelen=sprintf(str,"%s%s",(char *)sqlPASS,(char *)sqlUSER);
            hexdigest[32]=0;
            md5_hex((char *)hexdigest, str, challengelen,(char *)sql->salt);
    
            *textptr='p';
            textptr+=5;
            textptr+=sprintf(textptr,"md5%s",(char *)hexdigest)+1;
            len=textptr-buf-1;	            
            buf[1]=(len >> 24) & 0xff;
            buf[2]=(len >> 16) & 0xff;
            buf[3]=(len >> 8) & 0xff;
            buf[4]= len & 0xff;
            sql->state = SQL_AUTH_MD5_SENT;
        break;
        
        case SQL_PENDING_QUERY:	
            *textptr='Q';
            textptr+=5;
            textptr+=sprintf(textptr,"%s",sql->query)+1;
            len=textptr-buf-1;	
            buf[1]=(len >> 24) & 0xff;
            buf[2]=(len >> 16) & 0xff;
            buf[3]=(len >> 8) & 0xff;
            buf[4]= len & 0xff;
            sql->state=SQL_ACTIVE_QUERY;
        break;
        /*
        case SQL_QUERY_ACKED:
            printf("Sending remaining data after query acked > \r\n");
            sql->msendptr += sql->msentlen;
            sql->msentlen = 0;	
            if(sql->msendptr >= sql->mtextlen) 		//gesamtl?nge erreicht ?
            {
                sql->msendptr = sql->mtextlen = 0;
            }		
            goto next_sql_part;
        case SQL_PENDING_QUERY:			//neue Anfrage
            printf("Sending pending query > \r\n");
        case SQL_ACTIVE_QUERY:				
            printf("Sending active query > \r\n");
            sql->msentlen = 0;		//schon gesendet
            sql->mtextlen = 0;
            sql->msendptr = 0;
            *textptr='Q';
            textptr+=5;
            sql->mtextlen = strlen(sql->query)+1;		//ganze Query l?nge
            len = sql->mtextlen + 4;					//mit 0 am Ende
            buf[1]=(len >> 24) & 0xff;
            buf[2]=(len >> 16) & 0xff;
            buf[3]=(len >> 8) & 0xff;
            buf[4]= len & 0xff;
        next_sql_part:
            if(sql->mtextlen)
            {
                if((sql->mtextlen-sql->msendptr) < maxlen) 	maxlen = sql->mtextlen-sql->msendptr;		//wenn rest
                memcpy(textptr,sql->query+sql->msendptr,maxlen);
                sql->msentlen = maxlen;			//teilpaket
                textptr += sql->msentlen;
                sql->state=SQL_ACTIVE_QUERY;
            }
        break;        
        */
    }
    
    *textptr = 0;

	if(sql->sendptr == 0) sql->textlen = textptr-buf;

  	textptr = buf;
  	textptr += sql->sendptr;


  	if((sql->textlen - sql->sendptr)> tcp_sndbuf(tpcb)) 
    	 sql->sentlen = tcp_sndbuf(tpcb);
  	else sql->sentlen = sql->textlen - sql->sendptr;

	if (sql->sentlen) {
        sql->len=0;
        
        sql->p = pbuf_alloc(PBUF_RAW,sql->sentlen,PBUF_RAM);

        if (sql->p != NULL) {
            pbuf_take(sql->p,buf,sql->sentlen);
            printf("TCP_SENDDATA:\n");
            tcp_postgresql_send(tpcb,sql);
            sql->len=0;
        }
    }
}

/**
 * @brief  This functions closes the tcp connection
 * @param  tcp_pcb: pointer on the tcp connection
 * @param  sql: pointer on postgresql_state structure
 * @retval None
 */
void tcp_postgresql_connection_close(struct tcp_pcb *tpcb, struct tcp_postgresql_struct *sql)
{
    /* remove all callbacks */
    tcp_arg(tpcb, NULL);
    tcp_sent(tpcb, NULL);
    tcp_recv(tpcb, NULL);
    tcp_err(tpcb, NULL);
    tcp_poll(tpcb, NULL, 0);

    /* delete es structure */
    if (sql != NULL)
    {
        printf("[LWIP POSTGRESQL Client] connection CLOSED !\n");
        mem_free(sql);
    }

    /* close tcp connection */
    tcp_close(tpcb);
}
