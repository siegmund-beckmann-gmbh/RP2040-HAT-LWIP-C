#include "intercom.h"

#include "hexdump.h"

/* ============================================================================================ *
 * routines for CRC										*
 * Program-File											* 
 * ============================================================================================ */
 
const unsigned int crctab[256] = {	//polynom 16.12.5.1 0x8404
	0x0000, 0x1189, 0x2312, 0x329B, 0x4624, 0x57AD, 0x6536, 0x74BF,
	0x8C48, 0x9DC1, 0xAF5A, 0xBED3, 0xCA6C, 0xDBE5, 0xE97E, 0xF8F7,
	0x1081, 0x0108, 0x3393, 0x221A, 0x56A5, 0x472C, 0x75B7, 0x643E,
	0x9CC9, 0x8D40, 0xBFDB, 0xAE52, 0xDAED, 0xCB64, 0xF9FF, 0xE876,
	0x2102, 0x308B, 0x0210, 0x1399, 0x6726, 0x76AF, 0x4434, 0x55BD,
	0xAD4A, 0xBCC3, 0x8E58, 0x9FD1, 0xEB6E, 0xFAE7, 0xC87C, 0xD9F5,
	0x3183, 0x200A, 0x1291, 0x0318, 0x77A7, 0x662E, 0x54B5, 0x453C,
	0xBDCB, 0xAC42, 0x9ED9, 0x8F50, 0xFBEF, 0xEA66, 0xD8FD, 0xC974,
	0x4204, 0x538D, 0x6116, 0x709F, 0x0420, 0x15A9, 0x2732, 0x36BB,
	0xCE4C, 0xDFC5, 0xED5E, 0xFCD7, 0x8868, 0x99E1, 0xAB7A, 0xBAF3,
	0x5285, 0x430C, 0x7197, 0x601E, 0x14A1, 0x0528, 0x37B3, 0x263A,
	0xDECD, 0xCF44, 0xFDDF, 0xEC56, 0x98E9, 0x8960, 0xBBFB, 0xAA72,
	0x6306, 0x728F, 0x4014, 0x519D, 0x2522, 0x34AB, 0x0630, 0x17B9,
	0xEF4E, 0xFEC7, 0xCC5C, 0xDDD5, 0xA96A, 0xB8E3, 0x8A78, 0x9BF1,
	0x7387, 0x620E, 0x5095, 0x411C, 0x35A3, 0x242A, 0x16B1, 0x0738,
	0xFFCF, 0xEE46, 0xDCDD, 0xCD54, 0xB9EB, 0xA862, 0x9AF9, 0x8B70,
	0x8408, 0x9581, 0xA71A, 0xB693, 0xC22C, 0xD3A5, 0xE13E, 0xF0B7,
	0x0840, 0x19C9, 0x2B52, 0x3ADB, 0x4E64, 0x5FED, 0x6D76, 0x7CFF,
	0x9489, 0x8500, 0xB79B, 0xA612, 0xD2AD, 0xC324, 0xF1BF, 0xE036,
	0x18C1, 0x0948, 0x3BD3, 0x2A5A, 0x5EE5, 0x4F6C, 0x7DF7, 0x6C7E,
	0xA50A, 0xB483, 0x8618, 0x9791, 0xE32E, 0xF2A7, 0xC03C, 0xD1B5,
	0x2942, 0x38CB, 0x0A50, 0x1BD9, 0x6F66, 0x7EEF, 0x4C74, 0x5DFD,
	0xB58B, 0xA402, 0x9699, 0x8710, 0xF3AF, 0xE226, 0xD0BD, 0xC134,
	0x39C3, 0x284A, 0x1AD1, 0x0B58, 0x7FE7, 0x6E6E, 0x5CF5, 0x4D7C,
	0xC60C, 0xD785, 0xE51E, 0xF497, 0x8028, 0x91A1, 0xA33A, 0xB2B3,
	0x4A44, 0x5BCD, 0x6956, 0x78DF, 0x0C60, 0x1DE9, 0x2F72, 0x3EFB,
	0xD68D, 0xC704, 0xF59F, 0xE416, 0x90A9, 0x8120, 0xB3BB, 0xA232,
	0x5AC5, 0x4B4C, 0x79D7, 0x685E, 0x1CE1, 0x0D68, 0x3FF3, 0x2E7A,
	0xE70E, 0xF687, 0xC41C, 0xD595, 0xA12A, 0xB0A3, 0x8238, 0x93B1,
	0x6B46, 0x7ACF, 0x4854, 0x59DD, 0x2D62, 0x3CEB, 0x0E70, 0x1FF9,
	0xF78F, 0xE606, 0xD49D, 0xC514, 0xB1AB, 0xA022, 0x92B9, 0x8330,
	0x7BC7, 0x6A4E, 0x58D5, 0x495C, 0x3DE3, 0x2C6A, 0x1EF1, 0x0F78};

#define MAX_MESSAGE_RECS 64
ICOM_MESSAGE MessageList[MAX_MESSAGE_RECS];

const char Intercom_Header[15] = {'B','e','C','k','M','a','N','n','_','R','e','M','o','T','e'};

uint8_t IntercomPollCount  = 0;
uint8_t MsgListBufIn	   = 0;
uint8_t MsgListBufOut	   = 0;
uint8_t MsgBufTimeout	   = 0;
uint8_t MsgBufRepeat	   = 0;		

struct ip4_addr callerIP;

bool IntercomConnected = false;

/* Functions */

unsigned int CalcCRC(unsigned int StartVal, unsigned char *Buf, unsigned int len)
{
unsigned int j,crc;
unsigned char cp;

crc = StartVal;

  for (j=0;j<len;j++)
  {
   	crc = ((crc >> 8) ^ crctab[(crc ^ *Buf++) & 0xFF]);    	
  }

  return crc;
}


unsigned int CalcCRC2(unsigned char *Buf, unsigned long Buflen)
{
unsigned int crc;
unsigned char cp;
unsigned long j;

crc = 0;

  for (j=0;j<Buflen;j++)
  {
		      
   	crc = ((crc >> 8) ^ crctab[(crc ^ *Buf++) & 0xFF]);    	   	
  }

  return crc;
}


void udp_intercom_init(const struct ip4_addr *server_addr, uint16_t listenPort) 
{
    void *passed_data = NULL;
    upcb = udp_new();

    udp_recv(upcb, udp_intercom_received, passed_data);
    udp_bind(upcb, server_addr, listenPort);    

	MsgBufTimeout		= 0;
	MsgBufRepeat		= 0;					

}

static void udp_intercom_received(void *passed_data, struct udp_pcb *upcb, struct pbuf *p, const struct ip4_addr *addr, u16_t port) 
{    
 unsigned char *rxpointer,*txpointer,*crcpointer,*datapointer;
 uint16_t cmd, check, rxlen, txlen; 

 
    // Do something with the packet that was just received        

    if(p == NULL) return;

    printf("Received data ! len=%d ip=%s port=%u\r\n", p->len, ip4addr_ntoa(addr), port );    

    hexdump(p->payload, p->len, 16, 8);

	IntercomPollCount = 0;
	IntercomConnected = true;

	rxpointer = p->payload;
	txpointer = p->payload + sizeof(Intercom_Header);
		
	if (strncmp(rxpointer, &Intercom_Header[0], sizeof(Intercom_Header))==0)
	{								
		memcpy(&callerIP, addr, sizeof(struct ip4_addr)); // for Messsages
				
		rxpointer += sizeof(Intercom_Header);
				
		cmd = *(unaligned_ushort *)rxpointer;
		crcpointer=rxpointer;
		rxpointer+=2;
		
		while ((cmd != ICOM_COMMAND_END) && ((rxpointer - (unsigned char*)p->payload) < p->len))
		{
			rxlen = *(unaligned_ushort *)rxpointer;
			check = *(unaligned_ushort *)(rxpointer + rxlen + 2);			
			rxpointer+=2;  // Pointer auf Datenbeginn
			datapointer=rxpointer;

            //printf("ICOM_COMMAND = %04X LENGTH=%04X CRC=%04X\r\n", cmd, rxlen, check );    
			
			if (check==CalcCRC2(crcpointer,rxlen+4))
			{			
                printf("ICOM_COMMAND = %04X LENGTH=%04X CRC=%04X\r\n", cmd, rxlen, check );    

				crcpointer=txpointer;
				*(unaligned_ushort *)txpointer = cmd;
				txpointer+=4;
				//txlen spaeter
				
				switch(cmd)
				{
				case ICOM_COMMAND_SYSTEM_CONFIG:
					*(unaligned_ushort *)txpointer = 0x0000;				
					txpointer+=2;									
				break;
				case ICOM_COMMAND_WRITE_CONFIG:				
					*(unaligned_ushort *)txpointer = 0x0000;
					txpointer+=2;
				break;
								
				case ICOM_COMMAND_SET_TIME:
				
				case ICOM_COMMAND_GET_TIME:
				break;
                }

			} //CRC
			
			rxpointer+=rxlen+2;
			cmd = *(unaligned_ushort *)rxpointer;
			crcpointer=rxpointer;
			rxpointer+=2;				
		}
				
				
		if ((txpointer - (unsigned char*)p->payload - sizeof(Intercom_Header)) > 0)
		{
			*(unaligned_ushort *)txpointer = ICOM_COMMAND_END;
			txpointer+=2;
			
			//Rücksenden an Anfragenden			
			//uip_send(uip_appdata,(txpointer-uip_appdata));
            //udp_sendto(upcb, p, addr, port);
		}
	}	

			
	if (strncmp(rxpointer,"ReSeT",5)==0)
	{							
		if (strncmp(rxpointer+5,"NeWiP",5)==0)
		{

		}
	}

    pbuf_free(p);

}

static void udp_intercom_send(struct udp_pcb *upcb) 
{

}

/* MESSAGE INTERCOM SYSTEM PORT 1010*/

void intercomMessage_poll(void) {

uint32_t handle;
unsigned char *rxpointer,*txpointer,*crcpointer;
unsigned int cmd, check, rxlen, txlen, s;
unsigned long index;

uint8_t buf[256];

	/* Check Timeouts for Offers */
	if (++IntercomPollCount==10) {		
		IntercomConnected = false;		
		printf("InterCom Connection timed out!\r\n");
	}		

	if (IntercomConnected) 
	{
		// check for message in FIFO
		printf("InterCom Connection check for message...\r\n");

		if (MsgListBufIn != MsgListBufOut)
		{	
			txpointer = &buf[0];
			memcpy(txpointer, Intercom_Header,sizeof(Intercom_Header));
			txpointer += sizeof(Intercom_Header);

			if (!MsgBufTimeout)
			{				
				if ( (MessageList[MsgListBufOut].len<=MAX_MESSAGE_LEN) && MessageList[MsgListBufOut].status)
				{							
					//Message senden
					MsgBufTimeout = ICOM_TIMEOUT;

					if (MessageList[MsgListBufOut].status == 1) 
					{
						MsgBufRepeat=ICOM_SEND_REPEAT;					
						MessageList[MsgListBufOut].status = 2;
					}
								
					crcpointer=txpointer;
					*(unaligned_ushort *)txpointer = MessageList[MsgListBufOut].id;
					txpointer+=2;					
					*(unaligned_ushort *)txpointer = MessageList[MsgListBufOut].len + 4;
					txpointer+=2;		
					*(unsigned long *)txpointer = MessageList[MsgListBufOut].index;
					txpointer+=4;

					for (s=0;s<MessageList[MsgListBufOut].len;s++) *txpointer++= MessageList[MsgListBufOut].Data[s];
						
					txlen = txpointer-crcpointer;
					//*(unsigned int far*)(crcpointer+2) = txlen-4;
					*(unaligned_ushort *)txpointer = CalcCRC2(crcpointer,txlen);
					txpointer+=2;										

					printf("SENDING MESSAGE %04X Index=%08lX repeat=%u",MessageList[MsgListBufOut].id, MessageList[MsgListBufOut].index, MsgBufRepeat);
				}
				else 
				{
					printf("INVALID MESSAGE %04X",MessageList[MsgListBufOut].id);
					if (++MsgListBufOut>=MAX_MESSAGE_RECS) MsgListBufOut=0;	//discard !!!	
					MsgBufTimeout=0;
					return;
				}
			}
			else 
			{
				if (--MsgBufTimeout==0) 
				{
					if (MsgBufRepeat)
					{
						if (--MsgBufRepeat==0)
						{
							printf("DISCARDING MESSAGE %04X",MessageList[MsgListBufOut].id);
							if (++MsgListBufOut>=MAX_MESSAGE_RECS) MsgListBufOut=0;	//discard !!!	
							MsgBufTimeout=0;
						}
					}
				}

				return;
			} 
	
			*(unaligned_ushort *)txpointer = ICOM_COMMAND_END;
			txpointer+=2;		
			txlen=txpointer - buf;

			struct pbuf *p;    		
        	p = pbuf_alloc(PBUF_RAW,txlen,PBUF_RAM);

        	if (p != NULL) {
            	pbuf_take(p,buf,txlen);
				struct udp_pcb *mpcb = udp_new();
				udp_sendto(mpcb, p, &callerIP, INTERCOM_MESSAGE_PORT);
				pbuf_free(p);
			}

		} // queue is empty !
	} // not connected !
}


void addMessage(uint16_t id, uint16_t len, uint8_t* data){

}
