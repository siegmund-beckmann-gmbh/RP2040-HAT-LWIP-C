#include "intercom.h"

#include "hexdump.h"

const APP MyApplication={
    "APF",
    "Phoenix-RemoteII",
    "Ver 1.00 07.11.2024",
    100    
};

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

uint16_t indexCounter = 0;

struct ip4_addr callerIP;

bool IntercomConnected = false;

bool crc_error = false;

bool AusgabeLeuchte = false;
bool DirektEintritt = false;
bool EintrittSignal = false;

pico_unique_board_id_t SystemID;

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


void udp_intercom_init() 
{
    void *passed_data = NULL;
    upcb = udp_new();
	mpcb = udp_new();

    udp_recv(upcb, udp_intercom_received, passed_data);
    udp_bind(upcb, IP4_ADDR_ANY, INTERCOM_CLIENT_PORT);    
	
	udp_recv(mpcb, udp_message_received, passed_data);
	udp_bind(mpcb, IP4_ADDR_ANY, INTERCOM_MESSAGE_PORT);    

	for (int cl=0;cl<MAX_MESSAGE_RECS;cl++)
	{
		MessageList[cl].status=0;
		MessageList[cl].id=ICOM_COMMAND_END;
		MessageList[cl].index=0;
	}

	MsgBufTimeout		= 0;
	MsgBufRepeat		= 0;
	MsgListBufIn		= 0;
	MsgListBufOut		= 0;

}

static void udp_intercom_received(void *passed_data, struct udp_pcb *upcb, struct pbuf *p, const struct ip4_addr *addr, u16_t port) 
{    
 unsigned char *rxpointer,*txpointer,*crcpointer,*datapointer;

 uint16_t cmd, ccmd, check, rxlen, txlen, s,taglen,count,k, OpenOptions;
 uint8_t cf_byte,tag,testmode,ani,aniadr,chn,ectype,Derr;
 uint32_t amount, UUID0,UUID1,UUID2,UUID3;
 char str[64];
 uint8_t by1,by2,by3;
 uint16_t h,i;
 
    // Do something with the packet that was just received        

    if(p == NULL) return;

    printf("ICOM_PORT Received data ! len=%d ip=%s port=%u\r\n", p->len, ip4addr_ntoa(addr), port );    

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

				case ICOM_COMMAND_BILL_STATUS:
					// Banknoten	
					if (!crc_error) 
					{
						for (s=0;s<MDB_MAXNOTES;s++) 
						{
							*(unaligned_ushort *)txpointer=SysVar.Bill[s].Count;
							txpointer+=2;
						}				
					}
					else *(unaligned_ushort *)(txpointer-4) = 0xFFFF;
				break;
				case ICOM_COMMAND_COIN_STATUS:
					if (!crc_error) 
					{				
						for (s=0;s<MDB_MAXCOINS;s++) 
						{
						  	for (k=0;k<MDB_MAXCOINS;k++)
							 if (MDB_Changer1.PhysTubes[k]==s) break;
						 
							if (k<MDB_MAXCOINS) 
							{
								if (SysVar.Tube[k].Deroute)			
								{	
									if (SysVar.Tube[k].Deroute<=MAX_HOPPER)
									 	 *(unaligned_ushort *)txpointer=SysVar.Hopper[SysVar.Tube[k].Deroute-1].Fill;
									else *(unaligned_ushort *)txpointer=0;
								}
								else *(unaligned_ushort *)txpointer=MDB_Changer1.TubeStatus[s];
							}
							else *(unaligned_ushort *)txpointer=MDB_Changer1.TubeStatus[s];
						
							txpointer+=2;									
						}				
					
						if (MDB_Changer1.Problem==6) // cassette removed
						     *(unaligned_ushort *)txpointer++=1;
						else *(unaligned_ushort *)txpointer++=0;						
					}
					else *(unaligned_ushort *)(txpointer-4) = 0xFFFF;										
				break;
				case ICOM_COMMAND_TUBE_STATUS:				
					for (s=0;s<MDB_MAXCOINS;s++) 
					{
					  	for (k=0;k<MDB_MAXCOINS;k++)
						 if (MDB_Changer1.PhysTubes[k]==s) break;
						 
						if (k<MDB_MAXCOINS) 
						{
							if (SysVar.Tube[k].Deroute)			
							{	
								if (SysVar.Tube[k].Deroute<=MAX_HOPPER)
								 	 *(unaligned_ushort *)txpointer=SysVar.Hopper[SysVar.Tube[k].Deroute-1].Fill;
								else *(unaligned_ushort *)txpointer=0;
							}
							else *(unaligned_ushort *)txpointer=SysVar.Tube[s].Fill;							
						}
						else *(unaligned_ushort *)txpointer=SysVar.Tube[s].Fill;
						
						txpointer+=2;									
					}				
				
				break;
				case ICOM_COMMAND_HOPPER_STATUS:				
					if (!crc_error) 
					{
						for (s=0;s<MAX_HOPPER;s++) 
						{
							*(unaligned_ushort *)txpointer=SysVar.Hopper[s].Fill;
							txpointer+=2;									
						}				
					
						for (s=0;s<MAX_HOPPER;s++) 					
						{
							if (SysVar.Hopper[s].Blocked)
								 *txpointer++=(SysVar.Hopper[s].Status | 0x80);
							else *txpointer++=SysVar.Hopper[s].Status;
						}
					}
					else *(unaligned_ushort *)(txpointer-4) = 0xFFFF;
					
				break;
				
				case ICOM_COMMAND_CASHBOX_STATUS:				
					if (!crc_error) 
					{
						for (s=0;s<MDB_MAXCOINS;s++) 
						{
							*(unaligned_ushort *)txpointer=SysVar.Coin[s].Count;
							txpointer+=2;									
						}								
					}
					else *(unaligned_ushort *)(txpointer-4) = 0xFFFF;
				break;

				case ICOM_COMMAND_CLEAR_HOPPER:
					*(unaligned_ushort *)txpointer = 0x0000;
					
					chn=*(unsigned char *)rxpointer;

					if (!SysVar.Hopper[(chn & 0x0F)].Blocked)
					{
						MDB_Changer2.Payout = chn;
						MDB_Changer2.NewRequest = CmdChanger_ExpPayout;
						MDB_Changer2.NewSequence=0;
						SysVar.Hopper[chn].Ready=0;
					}
					else *(unaligned_ushort *)txpointer = 0xFFFF;
															
					txpointer+=2;
					
					//CalcCoinCRC();
				break;
				
				case ICOM_COMMAND_CHANGER_TOTAL:
					DispenseAmount(10,1); 	// MDB_ChangerTotal berechnen
					*(unaligned_ulong *)txpointer =MDB_ChangerTotal;
					txpointer+=4;
					*(unaligned_ulong *)txpointer =MDB_Changer1.ChangerAmount;
					txpointer+=4;
					*(unaligned_ulong *)txpointer =MDB_Changer2.ChangerAmount;
					txpointer+=4;
				break;
								
				case ICOM_COMMAND_GET_TUBE_ROUTING:
					for (s=0;s<MDB_MAXCOINS;s++)
					{
					  	for (k=0;k<MDB_MAXCOINS;k++)
						 if (MDB_Changer1.PhysTubes[k]==s) break;
						 
						if (k<MDB_MAXCOINS) 
						{
							*txpointer++=SysVar.Tube[k].Deroute;
						}
						else *txpointer++=0;												 
					}					 
				break;
				
				case ICOM_COMMAND_SET_TUBE_ROUTING:
					for (s=0;s<MDB_MAXCOINS;s++) 
					{
					  	for (k=0;k<MDB_MAXCOINS;k++)
						 if (MDB_Changer1.PhysTubes[k]==s) break;
						 
						if (k<MDB_MAXCOINS) 
						{						
							SysVar.Tube[k].Deroute=*datapointer++;
							*txpointer++=SysVar.Tube[k].Deroute;
						}
						else *txpointer++=*datapointer++;
					}				
										
					//writeConfigFile(); // V1.38	
					//CalcCoinCRC(); //V1.29
				break;
				
				case ICOM_COMMAND_CMD_SET_OUTPUTS:	// V1.24
				
					chn=*(unsigned char *)rxpointer & 0x03;
					h = 0x0001 << chn;
					if (*(unsigned char *)(rxpointer+1)) 
					{
						// set Relais
						MDB_Changer2.ManualDispenseEnable = (MDB_Changer2.ManualDispenseEnable | h); 
						//RelaisStat[chn]=1;
					}
					else
					{
						// clr Relais
						MDB_Changer2.ManualDispenseEnable = (MDB_Changer2.ManualDispenseEnable & ~h);
						//RelaisStat[chn]=0;				
					}		
					
				     MDB_Changer2.NewSequence=Changer_Sequence_COINTYPE;	//002
				     MDB_Changer2.NewRequest=CmdChanger_CoinType;
							
				break;
				
				case ICOM_COMMAND_CMD_SET_LIGHT:
					chn=*(unsigned char *)rxpointer;
					if (chn!=0) 
						 AusgabeLeuchte = true;
					else AusgabeLeuchte = false;					
				break;
				case ICOM_COMMAND_CMD_SET_DIRECT:
					chn=*(unsigned char *)rxpointer;
					if (chn!=0) 
						 DirektEintritt = true;
					else DirektEintritt = false;					
				break;
				case ICOM_COMMAND_CMD_SET_EXTRA:
					chn=*(unsigned char *)rxpointer;
					if (chn!=0) 
						 EintrittSignal = true;
					else EintrittSignal = false;									
				break;

				case ICOM_COMMAND_GET_PRIORIETIES:	//Für DISPENSE_AMOUNT
					for (s=0;s<MDB_MAXCOINS;s++)
					{
					  	for (k=0;k<MDB_MAXCOINS;k++)
						 if (MDB_Changer1.PhysTubes[k]==s) break;
						 
						if (k<MDB_MAXCOINS) 
						{
							*txpointer++=SysVar.Tube[k].Prio;
						}
						else *txpointer++=0;												 
					}
					
					for (s=0;s<MAX_HOPPER;s++) *txpointer++=SysVar.Hopper[s].Prio;
				break;
				
				case ICOM_COMMAND_SET_PRIORIETIES:				
					for (s=0;s<MDB_MAXCOINS;s++) 
					{
					  	for (k=0;k<MDB_MAXCOINS;k++)
						 if (MDB_Changer1.PhysTubes[k]==s) break;
						 
						if (k<MDB_MAXCOINS) 
						{						
							SysVar.Tube[k].Prio=*datapointer++;
							*txpointer++=SysVar.Tube[k].Prio;
						}
						else *txpointer++=*datapointer++;						
					}
					for (s=0;s<MAX_HOPPER;s++)
					{
						SysVar.Hopper[s].Prio=*datapointer++;
						*txpointer++=SysVar.Hopper[s].Prio;
					}				
					
					//writeConfigFile(); // V1.38	
					//CalcCoinCRC(); //V1.29
				break;				
				
				case ICOM_COMMAND_BILL_FILL:
					for (s=0;s<MDB_MAXNOTES;s++) 
					{
						SysVar.Bill[s].Count=*(unaligned_ushort *)datapointer;
						datapointer+=2;									
						
						*(unaligned_ushort *)txpointer=SysVar.Bill[s].Count;
						txpointer+=2;									
					}		
					
					//CalcCoinCRC(); //V1.25				
				break;
				case ICOM_COMMAND_TUBE_FILL:
					for (s=0;s<MDB_MAXCOINS;s++) 
					{						
					  	for (k=0;k<MDB_MAXCOINS;k++)
						 if (MDB_Changer1.PhysTubes[k]==s) break;
						 
						if (k<MDB_MAXCOINS) 
						{						
							SysVar.Tube[k].Fill=*(unaligned_ushort *)datapointer;
							SysVar.Tube[k].LastFill=*(unaligned_ushort *)datapointer;
							*(unaligned_ushort *)txpointer=SysVar.Tube[k].Fill;
						}
						else
						{
							*(unaligned_ushort *)txpointer=0;
						}
						
						datapointer+=2;															
						txpointer+=2;															
					}													
					
				break;
				case ICOM_COMMAND_HOPPER_FILL:
					for (s=0;s<MAX_HOPPER;s++) 
					{
						SysVar.Hopper[s].Fill=*(unaligned_ushort *)datapointer;
						SysVar.Hopper[s].LastFill=*(unaligned_ushort *)datapointer;
						datapointer+=2;									
						
						*(unaligned_ushort *)txpointer=SysVar.Hopper[s].Fill;
						txpointer+=2;									
					}								
					
					for (s=0;s<MAX_HOPPER;s++) 					
					{
						if (SysVar.Hopper[s].Blocked)
							 *txpointer++=(SysVar.Hopper[s].Status | 0x80);
						else *txpointer++=SysVar.Hopper[s].Status;
					}
				
					//CalcCoinCRC(); //V1.25					
				break;
				case ICOM_COMMAND_HOPPER_VALUES:
					for (s=0;s<MAX_HOPPER;s++) 
					{
						SysVar.Hopper[s].Val=*(unaligned_ushort *)datapointer;
						datapointer+=2;									
						
						*(unaligned_ushort *)txpointer=SysVar.Hopper[s].Fill;
						txpointer+=2;									
					}								
					
					for (s=0;s<MAX_HOPPER;s++) 					
					{
						if (SysVar.Hopper[s].Blocked)
							 *txpointer++=(SysVar.Hopper[s].Status | 0x80);
						else *txpointer++=SysVar.Hopper[s].Status;
					}
					
					//CalcCoinCRC(); //V1.25					
				break;				
				case ICOM_COMMAND_CASHBOX_FILL:
					for (s=0;s<MDB_MAXCOINS;s++) 
					{
						SysVar.Coin[s].Count=*(unaligned_ushort *)datapointer;
						datapointer+=2;									
						
						*(unaligned_ushort *)txpointer=SysVar.Coin[s].Count;
						txpointer+=2;									
					}								
					
					crc_error=0;
					//CalcCoinCRC(); //V1.25					
				break;
				case ICOM_COMMAND_GENERAL_STATUS:
					check=SystemConfig & 0x0F;
					
					//if (CRTSCD_ready)  check |=0x10;
																				
					*(unaligned_ushort *)txpointer = check;
					txpointer+=2;
					
					*(unaligned_ushort *)txpointer = 0;  //SystemError;
					txpointer+=2;					
				
					txpointer+=sprintf(txpointer,"%s;%s;%s;",(char *)&MyApplication.Name,(char *)&MyApplication.VerInfo,(char *)&SystemID);				
				break;
				
				case ICOM_COMMAND_CLEAR_MESSAGES:
					for (i=0;i<MAX_MESSAGE_RECS;i++)
					{
						MessageList[i].status=0;
						MessageList[i].id=ICOM_COMMAND_END;
						MessageList[i].index=0;
					}				
					
					MsgListBufIn		= 0;
					MsgListBufOut		= 0;
					MsgBufTimeout		= 0;
					MsgBufRepeat		= 0;					
				break;
				
				case ICOM_COMMAND_ENABLE_COINS:
					EnableCoins(*(unaligned_ushort *)rxpointer);
					*(unaligned_ushort *)txpointer = 0x0000;
					txpointer+=2;
				break;
				
				case ICOM_COMMAND_ENABLE_DISPENSE:
					EnableManualDispense(*(unaligned_ushort *)rxpointer);
					*(unaligned_ushort *)txpointer = 0x0000;
					txpointer+=2;
					
					//Sound=0x01;
				break;
				
				case ICOM_COMMAND_ENABLE_BILLS:
					EnableBills(*(unaligned_ushort *)rxpointer);
					*(unaligned_ushort *)txpointer = 0x0000;
					txpointer+=2;					
				break;
				
				case ICOM_COMMAND_MDB_COIN_CONFIG:
					
					#if (WH_EMP==1)
					*txpointer++ = 'E';	// EMP
					
			  		*txpointer++ = MDB_Emp.Manufacturer[0];
					*txpointer++ = MDB_Emp.Manufacturer[1];
					*txpointer++ = MDB_Emp.Manufacturer[2];	  
					for (s=0;s<=11;s++) *txpointer++ = MDB_Emp.Serial[s];
					for (s=0;s<=11;s++) *txpointer++ = MDB_Emp.Model[s];	  
					*(unaligned_ushort *)txpointer = MDB_Emp.Version;						  
					txpointer+=2;
	  				*(unaligned_ulong *)txpointer = MDB_Emp.OptionBits;					
					txpointer+=4;
					
					*txpointer++ = MDB_Emp.Level;
					*(unaligned_ushort *)txpointer = MDB_Emp.Currency;
					txpointer+=2;
					*txpointer++ = MDB_Emp.CoinScaling;
					*txpointer++ = MDB_Emp.Decimals;					
									
	  				for (s=0;s<MDB_MAXCOINS;s++) *txpointer++ =MDB_Emp.CoinCredit[s];
					
					for (s=0;s<8;s++) *txpointer++ =MDB_Emp.CoinRouting[s]
					#endif
					
					#if (TUBE_CHANGER==1)
					*txpointer++ = 'T';	// TUBECHANGER
					
			  		*txpointer++ = MDB_Changer1.Manufacturer[0];
					*txpointer++ = MDB_Changer1.Manufacturer[1];
					*txpointer++ = MDB_Changer1.Manufacturer[2];	  
					for (s=0;s<=11;s++) *txpointer++ = MDB_Changer1.Serial[s];
					for (s=0;s<=11;s++) *txpointer++ = MDB_Changer1.Model[s];	  
					*(unaligned_ushort *)txpointer = MDB_Changer1.Version;						  
					txpointer+=2;
	  				*(unaligned_ulong *)txpointer = MDB_Changer1.OptionalFeatures;					
					txpointer+=4;
					
					*txpointer++ = MDB_Changer1.Level;
					*(unaligned_ushort *)txpointer = MDB_Changer1.Currency;
					txpointer+=2;
					*txpointer++ = MDB_Changer1.CoinScaling;
					*txpointer++ = MDB_Changer1.Decimals;					
									
	  				for (s=0;s<MDB_MAXCOINS;s++) *txpointer++ = MDB_Changer1.CoinCredit[s];
					
					*(unaligned_ushort *)txpointer = MDB_Changer1.CoinRouting;
					txpointer+=2;
					
					#endif
					
				break;

				case ICOM_COMMAND_MDB_BILL_CONFIG:
					
					*txpointer++ = 'B';	// BILL
					
			  		*txpointer++ = MDB_Validator.Manufacturer[0];
					*txpointer++ = MDB_Validator.Manufacturer[1];
					*txpointer++ = MDB_Validator.Manufacturer[2];	  
					for (s=0;s<=11;s++) *txpointer++ = MDB_Validator.Serial[s];
					for (s=0;s<=11;s++) *txpointer++ = MDB_Validator.Model[s];	  
					*(unaligned_ushort *)txpointer = MDB_Validator.Version;						  
					txpointer+=2;
	  				*(unaligned_ulong *)txpointer = MDB_Validator.OptionalFeatures;					
					txpointer+=4;
					
					*txpointer++ = MDB_Validator.Level;
					*(unaligned_ushort *)txpointer = MDB_Validator.Currency;
					txpointer+=2;
					*txpointer++ = MDB_Validator.BillScaling;
					*txpointer++ = MDB_Validator.Decimals;					
									
	  				for (s=0;s<MDB_MAXCOINS;s++) *txpointer++ =MDB_Validator.BillTypeCredit[s];
					
				    *(unaligned_ushort *)txpointer = MDB_Validator.StackerCapacity;
					txpointer+=2;
					*(unaligned_ushort *)txpointer = MDB_Validator.BillSecurityLevels;
					txpointer+=2;										
				break;

				case ICOM_COMMAND_MDB_HOPPER_CONFIG:
					
					*txpointer++ = 'H';	// HOPPER
					
			  		*txpointer++ = MDB_Changer2.Manufacturer[0];
					*txpointer++ = MDB_Changer2.Manufacturer[1];
					*txpointer++ = MDB_Changer2.Manufacturer[2];	  
					for (s=0;s<=11;s++) *txpointer++ = MDB_Changer2.Serial[s];
					for (s=0;s<=11;s++) *txpointer++ = MDB_Changer2.Model[s];	  
					*(unaligned_ushort *)txpointer = MDB_Changer2.Version;						  
					txpointer+=2;
	  				*(unaligned_ulong *)txpointer = MDB_Changer2.OptionalFeatures;					
					txpointer+=4;
					
					*txpointer++ = MDB_Changer2.Level;
					*(unaligned_ushort *)txpointer = MDB_Changer2.Currency;
					txpointer+=2;
					*txpointer++ = MDB_Changer2.CoinScaling;
					*txpointer++ = MDB_Changer2.Decimals;					
																		
					for (s=MAX_HOPPER;s<MDB_MAXCOINS;s++) MDB_Changer2.CoinCredit[s]=0;
					for (s=0;s<MAX_HOPPER;s++) 
					{						
						MDB_Changer2.CoinCredit[s]=SysVar.Hopper[s].Val / MDB_Changer2.CoinScaling;
					}
														
	  				for (s=0;s<MDB_MAXCOINS;s++) *txpointer++ =MDB_Changer2.CoinCredit[s];					

					*(unaligned_ushort *)txpointer = MDB_Changer2.CoinRouting;
					txpointer+=2;
										
				break;

				case ICOM_COMMAND_MDB_READER_CONFIG:
					
					*txpointer++ = 'R';	// Reader
					
			  		*txpointer++ = MDB_Cardreader.Manufacturer[0];
					*txpointer++ = MDB_Cardreader.Manufacturer[1];
					*txpointer++ = MDB_Cardreader.Manufacturer[2];	  
					for (s=0;s<=11;s++) *txpointer++ = MDB_Cardreader.Serial[s];
					for (s=0;s<=11;s++) *txpointer++ = MDB_Cardreader.Model[s];	  
					*(unaligned_ushort *)txpointer = MDB_Cardreader.Version;						  
					txpointer+=2;
	  				*(unaligned_ulong *)txpointer = MDB_Cardreader.OptionalFeatures;					
					txpointer+=4;
					
					*txpointer++ = MDB_Cardreader.Level;
					*(unaligned_ushort *)txpointer = MDB_Cardreader.AVSFeature1;
					txpointer+=2;
					*txpointer++ = MDB_Cardreader.Scaling;
					*txpointer++ = MDB_Cardreader.Decimals;					
																																
				break;

				case ICOM_COMMAND_DISPENSE_AMOUNT:		// funktioniert nur durch Bestandsführung 		
					*(unaligned_ushort *)txpointer = 0x0000;
					
					amount=*(unaligned_ulong *)rxpointer;
					testmode=*(char *)(rxpointer+4);
										
					//varMoney(str, (long far*)&amount, MainCurrency,1);
					//printf("DISPENSE Amount= %s %s",str,testmode ? "test":"   ");
					
					if (!DispenseAmount(amount,testmode)) *(unaligned_ushort *)txpointer = 0xFFFF;
					
					txpointer+=2;								
				break;

				case ICOM_COMMAND_DISPENSE_COIN:
					*(unaligned_ushort *)txpointer = 0x0000;
					
					chn=*(unaligned_ushort *)rxpointer;
					count=*(unaligned_ushort *)(rxpointer+1);
					
					if ((chn<21) && (count<1000))
					{						
 					 	PayOutTubes[chn].TubeOut +=count;			// tatsächlich ausgeben, ohne Rückmeldung
						printf("DISPENSE chn = %02u count=%02u",chn,count);						
						
						#if (TUBE_CHANGER==1)			
			 			if (chn<MDB_MAXCOINS) 			
						{
							MDB_Changer1.TubeStatus[chn]-=count;	//bis zur Auffrischung
							
							for (h=0;h<MDB_MAXCOINS;h++) if (MDB_Changer1.PhysTubes[h]==chn) break;				
							if (h<MDB_MAXCOINS) 
							{
								SysVar.Tube[h].Fill -=count;	// Fuellstande verringern
							}
						}
						#endif
						
			 			if ((chn>15) && (chn<21)) 		
						{	
							if (!SysVar.Hopper[(chn & 0x0F)].Blocked)
							{
						 		SysVar.Hopper[(chn & 0x0F)].Fill -=count;	// Fuellstande verringern
							}
							else 
							{
								printf("HOPPER chn = %02u BLOCKED!",(chn & 0x0F));
								
								str[0]=count;
								str[1]=chn & 0x07;
								
								addMessage(ICOM_MESSAGE_PAYOUT_ERROR, 2, &str[0]);
								
								*(unaligned_ushort *)txpointer = 0xFFFF;																
							}
						}
					}
					else *(unaligned_ushort *)txpointer = 0xFFFF;
															
					txpointer+=2;	
					
					//CalcCoinCRC(); //V1.29
															
				break;
				
				case ICOM_COMMAND_RGBANI:
					*(unaligned_ushort *)txpointer = 0x0000;
					
					aniadr=*(char *)rxpointer;
					ani=*(char *)(rxpointer+1);
					if ((aniadr<3) && (ani<RGBANI_MAX_ANI)) 
					{						
						FRONTRGB_Ani[aniadr]=ani;
						
						switch(aniadr) 
						{
						case 0:
							//CoinIllumAni=ani;
						break;
						case 1:
							//CardIllumAni=ani;
						break;
						case 2:
							//BillIllumAni=ani;
						break;
						}
					}
					
					txpointer+=2;												
				break;
								
				case ICOM_COMMAND_CARDREADER_DENY:
					*(unaligned_ushort *)txpointer = 0x0000;
					if (MDB_Cardreader.Sequence==Cardreader_Sequence_SESSION)
					{
						MDB_Cardreader.Sequence=Cardreader_Sequence_CANCELSESSION;
						MDB_Cardreader.NextRequest=CmdCardreader_VendSessionComplete;
					}
					txpointer+=2;				
				break;

				case ICOM_COMMAND_CARDREADER_ENABLE:
					*(unaligned_ushort *)txpointer = 0x0000;
					chn=*(unsigned char *)rxpointer;
					
					if (chn)					
						 MDB_Cardreader.Inhibit=0;
					else 
					{
						MDB_Cardreader.Inhibit=1;
						
						if (MDB_Cardreader.Sequence==Cardreader_Sequence_SESSION)
						{
							MDB_Cardreader.Sequence=Cardreader_Sequence_CANCELSESSION;
							MDB_Cardreader.NextRequest=CmdCardreader_ReaderCancel;
						}
					}
					
					txpointer+=2;
				break;
				
				case ICOM_COMMAND_CARDREADER_PAYMENT:
					*(unaligned_ushort *)txpointer = 0x0000;
					amount=*(unaligned_ulong *)rxpointer;
										
					//varMoney(str, (long far*)&amount, MainCurrency,1);
					//printf("CARDREADER Amount= %s",str);
					
					if (amount<=MDB_Cardreader.DispFundsAvailable)
					{						
						MDB_Cardreader.ItemPrice=amount;	
						MDB_Cardreader.ItemNumber=0xFFFF;
						//VEND_REQUEST
						MDB_Cardreader.NextRequest=CmdCardreader_VendRequest;
					}
					else *(unaligned_ushort *)txpointer = 0xFFFF;				

					txpointer+=2;	
				break;

				case ICOM_COMMAND_CMD_ADD_QUEUE:
					UUID0=*(unaligned_ulong *)datapointer;
					datapointer+=4;					
					UUID1=*(unaligned_ulong *)datapointer;
					datapointer+=4;					
					UUID2=*(unaligned_ulong *)datapointer;
					datapointer+=4;					
					UUID3=*(unaligned_ulong *)datapointer;
					datapointer+=4;					
					OpenOptions=*(unaligned_ulong *)datapointer;
					datapointer+=2;
					SysVar.DirEntryTimeout=*(unsigned char *)datapointer++;
					SysVar.DirEntryPause=*(unsigned char *)datapointer++;
					SysVar.DirEntryRepeat=*(unsigned char *)datapointer++;
					// PutOpenQueue(UUID0,UUID1,UUID2,UUID3,OpenOptions);										
				break;

								
				default:
					*(unaligned_ushort*)txpointer = 0xFFFF;
					txpointer+=2;				
				break;	
				}	
				
				txlen = txpointer-crcpointer;
				*(unaligned_ushort *)(crcpointer+2) = txlen-4;																	
				*(unaligned_ushort *)txpointer = CalcCRC2(crcpointer,txlen);
				txpointer+=2;				
				
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

/* MESSAGE INTERCOM SYSTEM PORT 1010*/

void intercomMessage_poll(void) {

unsigned char *rxpointer,*txpointer,*crcpointer;
unsigned int cmd, check, rxlen, txlen, s;
unsigned long index;

uint8_t buf[256];

	/* Check Timeouts for Offers */
	if (IntercomConnected) 
	{
		if (++IntercomPollCount >= 10) {		
			IntercomConnected = false;		
			printf("InterCom Connection timed out!\r\n");
			return;
		}		

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
					*(unaligned_ulong *)txpointer = MessageList[MsgListBufOut].index;
					txpointer+=4;

					for (s=0;s<MessageList[MsgListBufOut].len;s++) *txpointer++= MessageList[MsgListBufOut].Data[s];
						
					txlen = txpointer-crcpointer;
					//*(unsigned int far*)(crcpointer+2) = txlen-4;
					*(unaligned_ushort *)txpointer = CalcCRC2(crcpointer,txlen);
					txpointer+=2;										

					printf("SENDING MESSAGE %04X Index=%08lX repeat=%u\r\n",MessageList[MsgListBufOut].id, MessageList[MsgListBufOut].index, MsgBufRepeat);
				}
				else 
				{
					printf("INVALID MESSAGE %04X\r\n",MessageList[MsgListBufOut].id);
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
							printf("DISCARDING MESSAGE %04X\r\n",MessageList[MsgListBufOut].id);
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
				printf("Sending message len=%d ip=%s \r\n", p->len, ip4addr_ntoa(&callerIP));    
				udp_sendto(mpcb, p, &callerIP, INTERCOM_MESSAGE_PORT);
				pbuf_free(p);
			}

		} // queue is empty !
	} // not connected !
}

static void udp_message_received(void *passed_data, struct udp_pcb *upcb, struct pbuf *p, const struct ip4_addr *addr, u16_t port) 
{    
 unsigned char *rxpointer,*txpointer,*crcpointer,*datapointer;
 uint16_t cmd, check, rxlen, txlen; 
 unsigned long index;

    // Do something with the packet that was just received        

    if(p == NULL) return;

    printf("MESSAGE_PORT Received data ! len=%d ip=%s port=%u\r\n", p->len, ip4addr_ntoa(addr), port );    

    hexdump(p->payload, p->len, 16, 8);

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
			
			if (check==CalcCRC2(crcpointer,rxlen+4))
			{			
				index=*(unaligned_ulong *)rxpointer;
				
				printf("ICOM_MESSAGE_RESPONSE = %04X LENGTH=%04X TIMEOUT=%u IDX=%08lX",cmd,rxlen,MsgBufTimeout,index);
				
				if (index==MessageList[MsgListBufOut].index)
				{
					printf("ICOM_MESSAGE_INDEX = %08lX ACK", MessageList[MsgListBufOut].index);
					
					if (++MsgListBufOut>=MAX_MESSAGE_RECS) MsgListBufOut=0;	//erledigt !
					MsgBufTimeout=0;								
				}

			} //CRC
			
			rxpointer+=rxlen+2;
			cmd = *(unaligned_ushort *)rxpointer;
			crcpointer=rxpointer;
			rxpointer+=2;				
		}								
	}	
			
    pbuf_free(p);

}



void addMessage(uint16_t id, uint16_t len, uint8_t* data){

uint32_t handle;

	printf("ADD MESSAGE ID %02X BufIn=%u BufOut=%u\r\n",id,MsgListBufIn,MsgListBufOut);

    if ((MsgListBufIn+1) != MsgListBufOut)
	{
		if (len<=MAX_MESSAGE_LEN)				
		{			
			MessageList[MsgListBufIn].status=1;
		
			MessageList[MsgListBufIn].id=id;
		
			MessageList[MsgListBufIn].len=len;
		
			memcpy(&MessageList[MsgListBufIn].Data[0],data,len);			
					
		   	handle = random();
		   	//handle =(handle<<8)+ Time.S;
		   	//handle =(handle<<16)+ indexCounter++;
		
			MessageList[MsgListBufIn].index=handle;
		
			if (++MsgListBufIn>=MAX_MESSAGE_RECS) MsgListBufIn=0;
		}
		else printf("MESSAGE SEND LENGTH ERROR\r\n");
	}
	else printf("MESSAGE SEND BUFFER FULL\r\n");		

}
