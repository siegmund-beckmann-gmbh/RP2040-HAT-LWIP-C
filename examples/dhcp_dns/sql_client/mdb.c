/**************************************************************************
****			    Beckmann GmbH  				***
**************************************************************************/
// MDB HANDLING UART 0

#include    "mdb.h" // Header File

//*******************VARIABLES**************************

#define MDB_BUFF_SIZE	60
#define MDB_MAX_RET		3

#define FIFO_SIZE 32
queue_t MDBfifo;

const uint MDB_PIN_TX = 2;
const uint MDB_PIN_RX = 3;
const uint MDB_SERIAL_BAUD = 9600;

static PIO pio = pio0;
static int8_t pio_irq;
static uint smtx = 0;
static uint smrx = 1;

// IRQ called when the pio fifo is not empty, i.e. there are some characters on the uart
// This needs to run as quickly as possible or else you will lose characters (in particular don't printf!)
static void pio_irq_func(void) {
    while(!pio_sm_is_rx_fifo_empty(pio, smrx)) {
        uint16_t c = uart_rx_program_getc(pio, smrx);
		HandleMDB(c);
    }
}

unsigned char SimpleMode=1;

unsigned char MDB_timeout=0;
unsigned char MDB_RETcount=0;	//ab V3.67
unsigned int  MDB_Startup;
unsigned char MDB_chk;
unsigned char MDB_buff[MDB_BUFF_SIZE];
unsigned char MDB_buffpointer;
unsigned char MDB_DeviceCount;
unsigned long MDB_ChangerTotal;
unsigned char MDB_ACK_Delay=0;

unsigned char MDBcommand[MDB_BUFF_SIZE];
unsigned char MDBcommand_len;	
char MDBstat=0;

unsigned char GCardpay=0;
unsigned char LastGCardpay		= 0;
unsigned char GCardInserted	= 0;
long GCardPayValue = 0;
long TotGCardPayValue = 0;

const unsigned char ExpRequestID_MfrCode[4]="BEC";	  
const unsigned char ExpRequestID_Serial[13]="000000000001";	  
const unsigned char ExpRequestID_Model[13] ="    BE_ZENT5";
const unsigned char ExpRequestID_Version[2] = {0x10,0x10};

static SAVEVARS	SysVar;

static struct COIN_OVERRIDEtag	CoinOver[16];
static struct BILL_OVERRIDEtag BillOver[16];

EmpTag MDB_Emp;

ChangerTag MDB_Changer1;
#define pMDB_Changer1 (ChangerTag *)&MDB_Changer1

ChangerTag MDB_Changer2;
#define pMDB_Changer2 (ChangerTag *)&MDB_Changer2

CardreaderTag MDB_Cardreader;
ValidatorTag MDB_Validator;
AVDTag MDB_AVD;
PARKIOTag MDB_ParkIO;

TubeCfg PayOutTubes[32];

unsigned char ValidatorNotesEN[MDB_MAXNOTES];
unsigned char EmpCoinsEN[MDB_MAXCOINS];

unsigned int  BiggestCoin 	= 0;
unsigned int  HopperValue	= 0;
unsigned long SelHopperAmount	= 0;
unsigned long AKZmax 		= 0;

unsigned int  FillHopper	= 0;
unsigned char SelHopper		= 0xFF;
unsigned char SelHopperStatus	= 0;
unsigned char SelHopperReady	= 0;
unsigned char LastSelHopperReady= 0;
unsigned char SelHopperBlocked	= 0;
unsigned char SelHopperPrio		= 0;

//-----------------------------FUNCTIONS--------------------------------------

void putMDBevent(MDB_EVENT *event ) {
	if (!queue_try_add(&MDBfifo, event)) {
		printf("MDB fifo full");
	}			      			
}

void EnableCoins(unsigned int enable)
{ 	
	#if (WH_EMP==1)		     
	MDB_Emp.CoinEnable=enable;
	
	if (MDB_Emp.CoinEnable && MDB_Emp.ready && (!MDB_Emp.Problem))   
    {		 
     	if (!MDB_Emp.Problem) 
		{
			InsertEnable=1;			
     		CheckCoinToMain();
			if (!SimpleMode) IlluminateCoinInsert(1); 
		}
			
     	MDB_Emp.NewRequest=CmdEmp_CoinType;
    }
    else 
	{
		InsertEnable=0;	
		if (!SimpleMode) IlluminateCoinInsert(0);
	}
	#endif	

	#if (TUBE_CHANGER==1)    
	MDB_Changer1.CoinEnable=enable;		
	
  	MDB_Changer1.NewSequence=Changer_Sequence_COINTYPE;	//002
 	MDB_Changer1.NewRequest=CmdChanger_CoinType;
	 	 	 
    if (MDB_Changer1.CoinEnable && MDB_Changer1.ready && (!MDB_Changer1.Problem))
	{
     	//InsertEnable=1;
		CheckCoinToMain();
		//if (!SimpleMode) IlluminateCoinInsert(1);
	}
    else 
	{
		//InsertEnable=0;	
		//if (!SimpleMode) IlluminateCoinInsert(0);
	}
	#endif		
}

void EnableManualDispense(unsigned int enable)
{ 	
	#if (TUBE_CHANGER==1)    
	MDB_Changer1.ManualDispenseEnable=enable;			
  	MDB_Changer1.NewSequence=Changer_Sequence_COINTYPE;	//002
 	MDB_Changer1.NewRequest=CmdChanger_CoinType;	 	 	 
	#endif		
}


void EnableIllumination()
{
    // AusgabeLeuchte = 1;
}


void EnableBills(unsigned int enable)
{
	unsigned char str[80];
	
	MDB_Validator.BillEnable=enable;
	
	//sprintf(str,"Bill Enable received %u",MDB_Validator.BillEnable);
	//Output_Logfile(str, DEFAULT_LOG);	
	
	if (enable)
	{
     	MDB_Validator.Inhibit=0;
	 	MDB_Validator.Inhibited=1;
     	//if (MDB_Validator.ready) 
		 //if (!SimpleMode) IlluminateBillInsert(1);     
	}
	else
	{
     	MDB_Validator.Inhibit=1;
	 	MDB_Validator.Inhibited=0;
     	//if (MDB_Validator.ready)
		 //if (!SimpleMode) IlluminateBillInsert(0);		
	}
	
	// if (SysVar.BillEscrowMode==0) MDB_Changer1.NewRequest=CmdValidator_BillType;
}     

void ToggleInhibit(void)
{
	if(MDB_Validator.Inhibit==0) EnableBills(0);
	else EnableBills(MDB_Validator.BillEnable);	
}			

//----------------------CARDREADER----------------------------------
void ClearCardreaderDisplay (void)
{
    unsigned int k;
    for (k=0;k<32;k++) MDB_Cardreader.DisplayData[k]=' ';
    MDB_Cardreader.DisplayData[32]=0;
}
     
//-----------------------------FUNCTIONS--------------------------------------

void InitEmp()
{
	unsigned int s;
	
	MDB_Emp.DevLost=0;
	MDB_Emp.Level=0;
	MDB_Emp.ResetTime=1000;
	MDB_Emp.LastRequest=CmdChanger_Reset;
	MDB_Emp.NextRequest=CmdChanger_Reset;
	MDB_Emp.NewRequest=0;
	MDB_Emp.NewSequence=0;
	MDB_Emp.GetNextStatus=1;
	MDB_Emp.CoinToMain=0x0004;
	MDB_Emp.CoinEnable=0xFFFF;		//alle Kanal 	
	
	for (s=0;s<3;s++) 
        MDB_Emp.Manufacturer[s]=0x20;
        MDB_Emp.Manufacturer[3]=0x00;
	for (s=0;s<12;s++) 
	{
         MDB_Emp.Model[s]      =0x20;
         MDB_Emp.Serial[s]     =0x20;		
        }
        MDB_Emp.Model[12]=0;
        MDB_Emp.Serial[12]=0;		
        MDB_Emp.Version=0;
         
	MDB_Emp.LastStatus=0;
	MDB_Emp.ready=0;
	MDB_Emp.Problem=0;
}

void InitChanger1()
{
	unsigned int s;
	
	MDB_Changer1.DevLost=0;
	MDB_Changer1.Dispense=0;
	MDB_Changer1.Level=0;
	MDB_Changer1.NextTubestatus=TubeStatusRepeat;	
	MDB_Changer1.NextDiag=DiagRepeat;	
	MDB_Changer1.ResetTime=1500;
	MDB_Changer1.Sequence=Changer_Sequence_INIT;	//001
	MDB_Changer1.LastRequest=CmdChanger_Reset;
	MDB_Changer1.NextRequest=CmdChanger_Reset;
	MDB_Changer1.NewRequest=0;
	MDB_Changer1.NewSequence=0;
	MDB_Changer1.FeatureEnable=0x00000000;
	MDB_Changer1.ManualDispenseEnable=0xFFFF;
	MDB_Changer1.CoinEnable=0x0000;		//Kanal 1-6
	
	for (s=0;s<MDB_MAXCOINS;s++) MDB_Changer1.TubeStatus[s]=0;
		
	for (s=0;s<3;s++) MDB_Changer1.Manufacturer[s]=0x20;	
    MDB_Changer1.Manufacturer[3]=0x00;
		
	for (s=0;s<12;s++) 
	{
         MDB_Changer1.Model[s]      =0x20;
         MDB_Changer1.Serial[s]     =0x20;		
    }
 
    MDB_Changer1.Model[12]=0;
    MDB_Changer1.Serial[12]=0;		
    MDB_Changer1.Version=0;
	MDB_Changer1.ready=0;
	MDB_Changer1.LastProblem=0;
	MDB_Changer1.Problem=0;
	MDB_Changer1.DiagnosticCommand=0;
	
	for (s=0;s<6;s++) MDB_Changer1.TubeCountAF[s]=0xFF;
	for (s=0;s<2;s++) MDB_Changer1.HopperCount[s]=0xFFFF;
	
	MDB_Changer1.SetTubes=0;
	
	MDB_Changer1.CoinToMain=0x0000;
	
}
	
void InitChanger2()
{
	unsigned int s;
	
	MDB_Changer2.DevLost=0;
	MDB_Changer2.Dispense=0;
	MDB_Changer2.Level=0;
	MDB_Changer2.Sequence=Changer_Sequence_INIT;	//001
	MDB_Changer2.LastRequest=CmdChanger_Reset;
	MDB_Changer2.NextRequest=CmdChanger_Reset;
	MDB_Changer2.NewRequest=0;
	MDB_Changer2.NewSequence=0;
	MDB_Changer2.NextTubestatus=TubeStatusRepeat;
	MDB_Changer2.NextDiag=0;	
	MDB_Changer2.ResetTime=1800;
	MDB_Changer2.Sequence=0x0000;
	MDB_Changer2.FeatureEnable=0x00000000;
	MDB_Changer2.ManualDispenseEnable=0x0000;

	for (s=0;s<MDB_MAXCOINS;s++) 
	 {
		MDB_Changer2.TubeStatus[s]=0;	 	
	 }
	 	
	for (s=0;s<3;s++) 
        MDB_Changer2.Manufacturer[s]=0x20;
        MDB_Changer2.Manufacturer[3]=0x00;
	for (s=0;s<12;s++) 
	{
         MDB_Changer2.Model[s]      =0x20;
         MDB_Changer2.Serial[s]     =0x20;		
        }
        MDB_Changer2.Model[12]=0;
        MDB_Changer2.Serial[12]=0;		
        MDB_Changer2.Version=0;
	MDB_Changer2.ready=0;

	// for (s=0;s<MAX_HOPPER;s++) SysVar.Hopper[s].Blocked=0;
	SelHopperBlocked=0;
	SelHopperPrio=0;
	
	MDB_Changer1.DiagnosticCommand=0;		

	//CalcCoinCRC(); //V1.26		
}

void InitCardreader()
{
	unsigned int s;
	
	MDB_Cardreader.DevLost=0;
	MDB_Cardreader.NextRequest=CmdCardreader_Reset;
	MDB_Cardreader.NewRequest=0;
	MDB_Cardreader.Level=0;	
	MDB_Cardreader.ItemNumber=0xFFFF;	//Vend Request / Cash Sale
	MDB_Cardreader.ItemPrice=0;		//Vend Request / Cash Sale
	MDB_Cardreader.FeatureEnable=0x00000000;
    MDB_Cardreader.OptionalFeatures=0x00000000;
	MDB_Cardreader.ResetTime=2000;
	
	for (s=0;s<3;s++) MDB_Cardreader.Manufacturer[s]=0x20;
    MDB_Cardreader.Manufacturer[3]=0x00;
	
	for (s=0;s<12;s++) 
	{
         MDB_Cardreader.Model[s]      =0x20;
         MDB_Cardreader.Serial[s]     =0x20;		
    }
 
    MDB_Cardreader.Model[12]=0;
    MDB_Cardreader.Serial[12]=0;			
    MDB_Cardreader.Version=0;
	MDB_Cardreader.ready=0;
	
	for (s=0;s<5;s++) MDB_Cardreader.DRAV[s]=0x20;
	MDB_Cardreader.DRAV[5]=0;
	MDB_Cardreader.AVSFeature1=0;
	MDB_Cardreader.AVSFeature2=0;
	MDB_Cardreader.DiagnosticCommand=0;
	MDB_Cardreader.AVSConfiguration=0;
	
	MDB_Cardreader.Sequence=Cardreader_Sequence_INIT;
	MDB_Cardreader.Inhibit= 1;
	MDB_Cardreader.Inhibit_Old=0;
	
}

void InitValidator()
{
	unsigned int s;

	MDB_Validator.DevLost=0;
	
	MDB_Validator.LastRequest=CmdValidator_Poll;
	MDB_Validator.NextRequest=CmdValidator_Poll;
	MDB_Validator.NewRequest=0;
	MDB_Validator.NewSequence=0;
	MDB_Validator.ResetTime=2000;
	MDB_Validator.NextStackerStatus=StackerStatusRepeat;
	MDB_Validator.Level=0;	
	MDB_Validator.Security=0xFFFF;	
	MDB_Validator.EscrowStatus=0;	
	MDB_Validator.Sequence=Validator_Sequence_INIT;	//001
	MDB_Validator.FeatureEnable=0x00000000;
	MDB_Validator.BillEnable=0x0000;
	MDB_Validator.BillEscrowEnable=0x0000;
	MDB_Validator.AKZ_Max= 1000000;
	MDB_Validator.AKZ_Min= 0;
    MDB_Validator.Inhibit=1;		//gesperrt
	MDB_Validator.Inhibited=1;	
	MDB_Validator.accepted= 0;

	for (s=0;s<3;s++) 
        MDB_Validator.Manufacturer[s]=0x20;
        MDB_Validator.Manufacturer[3]=0x00;
	for (s=0;s<12;s++) 
	{
         MDB_Validator.Model[s]      =0x20;
         MDB_Validator.Serial[s]     =0x20;		
    }
 
    MDB_Validator.Model[12]=0;
    MDB_Validator.Serial[12]=0;		
    MDB_Validator.Version=0;
	MDB_Validator.ready=0;
}

void InitAVD(void)
{
	unsigned int s;

	MDB_AVD.DevLost=0;
	
	MDB_AVD.LastRequest=CmdAVD_Reset;
	MDB_AVD.NextRequest=CmdAVD_Reset;
	
	MDB_AVD.ResetTime=2500;
	MDB_AVD.Level=0;	

	for (s=0;s<3;s++) 
        MDB_AVD.Manufacturer[s]=0x20;
        MDB_AVD.Manufacturer[3]=0x00;
	for (s=0;s<12;s++) 
	{
         MDB_AVD.Model[s]      =0x20;
         MDB_AVD.Serial[s]     =0x20;		
    }
 
    MDB_AVD.Model[12]=0;
    MDB_AVD.Serial[12]=0;		
    MDB_AVD.Version=0;
	MDB_AVD.ready=0;
	
	for (s=0;s<5;s++) 
	 MDB_AVD.DRAV[s]=0x20;
	MDB_AVD.DRAV[5]=0;
	
	MDB_AVD.AVSFeature1=0;
	MDB_AVD.AVSFeature2=0;
	MDB_AVD.DiagnosticCommand=0;
	MDB_AVD.AVSConfiguration=0;
	
}

void InitParkIO(void)
{
	unsigned int s;

	MDB_ParkIO.DevLost=0;
	
	MDB_ParkIO.LastRequest=CmdParkIO_Reset;
	MDB_ParkIO.NextRequest=CmdParkIO_Reset;
	
	MDB_ParkIO.ResetTime=2500;
	MDB_ParkIO.Level=0;	

	for (s=0;s<3;s++) 
        MDB_ParkIO.Manufacturer[s]=0x20;
        MDB_ParkIO.Manufacturer[3]=0x00;
	for (s=0;s<12;s++) 
	{
         MDB_ParkIO.Model[s]      =0x20;
         MDB_ParkIO.Serial[s]     =0x20;		
    }
 
    MDB_ParkIO.Model[12]=0;
    MDB_ParkIO.Serial[12]=0;		
    MDB_ParkIO.Version=0;
	MDB_ParkIO.ready=0;			
	MDB_ParkIO.ResetFlag=1;
	
	MDB_ParkIO.Tara=0;
}

void InitMDB(void)
{
	unsigned int s;
	char str[80];

    uint txoffset = pio_add_program(pio, &uart_tx_program);
    uart_tx_program_init(pio, smtx, txoffset, MDB_PIN_TX, MDB_SERIAL_BAUD);
    uint rxoffset = pio_add_program(pio, &uart_rx_program);
    uart_rx_program_init(pio, smrx, rxoffset, MDB_PIN_RX, MDB_SERIAL_BAUD);

// Find a free irq
    static_assert(PIO0_IRQ_1 == PIO0_IRQ_0 + 1 && PIO1_IRQ_1 == PIO1_IRQ_0 + 1, "");
    pio_irq = (pio == pio0) ? PIO0_IRQ_0 : PIO1_IRQ_0;
    if (irq_get_exclusive_handler(pio_irq)) {
        pio_irq++;
        if (irq_get_exclusive_handler(pio_irq)) {
            panic("All IRQs are in use");
        }
    }
	
    // Enable interrupt
    irq_add_shared_handler(pio_irq, pio_irq_func, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY); // Add a shared IRQ handler
    irq_set_enabled(pio_irq, true); // Enable the IRQ
    const uint irq_index = pio_irq - ((pio == pio0) ? PIO0_IRQ_0 : PIO1_IRQ_0); // Get index of the IRQ
    pio_set_irqn_source_enabled(pio, irq_index, pis_sm0_rx_fifo_not_empty + smrx, true); // Set pio to tell us when the FIFO is NOT empty

	queue_init(&MDBfifo, sizeof(MDB_EVENT), FIFO_SIZE);    

	MDB_timeout=0;
	MDB_Startup=200;

	InitEmp();
	InitChanger1();
	InitChanger2();
	
	for (s=0;s<32;s++) PayOutTubes[s].TubeOut  = 0;	//*D*/	 	
	
	MDB_Changer1.ChangerAmount=0;
	MDB_Changer2.ChangerAmount=0;
	
	InitCardreader();	
	MDB_Cardreader.NonResponseTime= 5;	//5 Sekunden
	MDB_Cardreader.LastRequest=CmdCardreader_Reset;
	ClearCardreaderDisplay();
	
	InitValidator();
			
	MDB_buffpointer=0;
	MDB_DeviceCount=0xFF;
 	for (s=0;s<MDB_BUFF_SIZE;s++) MDB_buff[s]=0;	

	InitAVD();	
	MDB_AVD.NonResponseTime= 5;	//5 Sekundend
	MDB_AVD.LastRequest=CmdAVD_Reset;	

	InitParkIO();	
	MDB_ParkIO.NonResponseTime= 5;	//5 Sekundend
	MDB_ParkIO.LastRequest=CmdParkIO_Reset;

#if(TUBE_CHANGER==1) 	
 	for (s=0;s<MDB_MAXCOINS;s++) 
 	{
 		//SysVar.Tube[s].LastFill=SysVar.Tube[s].Fill;
 		if (SysVar.Tube[s].Fill<SysVar.Tube[s].Threshold) 
 			SysVar.Tube[s].Hysteresis=SysVar.SetHysteresis;
 		else	SysVar.Tube[s].Hysteresis=0;
 	}
#endif
 	for (s=0;s<MAX_HOPPER;s++) 
 	{
		if (SysVar.Hopper[s].LastFill!=SysVar.Hopper[s].Fill)
		{
			//sprintf(str,"WARNING: Memory altered for Hopper%u LastFill=%05u Fill=%05u !",s,SysVar.Hopper[s].LastFill,SysVar.Hopper[s].Fill);
			//Output_Logfile(str, DEFAULT_LOG);
		}
		
 		SysVar.Hopper[s].LastFill=SysVar.Hopper[s].Fill;
 		if (SysVar.Hopper[s].Fill<SysVar.Hopper[s].Threshold) 
 			SysVar.Hopper[s].Hysteresis=SysVar.SetHysteresis;
 		else	SysVar.Hopper[s].Hysteresis=0;
 	}
	
	// CalcCoinCRC(); //V1.25
}

void MDBTimeout(void)
{ 
	return;
}	


void HandleMDB(unsigned int by)
{ 
  unsigned char s,cr;
  int h,k,ww;
  MDB_EVENT MDBEvent;
	
	MDB_timeout=MDB_reload;
	MDB_buff[MDB_buffpointer]=by&0xFF;
	MDB_buffpointer++;
	if (MDB_buffpointer>=MDB_BUFF_SIZE)
	{
		   MDB_buffpointer=0;	
		   
	}
	
	if((by&0xFF00)==0x0100)	//lastbyte received
	{	  		   
    	if(MDB_buffpointer ==1)
	    {
        	switch(MDB_buff[0])
	        {
		  	case MDB_nak :
		  
		    		switch(MDB_DeviceCount)
     	      		{
                	case 0 :		//Changer 1  or EMP	                
					#if (TUBE_CHANGER==1)
                    	MDB_Changer1.NextRequest=MDB_Changer1.LastRequest;  //repeat next time
                	#elif (WH_EMP==1)
                  		MDB_Emp.NextRequest=MDB_Emp.LastRequest;  //repeat next time	                  	                  
                    #endif                      
                  	break;
                	case 1 :		//Changer 2
                    	MDB_Changer2.NextRequest=MDB_Changer2.LastRequest;  //repeat next time
                  	break;	         
                	case 2 :		//Cardreader
                  		MDB_Cardreader.NextRequest=MDB_Cardreader.LastRequest;  //repeat next time
                  	break;
                	case 3 :		//Validator
                  		MDB_Validator.NextRequest=MDB_Validator.LastRequest;  //repeat next time
                  	break;
                	case 4 :		//Age Verification Device
                  		MDB_AVD.NextRequest=MDB_AVD.LastRequest;  //repeat next time					
                  	break;
                	case 5 :		//ParkIO
                  		MDB_ParkIO.NextRequest=MDB_ParkIO.LastRequest;  //repeat next time
                  	break;
	              		}    		    
	              
					//Logfile nak
					
			   	  	MDBEvent.Type    = EvTypeMDB_NAK;
	   	   		  	MDBEvent.Length  = 1;
	   	   		  	MDBEvent.Data[0] = MDB_DeviceCount;
	   	   		  	putMDBevent(&MDBEvent);
					
	    	break; 	
	    
		  	case MDB_ack :
		    		Analyze_MDB(MDB_buffpointer);					
	    	break; 	    		
		  	}
	    }
    	else  	
      	{			// data + chk received
        	cr=0;
        	for (s=0;s<(MDB_buffpointer-1);s++) cr=cr+MDB_buff[s];
        	if (cr==MDB_buff[MDB_buffpointer-1]) 
          	{	          		        
					MDB_ACK_Delay=1;            		
					uart_tx_program_putc(pio, smtx, MDB_ack); //ACK
					//MDB_buff[MDB_buffpointer-1]=0;	//CRC loeschen
            		Analyze_MDB(MDB_buffpointer);				
          	}
        	else 
         	{							
				// ACHTUNG hier Begrenzung auf max 3x RET einbauen
				if (MDB_RETcount<MDB_MAX_RET)
				{	
					MDB_RETcount++;

					uart_tx_program_putc(pio, smtx, MDB_ret); //RET
					
		    		MDB_timeout=MDB_reload;					
				
			    	MDB_buffpointer=0;
				}
            }                                                                   
      	}
	}
	  	  
	return;
}	
//*************************MDB RECEIVE********************************************	
void Analyze_MDB(unsigned char buffp)
{	
	MDB_buffpointer=0;
	MDB_RETcount=0;
	
	 switch(MDB_DeviceCount)
	     {
	       case 0 :		//Changer
		 		#if (TUBE_CHANGER==1)
                 RX_Handle_Changer(pMDB_Changer1,buffp);
	            #elif (WH_EMP==1)
	         	 RX_Handle_Emp(buffp);
             	#endif
	         break;

	       case 1:		//Changer
                 RX_Handle_Changer(pMDB_Changer2,buffp);
	         break;	         

	       case 2 :		//Cardreader
	         RX_Handle_Cardreader(buffp);
	         break;

	       case 3 :		//Validator
	         RX_Handle_Validator(buffp);
	         break;
			 			 
	       case 4 :		//Age Verification Device
	         RX_Handle_AVD(buffp);
	         break;

	       case 5 :		//ParkIO
	         RX_Handle_ParkIO(buffp);
	         break;
  	     }	
}

//------------------------------WH_EMP-----------------------------------------
#if (WH_EMP==1)
void RX_Handle_Emp(unsigned char buff_point)
{
  unsigned int k,s,coinval,LastCoinToMain;
  unsigned char TubeNo,channel,stat;
	
  if (!MDB_Emp.DevLost) MDB_Startup		+= 10;
  
  MDB_Emp.DevLost=MDB_LostCount;
  switch(MDB_Emp.LastRequest)
    {
	case CmdEmp_Reset :
	  MDB_Emp.NextRequest=CmdEmp_Poll;
	  break; 	
	case CmdEmp_Status :

	  MDB_Emp.Level	 =MDB_buff[0];
	  MDB_Emp.Currency=MDB_buff[2]+256*MDB_buff[1];
	  MDB_Emp.CoinScaling=MDB_buff[3];
	  MDB_Emp.Decimals   =MDB_buff[4];
	  MDB_Emp.SorterInfo =MDB_buff[5];
	  // Sortierschacht ?
	  for (s=0;s<8;s++) MDB_Emp.CoinRouting[s]=MDB_buff[6+s];
	  BiggestCoin=0;
	  
	  for (s=14;s<30;s++) 
	   if (s<buff_point-1)
	   {
	  		MDB_Emp.CoinCredit[s-14]=MDB_buff[s];
			if (MDB_Emp.CoinCredit[s-14]>BiggestCoin) BiggestCoin=MDB_Emp.CoinCredit[s-14];
	   }
	   else MDB_Emp.CoinCredit[s-14]=0;
	  	  
	  BiggestCoin *=MDB_Emp.CoinScaling;
	  
	  if (SysVar.CoinRouting)
	  for (s=0;s<8;s++)
	  {
	  	// Tube links
	  	if ((MDB_Emp.CoinRouting[s] & 0x80) && (SysVar.Hopper[0].Prio<Priorities)) 
		 SysVar.Hopper[0].Val = (unsigned int)MDB_Emp.CoinCredit[2*s] * (unsigned int)MDB_Emp.CoinScaling;
	  	if ((MDB_Emp.CoinRouting[s] & 0x08) && (SysVar.Hopper[0].Prio<Priorities)) 
		 SysVar.Hopper[0].Val = (unsigned int)MDB_Emp.CoinCredit[2*s+1] * (unsigned int)MDB_Emp.CoinScaling;
	  	// Tube mitte-links
	  	if ((MDB_Emp.CoinRouting[s] & 0x40) && (SysVar.Hopper[1].Prio<Priorities)) 
		 SysVar.Hopper[1].Val = (unsigned int)MDB_Emp.CoinCredit[2*s] * (unsigned int)MDB_Emp.CoinScaling;
	  	if ((MDB_Emp.CoinRouting[s] & 0x04) && (SysVar.Hopper[1].Prio<Priorities)) 
		 SysVar.Hopper[1].Val = (unsigned int)MDB_Emp.CoinCredit[2*s+1] * (unsigned int)MDB_Emp.CoinScaling;
	  	// Tube mitte-rechts
	  	if ((MDB_Emp.CoinRouting[s] & 0x20) && (SysVar.Hopper[2].Prio<Priorities)) 
		 SysVar.Hopper[2].Val = (unsigned int)MDB_Emp.CoinCredit[2*s] * (unsigned int)MDB_Emp.CoinScaling;
	  	if ((MDB_Emp.CoinRouting[s] & 0x02) && (SysVar.Hopper[2].Prio<Priorities)) 
		 SysVar.Hopper[2].Val = (unsigned int)MDB_Emp.CoinCredit[2*s+1] * (unsigned int)MDB_Emp.CoinScaling;
	  	// Tube rechts
	  	if ((MDB_Emp.CoinRouting[s] & 0x10) && (SysVar.Hopper[3].Prio<Priorities)) 
		 SysVar.Hopper[3].Val = (unsigned int)MDB_Emp.CoinCredit[2*s] * (unsigned int)MDB_Emp.CoinScaling;
	  	if ((MDB_Emp.CoinRouting[s] & 0x01) && (SysVar.Hopper[3].Prio<Priorities)) 
		 SysVar.Hopper[3].Val = (unsigned int)MDB_Emp.CoinCredit[2*s+1] * (unsigned int)MDB_Emp.CoinScaling;
	  }
	  
	  //CalcCoinCRC(); //V1.25
	  
	  MDB_Emp.NextRequest=CmdEmp_Poll;	//	POLL
	  break; 	    		
        case CmdEmp_Poll :				
          	MDB_Emp.NextRequest=CmdEmp_Poll;	//	POLL

	  		if (buff_point>1)			// wenn 1 dann nur ACK
	  		{
	      		for (s=0;s<(buff_point-1);s++) MDB_Emp.Status[s]=MDB_buff[s];
				s=0;
				
				stat=MDB_Emp.Status[s];
	      		switch(stat & 0xF0)
	      		{ 
				case 0x70: //ist Münze	   kanal höher
				case 0x80: //ist Münze	        	        	
				
					channel=stat & 0x0F;					
														
					coinval= (unsigned int)MDB_Emp.CoinScaling * (unsigned int)MDB_Emp.CoinCredit[channel]; 
					
					if (CoinOver[channel].active)
					{
						// bei Mehfachwaehrung hier ersetzen ab V3.63
					    coinval= CoinOver[channel].Value;
					}
				    										
	        		MDB_Emp.LastValue=coinval;
		
	        		if ((s+1)<buff_point)
	        			 TubeNo = MDB_Emp.Status[++s] & 0x0F;
	        		else
	        		{
		        		if (stat & 0x01)
		        	 	 	 TubeNo = (MDB_Emp.CoinRouting[(stat>>1) & 0x07] & 0x0F);
		        		else TubeNo = (MDB_Emp.CoinRouting[(stat>>1) & 0x07]>>4) &0x0F;
	        		}
        	 
	        		if(TubeNo) 
	        		{	
	        			k=0;
	        			while ((TubeNo & 0x0F))
	        			{ 
	        				TubeNo = TubeNo<<1;
	        				k++;
	        			}
        								
	        			SysVar.Hopper[k-1].Fill++;
	        		}
	        		else
	        		{
	        			// Hauptkasse !!!
						SysVar.Coin[channel].Count++;
	        		}
					
					//CalcCoinCRC(); //V1.25
					   
		   			MDBEvent.Type    = EvTypeMDB_EmpCoinInserted;			// Coin inserted
   	   				MDBEvent.Length  = 3;
   	   				MDBEvent.Data[0] = coinval;
   	   				MDBEvent.Data[1] = coinval>>8;
					MDBEvent.Data[2] = channel;
   	   				putMDBevent(&MDBEvent);
	   		
   	   				LastCoinToMain = MDB_Emp.CoinToMain;
					CheckCoinToMain();
					if (MDB_Emp.CoinToMain != LastCoinToMain) MDB_Emp.NextRequest = CmdEmp_CoinType;
					
				break;					
	      		case 0x90:
	            	// Fehlermeldungen
            		if (MDB_Emp.LastStatus != stat)
            		{
	      				if (MDB_Emp.Status[s]<0x9F)
	      		 		{
							
		   	  				MDBEvent.Type    = EvTypeMDB_EmpStatus;
   	   		  				MDBEvent.Length  = 1;
   	   		  				MDBEvent.Data[1] = (MDB_Emp.Status[s] &0x0F);
   	   		  				putMDBevent(&MDBEvent);
	      		 		}
	      			}						 	      		 	      	
	      		break;
				case 0x00:
		      		if (stat==0x07)
		      		{
		      			MDB_Emp.NextRequest=CmdEmp_ExpIdentification;
		      			MDB_Emp.Problem=0;
		      		}   		      
				break;
      			}
				
	      		MDB_Emp.LastStatus=stat;					
      
	    	} //von buff_point>1
	    	else
	    	{
	    		//nur ACK .... alles OK	    	
			
	  			for (s=0;s<MDB_MAXCOINS;s++) MDB_Emp.Status[s]=0;
				MDB_Emp.LastStatus=0;
	    	}
	    
	  		if(MDB_Emp.NextRequest == CmdEmp_Poll)
	  		{	    
	  			if(MDB_Emp.GetNextStatus>0) 
	    		{
	    			MDB_Emp.GetNextStatus--;
	    		}
	  			else
	    		{
	    			MDB_Emp.GetNextStatus=EmpStatusRepeat;
	    			MDB_Emp.NextRequest=CmdEmp_Status;
	    		} 	    		    
	  		}
      	  	    
	  break; 	    		
	case CmdEmp_CoinType :	//M�nzfreigabe gesendet
	  MDB_Emp.NextRequest=CmdEmp_Poll;
	  break; 	    		
	case CmdEmp_CoinRouting :  //M�nzverteilung
	  MDB_Emp.NextRequest=CmdEmp_Poll;    		  
	  break; 	    		
	case CmdEmp_ExpIdentification :
      MDB_Emp.Manufacturer[0]=MDB_buff[0];
	  MDB_Emp.Manufacturer[1]=MDB_buff[1];
	  MDB_Emp.Manufacturer[2]=MDB_buff[2];
	  for (s=0;s<12;s++) MDB_Emp.Serial[s]=MDB_buff[3+s];
	  for (s=0;s<12;s++) MDB_Emp.Model[s]=MDB_buff[15+s];
	  MDB_Emp.Version=MDB_buff[28]+256*MDB_buff[27];	  
	  MDB_Emp.OptionBits=((long)MDB_buff[29]*0x1000000)+((long)MDB_buff[30]*0x10000)+(MDB_buff[31]*0x100)+MDB_buff[32];
          MDB_Emp.NextRequest=CmdEmp_ExpRequest;    		  
	  break; 	    		
	case CmdEmp_ExpExtendedCoinType :  //erweiterte M�nzfreigabe
          MDB_Emp.NextRequest=CmdEmp_Poll; 
	  break; 	    	
        case CmdEmp_ExpRequest :
          MDB_Emp.CoinEnable=(MDB_buff[0]*0x100)+MDB_buff[1];
          MDB_Emp.CoinToMain=(MDB_buff[2]*0x100)+MDB_buff[3];
          MDB_Emp.MainTubeNumber=MDB_buff[4];
          for (s=0;s<8;s++) MDB_Emp.CoinRouting[s]=MDB_buff[5+s];
          MDB_Emp.ready=1;
		  SystemConfig |=0x01;
          MDB_Emp.NextRequest=CmdEmp_CoinType;    		
	  break; 	    			  	
	case CmdEmp_ExpExtendedRequest :
	  MDB_Emp.CoinEnable=((long)MDB_buff[0]*0x1000000)+((long)MDB_buff[1]*0x10000)+(MDB_buff[2]*0x100)+MDB_buff[3];
	  MDB_Emp.CoinToMain=(MDB_buff[4]*0x100)+MDB_buff[5];
	  MDB_Emp.MainTubeNumber=MDB_buff[6];
	  for (s=0;s<8;s++) MDB_Emp.CoinRouting[s]=MDB_buff[7+s];
          MDB_Emp.NextRequest=CmdEmp_CoinType;    		
	  break; 	    		
	case CmdEmp_ExpMasterSlave :
	  for (s=0;s<8;s++) MDB_Emp.MasterSlave[s]=MDB_buff[s];
          MDB_Emp.NextRequest=CmdEmp_Poll;    		
	  break; 	    			
    }				
	
}  
#endif
//------------------------------CHANGER----------------------------------------------
void RX_Handle_Changer(ChangerTag *MDB_Changer, unsigned char buff_point)
{	
  unsigned int h,k,s,fill;
  unsigned long coinval;
  		   long addval;
  unsigned char tb=0;
  unsigned char stat,err,ctype,mc,hop,sec;
  char hChanged=0; 
  unsigned char diff=0;

  MDB_EVENT MDBEvent;
      	
  MDB_Changer->DevLost=MDB_LostCount;
   
  switch(MDB_Changer->LastRequest)
    {
	case CmdChanger_Reset : 
	  MDB_Changer->NextRequest=CmdChanger_Poll;
	  break; 	
	  
	case CmdChanger_Status : // after Changer was Reset SEQUENCE=INIT
	  MDB_Changer->Level	 	=MDB_buff[0];	  
	  MDB_Changer->Currency		=MDB_buff[2]+256*MDB_buff[1];
	  MDB_Changer->CoinScaling	=MDB_buff[3];
	  MDB_Changer->Decimals	 	=MDB_buff[4];
	  
	  if (MDB_Changer == pMDB_Changer1) 
	  {
	  	   MDB_Changer->CoinRouting=MDB_buff[6]+256*MDB_buff[5];
		   //if (MDB_Changer->CoinRouting==0) MDB_Changer->CoinRouting=0x000F; // Notfall
	  }
	  else MDB_Changer->CoinRouting= 0x001F;
	  
	  k=MDB_Changer->CoinRouting;
	  
	  for (s=0;s<MDB_MAXCOINS;s++)
	  {
		  	MDB_Changer->PhysTubes[s] = 0xFF;	//unbenutzte Tube
			MDB_Changer->AcceptedCoins[s] = 0xFF;	//nicht akzeptierte Muenze
			
	  		if (k & 0x0001) 
	    	{
	    		MDB_Changer->PhysTubes[tb] = s;
	    		tb++;
	    	}
		
	   		k = k>>1;	   
	  }	  	  	  
	
	  tb=0;
	  
	  for (s=7;s<23;s++)
	  { 
	  	if (s<buff_point-1)
		{
	  		MDB_Changer->CoinCredit[s-7]=MDB_buff[s];
			 
      		coinval= (unsigned int)MDB_buff[s]*(unsigned int)MDB_Changer->CoinScaling;
			
			/*
			if (CoinOver[s-7].active)
			{
				// bei Mehfachwaehrung hier ersetzen
			    coinval= CoinOver[s-7].Value;
			}
			*/
			 			 
			if ( coinval>=SysVar.SmallestCoin ) MDB_Changer->AcceptedCoins[tb++]=s-7;
		}
	   	else MDB_Changer->CoinCredit[s-7]=0;
	  }

#if (TUBE_CHANGER==1)
	  if (MDB_Changer == pMDB_Changer1)
	  {
		  for (s=0;s<MDB_MAXCOINS;s++)
		   if (MDB_Changer->PhysTubes[s]!=0xFF)
		   {
			    if (SysVar.Tube[s].Prio<MAX_PRIORITY)
				{
		  			SysVar.Tube[s].Val=(unsigned int)MDB_Changer->CoinCredit[MDB_Changer->PhysTubes[s]]*(unsigned int)MDB_Changer->CoinScaling;
					
					if (CoinOver[MDB_Changer->PhysTubes[s]].active)
					{
						// bei Mehfachwaehrung hier ersetzen ab V3.63
					    SysVar.Tube[s].Val= CoinOver[MDB_Changer->PhysTubes[s]].Value;
					}					
					
					if (SysVar.Tube[s].Deroute)
					{
						if ((SysVar.Tube[s].Deroute-1)<MAX_HOPPER) SysVar.Hopper[SysVar.Tube[s].Deroute-1].Val=SysVar.Tube[s].Val;
						SysVar.Tube[s].avail=0;
					}
					else SysVar.Tube[s].avail=1;
				}
				else
				{
					SysVar.Tube[s].Val=0;
					SysVar.Tube[s].avail=2;
				}				
		   }
		   else 
		   {
			   SysVar.Tube[s].avail=0;
			   SysVar.Tube[s].Val=0;
			   SysVar.Tube[s].Ready=0;
		   }	
		   
		   //CalcCoinCRC(); //V1.26
		   	   
	  }
#endif	
	  
	  if(MDB_Changer->Level==3) 
	   MDB_Changer->NextRequest=CmdChanger_ExpIdentification; //erweiterte Info Holen
	  else 	  
	   MDB_Changer->NextRequest=CmdChanger_TubeStatus;

	  break; 	    		
	  
	case CmdChanger_TubeStatus :	  
	  MDB_Changer->TubeFullStatus=MDB_buff[1]+256*MDB_buff[0];
	  
	  
#if (TUBE_CHANGER==1)	  	  
	  if (MDB_Changer == pMDB_Changer1)
	  { 		  				 
		  
	  	for (s=0;s<MDB_MAXCOINS;s++) 
		{
			MDB_Changer->TubeStatus[s]=MDB_buff[2+s];
      		coinval= (unsigned int)MDB_Changer->CoinScaling * (unsigned int)MDB_Changer->CoinCredit[s];
			
			if (CoinOver[s].active)
			{
				// bei Mehfachwaehrung hier ersetzen ab V3.63
			    coinval= CoinOver[s].Value;
			}
							
			for (h=0;h<MDB_MAXCOINS;h++) if (MDB_Changer->PhysTubes[h]==s) break;
			
			if (h<MDB_MAXCOINS)
			{				
				if (MDB_Changer->Sequence!=Changer_Sequence_INIT)
				{
					if (!SysVar.Tube[h].Deroute) // V1.23
					{
						if (SysVar.Tube[h].Fill!=MDB_Changer->TubeStatus[s]) diff=1;
					}
				}					
				
				SysVar.Tube[h].Fill=MDB_Changer->TubeStatus[s];
				
				if (MDB_Changer->TubeStatus[s] && !MDB_Changer->Problem)
				 SysVar.Tube[h].Ready=1;
				 
  	 			if ((SelHopper<8) && (h==(SelHopper-1)))
  	 			{
					if (MDB_Changer->TubeStatus[s] && !MDB_Changer->Problem)
  	 					 SelHopperStatus=0x51;
					else SelHopperStatus=0;
  	 				SelHopperBlocked=0;
  	 			}	  	 				
				
				if (SysVar.Tube[h].Deroute) 
					 SysVar.Tube[h].avail=0;
				else
				if (SysVar.Tube[h].Prio>=MAX_PRIORITY)
					 SysVar.Tube[h].avail=2;
				else SysVar.Tube[h].avail=1;
			}						
		}

		if (diff) 
		{
			MDBEvent.Type    = EvTypeMDB_ChangerTubeStatusChanged;
			MDBEvent.Length  = 0;
			putMDBevent(&MDBEvent);
		}
		
	  }	  
#endif

	  if (MDB_Changer == pMDB_Changer2)
	  {	  	 		  
		 hChanged=0;
		  
	  	 for (s=0;s<MDB_MAXCOINS;s++) 
	  	 {
	  	 	if (s<MAX_HOPPER) 
	  	 	{
				stat=MDB_buff[2+s];
				MDB_Changer->TubeStatus[s]=stat & 0x13;
				
				if (stat!=SysVar.Hopper[s].Status) hChanged=1;
								
				if (!(SysVar.Hopper[s].Status & 0x10) && (stat & 0x10))
				{
					//Hopper eingesteckt
   	  				MDBEvent.Type    = EvTypeMDB_HopperInserted;
 		  			MDBEvent.Length  = 1;
   	   		  		MDBEvent.Data[0] = s;
					putMDBevent(&MDBEvent);
				}
				
				SysVar.Hopper[s].Status=stat;
					  	 						
				if (!(stat & 0x10)) 
				{
					SysVar.Hopper[s].Ready=0;
					//SysVar.Hopper[s].Status=0;
				}
				else
				{		  	 			  	 			  	 		
		  	 		if (stat & 0x40) 
					{
						 SysVar.Hopper[s].Blocked=0;
						 SysVar.Hopper[s].Ready=1;
					}
		  	 		else 
					{
						SysVar.Hopper[s].Blocked=1;
						SysVar.Hopper[s].Ready=0;
					}					
				}
																				
			#if (TUBE_CHANGER==1)
	  	 		if ((SelHopper>7) && ((SelHopper-8)==s))
			#else
	  	 		if (SelHopper==s)
			#endif			 
	  	 		{
	  	 			SelHopperStatus=SysVar.Hopper[s].Status;
	  	 			SelHopperBlocked=SysVar.Hopper[s].Blocked;
					SelHopperPrio=SysVar.Hopper[s].Prio;
	  	 		}	  	 							  	 		
		 	}
			else 
			{
				MDB_Changer->TubeStatus[s]=MDB_buff[2+s];
				// if (s==CD100_HOPPER) SysVar.CD100Hopper.Status=MDB_buff[2+s] & 0x77;
			}
		
			// s=13 Muenzregister 5bit   s=14 Relaiszustand bit 0-2  s=15 Eingangszustand bit 0/1 Motor�ffner bit 2-4 Optos
	  	 }	  	 	  	 
		 
		 //CalcCoinCRC(); //V1.25
	  }
	  
	  if (hChanged)
	  {		
   			MDBEvent.Type    = EvTypeMDB_HopperStatusChanged;
 		 	MDBEvent.Length  = 0;
			putMDBevent(&MDBEvent);
	  }
	  
	  MDB_Changer->NextRequest=CmdChanger_Poll;

	  if (!MDB_Changer->ready) {
			MDBEvent.Type    = EvTypeMDB_ChangerReady;
			MDBEvent.Length  = 0;
			putMDBevent(&MDBEvent);
	  }

	  MDB_Changer->ready=1;
	  MDB_Changer->Sequence=0;	  
	  		
	  break; 	    		
	  
	case CmdChanger_Poll :					//	POLL
	  
		if (buff_point>1)		// wenn 1 dann nur ACK
		{
			for (s=0;s<(buff_point-1);s++) MDB_Changer->Status[s]=MDB_buff[s];		  
	  	  	//	ANALYZE
	      	for (s=0;s<(buff_point-1);s++) 
	      	{	        	
	        	if((MDB_Changer->Status[s] & 0xC0)==0)
	          	{ 
		        	if((MDB_Changer->Status[s] & 0x30) == 0x10)
	          		{ 	
	          			if(MDB_Changer->Status[s] & 0x08)
	          			{
							hop=MDB_Changer->Status[s] & 0x07;
							if (hop<MAX_HOPPER)
							{
		          				// Zaehlung								
			   	  				MDBEvent.Type    = EvTypeMDB_HopperCounted;
	   	   		  				MDBEvent.Length  = 3;
	   	   		  				MDBEvent.Data[0] = MDB_Changer->Status[s] & 0x07;	//Hopper
	   	   		  				MDBEvent.Data[1] = MDB_Changer->Status[s+1];
	   	   		  				MDBEvent.Data[2] = MDB_Changer->Status[s+2];
	   	   		  				putMDBevent(&MDBEvent);
							}
	          			}
	          			else
	          			if ((MDB_Changer->Status[s] & 0x0F)==1)
	          			{
							hop=MDB_Changer->Status[s+2] & 0x07;
							if (hop<MAX_HOPPER)
							{
		          				SysVar.Hopper[MDB_Changer->Status[s+2] & 0x07].Blocked=1;
								//CalcCoinCRC(); //V1.25
		          				// Timeout PayOut
		          				// Z2= Rest
		          				// Z3= Hopper 0x00 - 0x04
								
			   	  				MDBEvent.Type    = EvTypeMDB_HopperTimeout;
	   	   		  				MDBEvent.Length  = 2;
	   	   		  				MDBEvent.Data[0] = MDB_Changer->Status[s+1];
	   	   		  				MDBEvent.Data[1] = MDB_Changer->Status[s+2] & 0x07;
	   	   		  				putMDBevent(&MDBEvent);
							}
	          			}
	          			else
	          			if ((MDB_Changer->Status[s] & 0x0F)==2)		//achtung erst ab V0.62 und AD3Z5 rev 1.02
	          			{
							hop=MDB_Changer->Status[s+2] & 0x07;
							if (hop<MAX_HOPPER)
							{
		          				// PaidOut
		          				// Z2= Count
		          				// Z3= Hopper 0x00 - 0x04
								
			   	  				MDBEvent.Type    = EvTypeMDB_HopperPaidOut;
	   	   		  				MDBEvent.Length  = 2;
	   	   		  				MDBEvent.Data[0] = MDB_Changer->Status[s+1];
	   	   		  				MDBEvent.Data[1] = MDB_Changer->Status[s+2] & 0x07;
	   	   		  				putMDBevent(&MDBEvent);
							}
	          			}
	          			else
	          			if ((MDB_Changer->Status[s] & 0x0F)==0)
	          			{
							hop=MDB_Changer->Status[s+2] & 0x07;
							if (hop<MAX_HOPPER)
							{
		          				// Hopper removed
		          				// Z3= Hopper 0x00 - 0x04
								
			   	  				MDBEvent.Type    = EvTypeMDB_HopperRemoved;
	   	   		  				MDBEvent.Length  = 1;
	   	   		  				MDBEvent.Data[0] = MDB_Changer->Status[s+2] & 0x07;
	   	   		  				putMDBevent(&MDBEvent);
							}
	          			}	
						
	          			s+=2;          			          				          		
	          		}
	          		else
	          		{	
	            		//one Byte status
	              	              
	            		if((MDB_Changer->Status[s])==0x0B)
	            		{  
	      					//0x0B = Changer was Reset
		             		MDB_Changer->NextRequest=CmdChanger_Status;
		             		MDB_Changer->Sequence=Changer_Sequence_INIT;
		    			}
	            		
	      				//0x01 = Escrow Request
	      				//0x02 = Payout Busy			/Payout Busy setzen
	      				//0x03 = No Credit
	      				//0x04 = Defective Tube Sensor
	      				//0x05 = Double Arrival (two Coins too fast)
	      				//0x06 = Acceptor removed from changer
	      				//0x07 = Tube JAM after payout
	      				//0x08 = Rom Checksum Error
	      				//0x09 = Coin routing Error
	      				//0x0A = Changer Busy
						//0x0B = Changer was Reset
	      				//0x0C = Coin JAM in Acceptor
													
						if(MDB_Changer->LastStatus!=MDB_Changer->Status[s])
						{							
							MDBEvent.Type    = EvTypeMDB_ChangerStatus;
							MDBEvent.Length  = 2;
							if (MDB_Changer == pMDB_Changer2)
								MDBEvent.Data[0] = 0x16;
							else MDBEvent.Data[0] = 0x10;
							MDBEvent.Data[1] = MDB_Changer->Status[s];
							putMDBevent(&MDBEvent);
						}										      										
					 
	            		MDB_Changer->LastStatus=MDB_Changer->Status[s];						
	      	    	}
	        	}
	        	else 
	     		{ 	//two Byte status
					stat=MDB_Changer->Status[s];
					mc=MDB_Changer->Status[s+1];
		      		coinval= (unsigned int)MDB_Changer->CoinScaling * (unsigned int)MDB_Changer->CoinCredit[stat & 0x0F];
					
					/*
					if (CoinOver[stat & 0x0F].active)
					{
						// bei Mehfachwaehrung hier ersetzen ab V3.63
					    coinval= CoinOver[stat & 0x0F].Value;
					}
					*/					
					
                  	//MDB_Changer->TubeStatus[stat & 0x0F]=MDB_Changer->Status[s+1]; // Anzahl Münzen
		      
	      	      	switch(stat & 0xF0)
	      	        {
	      	        case 0x40 :	//Coin In Cashbox
		          		//coinval  		Hier EVENT !!!
	                  	MDB_Changer->LastValue=coinval;
					  
	  				 	SysVar.Coin[stat & 0x0F].Count++;
						//CalcCoinCRC(); //V1.25
	                  					  	
		   	  			MDBEvent.Type    = EvTypeMDB_CoinInCashbox;
   	   		  			MDBEvent.Length  = 3;
   	   		  			MDBEvent.Data[0] = coinval;
   	   		  			MDBEvent.Data[1] = coinval>>8;
						MDBEvent.Data[2] = stat & 0x0F;
   	   		  			putMDBevent(&MDBEvent);
							                  
		          	break;
	                case 0x50 :	//Coin In Tubes
	                  //coinval  		Hier EVENT !!!
	                  	MDB_Changer->LastValue=coinval;
						sec=0;						
					  					  
					  	for (k=0;k<MDB_MAXCOINS;k++)
						 if (MDB_Changer1.PhysTubes[k]==(stat & 0x0F))
						{								
							if (SysVar.Tube[k].Deroute) 
							{
								SysVar.Hopper[SysVar.Tube[k].Deroute-1].Fill++;
								// CalcCoinCRC(); //V1.26
								sec=1;
							}
							else sec=mc-SysVar.Tube[k].Fill;
							
							SysVar.Tube[k].Fill=mc;
							MDB_Changer1.TubeStatus[stat & 0x0F]=mc; // sofort ändern														
							break;
						}
										 						 
			   	  		MDBEvent.Type    = EvTypeMDB_CoinInTube;
	   	   		  		MDBEvent.Length  = 4;
	   	   		  		MDBEvent.Data[0] = coinval;
	   	   		  		MDBEvent.Data[1] = coinval>>8;
						MDBEvent.Data[2] = stat & 0x0F;
						MDBEvent.Data[3] = sec;
							
	   	   		  		putMDBevent(&MDBEvent);
						
	                break;
	                case 0x70 :	//Coin Rejected		
								
	                  	MDB_Changer->LastValue=coinval;
						if ((stat==0x75) && (mc==0x07)) // Recycler F�llstands�nderung
						{							
			   	  			MDBEvent.Type    = EvTypeMDB_CoinRejected;
	   	   		  			MDBEvent.Length  = 4;
	   	   		  			MDBEvent.Data[0] = coinval;
	   	   		  			MDBEvent.Data[1] = coinval>>8;
							MDBEvent.Data[2] = stat;
							MDBEvent.Data[3] = mc;
							
   	   		  				putMDBevent(&MDBEvent);
						}
	                break;
	                case 0x80 :	//Coins dispensed manually
	                case 0x90 :	
	                case 0xA0 :	
	                case 0xB0 :	
	                case 0xC0 :	
	                case 0xD0 :	
	                case 0xE0 :	
	                case 0xF0 :	
											
											
					  	for (k=0;k<MDB_MAXCOINS;k++)
						 if (MDB_Changer1.PhysTubes[k]==(stat & 0x0F))
						{					  											 
							//MDB_Changer->NextRequest=CmdChanger_TubeStatus;
							if (mc) mc-=((stat & 0x70)>>4);
							SysVar.Tube[k].Fill=mc;
							MDB_Changer1.TubeStatus[stat & 0x0F]=mc; // sofort �ndern
							break;
						}						
						
		   	  			MDBEvent.Type    = EvTypeMDB_CoinDispensedManually;
   	   		  			MDBEvent.Length  = 4;
   	   		  			MDBEvent.Data[0] = coinval;
   	   		  			MDBEvent.Data[1] = coinval>>8;
						MDBEvent.Data[2] = stat & 0x0F;
						MDBEvent.Data[3] = ((stat & 0x70)>>4);

   	   		  			putMDBevent(&MDBEvent);						
					
	                break;
	      	        }
	      	      	s++;
	          	} //end two Byte Status 
	        } //von for			
	    } //von buff_point>1
	  	else
	  	{
			//ACK only

	  		//Payout Busy l�schen
	  		MDB_Changer->LastStatus = 0;
	  		MDB_Changer->Sequence=0;
			
			// noch etwas auszugeben ?	  
 	  		coinval=0;	
          	for (s=0;s<32;s++) coinval=coinval+PayOutTubes[s].TubeOut;
          	
          	if (coinval !=0) 
          	 MDB_Changer->NextRequest=CmdChanger_Dispense;
          	else 
          	 MDB_Changer->NextRequest=CmdChanger_Poll;			
	  	}  
	  	  	
	  	if(!MDB_Changer->Sequence)
	  	{	  
	  		if(MDB_Changer->NextRequest == CmdChanger_Poll)
	  		{								
				if (MDB_Changer->LastStatus!=0x02) // V1.23 nicht w�hrend Ausgabe !!!
				{
		  			if(MDB_Changer->NextTubestatus>0)
		    		{		      		
						MDB_Changer->NextTubestatus--;
					
						if (MDB_Changer == pMDB_Changer1) 												
						{							
							if(MDB_Changer->NextDiag>0)
							{							
								MDB_Changer->NextDiag--;
							}
							else
							{
								if (MDB_Changer->OptionalFeatures && 0x00000002)	//ab V2.89
								{								
			      					MDB_Changer->NextDiag=DiagRepeat;
			      					MDB_Changer->NextRequest=CmdChanger_ExpSendDiagnosticStatus;
								}
							}
						}
			    	}
		  			else
		    		{
						MDB_Changer->NextTubestatus=TubeStatusRepeat; // every 5 polls
						MDB_Changer->NextRequest=CmdChanger_TubeStatus;  // regelm�ssige Abfrage						
						//Sound=0x01;
											
						if (MDB_Changer == pMDB_Changer1) 
						{		
							if (MDB_Changer->Toggle)
							{
								MDB_Changer->Toggle=0;
																			
								#if (CHANGER_EXPANSION==1)
								if (MDB_Changer->Version>=273) // ab Version 1.11 BCD => 256+16+1 = 273
								{
									// Progstate Hopper beachten
									if (!MDB_Changer->SetTubes)
									{
										for (s=0;s<6;s++) MDB_Changer1.TubeCountAF[s]=0xFF;
										for (s=0;s<2;s++) MDB_Changer1.HopperCount[s]=0xFFFF;
									}
									else MDB_Changer->SetTubes=0;
							
									MDB_Changer->DiagnosticCommand=ExpDiag_SetTubeCounter;
									MDB_Changer->NextRequest=CmdChanger_ExpDiagnostic;							
								}
								#else
								if (strncmp(MDB_Validator.Model,"B2B",3)==0) // ab V2.89							
								{
									//Recycler
									MDB_Changer->Toggle=0;
									MDB_Changer->DiagnosticCommand=ExpDiag_RecycledBill;
									MDB_Changer->NextRequest=CmdChanger_ExpDiagnostic;							
								}						
								#endif
							}
							else MDB_Changer->Toggle=1;							
						}
		    		} 
				}
				else
				{
					MDB_Changer->NextTubestatus=TubeStatusRepeat;
					MDB_Changer->NextDiag=DiagRepeat;
				}
	  		}
	  	}	              
	  break; 	    		
	case CmdChanger_CoinType :	//M�nzfreigabe gesendet
		#if (CHANGER_EXPANSION==1)
		if (MDB_Changer == pMDB_Changer1)
		{
			MDB_Changer->DiagnosticCommand=ExpDiag_SuppressCoinRouting;
			MDB_Changer->NextRequest=CmdChanger_ExpDiagnostic;
		}	
		else 
		#endif		
	  if (MDB_Changer->Sequence==Changer_Sequence_INIT)
	    MDB_Changer->NextRequest=CmdChanger_TubeStatus;
	  else
	    MDB_Changer->NextRequest=CmdChanger_Poll;		
	  break; 	    		
	case CmdChanger_Dispense :
  	  //Payout Busy l�schen
	  MDB_Changer->LastStatus = 0;
	  if (MDB_Changer == pMDB_Changer1) 
       	   MDB_Changer->NextRequest=CmdChanger_Poll;
	  else MDB_Changer->NextRequest=CmdChanger_Dispense;
	  break; 	    		
	case CmdChanger_ExpIdentification :
          MDB_Changer->Manufacturer[0]=MDB_buff[0];
	  MDB_Changer->Manufacturer[1]=MDB_buff[1];
	  MDB_Changer->Manufacturer[2]=MDB_buff[2];
	  MDB_Changer->Manufacturer[3]=0;		/*D*/
	  for (s=0;s<12;s++) MDB_Changer->Serial[s]=MDB_buff[3+s];
	  MDB_Changer->Serial[12]=0;			/*D*/
	  for (s=0;s<12;s++) MDB_Changer->Model[s]=MDB_buff[15+s];
	  MDB_Changer->Model[12]=0;			/*D*/
	  MDB_Changer->Version=MDB_buff[28]+256*MDB_buff[27];
	  MDB_Changer->OptionalFeatures=(MDB_buff[29]*0x1000000)+(MDB_buff[30]*0x10000)+(MDB_buff[31]*0x100)+MDB_buff[32];
	  if (MDB_Changer->OptionalFeatures)	//ab V2.84
	  {
	  	 MDB_Changer->FeatureEnable=MDB_Changer->OptionalFeatures;
         MDB_Changer->NextRequest=CmdChanger_ExpFeatureEnable;
	  }
	  else MDB_Changer->NextRequest=CmdChanger_CoinType;
	  break; 	    		
	case CmdChanger_ExpFeatureEnable :
          MDB_Changer->NextRequest=CmdChanger_CoinType; //und weiter    		
	  break; 	    		
	case CmdChanger_ExpPayout :
          MDB_Changer->NextRequest=CmdChanger_Poll;    		
	  break; 	    		
	case CmdChanger_ExpPayoutStatus	:
	  for (s=0;s<16;s++) MDB_Changer->PayoutStatus[s]=MDB_buff[s];
          MDB_Changer->NextRequest=CmdChanger_Poll;    		
	  break; 	    		
	case CmdChanger_ExpPayoutValuePoll :
	  MDB_Changer->PayoutValuePoll=MDB_buff[0]; 
	  MDB_Changer->NextRequest=CmdChanger_Poll;    		
	  break; 	    		
	case CmdChanger_ExpSendDiagnosticStatus	:
	
		MDB_Changer->ready=1;
		
		for (s=0;s<(buff_point-1);s++) MDB_Changer->SendDiagnosticStatus[s]=MDB_buff[s];
		
		err=0;
		
      	for (s=0;s<(buff_point-1);s+=2)
      	{	        				
        	switch(MDB_buff[s])
          	{
			case 0x01:		// Powering up
				MDB_Changer->Problem=7;
			break;
			case 0x02:		// Powering down
				MDB_Changer->Problem=8;
			break;
			case 0x03:		// OKAY
				MDB_Changer->Problem=0;
			break;				
			case 0x04:		// Keypad Shift
			break;				
			case 0x05:		// Manual Fill
			break;				
			case 0x06:		// Inhibited
			break;				
			case 0x10:		// General Changer Error
				MDB_Changer->Problem=1;
				err=1;
			break;				
			case 0x11:		// Discriminator module error
				MDB_Changer->Problem=2;
				err=1;
			break;				
			case 0x12:		// Accept gate module error
				MDB_Changer->Problem=3;
				err=1;
			break;				
			case 0x13:		// Seperator module error
				MDB_Changer->Problem=4;
				err=1;
			break;				
			case 0x14:		// Dispenser module error
				MDB_Changer->Problem=5;
				err=1;
			break;				
			case 0x15:		// Coin cassette or tube module error
				MDB_Changer->Problem=6;
				err=1;				
			break;								
			}	
				
			//if (MDB_Changer->Problem)
			{									
				if (MDB_Changer->LastProblem!=MDB_Changer->Problem)
				{
					
	   	  			MDBEvent.Type    = EvTypeMDB_ChangerProblem;
	   		  		MDBEvent.Length  = 2;
	   		  		MDBEvent.Data[0] = MDB_Changer->Problem;
	   		  		MDBEvent.Data[1] = MDB_buff[s+1];
	   		  		putMDBevent(&MDBEvent);
				}		
			}
			
			MDB_Changer->LastProblem=MDB_Changer->Problem;
		}
		
		if (!err) MDB_Changer->Problem=0;
	
        MDB_Changer->NextRequest=CmdChanger_Poll;    		                  
	  break; 	    		
	case CmdChanger_ExpSendControlledManualFillReport :
	  		//for (s=0;s<16;s++) MDB_Changer->SendControlledManualFillReport[s]=MDB_buff[s];
          MDB_Changer->NextRequest=CmdChanger_Poll;    		
	  break; 	    		
	case CmdChanger_ExpSendControlledManualPayoutReport :
	  		//for (s=0;s<16;s++) MDB_Changer->SendControlledManualPayoutReport[s]=MDB_buff[s];
          MDB_Changer->NextRequest=CmdChanger_Poll;    		
	  break; 	    		
	case CmdChanger_ExpDiagnostic :
	  	if (MDB_Changer == pMDB_Changer1)
	  	{	
			switch(MDB_Changer->DiagnosticCommand)
			{
			case ExpDiag_SuppressCoinRouting:
				MDB_Changer->NextRequest=CmdChanger_Poll;
			break;
			case ExpDiag_SetHopperCounter:
				for (s=0;s<(buff_point-1);s++) 	
				{
					if ((s%3)==0) 
					{
						ctype=MDB_buff[s];
						MDB_Changer->CoinTypeHopper[s/3]=ctype;
					}
					
					if (ctype!=0xFF)
					{
						if ((s%3)==1) fill=MDB_buff[s]*256;
						if ((s%3)==2) 
						{
							fill+=MDB_buff[s];
							MDB_Changer->TempFill[ctype]+=fill;
						}					
					}
				}		
												
				MDB_Changer->DiagnosticCommand=ExpDiag_TubeStatus;
				MDB_Changer->NextRequest=CmdChanger_ExpDiagnostic;				
						
			break;
			case ExpDiag_SetTubeCounter:		
				for (s=0;s<MDB_MAXCOINS;s++) MDB_Changer->TempFill[s]=0;
			
				for (s=0;s<(buff_point-1);s++) 	
				{
					if ((s%2)==0) 
					{
						ctype=MDB_buff[s];
						MDB_Changer->CoinTypeAF[s/2]=ctype;
					}
					
					if (ctype!=0xFF)
					{
						if ((s%2)==1) MDB_Changer->TempFill[ctype]+=MDB_buff[s];
					}
				}				
								
				MDB_Changer->DiagnosticCommand=ExpDiag_SetHopperCounter;
				MDB_Changer->NextRequest=CmdChanger_ExpDiagnostic;				
			break;
			case ExpDiag_TubeStatus:
			
				// summierte Fuellstaende setzen
	  			for (s=0;s<MDB_MAXCOINS;s++) 
				{						
					if (MDB_Changer->TempFill[s]>=MDB_buff[s+MDB_MAXCOINS])
						 MDB_Changer->TempFill[s]-=MDB_buff[s+MDB_MAXCOINS];
					else MDB_Changer->TempFill[s]=0;
					MDB_Changer->SafeCoins[s]=MDB_buff[s+MDB_MAXCOINS];
					
					for (h=0;h<MDB_MAXCOINS;h++) if (MDB_Changer->PhysTubes[h]==s) break;
			
					if (h<MDB_MAXCOINS)
					{			
						if (SysVar.Tube[h].Fill!=MDB_Changer->TempFill[s]) diff=1;
						
				 		SysVar.Tube[h].Fill=MDB_Changer->TempFill[s];
				
						if (MDB_Changer->TempFill[s] && !MDB_Changer->Problem)
				 		 SysVar.Tube[h].Ready=1;

		  	 			if ((SelHopper<8) && (h==(SelHopper-1)))
		  	 			{
							if (MDB_Changer->TubeStatus[s] && !MDB_Changer->Problem)
		  	 					 SelHopperStatus=0x51;
							else SelHopperStatus=0;
		  	 				SelHopperBlocked=0;
		  	 			}	  	 				
				
						if (SysVar.Tube[h].Deroute) 
							 SysVar.Tube[h].avail=0;
						else
						if (SysVar.Tube[h].Prio>=MAX_PRIORITY)
							 SysVar.Tube[h].avail=2;
						else SysVar.Tube[h].avail=1;
					}
				}
				
				MDBEvent.Type    = EvTypeMDB_TubesNewFillStatus;
				MDBEvent.Length  = 2;
				MDBEvent.Data[0] = 0x24;
				MDBEvent.Data[1] = diff;
				putMDBevent(&MDBEvent);
			
				MDB_Changer->NextRequest=CmdChanger_Poll;				
			break;
			case ExpDiag_RecycledBill:
			
				MDB_Changer->NextRequest=CmdChanger_Poll;
			
				if ((MDB_buff[0]=='N') && (MDB_buff[1]=='R') && (MDB_buff[2]=='I'))
				{
					switch((MDB_buff[3] & 0xF0))
					{
					case 0x10:	//Bill to Recycler
					break;
					case 0x20:	//Manual fill
					break;
					case 0x40:	//Bill dispensed as change
					break;
					case 0x60:	//Bill dispensed manually
					break;
					case 0x70:	//Bill Stacked manually
						coinval= (unsigned int)MDB_Changer->CoinScaling * (unsigned int)MDB_Changer->CoinCredit[MDB_buff[3] & 0x0F];
						
						if (CoinOver[MDB_buff[3] & 0x0F].active)
						{
							// bei Mehfachwaehrung hier ersetzen ab V3.63
						    coinval= CoinOver[MDB_buff[3] & 0x0F].Value;
						}						
					
						for (h=0;h<MDB_MAXCOINS;h++) 
						 if (coinval==SysVar.BillCredit[h]) 
						 {						
							SysVar.Bill[h].Count+=MDB_buff[4];
							break;
						 }																
				
						// CalcCoinCRC(); //V1.25

						
		   	  			MDBEvent.Type    = EvTypeMDB_BillStackedManually;
   	   		  			MDBEvent.Length  = 4;
   	   		  			MDBEvent.Data[0] = coinval;
   	   		  			MDBEvent.Data[1] = coinval>>8;
						MDBEvent.Data[2] = MDB_buff[4];
						MDBEvent.Data[3] = MDB_buff[3] & 0x0F;
   	   		  			putMDBevent(&MDBEvent);				
						
						MDB_Changer->NextRequest=CmdChanger_TubeStatus;												 
					break;
					}
				}			
			
			break;
			default:
				MDB_Changer->NextRequest=CmdChanger_Poll;
			break;
			}          	
		}
	  break; 	    		
    }		
}  
//------------------------------CARDREADER----------------------------------------------
void RX_Handle_Cardreader(unsigned char buff_point)
{	
  unsigned int s;
    
  MDB_Cardreader.DevLost=MDB_LostCount;
  MDB_Cardreader.NonResponseCount=MDB_Cardreader.NonResponseTime*1000;
  MDB_Cardreader.NextRequest=CmdCardreader_Poll; 
  switch(MDB_Cardreader.LastRequest)
    {
	case CmdCardreader_Reset :
	  MDB_Cardreader.ResetTime=Cardreader_SetupTime;		//Wartezeit 	  
	  break;
	case CmdCardreader_SetupConfigData :
	  Handle_Cardreader_Status(0,buff_point);		// ReaderConfigData expected
	  break;
	case CmdCardreader_SetupMaxMinPrice :
	  if(MDB_Cardreader.FeatureEnable & 0x06) 
	  {
		  MDB_Cardreader.DiagnosticCommand=ExpDiag_AgeVerificationOnOff;	  
		  MDB_Cardreader.NextRequest=CmdCardreader_ExpDiagnostic; //INIT STEP 6 LAST LEVEL 3+
	  }
	  else MDB_Cardreader.NextRequest=CmdCardreader_ExpRequestID; // INIT STEP 3
	  break;
    case CmdCardreader_Poll :
          if (buff_point>1) 
            {
              for (s=0;s<35;s++) MDB_Cardreader.TMPStatus[s]=0;
              for (s=0;s<(buff_point-1);s++) MDB_Cardreader.TMPStatus[s]=MDB_buff[s];
              for (s=0;s<(buff_point-1);s++) s=Handle_Cardreader_Status(s,buff_point);
               //Handle_Cardreader_Status(0,buff_point);  //nur ein Ergebnisb
            }               
	  break;
	case CmdCardreader_VendRequest :
	  Handle_Cardreader_Status(0,buff_point);		// VendApproved/Denied expected 
	  break;
	case CmdCardreader_VendCancel :
	  Handle_Cardreader_Status(0,buff_point);		// VendDenied expected 
	  break;
	case CmdCardreader_VendSuccess :
	  MDB_Cardreader.NextRequest=CmdCardreader_VendSessionComplete;
	  break;
	case CmdCardreader_VendFailiure :
	  MDB_Cardreader.NextRequest=CmdCardreader_VendSessionComplete;
	  break;	  
        case CmdCardreader_VendSessionComplete :
	  Handle_Cardreader_Status(0,buff_point);		// EndSession expected 
	  break;
	//case CmdCardreader_VendCashSale :
	//  break;
	case CmdCardreader_VendNegative :
	  Handle_Cardreader_Status(0,buff_point);		// VendApproved expected 
	  break;
	//case CmdCardreader_ReaderDisable :
	//  break;
	case CmdCardreader_ReaderEnable :
	  //MDB_Cardreader.ready=1;		  	  	  
	  // SystemConfig |=0x08;
	  break;
	case CmdCardreader_ReaderCancel :
	  Handle_Cardreader_Status(0,buff_point);		// Cancelled expected 
	  break;
	//case CmdCardreader_ReaderDataEntryResponse :
	//  break;
	case CmdCardreader_RevalueRequest :
	  Handle_Cardreader_Status(0,buff_point);		// Revalue Approved/Denied expected 
	  break;
	case CmdCardreader_RevalueLimitRequest :
	  Handle_Cardreader_Status(0,buff_point);		// Revalue Limit Amount expected 
	  break;
	case CmdCardreader_ExpRequestID :
	  Handle_Cardreader_Status(0,buff_point);		// Peripheral ID expected 
	  break;
	case CmdCardreader_ExpReadUserFile :
	  Handle_Cardreader_Status(0,buff_point);		// UserDataFile ID expected
	  break;
	case CmdCardreader_ExpWriteTimeDate :			
	
	end_rx_init1:
	
		if(MDB_Cardreader.Sequence==Cardreader_Sequence_INIT)
		{
		}
		
		MDB_Cardreader.Sequence=0;			//ausstiegspunkt INIT ?
		
		if(MDB_Cardreader.Inhibit)
			MDB_Cardreader.NextRequest=CmdCardreader_ReaderDisable;
		else
			MDB_Cardreader.NextRequest=CmdCardreader_ReaderEnable;	  
			
		MDB_Cardreader.Inhibit_Old  = MDB_Cardreader.Inhibit;	//RRR
		break;	 
	case CmdCardreader_ExpOptionalFeatureEnabled :
	  if(MDB_Cardreader.FeatureEnable & 0x06) 
	  {
		  MDB_Cardreader.NextRequest=CmdCardreader_SetupMaxMinPrice; //INIT STEP 5 LEVEL 3+
	  }
	  else 
	  {
		  goto end_rx_init1;
		  
		  //MDB_Cardreader.DiagnosticCommand=ExpDiag_AgeVerificationOnOff;	  
		  //MDB_Cardreader.NextRequest=CmdCardreader_ExpDiagnostic; // INIT STEP Last no EXP Feature
	  }
	  break;	  
	case CmdCardreader_ExpDiagnostic :
	Handle_Cardreader_Status(0,buff_point);		// DRAVP expected	
	  break;
     }	  

}


unsigned char Handle_Cardreader_Status(unsigned char buffpos,unsigned char bufflen)
{
  unsigned int s,p;
  unsigned long hi;
  unsigned char lo;
  char amt[16];

  MDB_EVENT MDBEvent;
    
  if (buffpos>=(bufflen-1)) return 0xFF;	// is an END
  switch(MDB_buff[buffpos])	//Position auf STATUSBYTE
    {

	case CardreaderStatus_JustReset :
	  MDB_Cardreader.NextRequest=CmdCardreader_SetupConfigData;	// INIT STEP 1
	  break;
	case CardreaderStatus_ReaderConfigData :
	  MDB_Cardreader.Level=MDB_buff[buffpos+1]; 
 	  MDB_Cardreader.CountryCode=(MDB_buff[buffpos+2]*0x100)+MDB_buff[buffpos+3]; 		
 	  MDB_Cardreader.Scaling=MDB_buff[buffpos+4]; 
 	  MDB_Cardreader.Decimals=MDB_buff[buffpos+5]; 
 	  MDB_Cardreader.NonResponseTime=MDB_buff[buffpos+6];
 	  MDB_Cardreader.MiscOptions=MDB_buff[buffpos+7];
	  buffpos=buffpos+7;
	  MDB_Cardreader.NextRequest=CmdCardreader_SetupMaxMinPrice;	// INIT STEP 2
	  break;
	case CardreaderStatus_DisplayRequest :
	  ClearCardreaderDisplay();
	  MDB_Cardreader.DisplayTime=MDB_buff[buffpos+1]; 
	  p=0;	  
	  for (s=0;s<(SetupConfigData_Columns*SetupConfigData_Rows);s++) 
	  {
		  MDB_Cardreader.DisplayData[p++]=MDB_buff[buffpos+2+s];
		  if (p==SetupConfigData_Columns) MDB_Cardreader.DisplayData[p++]=0x0D;
	  }
	  MDB_Cardreader.DisplayData[p]=0;
	  
	  MDB_Cardreader.DisplayCount=MDB_Cardreader.DisplayTime*200;
	  	  	
	    MDBEvent.Type    = EvTypeMDB_CardreaderDisplay;			// DisplayEvent
		MDBEvent.Length  = (SetupConfigData_Columns*SetupConfigData_Rows)+1;
		for (s=0;s<(SetupConfigData_Columns*SetupConfigData_Rows);s++) MDBEvent.Data[s]=MDB_buff[buffpos+2+s];
		MDBEvent.Data[s]= 0;		
   	   	putMDBevent(&MDBEvent);		
		
	  buffpos=buffpos+(SetupConfigData_Columns*SetupConfigData_Rows)+1;		
	  
	  break;
	case CardreaderStatus_BeginSession :
	
	  	if(MDB_Cardreader.FeatureEnable & 0x06) 	//EXPANDED CURRENCY MODE
	    {
	      MDB_Cardreader.FundsAvailable=(MDB_buff[buffpos+1]*0x1000000)+(MDB_buff[buffpos+2]*0x10000)+(MDB_buff[buffpos+3]*0x100)+MDB_buff[buffpos+4];
	      MDB_Cardreader.DispFundsAvailable=MDB_Cardreader.FundsAvailable*MDB_Cardreader.Scaling;
	      buffpos=buffpos+4;
	    }
	    else
	    {
	      MDB_Cardreader.FundsAvailable=((long)MDB_buff[buffpos+1]*0x100)+(long)MDB_buff[buffpos+2];
	      MDB_Cardreader.DispFundsAvailable=MDB_Cardreader.FundsAvailable*MDB_Cardreader.Scaling;
	      buffpos=buffpos+2;
	    }
		
		if (MDB_Cardreader.DispFundsAvailable==0xFFFFFFFF)
		{
			// unbegrenzt geld
			MDB_Cardreader.DispFundsAvailable=999999;
		}		
		
	  	if(MDB_Cardreader.Level>1)
	    { 
	      	buffpos++;
	      	MDB_Cardreader.PaymentMediaID.B[3]=MDB_buff[buffpos];
	      	MDB_Cardreader.PaymentMediaID.B[2]=MDB_buff[buffpos+1];
	      	MDB_Cardreader.PaymentMediaID.B[1]=MDB_buff[buffpos+2];
	      	MDB_Cardreader.PaymentMediaID.B[0]=MDB_buff[buffpos+3];
	      	MDB_Cardreader.PaymentType=MDB_buff[buffpos+4]; 
	      	MDB_Cardreader.PaymentData[0]=MDB_buff[buffpos+5]; 
	      	MDB_Cardreader.PaymentData[1]=MDB_buff[buffpos+6]; 
	      	buffpos=buffpos+6;
		  
	      	if(MDB_Cardreader.FeatureEnable & 0x06)
	      	{ 
	          	buffpos++;
		  	  	MDB_Cardreader.UserLanguage[0]=MDB_buff[buffpos]; 
	          	MDB_Cardreader.UserLanguage[1]=MDB_buff[buffpos+1];
		      	MDB_Cardreader.UserCurrencyCode[0]=MDB_buff[buffpos+2]; 
	          	MDB_Cardreader.UserCurrencyCode[1]=MDB_buff[buffpos+3];	          
	          	MDB_Cardreader.UserCardOptions=MDB_buff[buffpos+4];	          
	          	buffpos=buffpos+4;
	        }
	    }
		
		//varMoney(amt, (long *)&MDB_Cardreader.DispFundsAvailable, MainCurrency, 0);					
		sprintf(MDB_Cardreader.DisplayData,"%d",MDB_Cardreader.DispFundsAvailable);
		MDB_Cardreader.DisplayCount=0;
						
    	MDBEvent.Type    = EvTypeMDB_CardreaderDisplay;			// DisplayEvent
		MDBEvent.Length  = strlen(MDB_Cardreader.DisplayData)+1;
		for (s=0;s<strlen(MDB_Cardreader.DisplayData);s++) MDBEvent.Data[s]=MDB_Cardreader.DisplayData[s];
		MDBEvent.Data[s]= 0;		
   	   	putMDBevent(&MDBEvent);				
	
    	MDBEvent.Type    = EvTypeMDB_CardreaderFunds;			// FundsEvent
		MDBEvent.Length  = 9;
		MDBEvent.Data[0] = (MDB_Cardreader.DispFundsAvailable & 0xFF);
		MDBEvent.Data[1] = ((MDB_Cardreader.DispFundsAvailable >> 8) & 0xFF);
		MDBEvent.Data[2] = ((MDB_Cardreader.DispFundsAvailable >> 16) & 0xFF);
		MDBEvent.Data[3] = ((MDB_Cardreader.DispFundsAvailable >> 24) & 0xFF);
		MDBEvent.Data[4] = MDB_Cardreader.PaymentMediaID.B[0];
		MDBEvent.Data[5] = MDB_Cardreader.PaymentMediaID.B[1];
		MDBEvent.Data[6] = MDB_Cardreader.PaymentMediaID.B[2];
		MDBEvent.Data[7] = MDB_Cardreader.PaymentMediaID.B[3];
		MDBEvent.Data[8] = MDB_Cardreader.PaymentType;		
   	   	putMDBevent(&MDBEvent);
		
		MDB_Cardreader.Sequence=Cardreader_Sequence_SESSION;
		
	  break;
	case CardreaderStatus_SessionCancelRequest :
	    MDB_Cardreader.NextRequest=CmdCardreader_VendSessionComplete; // Karte gezogen Session Comlpete
	  break;
	case CardreaderStatus_VendApproved :
	
	  	if(MDB_Cardreader.FeatureEnable & 0x06) 	//EXPANDED CURRENCY MODE
	    {
	      	MDB_Cardreader.VendAmount=(MDB_buff[buffpos+1]*0x1000000)+(MDB_buff[buffpos+2]*0x10000)+(MDB_buff[buffpos+3]*0x100)+MDB_buff[buffpos+4];
            buffpos=buffpos+4;
	    }
	    else
	    {
	      	MDB_Cardreader.VendAmount=(MDB_buff[buffpos+1]*0x100)+MDB_buff[buffpos+2];
	      	buffpos=buffpos+2;
	    }	
		
		MDB_Cardreader.DispFundsAvailable-=MDB_Cardreader.VendAmount;
		
		//DoPaying
		
	   	MDBEvent.Type    = EvTypeMDB_CardreaderVendApproved;			// Geldkarte VendApproved
 	   	MDBEvent.Length  = 4;
   	   	MDBEvent.Data[0] = MDB_Cardreader.VendAmount;
   	   	MDBEvent.Data[1] = MDB_Cardreader.VendAmount>>8;
		MDBEvent.Data[2] = MDB_Cardreader.VendAmount>>16;
		MDBEvent.Data[3] = MDB_Cardreader.VendAmount>>24;			  
   	    putMDBevent(&MDBEvent);
		
		MDB_Cardreader.NextRequest=CmdCardreader_VendSuccess;
		
	  break;
	case CardreaderStatus_VendDenied :
		MDB_Cardreader.Sequence=Cardreader_Sequence_SESSION;		//zurück zur Session	
		MDB_Cardreader.NextRequest=CmdCardreader_VendSessionComplete;	
		
	   	MDBEvent.Type    = EvTypeMDB_CardreaderVendDenied;			// Geldkarte VendDenied
 	   	MDBEvent.Length  = 4;
   	   	MDBEvent.Data[0] = MDB_Cardreader.VendAmount;
   	   	MDBEvent.Data[1] = MDB_Cardreader.VendAmount>>8;
		MDBEvent.Data[2] = MDB_Cardreader.VendAmount>>16;
		MDBEvent.Data[3] = MDB_Cardreader.VendAmount>>24;			  
   	    putMDBevent(&MDBEvent);
		
	  break;
	case CardreaderStatus_EndSession :
		if (MDB_Cardreader.Inhibit) MDB_Cardreader.NextRequest=CmdCardreader_ReaderDisable;
	
		MDB_Cardreader.Sequence = 0;		//Cancel Session zurücknehmen 		
	  break;
	case CardreaderStatus_Cancelled	:
		if(MDB_Cardreader.Sequence & Cardreader_Sequence_CANCELREADER) 
		{
			if(MDB_Cardreader.Sequence & Cardreader_Sequence_SESSION)  	//RRR falls noch andere session
			{
				MDB_Cardreader.NextRequest=CmdCardreader_VendSessionComplete;
				MDB_Cardreader.Sequence = 0;
			}
			else
			{
				if (MDB_Cardreader.Inhibit) MDB_Cardreader.NextRequest=CmdCardreader_ReaderDisable; //RRR dann erst disablen
				MDB_Cardreader.Sequence = 0;
			}
		}
			 
	  	break;
	case CardreaderStatus_PeripheralID :
	  MDB_Cardreader.Manufacturer[0]=MDB_buff[buffpos+1];
	  MDB_Cardreader.Manufacturer[1]=MDB_buff[buffpos+2];
	  MDB_Cardreader.Manufacturer[2]=MDB_buff[buffpos+3];
	  for (s=0;s<12;s++) MDB_Cardreader.Serial[s]=MDB_buff[buffpos+4+s];
	  for (s=0;s<12;s++) MDB_Cardreader.Model[s]=MDB_buff[buffpos+16+s];
	  MDB_Cardreader.Version=MDB_buff[29]+256*MDB_buff[28];
	  buffpos=buffpos+29;	  
	  
	  if( (MDB_Cardreader.Level==3) && ((bufflen-buffpos)>4) )
	  {
	      buffpos++;
	      MDB_Cardreader.OptionalFeatures=(MDB_buff[buffpos]*0x1000000)+(MDB_buff[buffpos+1]*0x10000)+(MDB_buff[buffpos+2]*0x100)+MDB_buff[buffpos+3];
          MDB_Cardreader.FeatureEnable=ExandedCurrencyMode && MDB_Cardreader.OptionalFeatures; //32 Bit Betr�ge wenn m�glich
	      MDB_Cardreader.NextRequest=CmdCardreader_ExpOptionalFeatureEnabled; //INIT STEP 4 LEVEL 3
	      buffpos=buffpos+3;	      
	    }
	  else 
	  {
		  //keine AVD-Funktion
	  	  MDB_Cardreader.Sequence=0;			//ausstiegspunkt INIT ?
	  	  if(MDB_Cardreader.Inhibit)
		   	MDB_Cardreader.NextRequest=CmdCardreader_ReaderDisable;
	  	  else
		  	MDB_Cardreader.NextRequest=CmdCardreader_ReaderEnable;		  
	  }
	  
	  MDB_Cardreader.ready=1;		  	  	  
	  

	  	// Hier Ready Event
		
	  	MDBEvent.Type    = EvTypeMDB_CardreaderReady;			
 	   	MDBEvent.Length  = 0;
   	    putMDBevent(&MDBEvent);
	  
	  break;
	case CardreaderStatus_Malfunction :
	
	  	MDB_Cardreader.ErrorCode=MDB_buff[buffpos+1]; 
	  	buffpos=buffpos++;
	  
	  	// Hier Error Event
		
	  	MDBEvent.Type    = EvTypeMDB_CardreaderError;			
 	   	MDBEvent.Length  = 1;
   	   	MDBEvent.Data[0] = MDB_Cardreader.ErrorCode;
   	    putMDBevent(&MDBEvent);
	  
	  break;
	case CardreaderStatus_CmdOutOfSequence :
	
	  	if(MDB_Cardreader.Level>1)
	    {
	      MDB_Cardreader.Status=MDB_buff[buffpos+1]; 
	      buffpos=buffpos+1;
	      InitCardreader();
	    }
          else InitCardreader();
	  break;
	case CardreaderStatus_RevalueApproved :
   	  //buffpos++;			//und weiter
	  break;
	case CardreaderStatus_RevalueDenied :
          //buffpos++;			//und weiter
	  break;
	case CardreaderStatus_RevalueLimitAmount :
	
	  	if(MDB_Cardreader.FeatureEnable & 0x06) 	//EXPANDED CURRENCY MODE
	    {
	      MDB_Cardreader.RevalueLimitAmount=(MDB_buff[buffpos+1]*0x1000000)+(MDB_buff[buffpos+2]*0x10000)+(MDB_buff[buffpos+3]*0x100)+MDB_buff[buffpos+4];
	      buffpos=buffpos+4;
	    }
	    else
	    {
	      MDB_Cardreader.RevalueLimitAmount=(MDB_buff[buffpos+1]*0x100)+MDB_buff[buffpos+2];
	      buffpos=buffpos+2;
	    }		
	  break;
	case CardreaderStatus_UserFileData :
	  break;
	case CardreaderStatus_TimeDateRequest :
	  MDB_Cardreader.NextRequest=CmdCardreader_ExpWriteTimeDate;
	  								//und Zeit Senden
	  break;
	case CardreaderStatus_DataEntryRequest :
	  MDB_Cardreader.DataEntryLenRep=MDB_buff[buffpos+1]; 
	  buffpos=buffpos++;
	  break;
	case CardreaderStatus_DataEntryCancel :
	  break;
	case CardreaderStatus_DiagnosticResponse :
	  switch(MDB_buff[buffpos+1])
	  {
	  case ExpDiag_AgeVerificationOnOff: //DRAVP	  
	  	if(MDB_buff[buffpos+2]==0x06)
	  	{
			MDB_Cardreader.AVSFeature1=MDB_buff[buffpos+3]; 
	  		for (s=0;s<5;s++) MDB_Cardreader.DRAV[s]=MDB_buff[buffpos+4+s];
			buffpos+=8;
		  	MDB_Cardreader.DiagnosticCommand=ExpDiag_ConfigureVendProcess;
		  	MDB_Cardreader.NextRequest=CmdCardreader_ExpDiagnostic;
	  	}
	  break;
	  case ExpDiag_AgeVerificationStatus: //DRAVS
	  	if(MDB_buff[buffpos+2]==0x07)
	  	{
			MDB_Cardreader.AVSFeature1=MDB_buff[buffpos+3];
			MDB_Cardreader.AVSFeature2=MDB_buff[buffpos+4];
	  		//for (s=0;s<5;s++) MDB_Cardreader.DRAV[s]=MDB_buff[buffpos+5+s];
			buffpos+=9;
			
			if (!(MDB_Cardreader.AVSFeature1 & 0x40))	//Age Verificaton in Progress
			{
				// if (MDB_Cardreader.AVSFeature1 & 0x10) AgeVerified=1;
				
		    	MDBEvent.Type    = EvTypeMDB_CardreaderAgeVerificationStatus;			// DisplayEvent
				MDBEvent.Length  = 2;
				MDBEvent.Data[0]= MDB_Cardreader.AVSFeature1;
				MDBEvent.Data[1]= MDB_Cardreader.AVSFeature2;		
		   	   	putMDBevent(&MDBEvent);
			}
	  	}
	  break;
	  case ExpDiag_ConfigureVendProcess: //CFGVP
	  	if(MDB_buff[buffpos+2]==0x06)
	  	{
	  		MDB_Cardreader.AVSConfiguration=MDB_buff[buffpos+3]; 
			MDB_Cardreader.NextRequest=CmdCardreader_ExpWriteTimeDate;
		}
	  break;
	  }
	break;
    }	
  return buffpos;      
}
//------------------------------VALIDATOR----------------------------------------------
void RX_Handle_Validator(unsigned char buff_point)
{	
  unsigned int s,p;
  unsigned int BillBitmask=1;
  unsigned char str[80];

  MDB_EVENT MDBEvent;
      
  MDB_Validator.DevLost=MDB_LostCount;
  
  switch(MDB_Validator.LastRequest)
    {
	case CmdValidator_Reset :
//	  MDB_Validator.ResetTime=Validator_SetupTime;		//Wartezeit 
	  MDB_Validator.NextRequest=CmdValidator_Poll;
	  break; 	
	case CmdValidator_Status :
	  MDB_Validator.Level=MDB_buff[0];
	  MDB_Validator.Currency=MDB_buff[2]+256*MDB_buff[1];
	  MDB_Validator.BillScaling=(MDB_buff[3]*0x100)+(MDB_buff[4] & 0xFF);
	  MDB_Validator.Decimals=MDB_buff[5];
	  MDB_Validator.StackerCapacity=(MDB_buff[6]*0x100)+(MDB_buff[7] & 0xFF);
	  MDB_Validator.BillSecurityLevels=(MDB_buff[8]*0x100)+(MDB_buff[9] & 0xFF);
	  MDB_Validator.Escrow=MDB_buff[10];
	  for (s=11;s<27;s++) 
	  {
		   if (s<buff_point-1)
		   {
				MDB_Validator.BillTypeCredit[s-11]=MDB_buff[s];
				SysVar.BillCredit[s-11]= (unsigned long)MDB_Validator.BillScaling * MDB_buff[s];
		   }
		   else 
		   {
			   MDB_Validator.BillTypeCredit[s-11]=0;
			   SysVar.BillCredit[s-11]=0;
		   }		   		   
	  }	  
	  
	  if(MDB_Validator.Level==1) MDB_Validator.NextRequest=CmdValidator_ExpLevel1Identification; //erweiterte Info Holen LEVEL 1
	  else if(MDB_Validator.Level==2) MDB_Validator.NextRequest=CmdValidator_ExpLevel2Identification; //erweiterte Info Holen LEVEL 2
	       else MDB_Validator.NextRequest=CmdValidator_Stacker;	//Level 0 nur Stacker
	  break; 	    		
	case CmdValidator_Security :
	  MDB_Validator.NextRequest=CmdValidator_Poll;
	  break; 	    		
	case CmdValidator_Poll :
	  MDB_Validator.NextRequest=CmdValidator_Poll;
	  
	  if (buff_point>1)		// wenn 1 dann nur ACK
	    {
	      for (s=0;s<16;s++) MDB_Validator.Status[s]=0;
	      for (s=0;s<(buff_point-1);s++) 
		  {
		  		MDB_Validator.Status[s]=MDB_buff[s];
		  }

	  	  // AUSWERTUNG BILL
	      for (s=0;s<(buff_point-1);s++) 
	      {
	           if((MDB_Validator.Status[s] & 0x80)==0x80) //BILL
		       { 
				    // bei Mehfachwaehrung hier ersetzen
				    if (BillOver[MDB_Validator.Status[s] & 0x0F].active)
				    {
				  		MDB_Validator.NoteValue= BillOver[MDB_Validator.Status[s] & 0x0F].Value;
				    }
			        else 
				    {	 
					    if  (MDB_Validator.BillTypeCredit[MDB_Validator.Status[s] & 0x0F]==0xFF) //ab V2.84
					    	 MDB_Validator.NoteValue=0xFFFFFFFF;
  						else MDB_Validator.NoteValue= (unsigned long)MDB_Validator.BillScaling * (unsigned long)MDB_Validator.BillTypeCredit[MDB_Validator.Status[s] & 0x0F];
  				  	}
				      		      
 		          	switch(MDB_Validator.Status[s] & 0xF0)					
		          	{
		          	case 0x90 :	//BILL In ESCROW POSITION						
				  
				  		p=s;
						while (p--) BillBitmask= BillBitmask<<1;
						
						//sprintf(str,"Bill ESCROW Bill Enable= %u Bitmask=%u",MDB_Validator.BillEnable,BillBitmask);
						//Output_Logfile(str, DEFAULT_LOG);							
		          		
						if ( (!SimpleMode && (MDB_Validator.NoteValue>=MDB_Validator.AKZ_Min) && (MDB_Validator.NoteValue<=MDB_Validator.AKZ_Max) && 
		                    ValidatorNotesEN[MDB_Validator.Status[s] & 0x0F]) ||
						   (SimpleMode && (MDB_Validator.BillEnable & BillBitmask)) )
		            	{
		              	
		                	if(MDB_Validator.Inhibit==1) 
		                	{
		                  		MDB_Validator.EscrowStatus=0; //wenn inhibit angefordert
		                  		MDB_Validator.EscrowTimeout=0;
		                	}
		                  	else
		                  	{
		                  		MDB_Validator.EscrowStatus=1; //ACCEPT
		                  		MDB_Validator.EscrowTimeout=Validator_EscrowTime;
		                  		MDB_Validator.EscrowChannel=MDB_Validator.Status[s] &0x0F;		                  	
		                  	}
		              	}
		              	else
		              	{
		              		MDB_Validator.EscrowStatus=0; 	//REJECT
		              		MDB_Validator.EscrowTimeout=0;		              		
		              	}
		              							
	    				MDBEvent.Type    = EvTypeMDB_BillInEscrow;			// Escrow Start
		    			MDBEvent.Length  = 6;
   	   		    		MDBEvent.Data[0] = MDB_Validator.NoteValue;
   	   		    		MDBEvent.Data[1] = MDB_Validator.NoteValue>>8;
   	   		    		MDBEvent.Data[2] = MDB_Validator.NoteValue>>16;
   	   		    		MDBEvent.Data[3] = MDB_Validator.NoteValue>>24;
						MDBEvent.Data[4] = MDB_Validator.EscrowStatus;
						MDBEvent.Data[5] = MDB_Validator.Status[s] & 0x0F;
   	   		    		putMDBevent(&MDBEvent);
		              	
		            	MDB_Validator.NextRequest=CmdValidator_Escrow;
		            	MDB_Validator.Sequence=Validator_Sequence_ESCROW;
		            break;
		            
	                case 0x80 :	//BILL Stacked
				   
				   		//if (MDB_Validator.EscrowStatus==1) // wieder ab V3.15
						{	                  	                  	                    
		                  //EVENT NOTE ACCEPTED !!!
		                    MDB_Validator.EscrowStatus=0;
	 		      	    	SysVar.Bill[MDB_Validator.Status[s] & 0x0F].Count++;
                  				
		                    MDB_Validator.LastValue=MDB_Validator.NoteValue;
				    		MDB_Validator.NextRequest=CmdValidator_Poll;
							
							//CalcCoinCRC(); //V1.25
		    				
				    		MDBEvent.Type    = EvTypeMDB_BillStacked;			// Bill inserted
	   	   		    		MDBEvent.Length  = 7;
	   	   		    		MDBEvent.Data[0] = MDB_Validator.NoteValue;
	   	   		    		MDBEvent.Data[1] = MDB_Validator.NoteValue>>8;
	   	   		    		MDBEvent.Data[2] = MDB_Validator.NoteValue>>16;
	   	   		    		MDBEvent.Data[3] = MDB_Validator.NoteValue>>24;
	   	   		    		MDBEvent.Data[4] = MDB_Validator.EscrowTimeout;
	   	   		    		MDBEvent.Data[5] = MDB_Validator.EscrowTimeout>>8;
							MDBEvent.Data[6] = MDB_Validator.Status[s] & 0x0F;
	   	   		    		putMDBevent(&MDBEvent);

	                    	MDB_Validator.EscrowTimeout=0;
						}
	            	break;
		            
	              case 0xA0 :	//BILL Returned
					  						
					    MDBEvent.Type    = EvTypeMDB_BillReturned;
		   	   		    MDBEvent.Length  = 7;
		   	   		    MDBEvent.Data[0] = MDB_Validator.NoteValue;
		   	   		    MDBEvent.Data[1] = MDB_Validator.NoteValue>>8;
   	   		    		MDBEvent.Data[2] = MDB_Validator.NoteValue>>16;
   	   		    		MDBEvent.Data[3] = MDB_Validator.NoteValue>>24;						
		   	   		    MDBEvent.Data[4] = MDB_Validator.EscrowTimeout;
		   	   		    MDBEvent.Data[5] = MDB_Validator.EscrowTimeout>>8;
						MDBEvent.Data[6] = MDB_Validator.Status[s] & 0x0F;
		   	   		    putMDBevent(&MDBEvent);
   	   		    
	                    MDB_Validator.EscrowStatus=0;
	                    MDB_Validator.EscrowTimeout=0;		              		
					    MDB_Validator.NextRequest=CmdValidator_Poll;
		            break;
		            
		          case 0xC0 :	//Disabled BILL Rejected
				  						
					    MDBEvent.Type    = EvTypeMDB_BillRejected;
		   	   		    MDBEvent.Length  = 7;
		   	   		    MDBEvent.Data[0] = MDB_Validator.NoteValue;
		   	   		    MDBEvent.Data[1] = MDB_Validator.NoteValue>>8;
   	   		    		MDBEvent.Data[2] = MDB_Validator.NoteValue>>16;
   	   		    		MDBEvent.Data[3] = MDB_Validator.NoteValue>>24;						
		   	   		    MDBEvent.Data[4] = MDB_Validator.EscrowTimeout;
		   	   		    MDBEvent.Data[5] = MDB_Validator.EscrowTimeout>>8;
						MDBEvent.Data[6] = MDB_Validator.Status[s] & 0x0F;
		   	   		    putMDBevent(&MDBEvent);
   	   		    
			            MDB_Validator.EscrowStatus=0;
			            MDB_Validator.EscrowTimeout=0;		              	
					    MDB_Validator.NextRequest=CmdValidator_Poll;
		            break;
	                }	              
   	            }				
	            else
	            {	//Status Commands
	            	if(MDB_Validator.Status[s]!=0) MDB_Validator.LastStatus=MDB_Validator.Status[s];

					switch(MDB_Validator.Status[s])
					{
					case 0x01://0x01 = Defective Motor
							MDB_Validator.EscrowStatus=0;	//ab 09.01.2006 V1.30
					break;
					case 0x02://0x02 = Sensor Problem
							MDB_Validator.EscrowStatus=0;	//ab 09.01.2006 V1.30
					break;
					case 0x03://0x03 = Validator Busy
					break;
					case 0x04://0x04 = Rom Checksum ERROR
							MDB_Validator.EscrowStatus=0;	//ab 09.01.2006 V1.30
					break;
					case 0x05://0x05 = Validator Jammed
							MDB_Validator.EscrowStatus=0;	//ab 09.01.2006 V1.30
					break;
					case 0x06://0x06 = Validator was Reset
					        MDB_Validator.Sequence=Validator_Sequence_INIT;
					        MDB_Validator.NextRequest=CmdValidator_Status;
							MDB_Validator.EscrowStatus=0;	//ab 09.01.2006 V1.30
					break;
					case 0x08://0x08 = Bill Box removed
							MDB_Validator.ready=0;
							MDB_Validator.EscrowStatus=0;	//ab 09.01.2006 V1.30
					break;
					case 0x09://0x09 = Unit Disabled		                  	
						if (SysVar.BillEscrowMode) 
						{
		                  if (MDB_Validator.EscrowStatus==1)
		                   MDB_Validator.EscrowTimeout=Validator_EscrowTime;
						}
					break;
					case 0x0A://0x0A = Invalid Escrow request (when no Bill in Escrow position)
					break;
					case 0x0B://0x0B = Bill Rejected (unknown Bill) Note gezogen
						MDB_Validator.NextRequest=CmdValidator_BillType;
						MDB_Validator.EscrowStatus=0;	//ab 09.01.2006 V1.30
					break;
					case 0x0C://0x0C 
					break;
					default:
						if((MDB_Validator.Status[s] & 0xC0)==0x40) return; // V1.41
					break;
				}							
	        		        		        	        	
	        	if ((MDB_Validator.Status[s] != 0x00) &&
	        	    (MDB_Validator.Status[s] != 0x03) &&
	        	    (MDB_Validator.Status[s] != 0x09))
	        	 {
					
				   	MDBEvent.Type    = EvTypeMDB_ValidatorStatus;			// MDBEvent
   	   				MDBEvent.Length  = 2;
		   	   		MDBEvent.Data[0] = 0x11;
	   	   			MDBEvent.Data[1] = MDB_Validator.Status[s];
   			   		putMDBevent(&MDBEvent);
	   	   		 }	        	
	            }	        
	        } //von for
	     } 
	     else //von buff_point>1   
	     {
		   	if (SysVar.BillEscrowMode) 
		   	{
	     		if (!MDB_Validator.EscrowTimeout && (MDB_Validator.EscrowStatus))	//keine Bestaetigung f�r Banknote erhalten
	     		{				
					if (strcmp(MDB_Validator.Manufacturer,"CCC"))
					{
			      	  	SysVar.Bill[MDB_Validator.EscrowChannel].Count++;
			      	  	MDB_Validator.EscrowStatus=0;
				  	
						//CalcCoinCRC(); //V1.25
															
				   	  	MDBEvent.Type    = EvTypeMDB_BillEscrowTimeout;			// MDBEvent
		   	   		  	MDBEvent.Length  = 8;
		   	   		  	MDBEvent.Data[0] = MDB_Validator.NoteValue;
		   	   		  	MDBEvent.Data[1] = MDB_Validator.NoteValue>>8;
	   	   		      	MDBEvent.Data[2] = MDB_Validator.NoteValue>>16;
	   	   		      	MDBEvent.Data[3] = MDB_Validator.NoteValue>>24;				  
		   	   		  	MDBEvent.Data[4] = MDB_Validator.LastStatus;
		   	   		  	MDBEvent.Data[5] = MDB_Validator.LastRequest;
		   	   		  	MDBEvent.Data[6] = MDB_Validator.LastRequest>>8;
						MDBEvent.Data[7] = MDB_Validator.EscrowChannel;
		   	   		  	putMDBevent(&MDBEvent);
					}
				}
			}  	
			
			MDB_Validator.LastStatus=0;	//nur ACK
	     }	
	   // ***************** INHIBIT ANALYSE ******************
	   if(MDB_Validator.LastStatus==0)
	     {			 
		   if(MDB_Validator.Sequence) MDB_Validator.Sequence=0;
	     }
  	   	
		if((MDB_Validator.Inhibit!=MDB_Validator.Inhibited) && (MDB_Validator.Sequence==0))
	   	{
	     	if(MDB_Validator.Inhibit==1)
	       	{
	       		MDB_Validator.BillEnable=0x0000;		//Sperren
     	       	MDB_Validator.BillEscrowEnable=0x0000;
               	MDB_Validator.NextRequest=CmdValidator_BillType;  	
        	}
            else
            {
				if (SysVar.BillEscrowMode) 
				{
	       			MDB_Validator.BillEnable=0xFFFF;		//Freigeben
     	       		MDB_Validator.BillEscrowEnable=0xFFFF;
				}
				else MDB_Validator.BillEscrowEnable=0x0000;
				
               	MDB_Validator.NextRequest=CmdValidator_BillType;  	
            }
        
	     }
	   break; 	    	
	   
		case CmdValidator_BillType :
	  		MDB_Validator.Sequence=0;		//Last Command after Init or Ena/Disa Notes
	  		if(MDB_Validator.BillEnable==0) //Kommando ausgef�hrt
	    		 MDB_Validator.Inhibited=1;
	    	else MDB_Validator.Inhibited=0;
	  		MDB_Validator.ready=1;	    
          	MDB_Validator.NextRequest=CmdValidator_Poll;
		break; 	    		
     	case CmdValidator_Escrow :
      	  	//MDB_Validator.Sequence=0;	//SOLANGE BIS der ZWEITE IMPULS
          	MDB_Validator.NextRequest=CmdValidator_Poll;
        break; 	    
      	case CmdValidator_Stacker :
	  		MDB_Validator.StackerStatus=MDB_buff[0];
// if (MDB_Validator.StackerStatus & 0x80) ---> Stacker Full
	  		if(MDB_Validator.Sequence==Validator_Sequence_INIT) MDB_Validator.NextRequest=CmdValidator_BillType;
	  		else MDB_Validator.NextRequest=CmdValidator_Poll;
        break; 	    		
      	case CmdValidator_ExpLevel1Identification :
          	MDB_Validator.Manufacturer[0]=MDB_buff[0];
	  		MDB_Validator.Manufacturer[1]=MDB_buff[1];
	  		MDB_Validator.Manufacturer[2]=MDB_buff[2];
	  		for (s=0;s<12;s++) MDB_Validator.Serial[s]=MDB_buff[3+s];
	  		for (s=0;s<12;s++) MDB_Validator.Model[s]=MDB_buff[15+s];
	  		MDB_Validator.Version=MDB_buff[28]+256*MDB_buff[27];	  
	  		MDB_Validator.NextRequest=CmdValidator_Stacker;
		break; 	    		
      	case CmdValidator_ExpLevel2FeatureEnable :	// Problem JCM Versteht nicht 
          	MDB_Validator.NextRequest=CmdValidator_Stacker;  //  P
        break; 	    		
      	case CmdValidator_ExpLevel2Identification :
          	MDB_Validator.Manufacturer[0]=MDB_buff[0];
	  		MDB_Validator.Manufacturer[1]=MDB_buff[1];
	  		MDB_Validator.Manufacturer[2]=MDB_buff[2];
	  		for (s=0;s<12;s++) MDB_Validator.Serial[s]=MDB_buff[3+s];
	  		for (s=0;s<12;s++) MDB_Validator.Model[s]=MDB_buff[15+s];
	  		MDB_Validator.Version=MDB_buff[28]+256*MDB_buff[27];	  
	  		MDB_Validator.OptionalFeatures=(MDB_buff[29]*0x1000000)+(MDB_buff[30]*0x10000)+(MDB_buff[31]*0x100)+MDB_buff[32];
	  		if(MDB_Validator.OptionalFeatures==0)
	    			MDB_Validator.NextRequest=CmdValidator_Stacker;
	   		else 	MDB_Validator.NextRequest=CmdValidator_ExpLevel2FeatureEnable; //sonst Enable oder Disable
        break; 	    		
      	case CmdValidator_ExpDiagnostic :
          MDB_Validator.NextRequest=CmdValidator_Poll;
          break; 	    			  	
    }	  
	
}


//------------------------------AVD----------------------------------------------
void RX_Handle_AVD(unsigned char buff_point)
{	
  	unsigned int s;
    
  	MDB_AVD.DevLost=MDB_LostCount;
  	MDB_AVD.NonResponseCount=MDB_AVD.NonResponseTime*1000;
  	MDB_AVD.NextRequest=CmdAVD_Poll;
	
  	switch(MDB_AVD.LastRequest)
  	{
	case CmdAVD_Reset :
	  MDB_AVD.ResetTime=AVD_SetupTime;		//Wartezeit 
	  Handle_AVD_Status(0,buff_point);
	  break;
	case CmdAVD_ReaderEnable :
	  MDB_AVD.ready=1;		  
	  break;
	case CmdAVD_ExpDiagnostic :
	Handle_AVD_Status(0,buff_point);		// DRAVP expected	
	  break;
    }	  

}

unsigned char Handle_AVD_Status(unsigned char buffpos,unsigned char bufflen)
{
  unsigned int s;
  
  if (buffpos>=(bufflen-1)) return 0xFF;	// is an END
  switch(MDB_buff[buffpos])	//Position auf STATUSBYTE
    {

	case AVDStatus_JustReset :
		  	MDB_AVD.DiagnosticCommand=ExpDiag_AgeVerificationOnOff;
		  	MDB_AVD.NextRequest=CmdAVD_ExpDiagnostic;
	  break;
	case AVDStatus_DiagnosticResponse :
	  switch(MDB_buff[buffpos+1])
	  {
	  case ExpDiag_AgeVerificationOnOff: //DRAVP	  
	  	if(MDB_buff[buffpos+2]==0x06)
	  	{
			MDB_AVD.AVSFeature1=MDB_buff[buffpos+3]; 
	  		for (s=0;s<5;s++) MDB_AVD.DRAV[s]=MDB_buff[buffpos+4+s];
			buffpos+=8;
		  	MDB_AVD.DiagnosticCommand=ExpDiag_ConfigureVendProcess;
		  	MDB_AVD.NextRequest=CmdAVD_ExpDiagnostic;
	  	}
	  break;
	  case ExpDiag_AgeVerificationStatus: //DRAVS
	  	if(MDB_buff[buffpos+2]==0x07)
	  	{
			MDB_AVD.AVSFeature1=MDB_buff[buffpos+3];
			MDB_AVD.AVSFeature2=MDB_buff[buffpos+4];
	  		//for (s=0;s<5;s++) MDB_AVD.DRAV[s]=MDB_buff[buffpos+5+s];
			buffpos+=9;			
	  	}
	  break;
	  case ExpDiag_ConfigureVendProcess: //CFGVP
	  	if(MDB_buff[buffpos+2]==0x06)
	  	{
	  		MDB_AVD.AVSConfiguration=MDB_buff[buffpos+3]; 
			MDB_AVD.NextRequest=CmdAVD_ReaderEnable;
		}
	  break;
	  }
	break;
    }	
  return buffpos;      
}

//------------------------------AVD----------------------------------------------
void RX_Handle_ParkIO(unsigned char buff_point)
{	
  	unsigned int s,thum;
	int  ttemp;
	long Whelp;
        
  	MDB_ParkIO.DevLost=MDB_LostCount;
  	MDB_ParkIO.NonResponseCount=MDB_ParkIO.NonResponseTime*1000;
  	MDB_ParkIO.NextRequest=CmdParkIO_Poll;
	
  	switch(MDB_ParkIO.LastRequest)
  	{
	case CmdParkIO_Reset :
	  MDB_ParkIO.ResetTime=ParkIO_SetupTime;		//Wartezeit 
  	  MDB_ParkIO.NextRequest=CmdParkIO_Identification;
	  break;
	case CmdParkIO_Poll :
	  
	  if (buff_point>1)		// wenn 1 dann nur ACK
	  {
	    for (s=0;s<16;s++) MDB_ParkIO.Status[s]=0;
	    for (s=0;s<(buff_point-1);s++) 
		{
			MDB_ParkIO.Status[s]=MDB_buff[s];
		}

	    if (MDB_ParkIO.Tidx>7) MDB_ParkIO.Tidx=0;
		
		MDB_ParkIO.InputStat=MDB_ParkIO.Status[0];
		MDB_ParkIO.RelaisStat=MDB_ParkIO.Status[1];
		
		if (MDB_ParkIO.InputStat & 0x20)
		{
			MDB_ParkIO.HumidityArray[MDB_ParkIO.Tidx]=256*MDB_ParkIO.Status[2]+MDB_ParkIO.Status[3];
			MDB_ParkIO.TempArray[MDB_ParkIO.Tidx]=256*(MDB_ParkIO.Status[4] & 0x7f)+ MDB_ParkIO.Status[5];
			if (MDB_ParkIO.Status[4] & 0x80) MDB_ParkIO.TempArray[MDB_ParkIO.Tidx]*=-1;
			
			MDB_ParkIO.Tidx++;
	
			ttemp=0;thum=0;
			for (s=0;s<8;s++)
			{
				ttemp+=MDB_ParkIO.TempArray[s];
				thum+=MDB_ParkIO.HumidityArray[s];
			}
			MDB_ParkIO.Temp=ttemp/8;
	
			MDB_ParkIO.Humidity=thum/8;
		}
	
		if (MDB_ParkIO.InputStat & 0x10)
	     MDB_ParkIO.Weight=0x1000000*(long)MDB_ParkIO.Status[6]+0x10000*(long)MDB_ParkIO.Status[7]+0x100*(long)MDB_ParkIO.Status[8]+MDB_ParkIO.Status[9];				
		
		MDB_ParkIO.Scaling=MDB_ParkIO.Status[11];
		
		MDB_ParkIO.LoTemp=256*(MDB_ParkIO.Status[12] & 0x7f)+ MDB_ParkIO.Status[13];
		if (MDB_ParkIO.Status[12] & 0x80) MDB_ParkIO.LoTemp*=-1;

		MDB_ParkIO.HiTemp=256*(MDB_ParkIO.Status[14] & 0x7f)+ MDB_ParkIO.Status[15];
		if (MDB_ParkIO.Status[14] & 0x80) MDB_ParkIO.HiTemp*=-1;
		
		if (MDB_ParkIO.InputStat & 0x20)
		{
			if (MDB_ParkIO.Humidity>599)	
			{
				MDB_ParkIO.RelaisStat |= 0x40;
				MDB_ParkIO.NextRequest=CmdParkIO_Output;
			}

			if (MDB_ParkIO.Humidity<550)	
			{
				MDB_ParkIO.RelaisStat &= 0xBF;
				MDB_ParkIO.NextRequest=CmdParkIO_Output;
			}

			if (MDB_ParkIO.Temp<240)	
			{
				MDB_ParkIO.RelaisStat |= 0x80;
				MDB_ParkIO.NextRequest=CmdParkIO_Output;
			}

			if (MDB_ParkIO.Temp>260)
			{
				MDB_ParkIO.RelaisStat &= 0x7F;
				MDB_ParkIO.NextRequest=CmdParkIO_Output;
			}
		}
		
  	  } 
	
		if(MDB_ParkIO.NextRequest==CmdParkIO_Poll) MDB_ParkIO.NextRequest=CmdParkIO_Input;
	  break;
	  
	case CmdParkIO_Input:	
	
		for (s=0;s<4;s++)
		 MDB_ParkIO.InputPulse[s]=MDB_buff[s];
	
	  break;
	  
	case CmdParkIO_Identification:	
	
      MDB_ParkIO.Manufacturer[0]=MDB_buff[0];
	  MDB_ParkIO.Manufacturer[1]=MDB_buff[1];
	  MDB_ParkIO.Manufacturer[2]=MDB_buff[2];
	  MDB_ParkIO.Manufacturer[3]=0;		/*D*/
	  for (s=0;s<12;s++) MDB_ParkIO.Serial[s]=MDB_buff[3+s];
	  MDB_ParkIO.Serial[12]=0;			/*D*/
	  for (s=0;s<12;s++) MDB_ParkIO.Model[s]=MDB_buff[15+s];
	  MDB_ParkIO.Model[12]=0;			/*D*/
	  MDB_ParkIO.Version=MDB_buff[28]+256*MDB_buff[27];	
	
	  MDB_ParkIO.ready=1;
	  break;
    }	  

}


//*************************MDB TRANSMIT*******************************************
void handleMDBtimer(void)
{
  unsigned int s;
  
	#if (WH_EMP==1)
	if(MDB_Emp.ResetTime) 			MDB_Emp.ResetTime--;
    #endif
	
	#if (TUBE_CHANGER==1)
	if(MDB_Changer1.ResetTime) 		MDB_Changer1.ResetTime--;
	#endif
		
	if(MDB_Changer2.ResetTime) 		MDB_Changer2.ResetTime--;
	
	if(MDB_Cardreader.ResetTime) 		MDB_Cardreader.ResetTime--;
	
	if(MDB_Cardreader.NonResponseCount) 	MDB_Cardreader.NonResponseCount--;
	
	if(MDB_Cardreader.DisplayCount) 
	{
		MDB_Cardreader.DisplayCount--;
	    if(MDB_Cardreader.DisplayCount==0) ClearCardreaderDisplay();
	}
	
	if(MDB_Validator.ResetTime) 	MDB_Validator.ResetTime--;
	if(MDB_Validator.EscrowTimeout) MDB_Validator.EscrowTimeout--;
	if(MDB_AVD.ResetTime) 			MDB_AVD.ResetTime--;
	if(MDB_AVD.NonResponseCount) 	MDB_AVD.NonResponseCount--;
	if(MDB_ParkIO.ResetTime) 			MDB_ParkIO.ResetTime--;
	if(MDB_ParkIO.NonResponseCount) 	MDB_ParkIO.NonResponseCount--;

	if(MDB_timeout) 			
	{
		if (--MDB_timeout==0)
		{
		  	/* wurden Daten erwartet?		  
			*/
		}
	}
	else 
	{		  		  
		  
	    if(MDB_DeviceCount<MDB_MAX_DEVICE) 
	    	MDB_DeviceCount++; 
	    else 
		{
			MDB_DeviceCount=0;
			if (MDB_Startup) --MDB_Startup;
		}
			    
	    switch(MDB_DeviceCount)
	     {
	       case 0 :		//Changer1 or EMP
		 	 #if (TUBE_CHANGER==1)
                if(MDB_Changer1.ResetTime==0) TX_Handle_Changer(pMDB_Changer1,MDB_Adr_Changer1);
	         #elif (WH_EMP==1)
	         	if(MDB_Emp.ResetTime==0) TX_Handle_Emp(MDB_Adr_Emp);
             #endif                                  
	        	break;
	       case 1 :		//Changer2
                 if(MDB_Changer2.ResetTime==0) TX_Handle_Changer(pMDB_Changer2,MDB_Adr_Changer2);
	        	break;	         
	       case 2 :		//Cardreader
	         	if(MDB_Cardreader.ResetTime==0) TX_Handle_Cardreader(MDB_Adr_Cashless);
	        	break;
	       case 3 :		//Validator		   		
	         	if(MDB_Validator.ResetTime==0) TX_Handle_Validator(MDB_Adr_BillValidator);
	        	break;
	       case 4 :		//Age Verification Device
	         	if(MDB_AVD.ResetTime==0) TX_Handle_AVD(MDB_Adr_AVD);
				break;
	       case 5 :		//Age Verification Device
	         	if(MDB_ParkIO.ResetTime==0) TX_Handle_ParkIO(MDB_Adr_ParkIO);
	        	break;
  	     }
  	  }   
  		
}

#if (WH_EMP==1)
//------------------------------EMP---------------------------------------------
void TX_Handle_Emp(unsigned int adr)
{  
  	unsigned int s;
  	int l;

	MDB_EVENT MDBEvent;

  	MDB_buffpointer=0;

  	if(MDB_Emp.DevLost!=0) 
    {
      	MDB_Emp.DevLost--;
      	if(MDB_Emp.DevLost==0) 
        {   	
									
  			MDBEvent.Type    = EvTypeMDB_EmpLost;
  			MDBEvent.Length  = 0;
  			putMDBevent(&MDBEvent);			

			InitEmp(); 
			return;
        }
    }
	
  if ((MDB_Emp.NextRequest==CmdEmp_Poll) && MDB_Emp.NewRequest)
  {
	  MDB_Emp.NextRequest=MDB_Emp.NewRequest;
	  MDB_Emp.Sequence=MDB_Emp.NewSequence;
	  MDB_Emp.NewRequest=0;
	  MDB_Emp.NewSequence=0;
  }
       
  MDB_Emp.LastRequest=MDB_Emp.NextRequest;
  switch(MDB_Emp.NextRequest)
    {
      case CmdEmp_Reset :
        TxMDB_Cmd(CmdEmp_Reset,adr);
        TxMDB_Chk();
        break; 	
      case CmdEmp_Status :
        TxMDB_Cmd(CmdEmp_Status,adr);
        TxMDB_Chk();
        break; 	    		
      case CmdEmp_Poll :
        TxMDB_Cmd(CmdEmp_Poll,adr);
        TxMDB_Chk();
        break; 	    		
      case CmdEmp_CoinType :
        TxMDB_Cmd(CmdEmp_CoinType,adr);
        TxMDB_Byte(MDB_Emp.CoinEnable>>8);
        TxMDB_Byte(MDB_Emp.CoinEnable);
        TxMDB_Byte(MDB_Emp.CoinToMain>>8);
        TxMDB_Byte(MDB_Emp.CoinToMain);
        TxMDB_Chk();  
        break; 	    		
      case CmdEmp_CoinRouting :
        TxMDB_Cmd(CmdEmp_CoinRouting,adr);
        TxMDB_Byte(MDB_Emp.MainTubeNumber);
        for (s=0;s<8;s++)  TxMDB_Byte(MDB_Emp.CoinRouting[s]);
        TxMDB_Chk();
        break; 	    		
      case CmdEmp_ExpIdentification :
        TxMDB_Cmd(CmdChanger_ExpIdentification,adr);
        TxMDB_Chk();
        break; 	    		
      case CmdEmp_ExpRequest :
        TxMDB_Cmd(CmdEmp_ExpRequest,adr);
        TxMDB_Chk();
        break; 	    		
      case CmdEmp_ExpExtendedCoinType :
        TxMDB_Cmd(CmdEmp_ExpExtendedCoinType,adr);
        TxMDB_Byte(MDB_Emp.CoinEnable>>24);
        TxMDB_Byte(MDB_Emp.CoinEnable>>16);
        TxMDB_Byte(MDB_Emp.CoinEnable>>8);
        TxMDB_Byte(MDB_Emp.CoinEnable);
        TxMDB_Byte(MDB_Emp.CoinToMain>>8);
        TxMDB_Byte(MDB_Emp.CoinToMain);
        TxMDB_Chk();
        break; 	    		
      case CmdEmp_ExpExtendedRequest :
        TxMDB_Cmd(CmdEmp_ExpExtendedRequest,adr);
        TxMDB_Chk();
        break; 	    		
      case CmdEmp_ExpMasterSlave :
        TxMDB_Cmd(CmdEmp_ExpMasterSlave,adr);
        TxMDB_Chk();
        break; 	    		      
    }      
	
}	//end TX_HANDLE_CHANGER
#endif

//------------------------------CHANGER----------------------------------------------
void TX_Handle_Changer(ChangerTag *MDB_Changer,unsigned int adr)
{
	char s,TubeOffs,TubeNo,TubeCount,found;	
	int l;

	MDB_EVENT MDBEvent;
	
  	MDB_buffpointer=0;	
  
  	if(MDB_Changer->DevLost!=0) 
    {
      	MDB_Changer->DevLost--;
      	if(MDB_Changer->DevLost==0) 
        {     		
  			MDBEvent.Type    = EvTypeMDB_ChangerLost;			// MDBEvent
  			MDBEvent.Length  = 1;
			if (adr==MDB_Adr_Changer1)
				 MDBEvent.Data[0]=1;
			else MDBEvent.Data[0]=2;
  			putMDBevent(&MDBEvent);

		#if (TUBE_CHANGER==1)        	
       		if (adr == MDB_Adr_Changer1) InitChanger1(); 
		#endif		  
       	  	if (adr == MDB_Adr_Changer2) InitChanger2();        	  
		  
          	//return;
        }
    }
	
  	if ((MDB_Changer->NextRequest==CmdChanger_Poll) && MDB_Changer->NewRequest)
  	{
	  	MDB_Changer->NextRequest=MDB_Changer->NewRequest;
	  	MDB_Changer->Sequence=MDB_Changer->NewSequence;
	  	MDB_Changer->NewRequest=0;
	  	MDB_Changer->NewSequence=0;
  	}
	    
  MDB_Changer->LastRequest=MDB_Changer->NextRequest;
  
  switch(MDB_Changer->NextRequest)
    {
      case CmdChanger_Reset :
        TxMDB_Cmd(CmdChanger_Reset,adr);
        TxMDB_Chk();
        break; 	
      case CmdChanger_Status :
        TxMDB_Cmd(CmdChanger_Status,adr);
        TxMDB_Chk();
        break; 	    		
      case CmdChanger_TubeStatus :
        TxMDB_Cmd(CmdChanger_TubeStatus,adr);
        TxMDB_Chk();
		//Sound=0x01;
        break; 	    		
      case CmdChanger_Poll :
        TxMDB_Cmd(CmdChanger_Poll,adr);
        TxMDB_Chk();
        break; 	    		
      case CmdChanger_CoinType :
        TxMDB_Cmd(CmdChanger_CoinType,adr);
        TxMDB_Byte(MDB_Changer->CoinEnable>>8);
        TxMDB_Byte(MDB_Changer->CoinEnable);
        TxMDB_Byte(MDB_Changer->ManualDispenseEnable>>8);
        TxMDB_Byte(MDB_Changer->ManualDispenseEnable);		//bit 0-2 Relais
        TxMDB_Chk();  
        break; 	    		
        
      case CmdChanger_Dispense :              
      
       	if (adr == MDB_Adr_Changer1) TubeOffs = 0x00; 
       	else TubeOffs = 0x10;
         	
 	// jeweilige Tube ausgeben
 	TubeNo = 0;
 	TubeCount = 0;
 	
 	found=0;
      	s=MDB_MAXCOINS;
      	while ( (s>0) && (found ==0)) 	
      	{
    	  if (PayOutTubes[TubeOffs+s-1].TubeOut != 0) found=1;    	 	    	 	
      	  s--;
        }      	 	
      	 	
       	// Wenn alle Tuben leer, dann Poll next=Poll;
        
        if ((MDB_Changer->LastStatus == 0) && (found ==1))
        {
         	TubeNo = (TubeOffs+s) % 16;
      	 	if (PayOutTubes[TubeOffs+s].TubeOut>15)
      	 	{
      	 	   	TubeCount = 0xF0;
      	 	   	PayOutTubes[TubeOffs+s].TubeOut -= 15;
      	 	}
      	 	else
      	 	{
      	 	   	TubeCount = PayOutTubes[TubeOffs+s].TubeOut*16;
      	 	   	PayOutTubes[TubeOffs+s].TubeOut = 0;
      	 	} 
        
        	MDB_Changer->LastStatus = 0x02;
        	MDB_Changer->Dispense = TubeNo + TubeCount;
	        
        	TxMDB_Cmd(CmdChanger_Dispense,adr);
        	TxMDB_Byte(MDB_Changer->Dispense);
        	TxMDB_Chk();			
						               
			MDBEvent.Type    = EvTypeMDB_ChangerDispenseInfo;
  			MDBEvent.Length  = 2;
  			MDBEvent.Data[0] = TubeNo;
			MDBEvent.Data[1] = TubeCount;
  			putMDBevent(&MDBEvent);
       	}
      	else       	
      	{
      		MDB_Changer->LastRequest=CmdChanger_Poll;
        	TxMDB_Cmd(CmdChanger_Poll,adr);
        	TxMDB_Chk();      	 	
      	}      	
      	
        break; 	    		
      case CmdChanger_ExpIdentification :
        TxMDB_Cmd(CmdChanger_ExpIdentification,adr);
        TxMDB_Chk();
        break; 	    		
      case CmdChanger_ExpFeatureEnable :
        TxMDB_Cmd(CmdChanger_ExpFeatureEnable,adr);
        TxMDB_Byte(MDB_Changer->FeatureEnable>>24);	
		TxMDB_Byte(MDB_Changer->FeatureEnable>>16);	
		TxMDB_Byte(MDB_Changer->FeatureEnable>>8);	
		TxMDB_Byte(MDB_Changer->FeatureEnable);	
        TxMDB_Chk();
        break; 	    		
      case CmdChanger_ExpPayout :
        TxMDB_Cmd(CmdChanger_ExpPayout,adr);
        TxMDB_Byte(MDB_Changer->Payout);
        TxMDB_Chk();
		        
		MDBEvent.Type    = EvTypeMDB_ChangerExpandedPayout;
  		MDBEvent.Length  = 1;
  		MDBEvent.Data[0] = MDB_Changer->Payout;
  		putMDBevent(&MDBEvent);
		
        break; 	    		
      case CmdChanger_ExpPayoutStatus	:
        TxMDB_Cmd(CmdChanger_ExpPayoutStatus,adr);
        TxMDB_Chk();
        break; 	    		
      case CmdChanger_ExpPayoutValuePoll :
        TxMDB_Cmd(CmdChanger_ExpPayoutValuePoll,adr);
        TxMDB_Chk();
        break; 	    		
      case CmdChanger_ExpSendDiagnosticStatus	:
        TxMDB_Cmd(CmdChanger_ExpSendDiagnosticStatus,adr);
        TxMDB_Chk();
        break; 	    		
      case CmdChanger_ExpSendControlledManualFillReport :
        TxMDB_Cmd(CmdChanger_ExpSendControlledManualFillReport,adr);
        TxMDB_Chk();
        break; 	    		
      case CmdChanger_ExpSendControlledManualPayoutReport :
        TxMDB_Cmd(CmdChanger_ExpSendControlledManualPayoutReport,adr);
        TxMDB_Chk();
        break; 	    		
      case CmdChanger_ExpDiagnostic :
	  
		switch(MDB_Changer->DiagnosticCommand)
		{
		case ExpDiag_SuppressCoinRouting:
        	TxMDB_Cmd(CmdChanger_ExpDiagnostic,adr);
			TxMDB_Byte(ExpDiag_SuppressCoinRouting);
			TxMDB_Byte(MDB_Changer->CoinToMain>>8);
			TxMDB_Byte(MDB_Changer->CoinToMain);			
		  	TxMDB_Chk();	  	  
		break;
		case ExpDiag_TubeStatus:
        	TxMDB_Cmd(CmdChanger_ExpDiagnostic,adr);
			TxMDB_Byte(ExpDiag_TubeStatus);
		  	TxMDB_Chk();	  	  
		break;
		case ExpDiag_SetHopperCounter:
        	TxMDB_Cmd(CmdChanger_ExpDiagnostic,adr);
			TxMDB_Byte(ExpDiag_SetHopperCounter);
			TxMDB_Byte(MDB_Changer->HopperCount[0]>>8);
			TxMDB_Byte(MDB_Changer->HopperCount[0]);
			TxMDB_Byte(MDB_Changer->HopperCount[1]>>8);
			TxMDB_Byte(MDB_Changer->HopperCount[1]);
			for (s=0;s<28;s++) TxMDB_Byte(0xFF);
		  	TxMDB_Chk();	  	  
		break;
		case ExpDiag_SetTubeCounter:		
        	TxMDB_Cmd(CmdChanger_ExpDiagnostic,adr);
			TxMDB_Byte(ExpDiag_SetTubeCounter);
			TxMDB_Byte(MDB_Changer->TubeCountAF[0]);
			TxMDB_Byte(MDB_Changer->TubeCountAF[1]);
			TxMDB_Byte(MDB_Changer->TubeCountAF[2]);
			TxMDB_Byte(MDB_Changer->TubeCountAF[3]);
			TxMDB_Byte(MDB_Changer->TubeCountAF[4]);
			TxMDB_Byte(MDB_Changer->TubeCountAF[5]);
		  	TxMDB_Chk();	  	  
		break;
		case ExpDiag_RecycledBill:		
        	TxMDB_Cmd(CmdChanger_ExpDiagnostic,adr);
			TxMDB_Byte(ExpDiag_RecycledBill);
		  	TxMDB_Chk();	  	  
		break;
		}	  
		
		MDB_timeout=120;
      break; 	    		
    }      
		
}	//end TX_HANDLE_CHANGER

//------------------------------CARDREADER----------------------------------------------
void TX_Handle_Cardreader(unsigned int adr)
{ 
  unsigned int l,s;

  MDB_EVENT MDBEvent;
  
  MDB_buffpointer=0;	
  
  	if(MDB_Cardreader.DevLost && (MDB_Cardreader.NonResponseCount==0)) 
    {
      	MDB_Cardreader.DevLost--;
      	if(MDB_Cardreader.DevLost==0)
      	{
  			MDBEvent.Type    = EvTypeMDB_CardreaderLost;			// MDBEvent
  			MDBEvent.Length  = 0;
  			putMDBevent(&MDBEvent);

	  		InitCardreader();
          	return;
        }  
    }
	  
  	if (MDB_Cardreader.NextRequest==CmdCardreader_Poll)
  	{	  		
		if (MDB_Cardreader.NewRequest) 
		{
		  	MDB_Cardreader.NextRequest=MDB_Cardreader.NewRequest;
		  	MDB_Cardreader.NewRequest=0;
		}
		else
		{		
		  	if(MDB_Cardreader.Inhibit != MDB_Cardreader.Inhibit_Old) //auto Inhibt erkennung
		  	{
		  		if(MDB_Cardreader.Inhibit)
				{
					if (MDB_Cardreader.Sequence==Cardreader_Sequence_VEND) 
					{					
					 	MDB_Cardreader.Sequence=Cardreader_Sequence_CANCELREADER;
					 	MDB_Cardreader.NextRequest=CmdCardreader_VendCancel; ///RRRR		erst cancel dann disable
						MDB_Cardreader.Inhibit_Old  = MDB_Cardreader.Inhibit;
					}
					
					if (MDB_Cardreader.Sequence==Cardreader_Sequence_SESSION) 
					{					
					 	MDB_Cardreader.Sequence=Cardreader_Sequence_CANCELREADER;
					 	MDB_Cardreader.NextRequest=CmdCardreader_ReaderCancel; ///RRRR		erst cancel dann disable
						MDB_Cardreader.Inhibit_Old  = MDB_Cardreader.Inhibit;
					}
										
					if (!MDB_Cardreader.Sequence) {
						MDB_Cardreader.NextRequest=CmdCardreader_ReaderDisable;
						MDB_Cardreader.Inhibit_Old  = MDB_Cardreader.Inhibit;
					}									
				}
				else 
				{
					if (!MDB_Cardreader.Sequence) {					
						MDB_Cardreader.NextRequest=CmdCardreader_ReaderEnable;
						MDB_Cardreader.Inhibit_Old  = MDB_Cardreader.Inhibit;
					}
				}				
		  	}
		}
  	}
		
  MDB_Cardreader.LastRequest=MDB_Cardreader.NextRequest;
  
  switch(MDB_Cardreader.NextRequest)
  {
  	case CmdCardreader_Reset :
   		//MDB_Cardreader.ResetTime=Cardreader_SetupTime;		//Wartezeit 200ms
		MDB_Cardreader.Sequence=Cardreader_Sequence_INIT;	
	  	TxMDB_Cmd(CmdCardreader_Reset,adr);
        TxMDB_Chk();
	  	break;
		
	case CmdCardreader_SetupConfigData :
	
	  	TxMDB_Cmd(CmdCardreader_SetupConfigData,adr);
	  	TxMDB_Byte(SetupConfigData_VMC_Level);	
	  	TxMDB_Byte(SetupConfigData_Columns);	
	  	TxMDB_Byte(SetupConfigData_Rows);	
	  	TxMDB_Byte(SetupConfigData_Display);	
        TxMDB_Chk();
	  	break;
		
	case CmdCardreader_SetupMaxMinPrice :
	
	  	TxMDB_Cmd(CmdCardreader_SetupMaxMinPrice,adr);
	  	if(MDB_Cardreader.FeatureEnable & 0x06) 	//EXPANDED CURRENCY MODE
	    {
	      	TxMDB_Byte(SetupMaxMinPrice_MaxPrice>>24);	//Max Price
	      	TxMDB_Byte(SetupMaxMinPrice_MaxPrice>>16);	
	    }
		
	  	TxMDB_Byte(SetupMaxMinPrice_MaxPrice>>8);	//Max Price
	  	TxMDB_Byte(SetupMaxMinPrice_MaxPrice);	
		
	  	if(MDB_Cardreader.FeatureEnable & 0x06)		//EXPANDED CURRENCY MODE
	    {
	      	TxMDB_Byte(SetupMaxMinPrice_MinPrice>>24);	//Max Price
	      	TxMDB_Byte(SetupMaxMinPrice_MinPrice>>16);	
	    }
		
	  	TxMDB_Byte(SetupMaxMinPrice_MinPrice>>8);	//Min Price
	  	TxMDB_Byte(SetupMaxMinPrice_MinPrice);	
		
	  	if(MDB_Cardreader.FeatureEnable & 0x06)		//EXPANDED CURRENCY MODE
	    {
	      TxMDB_Byte(VMC_Currency1);	//Currency Code
	      TxMDB_Byte(VMC_Currency2);	//Currency Code
	    }
		
        TxMDB_Chk();
	  	break;
    case CmdCardreader_Poll :
	  	TxMDB_Cmd(CmdCardreader_Poll,adr);
        TxMDB_Chk();
	  	break;
	case CmdCardreader_VendRequest :
		MDB_Cardreader.Sequence=Cardreader_Sequence_VEND;
	  	TxMDB_Cmd(CmdCardreader_VendRequest,adr);
	  	if(MDB_Cardreader.FeatureEnable & 0x06)		//EXPANDED CURRENCY MODE
	  	{
	      	TxMDB_Byte(MDB_Cardreader.ItemPrice>>24);	//Max Price
	      	TxMDB_Byte(MDB_Cardreader.ItemPrice>>16);	
	  	}
		
	  	TxMDB_Byte(MDB_Cardreader.ItemPrice>>8);	//Item Price
	  	TxMDB_Byte(MDB_Cardreader.ItemPrice);	
	  	TxMDB_Byte(MDB_Cardreader.ItemNumber>>8);	//Item Number 0xFFFF undefined
	  	TxMDB_Byte(MDB_Cardreader.ItemNumber);	
        TxMDB_Chk();
	  	break;
		
	case CmdCardreader_VendCancel :
		MDB_Cardreader.Sequence=Cardreader_Sequence_CANCELVEND;
	  	TxMDB_Cmd(CmdCardreader_VendCancel,adr);
        TxMDB_Chk();
	  	break;
		
	case CmdCardreader_VendSuccess :
		MDB_Cardreader.Sequence=Cardreader_Sequence_FINISHVEND;
	  	TxMDB_Cmd(CmdCardreader_VendSuccess,adr);
	  	TxMDB_Byte(MDB_Cardreader.ItemNumber>>8);	//Item Number 0xFFFF undefined
	  	TxMDB_Byte(MDB_Cardreader.ItemNumber);		  
        TxMDB_Chk();
	  	break;
		
	case CmdCardreader_VendFailiure :
	  	TxMDB_Cmd(CmdCardreader_VendFailiure,adr);
        TxMDB_Chk();
	  	break;	  
		
    case CmdCardreader_VendSessionComplete :
		MDB_Cardreader.Sequence=Cardreader_Sequence_CANCELSESSION;		//hier sequenz�nderung
	  	TxMDB_Cmd(CmdCardreader_VendSessionComplete,adr);
        TxMDB_Chk();	  
	  	break;
		
	case CmdCardreader_VendCashSale :
	  	TxMDB_Cmd(CmdCardreader_VendCashSale,adr);
	  	if(MDB_Cardreader.FeatureEnable & 0x06)		//EXPANDED CURRENCY MODE
	    {
	      	TxMDB_Byte(MDB_Cardreader.ItemPrice>>24);	
	      	TxMDB_Byte(MDB_Cardreader.ItemPrice>>16);	
	    }
	  	TxMDB_Byte(MDB_Cardreader.ItemPrice>>8);	//Item Price
	  	TxMDB_Byte(MDB_Cardreader.ItemPrice);	
	  	TxMDB_Byte(MDB_Cardreader.ItemNumber>>8);	//Item Number 0xFFFF undefined
	  	TxMDB_Byte(MDB_Cardreader.ItemNumber);	
        TxMDB_Chk();	  
	  	break;
		
	case CmdCardreader_VendNegative :
	  	TxMDB_Cmd(CmdCardreader_VendNegative,adr);
	  	if(MDB_Cardreader.FeatureEnable & 0x06)		//EXPANDED CURRENCY MODE
	    {
	      	TxMDB_Byte(MDB_Cardreader.ItemPrice>>24);	
	      	TxMDB_Byte(MDB_Cardreader.ItemPrice>>16);	
	    }
	  	TxMDB_Byte(MDB_Cardreader.ItemPrice>>8);	//Item Price
	  	TxMDB_Byte(MDB_Cardreader.ItemPrice);	
	  	TxMDB_Byte(MDB_Cardreader.ItemNumber>>8);	//Item Number 0xFFFF undefined
	  	TxMDB_Byte(MDB_Cardreader.ItemNumber);	
        TxMDB_Chk();	  
	  	break;
		
	case CmdCardreader_ReaderDisable :
	  	TxMDB_Cmd(CmdCardreader_ReaderDisable,adr);
        TxMDB_Chk();	  
	  	break;
		
	case CmdCardreader_ReaderEnable :
	  	TxMDB_Cmd(CmdCardreader_ReaderEnable,adr);
        TxMDB_Chk();	  		
	  	break;
	  
	case CmdCardreader_ReaderCancel :
		MDB_Cardreader.Sequence=Cardreader_Sequence_CANCELREADER;
	  	TxMDB_Cmd(CmdCardreader_ReaderCancel,adr);
        TxMDB_Chk();	  		
	  	break;
		
	case CmdCardreader_ReaderDataEntryResponse :
        TxMDB_Cmd(CmdCardreader_ReaderDataEntryResponse,adr);	//Tastatureingabe
        TxMDB_Chk();	  	  
	  	break;
		
	case CmdCardreader_RevalueRequest :
	  	TxMDB_Cmd(CmdCardreader_RevalueRequest,adr);
	  	if(MDB_Cardreader.FeatureEnable & 0x06)		//EXPANDED CURRENCY MODE
	    {
	      	TxMDB_Byte(MDB_Cardreader.RevalueAmount>>24);	
	      	TxMDB_Byte(MDB_Cardreader.RevalueAmount>>16);	
	    }
	  	TxMDB_Byte(MDB_Cardreader.RevalueAmount>>8);	//Item Price
	  	TxMDB_Byte(MDB_Cardreader.RevalueAmount);	
        TxMDB_Chk();	  
	  	break;
		
	case CmdCardreader_RevalueLimitRequest :
	  	TxMDB_Cmd(CmdCardreader_RevalueLimitRequest,adr);
        TxMDB_Chk();	  
	  	break;
		
	case CmdCardreader_ExpRequestID :
	  	TxMDB_Cmd(CmdCardreader_ExpRequestID,adr);
	  	for (s=0;s<3;s++)  TxMDB_Byte(ExpRequestID_MfrCode[s]);
	  	for (s=0;s<12;s++) TxMDB_Byte(ExpRequestID_Serial[s]);
	  	for (s=0;s<12;s++) TxMDB_Byte(ExpRequestID_Model[s]);
	  	TxMDB_Byte(ExpRequestID_Version[0]);	
	  	TxMDB_Byte(ExpRequestID_Version[1]);	
	  	TxMDB_Chk();	  
	  	break;
		
	case CmdCardreader_ExpReadUserFile :
	  	break;
	case CmdCardreader_ExpWriteUserFile :
	  	break;
	case CmdCardreader_ExpWriteTimeDate :
		/*
	  	TxMDB_Cmd(CmdCardreader_ExpWriteTimeDate,adr);	
	  	TxMDB_Byte(TimeReg[6]);	//BCD Year
	  	TxMDB_Byte(TimeReg[5] & 0x1F);	//BCD Month
	  	TxMDB_Byte(TimeReg[4]);	//BCD Day
	  	TxMDB_Byte(TimeReg[2] & 0x3F);//BCD Hours
	  	TxMDB_Byte(TimeReg[1]);	//BCD Minute
	  	TxMDB_Byte(TimeReg[0]);	//BCD Second
	  	TxMDB_Byte(TimeReg[3]);	//BCD WeekDay
	  	TxMDB_Byte(0xFF);		//BCD Week Number
	  	TxMDB_Byte(0xFF);		//BCD Summertime
	  	TxMDB_Byte(0xFF);		//BCD Holiday
	  	TxMDB_Chk();	  	  
		*/
	  	break;	  
		
	case CmdCardreader_ExpOptionalFeatureEnabled :
	  	TxMDB_Cmd(CmdCardreader_ExpOptionalFeatureEnabled,adr);	
	  	TxMDB_Byte(MDB_Cardreader.FeatureEnable>>24);	
	  	TxMDB_Byte(MDB_Cardreader.FeatureEnable>>16);	
	  	TxMDB_Byte(MDB_Cardreader.FeatureEnable>>8);	
	  	TxMDB_Byte(MDB_Cardreader.FeatureEnable);	
	  	TxMDB_Chk();	  	  
	  	break;	  
		
	case CmdCardreader_ExpDiagnostic :
		switch(MDB_Cardreader.DiagnosticCommand)
		{
		case ExpDiag_AgeVerificationOnOff:		
		  TxMDB_Cmd(CmdCardreader_ExpDiagnostic,adr);
		  TxMDB_Byte(ExpDiag_AgeVerificationOnOff);
		  TxMDB_Byte(0x06);	//length=6
		  TxMDB_Byte(0x12);	//Age=18
		  TxMDB_Byte('D');
		  TxMDB_Byte('R');
		  TxMDB_Byte('A');
		  TxMDB_Byte('V');
		  TxMDB_Byte('P');
		  TxMDB_Chk();	  	  
		break;
		case ExpDiag_ConfigureVendProcess:
		  TxMDB_Cmd(CmdCardreader_ExpDiagnostic,adr);
		  TxMDB_Byte(ExpDiag_ConfigureVendProcess);
		  TxMDB_Byte(0x06);	//length=6
		  
		  TxMDB_Byte(0x04);	//Card before Product
		  
		  TxMDB_Byte('C');
		  TxMDB_Byte('F');
		  TxMDB_Byte('G');
		  TxMDB_Byte('V');
		  TxMDB_Byte('P');
		  TxMDB_Chk();
		break;
		}	  
	  break;	          	
 	  }
	        
}	//end TX_HANDLE_CARDREADER
//------------------------------VALIDATOR----------------------------------------------
void TX_Handle_Validator(unsigned int adr)
{
  int l;
  MDB_EVENT MDBEvent;
	
  MDB_buffpointer=0;	
  
  if(MDB_Validator.DevLost!=0) 
  	{
      	MDB_Validator.DevLost--;
      	if(MDB_Validator.DevLost==0)
      	{						
  			MDBEvent.Type    = EvTypeMDB_ValidatorLost;
  			MDBEvent.Length  = 0;
  			putMDBevent(&MDBEvent);
			
        	InitValidator();	
          	return;
      	}
    }
	
  if ((MDB_Validator.NextRequest==CmdValidator_Poll) && MDB_Validator.NewRequest)
  {
	  MDB_Validator.NextRequest=MDB_Validator.NewRequest;
	  MDB_Validator.Sequence=MDB_Validator.NewSequence;
	  MDB_Validator.NewRequest=0;
	  MDB_Validator.NewSequence=0;
  }
	
  MDB_Validator.LastRequest=MDB_Validator.NextRequest;

  switch(MDB_Validator.NextRequest)
    {
      case CmdValidator_Reset :
        //MDB_Validator.ResetTime=Validator_SetupTime;		//Wartezeit 200ms
        TxMDB_Cmd(CmdValidator_Reset,adr);
        TxMDB_Chk();
//  	MDB_Validator.NextRequest=CmdValidator_Poll;
        break; 	
      case CmdValidator_Status :
        TxMDB_Cmd(CmdValidator_Status,adr);
        TxMDB_Chk();
        break; 	    		
      case CmdValidator_Security :
        TxMDB_Cmd(CmdValidator_Security,adr);
        TxMDB_Byte(MDB_Validator.Security>>8);
        TxMDB_Byte(MDB_Validator.Security);
        TxMDB_Chk();
        break; 	    		
      case CmdValidator_Poll :
        TxMDB_Cmd(CmdValidator_Poll,adr);
        TxMDB_Chk();
        break; 	    		
      case CmdValidator_BillType :
        TxMDB_Cmd(CmdValidator_BillType,adr);
        TxMDB_Byte(MDB_Validator.BillEnable>>8);
        TxMDB_Byte(MDB_Validator.BillEnable);
        TxMDB_Byte(MDB_Validator.BillEscrowEnable>>8);
        TxMDB_Byte(MDB_Validator.BillEscrowEnable);
        TxMDB_Chk();  
        break; 	    		
      case CmdValidator_Escrow :
        TxMDB_Cmd(CmdValidator_Escrow,adr);
        TxMDB_Byte(MDB_Validator.EscrowStatus);
        TxMDB_Chk();
        break; 	    
      case CmdValidator_Stacker :
        TxMDB_Cmd(CmdValidator_Stacker,adr);
        TxMDB_Chk();
        break; 	    		
      case CmdValidator_ExpLevel1Identification :
        TxMDB_Cmd(CmdValidator_ExpLevel1Identification,adr);
        TxMDB_Chk();
        break; 	    		
      case CmdValidator_ExpLevel2FeatureEnable :
        TxMDB_Cmd(CmdValidator_ExpLevel2FeatureEnable,adr);
        TxMDB_Byte(MDB_Validator.FeatureEnable>>24);	
		TxMDB_Byte(MDB_Validator.FeatureEnable>>16);	
		TxMDB_Byte(MDB_Validator.FeatureEnable>>8);	
		TxMDB_Byte(MDB_Validator.FeatureEnable);	
        TxMDB_Chk();
        break; 	    		
      case CmdValidator_ExpLevel2Identification :
        TxMDB_Cmd(CmdValidator_ExpLevel2Identification,adr);
        TxMDB_Chk();
        break; 	    		
      case CmdValidator_ExpDiagnostic :
        TxMDB_Cmd(CmdValidator_ExpDiagnostic,adr);
        TxMDB_Chk();
        break; 	    		
    }   
		   
}	//end TX_HANDLE_Validator

void TX_Handle_AVD(unsigned int adr)
{
  unsigned int s;

  MDB_EVENT MDBEvent;
  
  MDB_buffpointer=0;	
  
  if((MDB_AVD.DevLost!=0) && (MDB_AVD.NonResponseCount==0)) 
  {
      MDB_AVD.DevLost--;
      if(MDB_AVD.DevLost==0)
      {						
  			MDBEvent.Type    = EvTypeMDB_AVDLost;
  			MDBEvent.Length  = 0;
  			putMDBevent(&MDBEvent);
		
	  		InitAVD();
          	return;
      }  
  }
	
  if ((MDB_AVD.NextRequest==CmdAVD_Poll) && MDB_AVD.NewRequest)
  {
	  MDB_AVD.NextRequest=MDB_AVD.NewRequest;
	  MDB_AVD.NewRequest=0;
  }
	
  MDB_AVD.LastRequest=MDB_AVD.NextRequest;
  switch(MDB_AVD.NextRequest)
    {
    case CmdAVD_Reset :
	  TxMDB_Cmd(CmdAVD_Reset,adr);
      TxMDB_Chk();
	  MDB_AVD.NextRequest=CmdAVD_Poll; 
	  break;
    case CmdAVD_Poll :
        TxMDB_Cmd(CmdAVD_Poll,adr);
        TxMDB_Chk();
        break; 	    		
	case CmdAVD_ReaderEnable :
	  TxMDB_Cmd(CmdAVD_ReaderEnable,adr);
          TxMDB_Chk();	  
	  break;
	case CmdAVD_ExpWriteTimeDate :
	  /*
	  TxMDB_Cmd(CmdAVD_ExpWriteTimeDate,adr);	
	  TxMDB_Byte(TimeReg[6]);	//BCD Year
	  TxMDB_Byte(TimeReg[5] & 0x1F);	//BCD Month
	  TxMDB_Byte(TimeReg[4]);	//BCD Day
	  TxMDB_Byte(TimeReg[2] & 0x3F);//BCD Hours
	  TxMDB_Byte(TimeReg[1]);	//BCD Minute
	  TxMDB_Byte(TimeReg[0]);	//BCD Second
	  TxMDB_Byte(TimeReg[3]);	//BCD WeekDay
	  TxMDB_Byte(0xFF);		//BCD Week Number
	  TxMDB_Byte(0xFF);		//BCD Summertime
	  TxMDB_Byte(0xFF);		//BCD Holiday
	  TxMDB_Chk();	  	  
	  */
	  break;	  
	case CmdCardreader_ExpDiagnostic :
		switch(MDB_Cardreader.DiagnosticCommand)
		{
		case ExpDiag_AgeVerificationOnOff:		
		  TxMDB_Cmd(CmdCardreader_ExpDiagnostic,adr);
		  TxMDB_Byte(ExpDiag_AgeVerificationOnOff);
		  TxMDB_Byte(0x06);	//length=6
		  TxMDB_Byte(0x12);	//Age=18
		  TxMDB_Byte('D');
		  TxMDB_Byte('R');
		  TxMDB_Byte('A');
		  TxMDB_Byte('V');
		  TxMDB_Byte('P');
		  TxMDB_Chk();	  	  
		break;
		case ExpDiag_ConfigureVendProcess:
		  TxMDB_Cmd(CmdCardreader_ExpDiagnostic,adr);
		  TxMDB_Byte(ExpDiag_ConfigureVendProcess);
		  TxMDB_Byte(0x06);	//length=6
		  
		  TxMDB_Byte(0x04);	//Card before Product
		  
		  TxMDB_Byte('C');
		  TxMDB_Byte('F');
		  TxMDB_Byte('G');
		  TxMDB_Byte('V');
		  TxMDB_Byte('P');
		  TxMDB_Chk();	  	  
		break;
		}
	  
	  break;	          	
      
    }      
}	//end TX_HANDLE_AVD

void TX_Handle_ParkIO(unsigned int adr)
{
  unsigned int s,l;

  MDB_EVENT MDBEvent;
  
  MDB_buffpointer=0;	
  
  if((MDB_ParkIO.DevLost!=0) && (MDB_ParkIO.NonResponseCount==0)) 
  {
      MDB_ParkIO.DevLost--;
      if(MDB_ParkIO.DevLost==0)
      {						
  			MDBEvent.Type    = EvTypeMDB_ParkIOLost;
  			MDBEvent.Length  = 0;
  			putMDBevent(&MDBEvent);
			
	  		InitParkIO();
          	return;
      }  
  }
	
  if ((MDB_ParkIO.NextRequest==CmdParkIO_Poll) && MDB_ParkIO.NewRequest)
  {
	  MDB_ParkIO.NextRequest=MDB_ParkIO.NewRequest;
	  MDB_ParkIO.NewRequest=0;
  }
	
  MDB_ParkIO.LastRequest=MDB_ParkIO.NextRequest;
  switch(MDB_ParkIO.NextRequest)
    {
    case CmdParkIO_Reset :
	  TxMDB_Cmd(CmdParkIO_Reset,adr);
      TxMDB_Chk();
	  MDB_ParkIO.NextRequest=CmdParkIO_Identification; 
	  break;
    case CmdParkIO_Poll :
      TxMDB_Cmd(CmdParkIO_Poll,adr);
      TxMDB_Chk();
      break; 	    		
	case CmdParkIO_Identification :
	  TxMDB_Cmd(CmdParkIO_Identification,adr);
      TxMDB_Chk();	  
	  break;
	case CmdParkIO_Input :
	  TxMDB_Cmd(CmdParkIO_Input,adr);
      TxMDB_Chk();	  
	break;	  	
	case CmdParkIO_Output :
	  TxMDB_Cmd(CmdParkIO_Output,adr);
	  TxMDB_Byte(MDB_ParkIO.RelaisStat);
      TxMDB_Chk();	  
	break;	  	
	case CmdParkIO_SetConfig :
	  TxMDB_Cmd(CmdParkIO_SetConfig,adr);
	  
	  if (MDB_ParkIO.Tara==2)
	  {	
		MDB_ParkIO.LoTemp=MDB_ParkIO.Temp-20;
		MDB_ParkIO.HiTemp=MDB_ParkIO.Temp+20;
		  
	  	TxMDB_Byte(0x55);		
	  	TxMDB_Byte(MDB_ParkIO.LoTemp>>8);
		TxMDB_Byte(MDB_ParkIO.LoTemp);
	  	TxMDB_Byte(MDB_ParkIO.HiTemp>>8);
		TxMDB_Byte(MDB_ParkIO.HiTemp);
		
		MDB_ParkIO.Tara=0;
	  }
	  else
	  {
	  	TxMDB_Byte(0xAA);		
	  	TxMDB_Byte(MDB_ParkIO.Scaling>>8);
		TxMDB_Byte(MDB_ParkIO.Scaling);
		
		  if (MDB_ParkIO.Tara)
		  {
			  TxMDB_Byte(0x01);
			  TxMDB_Byte(0x01);
			  MDB_ParkIO.Tara=0;
		  }
		  else 
		  {
			  TxMDB_Byte(0x00);
			  TxMDB_Byte(0x00);
		  }
	  }
	  
      TxMDB_Chk();	  
	break;	  
    }      
		
}	//end TX_HANDLE_PARKIO

void TxMDB_Byte(unsigned int val)
{
  	uart_tx_program_putc(pio, smtx, val & 0xFF);
	MDBcommand[MDBcommand_len++]=val & 0xFF;
	MDB_chk=MDB_chk+(val & 0xFF);
}
//
void TxMDB_Cmd(unsigned int command,unsigned int adress)
{
	unsigned int cmd;
	cmd=(command & 0x07FF)+adress;
	if((command & 0x8000)==0x8000)
  	   {
		uart_tx_program_putc(pio, smtx, ((cmd>>8) & 0x0FF)+0x0100); //exp Command + SubCommand
		MDBcommand[0]=(cmd>>8) & 0xFF;
		uart_tx_program_putc(pio, smtx, cmd & 0xFF);
		MDBcommand[1]=cmd & 0xFF;
	    MDB_chk=(cmd>>8)+(cmd & 0xFF);
		MDBcommand_len=2;
	   } 
	  else
	   {
		uart_tx_program_putc(pio, smtx, ((cmd>>8) & 0x0FF)+0x0100);	    
		MDBcommand[0]=(cmd>>8) & 0xFF;
	    MDB_chk=(cmd>>8) & 0xFF;	   
		MDBcommand_len=1;
	   }
}
//
void TxMDB_Chk(void) 
{
  	unsigned int s;
	uart_tx_program_putc(pio, smtx, MDB_chk & 0xFF);
	MDBcommand[MDBcommand_len++]=MDB_chk & 0xFF;
  	MDB_timeout=MDB_reload;
  	MDB_buffpointer=0;
 	for (s=0;s<MDB_BUFF_SIZE;s++) MDB_buff[s]=0;
  	
}


//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++Geldr�ckgabe+++++++++++
unsigned char DispenseAmount(unsigned long amount, unsigned char testmode)
{
unsigned char h,s,pri,u,AltTube,apri;
unsigned long coins,hi,am,rest,anz;
unsigned int Tubes[32],lo,found,maxcalc;
unsigned char str[80];
unsigned char amt[16];
unsigned char amt2[16];

  if (amount==0) return 1;

  am=amount;
// Kopieren der CoinCredit / CoinScaling-Werte / Tubestatus

	for (s=0;s<MDB_MAXCOINS;s++) 
 	{
 		PayOutTubes[s].Credit = 0;

		#if (TUBE_CHANGER==1)	  		
		for (h=0;h<MDB_MAXCOINS;h++) if (MDB_Changer1.PhysTubes[h]==s) break;
		if (h<MDB_MAXCOINS)
		{			
			if (!MDB_Changer1.Problem && !SysVar.Tube[h].Deroute && (SysVar.Tube[h].Prio<MAX_PRIORITY))
			{
				PayOutTubes[s].Credit = (unsigned int)MDB_Changer1.CoinScaling * (unsigned int)MDB_Changer1.CoinCredit[s];
 				PayOutTubes[s].TubeFill = SysVar.Tube[h].Fill;
				PayOutTubes[s].Priority = SysVar.Tube[h].Prio;
				SysVar.Tube[h].Ready=1;
			}
			else SysVar.Tube[h].Ready=0;
		}		 		
		#endif
	}
	 
	for (s=0;s<MDB_MAXCOINS;s++) 
 	{ 	
		PayOutTubes[s+MDB_MAXCOINS].Credit 	= 0;
			
 		if (s<MAX_HOPPER)
 		{
 			if ((SysVar.Hopper[s].Status & 0x10) && (!SysVar.Hopper[s].Blocked) && (SysVar.Hopper[s].Prio<MAX_PRIORITY))
 			{
 		 	 	PayOutTubes[s+MDB_MAXCOINS].TubeFill 	= SysVar.Hopper[s].Fill;
 		 		PayOutTubes[s+MDB_MAXCOINS].Credit 	= SysVar.Hopper[s].Val;
				PayOutTubes[s+MDB_MAXCOINS].Priority 	= SysVar.Hopper[s].Prio;
			} 		 	 				
 		}
		else 	
		{			
			PayOutTubes[s+MDB_MAXCOINS].TubeFill 	= 0;
			PayOutTubes[s+MDB_MAXCOINS].Priority 	= 0;
		}
	}

					
// Berechnen der WechslerSummen

	MDB_Changer1.ChangerAmount=0;
#if (TUBE_CHANGER==1)	  
	for (s=0;s<MDB_MAXCOINS;s++)
	 MDB_Changer1.ChangerAmount += (unsigned long)PayOutTubes[s].TubeFill * (unsigned long)PayOutTubes[s].Credit;
	MDB_ChangerTotal = MDB_Changer1.ChangerAmount;	
#endif

	MDB_Changer2.ChangerAmount=0;
	for (s=0;s<MDB_MAXCOINS;s++)
	 MDB_Changer2.ChangerAmount += (unsigned long)PayOutTubes[s+MDB_MAXCOINS].TubeFill * (unsigned long)PayOutTubes[s+MDB_MAXCOINS].Credit;
	MDB_ChangerTotal = MDB_Changer2.ChangerAmount+MDB_Changer1.ChangerAmount;
		
	
// Vergleichen ob Betrag ausreichend
	if (!MDB_ChangerTotal) return 0;
	if (MDB_ChangerTotal<amount) return 0;
	if (amount ==0) return 1;

// Berechnung anhand der einzelnen Tuben
	
	AltTube=0;
	maxcalc=3;
	
	do
	{
		amount=am;
				
		for (s=0;s<32;s++) Tubes[s] = 0;
	// eigentliche Wechselroutine ******
		for (pri=MAX_PRIORITY;pri>0;pri--)
		for (s=32;s>0;s--)
		 {
		 	if (amount)
		 	if (PayOutTubes[s-1].Priority ==(pri-1))
		 	{
		 		if ((PayOutTubes[s-1].Credit) && (PayOutTubes[s-1].TubeFill))
		 	 	{
		 	 		coins = amount / PayOutTubes[s-1].Credit;
					
					
					//-------ab V3.58
					rest = amount % PayOutTubes[s-1].Credit;		
					
					if (rest)
					{
						//Alternative 
						found=0;
						for (apri=MAX_PRIORITY;apri>0;apri--)
						{
							for (u=32;u>0;u--)
							 if (PayOutTubes[u-1].Priority ==(apri-1))
							 {
								 if ((PayOutTubes[u-1].Credit) && (PayOutTubes[u-1].TubeFill))
								 {
									 if ((rest % PayOutTubes[u-1].Credit)==0)
									 {
										 anz = rest / PayOutTubes[u-1].Credit;
										 if (anz <= PayOutTubes[u-1].TubeFill) 
										 {
											 if (AltTube==s)	//komme zum 2.Mal hier durch
											 {
												// Alternative nehmen	 
												Tubes[u-1] += anz;
												amount-=rest;												
											 }
											 else AltTube=s;	// sonst Alternative merken
											 
											 found=1;
											 break;
										 }
									 }
								 }
							 }							 
							 if (found) break;
						}
					}

					//--------ab V3.58									
 	 	
		 	 		if (coins > PayOutTubes[s-1].TubeFill) 
		 	 	 	{
		 	 	 		Tubes[s-1] = PayOutTubes[s-1].TubeFill;
		 	 	 		amount = amount - ((unsigned long)PayOutTubes[s-1].TubeFill * (unsigned long)PayOutTubes[s-1].Credit);
		 	 	 	}
		 	 		else
		 	 	 	{
		 	 	 		Tubes[s-1] = coins;
		 	 	 		amount = amount - (coins * (unsigned long)PayOutTubes[s-1].Credit);
		 	 	 	}					
		 	 	}
		 	 }
		 }
		 
		// ***** Wechsel-Berechnung ende
	}
	while(amount && AltTube && maxcalc--);
 
 
 if (amount ==0) 
  {
	if (!testmode)
	{
		for (s=0;s<32;s++)
 		{
 			PayOutTubes[s].TubeOut += Tubes[s];			// tats�chlich ausgeben			
#if (TUBE_CHANGER==1)			
 			if (s<MDB_MAXCOINS) 			
			{
				for (h=0;h<MDB_MAXCOINS;h++) if (MDB_Changer1.PhysTubes[h]==s) break;				
				if (h<MDB_MAXCOINS) 
				{
					SysVar.Tube[h].Fill -=Tubes[s];	// Fuellstande verringern
				}
			}
#endif
 			if ((s>15) && (s<21)) 		
			{	
				if (!SysVar.Hopper[(s & 0x0F)].Blocked)
			 	 SysVar.Hopper[(s & 0x0F)].Fill -=Tubes[s];	// Fuellstande verringern  			
			}
        }  	        
   
   		//CalcCoinCRC(); //V1.25			
	
    }
  	return 1;	
  }	
 else return 0;
 
}


void SetAKZmax(uint32_t pay, uint8_t changemode)
{
	
  int s=0;
  int t=0;
  int h,der;
  unsigned int CoinBitmask=1;
  unsigned int BillBitmask=1;
  long BillAmount;
  long CoinAmount;

 	MDB_Validator.AKZ_Max =0;
 	MDB_Validator.BillEnable =0;
	
#if (WH_EMP==1)	
 	MDB_Emp.CoinEnable =0;
	MDB_Emp.AKZ_Max =0;
#endif

#if(TUBE_CHANGER==1)
 	MDB_Changer1.AKZ_Max =0;
	MDB_Changer1.CoinEnable=0x0000;
#endif

 	for (s=0;s<MDB_MAXNOTES;s++)
 	{
		// bei Mehfachwaehrung hier ersetzen
		if (BillOver[s].active)
		 	 BillAmount= BillOver[s].Value;
		else BillAmount=(unsigned long)MDB_Validator.BillTypeCredit[s] * (unsigned long)MDB_Validator.BillScaling;
		
 		if (BillAmount && pay && MDB_Validator.ready)
 		{
			if (BillAmount <= SysVar.HighestBill)
			{	
				if (changemode)	//reiner Wechsler
				{
					if (DispenseAmount(BillAmount,1)==1)
	 				{ 	 	
		 				if (BillAmount>MDB_Validator.AKZ_Max) MDB_Validator.AKZ_Max = BillAmount;
	 					ValidatorNotesEN[s] = 1;
	 					MDB_Validator.BillEnable |= BillBitmask;
	 				}
	 				else ValidatorNotesEN[s] = 0;
				}
				else
				{				
					
					if (SysVar.ChangeMaxAmount)
					{											
		 				if (BillAmount <= pay )
		  				{
		   					if (BillAmount>MDB_Validator.AKZ_Max) MDB_Validator.AKZ_Max = BillAmount;
		   					ValidatorNotesEN[s] = 1;
			  				MDB_Validator.BillEnable |= BillBitmask;
		  				}
		 				else
		 				if ( DispenseAmount(labs(BillAmount - (long)pay),1) && (labs(BillAmount - (long)pay) <= (long)SysVar.ChangeMaxAmount) )
		 				{ 	 							
			 				if (BillAmount>MDB_Validator.AKZ_Max) MDB_Validator.AKZ_Max = BillAmount;
		 					ValidatorNotesEN[s] = 1;
		 					MDB_Validator.BillEnable |= BillBitmask;
		 				}
		 				else ValidatorNotesEN[s] = 0;
					}	
					else
					if (pay>=SysVar.BillAccept[s])
					{
		 				if (BillAmount <= pay )
		  				{
		   					if (BillAmount>MDB_Validator.AKZ_Max) MDB_Validator.AKZ_Max = BillAmount;
		   					ValidatorNotesEN[s] = 1;
			  				MDB_Validator.BillEnable |= BillBitmask;
		  				}
		 				else
		 				if ( DispenseAmount(labs(BillAmount - (long)pay),1) )
		 				{ 	 							
			 				if (BillAmount>MDB_Validator.AKZ_Max) MDB_Validator.AKZ_Max = BillAmount;
		 					ValidatorNotesEN[s] = 1;
		 					MDB_Validator.BillEnable |= BillBitmask;
		 				}
		 				else ValidatorNotesEN[s] = 0;						
					}
					else ValidatorNotesEN[s] = 0;
					
				}
			}
			else ValidatorNotesEN[s] = 0;
		}
		else ValidatorNotesEN[s] = 0;
		   		
		BillBitmask = BillBitmask<<1;
 	}

 	for (s=0;s<MDB_MAXCOINS;s++) EmpCoinsEN[s] = 0;
	
	CheckCoinToMain();

  	if (!changemode)
 	for (s=0;s<MDB_MAXCOINS;s++)
 	{ 
	   #if (WH_EMP==1)	
				
		CoinAmount=(long)MDB_Emp.CoinCredit[s] * (long)MDB_Emp.CoinScaling;
		
		if (CoinOver[s].active)
		{
			// bei Mehfachwaehrung hier ersetzen ab V3.63
		    CoinAmount= CoinOver[s].Value;
		}		
			
		if ( CoinAmount && pay && MDB_Emp.ready && (!MDB_Emp.Problem))
 		{ 					
 			if (DispenseAmount(CoinAmount,1)==1)
			{			
	 			if (CoinAmount <= pay )
	  			{
	   				MDB_Emp.AKZ_Max = CoinAmount;
	   				EmpCoinsEN[s] = 1;
					MDB_Emp.CoinEnable |= CoinBitmask;
	  			}
	 			else
	 			if ((DispenseAmount(labs((long)(CoinAmount-(long)pay)),1)) && ((SysVar.ChangeMaxAmount==0) || ((SysVar.ChangeMaxAmount!=0) && (labs(CoinAmount - (long)pay) <= (long)SysVar.ChangeMaxAmount))) )
	 			{ 	 	
		 			MDB_Emp.AKZ_Max = CoinAmount;
	   				EmpCoinsEN[s] = 1;
					MDB_Emp.CoinEnable |= CoinBitmask;
	 			}
			}
			else
			{				
	 			if (CoinAmount == pay )
	  			{
	   				MDB_Emp.AKZ_Max = CoinAmount;
	   				EmpCoinsEN[s] = 1;
					MDB_Emp.CoinEnable |= CoinBitmask;
	  			}
	 			else	// ab V3.58
	 			if ((DispenseAmount(labs((long)(CoinAmount-(long)pay)),1)) && ((SysVar.ChangeMaxAmount==0) || ((SysVar.ChangeMaxAmount!=0) && (labs(CoinAmount - (long)pay) <= (long)SysVar.ChangeMaxAmount))) )
	 			{ 	 	
		 			MDB_Emp.AKZ_Max = CoinAmount;
	   				EmpCoinsEN[s] = 1;
					MDB_Emp.CoinEnable |= CoinBitmask;
	 			}
				else
				for(t=0;t<MAX_HOPPER;t++)
				 if ((SysVar.Hopper[t].Val == CoinAmount) && (SysVar.Hopper[t].Status & 0x10) && (!SysVar.Hopper[t].Blocked) && (SysVar.Hopper[t].Prio<Priorities))
	 			 { 	 	
		 			MDB_Emp.AKZ_Max = CoinAmount;
	   				EmpCoinsEN[s] = 1;
					MDB_Emp.CoinEnable |= CoinBitmask;
					break;
	 			 }				 
			}
			
			if ((s==0) && (pay<CoinAmount) && !MDB_Validator.AKZ_Max)
 			{ 	 	
	 			MDB_Emp.AKZ_Max = CoinAmount;
   				EmpCoinsEN[s] = 1;
				MDB_Emp.CoinEnable |= CoinBitmask;
 			}			
		}		 
   		CoinBitmask = CoinBitmask<<1;
		
	   #elif (TUBE_CHANGER==1)
	
		CoinAmount=(long)MDB_Changer1.CoinCredit[s] * (long)MDB_Changer1.CoinScaling;
		
		if (CoinOver[s].active)
		{
			// bei Mehfachwaehrung hier ersetzen ab V3.63
		    CoinAmount= CoinOver[s].Value;
		}				

 		if ( CoinAmount && pay && MDB_Changer1.ready)
 		{ 					
			if (DispenseAmount(CoinAmount,1)==1)
			{			
	 			if (CoinAmount <= pay )
	  			{		   				
					for (h=0;h<MDB_MAXCOINS;h++) if (MDB_Changer1.AcceptedCoins[h]==s) break;
					if (h<MDB_MAXCOINS) 
					{
						EmpCoinsEN[s] = 1;
						MDB_Changer1.CoinEnable |= CoinBitmask;
						MDB_Changer1.AKZ_Max = CoinAmount;
					}
	  			}
	 			else
	 			if ((DispenseAmount(labs((long)(CoinAmount-(long)pay)),1)) && ((SysVar.ChangeMaxAmount==0) || ((SysVar.ChangeMaxAmount!=0) && (labs(CoinAmount - (long)pay) <= (long)SysVar.ChangeMaxAmount))) )
	 			{ 	 	
					for (h=0;h<MDB_MAXCOINS;h++) if (MDB_Changer1.AcceptedCoins[h]==s) break;
					if (h<MDB_MAXCOINS) 
					{
						EmpCoinsEN[s] = 1;
						MDB_Changer1.CoinEnable |= CoinBitmask;
						MDB_Changer1.AKZ_Max = CoinAmount;
					}
	 			}
			}
			else
			{				
	 			if (CoinAmount == pay )
	  			{
					for (h=0;h<MDB_MAXCOINS;h++) if (MDB_Changer1.AcceptedCoins[h]==s) break;
					if (h<MDB_MAXCOINS) 
					{
						EmpCoinsEN[s] = 1;
						MDB_Changer1.CoinEnable |= CoinBitmask;
						MDB_Changer1.AKZ_Max = CoinAmount;
					}
	  			}
				else
				if (CoinAmount < pay )
				{
					for(t=0;t<MDB_MAXCOINS;t++)
					 if ((SysVar.Tube[t].Val == CoinAmount))
		 			 { 	 	
						for (h=0;h<MDB_MAXCOINS;h++) if (MDB_Changer1.AcceptedCoins[h]==s) break;
						if (h<MDB_MAXCOINS) 
						{
							EmpCoinsEN[s] = 1;
							MDB_Changer1.CoinEnable |= CoinBitmask;
							MDB_Changer1.AKZ_Max = CoinAmount;
						}
						break;
		 			 }				 
				}
	 			else	// ab V3.58
	 			if ((DispenseAmount(labs((long)(CoinAmount-(long)pay)),1)) && ((SysVar.ChangeMaxAmount==0) || ((SysVar.ChangeMaxAmount!=0) && (labs(CoinAmount - (long)pay) <= (long)SysVar.ChangeMaxAmount))) )
	 			{ 	 	
					for (h=0;h<MDB_MAXCOINS;h++) if (MDB_Changer1.AcceptedCoins[h]==s) break;
					if (h<MDB_MAXCOINS) 
					{
						EmpCoinsEN[s] = 1;
						MDB_Changer1.CoinEnable |= CoinBitmask;
						MDB_Changer1.AKZ_Max = CoinAmount;
					}
	 			}
				
			}

			if ((s==MDB_Changer1.PhysTubes[0]) && (pay<CoinAmount) && !MDB_Validator.AKZ_Max)
			{ 	 	
				for (h=0;h<MDB_MAXCOINS;h++) if (MDB_Changer1.AcceptedCoins[h]==s) break;
				if (h<MDB_MAXCOINS) 
				{
					EmpCoinsEN[s] = 1;
					MDB_Changer1.CoinEnable |= CoinBitmask;
					MDB_Changer1.AKZ_Max = CoinAmount;
				}
			}			
		}
	 
   		CoinBitmask = CoinBitmask<<1;			
	#endif		
 	}
 

 	if (MDB_Validator.AKZ_Max==0) 
 	{
 		//if (MDB_Validator.ready) 
		 //if (!SimpleMode) IlluminateBillInsert(0);
 		EnableBills(0);
 	}

#if (WH_EMP==1)	
 	if (MDB_Emp.AKZ_Max ==0) 
 	{
		if (ToPay==0) 
		{		
			if (MDB_Emp.ready && (!MDB_Emp.Problem))
 			if (!SimpleMode) IlluminateCoinInsert(0);
 			EnableCoins(0x0000);
		}
 	}
	
 	if (MDB_Emp.AKZ_Max>MDB_Validator.AKZ_Max) 
  		 AKZmax=MDB_Emp.AKZ_Max;
 	else AKZmax=MDB_Validator.AKZ_Max;	
#elif (TUBE_CHANGER==1)
 	if (MDB_Changer1.AKZ_Max ==0) 
 	{
		/*
		if (ToPay==0) 
		{		
			if (MDB_Changer1.ready && !MDB_Changer1.Problem) 
			 if (!SimpleMode) IlluminateCoinInsert(0);
 			EnableCoins(0x0000);
		}
		*/
 	}
	
 	if (MDB_Changer1.AKZ_Max>MDB_Validator.AKZ_Max) 
  		 AKZmax=MDB_Changer1.AKZ_Max;
 	else AKZmax=MDB_Validator.AKZ_Max;	
#endif
   
	/*
    if (AKZmax)
	{			
  	 	ChangePossible = DispenseAmount(labs((long)((long)AKZmax-(long)ToPay)),1);

		if (PaidCash) 
		     ReturnPossible = DispenseAmount(PaidCash,1); 
		else ReturnPossible=ChangePossible;		
	}
	else
	{
		if (pay)
		{
			ReturnPossible = 0;
			ChangePossible = 0;
		}
		else
		{
			ReturnPossible = 1;
			ChangePossible = 1;
		}
	}
 
 	if (SysVar.Cashless)
	{
		ReturnPossible = 1;
		ChangePossible = 1;		
	} 
	*/
}

void CheckCoinToMain()
{
  unsigned int h,k,s,der;
  char mask;
  #if (WH_EMP==1)		

	k=0x0001;
	MDB_Emp.CoinToMain=0;
	for (s=0;s<MDB_MAXCOINS;s++) 
	{
 	 	if (s & 0x01) 
 		mask= (MDB_Emp.CoinRouting[(s>>1)] & 0x0F);
  	 	else	mask= ((MDB_Emp.CoinRouting[(s>>1)]>>4) & 0x0F);
	  	 		  	 
		if (!mask) 
		{
			MDB_Emp.CoinToMain |=k;  
		}
			 		
		if ((mask & 0x08) && ((!(SysVar.Hopper[0].Status & 0x10)) || (SysVar.Hopper[0].Max && (SysVar.Hopper[0].Fill>=SysVar.Hopper[0].Max)) || (SysVar.Hopper[0].Prio>=Priorities))) 
		{
			MDB_Emp.CoinToMain |=k;			
		}
		if ((mask & 0x04) && ((!(SysVar.Hopper[1].Status & 0x10)) || (SysVar.Hopper[1].Max && (SysVar.Hopper[1].Fill>=SysVar.Hopper[1].Max)) || (SysVar.Hopper[1].Prio>=Priorities))) 
		{
			MDB_Emp.CoinToMain |=k;
		}
		if ((mask & 0x02) && ((!(SysVar.Hopper[2].Status & 0x10)) || (SysVar.Hopper[2].Max && (SysVar.Hopper[2].Fill>=SysVar.Hopper[2].Max)) || (SysVar.Hopper[2].Prio>=Priorities))) 
		{
			MDB_Emp.CoinToMain |=k;
		}
		if ((mask & 0x01) && ((!(SysVar.Hopper[3].Status & 0x10)) || (SysVar.Hopper[3].Max && (SysVar.Hopper[3].Fill>=SysVar.Hopper[3].Max)) || (SysVar.Hopper[3].Prio>=Priorities)))
		{
			MDB_Emp.CoinToMain |=k;
		}
  	 	
 		k = k<<1;	  	 	
  	 }	  	 	
#elif (TUBE_CHANGER==1)
	

	k=0x0001;
	MDB_Changer1.CoinToMain=0;
		
	for (s=0;s<MDB_MAXCOINS;s++) 
	{
		for (h=0;h<MDB_MAXCOINS;h++) if (MDB_Changer1.PhysTubes[h]==s) break;
		if (h<MDB_MAXCOINS) 
		{
			der = SysVar.Tube[h].Deroute;
			if (der--)
			{
				if (SysVar.Hopper[der].Max)
				 if (SysVar.Hopper[der].Fill>=SysVar.Hopper[der].Max)
				  MDB_Changer1.CoinToMain |=k;
			}
			else
			{				
				if (SysVar.Tube[h].Max)
				 if (SysVar.Tube[h].Fill>=SysVar.Tube[h].Max)
				  MDB_Changer1.CoinToMain |=k;
				  
				if (SysVar.Tube[h].Prio>=MAX_PRIORITY) 
				 MDB_Changer1.CoinToMain |=k;
			}
		}
		
		k = k<<1;
	}	

#endif
}

/**************************************************************************
****			    Beckmann GmbH 	                       ****
**************************************************************************/
