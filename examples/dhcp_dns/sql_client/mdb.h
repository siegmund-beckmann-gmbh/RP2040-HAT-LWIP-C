/* ============================================================================================ *
 * routines for MDB-Bus communications								*
 * Header-File											* 
 * ============================================================================================ */
 
#ifndef MDB_H
#define MDB_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hardware/pio.h"
#include "uart_tx9.pio.h"
#include "uart_rx9.pio.h"
#include "pico/util/queue.h"


//----------------------WH_EMP----------------------------

#define MDB_MAX_DEVICE 3   //   4=+AVD 5=+ParkIO
#define MAX_HOPPER 5
#define WH_EMP 0
#define TUBE_CHANGER 1
#define MAXCOINS 16
#define MAXBILLS 16
#define MAX_PRIORITY 8

enum event_types{
	EvTypeMDB_NAK = 1,
	EvTypeMDB_ChangerReady,
	EvTypeMDB_ChangerStatus,
	EvTypeMDB_ChangerTubeStatusChanged,
	EvTypeMDB_ChangerProblem,
	EvTypeMDB_ChangerLost,
	EvTypeMDB_ChangerDispenseInfo,
	EvTypeMDB_ChangerExpandedPayout,
	EvTypeMDB_CoinInCashbox,
	EvTypeMDB_CoinInTube,
	EvTypeMDB_CoinRejected,
	EvTypeMDB_CoinDispensedManually,
	EvTypeMDB_TubesNewFillStatus,
	EvTypeMDB_BillStackedManually,
	EvTypeMDB_ValidatorStatus,
	EvTypeMDB_ValidatorLost,
	EvTypeMDB_BillInEscrow,
	EvTypeMDB_BillStacked,
	EvTypeMDB_BillReturned,
	EvTypeMDB_BillRejected,
	EvTypeMDB_BillEscrowTimeout,
	EvTypeMDB_HopperInserted,
	EvTypeMDB_HopperStatusChanged,
	EvTypeMDB_HopperCounted,
	EvTypeMDB_HopperTimeout,
	EvTypeMDB_HopperPaidOut,
	EvTypeMDB_HopperRemoved,
	EvTypeMDB_CardreaderReady,
	EvTypeMDB_CardreaderError,
	EvTypeMDB_CardreaderLost,
	EvTypeMDB_CardreaderDisplay,
	EvTypeMDB_CardreaderFunds,
	EvTypeMDB_CardreaderVendApproved,
	EvTypeMDB_CardreaderVendDenied,
	EvTypeMDB_CardreaderAgeVerificationStatus,
	EvTypeMDB_EmpLost,
	EvTypeMDB_EmpStatus,
	EvTypeMDB_EmpCoinInserted,
	EvTypeMDB_AVDLost,
	EvTypeMDB_ParkIOLost
};

typedef struct __attribute__((packed)) MDBevent{
	uint16_t  Type;
	uint16_t  Length;
	uint8_t Data[60];
}MDB_EVENT;

typedef struct __attribute__((packed)) HOPPERtag{	//20
	uint16_t Val;
	uint16_t Fill;
	uint16_t LastFill;
	uint16_t Threshold;
	uint16_t Max;
 	uint8_t  Prio;
	uint8_t  Status;
	uint8_t  Ready;
	uint8_t  Hysteresis;	
	uint8_t  Blocked;
	uint8_t  ManChange;
	int32_t  Amount;
}HOPPER;

typedef struct __attribute__((packed)) COINStag{	//8
	uint16_t  Count;
	uint16_t  Credit;
	int32_t   Amount;
}COINS;

typedef struct __attribute__((packed)) BILLStag{	//8
	uint16_t Count;
	uint16_t Credit;
	int32_t  Amount;
}BILLS;

typedef struct __attribute__((packed)) TUBEStag{	//20
	uint8_t  Prio;
	uint8_t  Ready;
	uint16_t Val;
	uint16_t Fill;
	uint16_t LastFill;
	uint16_t Threshold;
	uint16_t Max;
	uint8_t  Hysteresis;
	uint8_t  ManChange;
	uint8_t  Deroute;
	uint8_t  avail;
	int32_t  Amount;
}TUBES;


typedef struct __attribute__((packed)) SAVEVARtag{

	struct HOPPERtag	Hopper[5];			
	struct COINStag		Coin[16];
	struct BILLStag		Bill[16];
	unsigned int        CRC;
	unsigned char		OldTubes[14];
		 	 long 		HopperTotalAmount;
	 	 	 long 		ChangeTotalAmount;
			 long 		ChangeMaxAmount;
			 long 		CoinTotalAmount;
			 long 		BillTotalAmount;
	
	unsigned char 		SetHysteresis;
		 	 long 		TotalAmount;
	unsigned long		HighestBill;
	unsigned long		BillCredit[16];
		
	struct TUBEStag		Tube[16];

	unsigned long		BillAccept[16];
	
	unsigned char 		VerificationAge;			//1254
	unsigned int		SmallestCoin;				//1425
				
	unsigned char		CRTactAni;					//1780
	unsigned char		CRToffAni;					//1781
	unsigned char		CRTerrAni;					//1782
	unsigned char		CRTremAni;					//1783
	
	unsigned char		BNAactAni;					//1784
	unsigned char		BNAoffAni;					//1785								
	unsigned char		BNAerrAni;					//1786								
	
	unsigned char		COINactAni;					//1787
	unsigned char		COINoffAni;					//1788
	unsigned char		COINerrAni;					//1789

	unsigned char		RGBdimming;					//1790
	unsigned char		RGBprescale;				//1791	
	
			 long 		CancelMaxAmount;			//1798
			 
	unsigned char		AcceptAllCoins;				//1805 V4.09	
			 long		ChangeBaseAmount;			//1806 V4.06			 
	unsigned char		AcceptAllBills;				//1810 V4.09
										
	unsigned char		DirEntryTimeout;
	unsigned char		DirEntryPause;
	unsigned char		DirEntryRepeat;
										
	unsigned int		checksum;				

}SAVEVARS;


typedef struct
        {
	unsigned char CoinScaling,Decimals,Level; 
	unsigned int  Currency;
	unsigned char SorterInfo;
	unsigned char TubeChannel[8];
	unsigned char CoinCredit[16];
	
	unsigned char MasterSlave[8];
	
	unsigned long CoinEnable;
	unsigned int  CoinToMain;
	
	unsigned char MainTubeNumber;
	unsigned char CoinRouting[8];
	
    unsigned char Manufacturer[4];	//incl Null
	unsigned char Serial[13];  	//incl Null	
	unsigned char Model[13];  	//incl Null
	unsigned int  Version;  
	unsigned long OptionBits;

	unsigned int  LastRequest,NextRequest,Sequence,ResetTime,NewRequest,NewSequence;
	unsigned char DevLost;
      
  	unsigned char Status[16];
	unsigned char LastStatus,Problem;
	unsigned int  LastValue;
	unsigned char GetNextStatus;
	unsigned int  AKZ_Max;
	unsigned char ready;
	}EmpTag;


//-----------------------changer------------------------
typedef struct
        {
	unsigned char Level,CoinScaling,Decimals,Dispense,Payout,PayoutValuePoll,Toggle; 
	unsigned int  Currency;
	unsigned int  CoinRouting;
	unsigned char CoinCredit[16];
	unsigned int  TubeFullStatus;
	unsigned char TubeStatus[16];				// = TUBE_COUNT !!!
    unsigned char Manufacturer[4];	//incl Null
	unsigned char Serial[13];  	//incl Null	
	unsigned char Model[13];  	//incl Null
	unsigned int  Version;  	
    unsigned long OptionalFeatures,FeatureEnable;
	unsigned char PayoutStatus[16];
	unsigned char SendDiagnosticStatus[16];
    unsigned int  CoinEnable;
	unsigned int  ManualDispenseEnable;
	unsigned int  LastRequest,NextRequest,Sequence,ResetTime,NewRequest,NewSequence;
	unsigned char NextTubestatus,NextDiag,DevLost;
	unsigned char Status[16];
	unsigned int  LastStatus,LastValue,Problem,LastProblem;
	unsigned long ChangerAmount;
	unsigned char PhysTubes[16];
	unsigned int  AKZ_Max;
	unsigned char ready,DiagnosticCommand,SetTubes,TubesRead,available;
	
	unsigned char TubeCountAF[6];				// ab Currenza V1.11
	unsigned char CoinTypeAF[6];				// ab Currenza V1.11
	unsigned int  HopperCount[4];				// ab Currenza V1.11
	unsigned char CoinTypeHopper[4];			// ab Currenza V1.11
	unsigned int  TempFill[16];
	unsigned char SafeCoins[16];
	unsigned int  CoinToMain;
	
	unsigned char AcceptedCoins[16];
	
	}ChangerTag;


typedef struct {				//*D*/	
	unsigned char Priority;	
	unsigned int  TubeOut;
	unsigned int  Credit;
	unsigned int  TubeFill;
}TubeCfg;

//-----------------------cardreader------------------------
typedef union
        {
        unsigned char B[4];
        unsigned int  I[2];
        unsigned long LO;
        }longdef;
        

typedef struct 
{
	unsigned char TMPStatus[34];
	unsigned char Status,DevLost,Level,Scaling,Decimals,NonResponseTime,MiscOptions,ErrorCode;
	unsigned int  LastRequest,NextRequest,Sequence,ResetTime,DisplayCount,NewRequest;
	unsigned long FeatureEnable,OptionalFeatures;
	unsigned char PaymentType,PaymentData[2],UserLanguage[2],UserCurrencyCode[2],UserCardOptions;
	unsigned int  ItemNumber,CountryCode;

	unsigned long ItemPrice,RevalueAmount,RevalueLimitAmount,VendAmount,FundsAvailable;
	unsigned long DispFundsAvailable,NonResponseCount;
	longdef       PaymentMediaID;
	unsigned char DisplayTime,DisplayData[34];
	unsigned char Manufacturer[4];	//incl Null
	unsigned char Serial[13];  	//incl Null
	unsigned char Model[13];  	//incl Null
	unsigned int  Version;
	unsigned char DataEntryLenRep;
	unsigned char ready;
	unsigned char Inhibit,Inhibit_Old;
	unsigned char DRAV[6];  	//incl Null
	unsigned char AVSFeature1,AVSFeature2,DiagnosticCommand,AVSConfiguration;
}CardreaderTag;

//-----------------------BillValidator---------------------
typedef struct 
{
    unsigned int  accepted;
	unsigned char Level,Decimals,Escrow,EscrowStatus,StackerStatus;
	unsigned int  Currency;
	unsigned int  BillScaling;
	unsigned char DecimalPlaces;
	unsigned int  StackerCapacity;
	unsigned int  BillSecurityLevels;
	unsigned char BillTypeCredit[16];
	unsigned char Manufacturer[4];
	unsigned char Serial[13];  
	unsigned char Model[13];  	
	unsigned int  Version;  	
    unsigned long OptionalFeatures,FeatureEnable;
	unsigned int  Security,BillEnable,BillEscrowEnable;
	unsigned int  LastRequest,NextRequest,Sequence,ResetTime,NewRequest,NewSequence;
	unsigned char NextStackerStatus,DevLost;
	unsigned char Status[16];
	unsigned long AKZ_Max,AKZ_Min;
	unsigned int  LastStatus,EscrowTimeout;
	unsigned long NoteValue,LastValue;
	unsigned char Inhibit,Inhibited;
	unsigned char ready,EscrowChannel;
}ValidatorTag;

//-----------------------Age Verification Device------------------------

typedef struct 
{
	unsigned char Status,DevLost,Level,NonResponseTime,MiscOptions,ErrorCode;
	unsigned int  LastRequest,NextRequest,ResetTime,NewRequest;	
	unsigned long NonResponseCount;
	unsigned char Manufacturer[4];	//incl Null
	unsigned char Serial[13];  	//incl Null
	unsigned char Model[13];  	//incl Null
	unsigned int  Version;
	unsigned char ready;
	unsigned char DRAV[6];  	//incl Null
	unsigned char AVSFeature1,AVSFeature2,DiagnosticCommand,AVSConfiguration;
}AVDTag;

//-----------------------ParkIO Device------------------------

typedef struct 
{
	unsigned char DevLost,Level,NonResponseTime,MiscOptions,ErrorCode;
	unsigned int  LastRequest,NextRequest,ResetTime,NewRequest;	
	unsigned long NonResponseCount;
	unsigned char Manufacturer[4];	//incl Null
	unsigned char Serial[13];  	//incl Null
	unsigned char Model[13];  	//incl Null
	unsigned int  Version;
	unsigned char ready,ResetFlag,Tidx,Hidx;		
	unsigned char Status[16];
				int  Temp, HiTemp,LoTemp;
				int  TempArray[8];				 
	unsigned int  Humidity;
	unsigned int  HumidityArray[8];
				long Weight;		
	unsigned char RelaisStat,InputStat;
	unsigned char InputPulse[4];
	unsigned char Scaling,Tara;
}PARKIOTag;

//*******************CONSTANTS**************************
#define MDB_reload  					40		//40ms
#define MDB_ack 					0x00
#define MDB_ret 					0xAA
#define MDB_nak 					0xFF
#define MDB_ChangerAdress 				0x08
#define TubeStatusRepeat 				0x05	//alle 5 mal Tube Status
#define DiagRepeat 						0x08	//alle 8 mal Diagnostics
#define StackerStatusRepeat 			0x0A	//alle 10 mal BillStacker Status
#define MDB_SetupTime 					250	//250ms Setup Time
#define MDB_LostCount					10 	//500ms Versuche
#define MAXVAL						10000
#define MDB_MAXCOINS					16
#define MDB_MAXNOTES					16

#define MDB_Adr_Changer1 				0x0800 	
#define MDB_Adr_Cashless 				0x1000
#define MDB_Adr_AuditSystem				0x1800
#define MDB_Adr_Display					0x2000
#define MDB_Adr_EnergyManagement 		0x2800
#define MDB_Adr_BillValidator			0x3000
#define MDB_Adr_USD1  					0x4000
#define MDB_Adr_USD2  					0x4800
#define MDB_Adr_USD3  					0x5000
#define MDB_Adr_Cashless2				0x6000	// ab MDB-V3.0
#define MDB_Adr_AVD  					0x6800
#define MDB_Adr_Changer3				0x7000	// ab MDB-V4.1
#define MDB_Adr_Emp						0x7800 		
#define MDB_Adr_Changer2 				0xF000 	
#define MDB_Adr_ParkIO 					0xF800

#define CmdEmp_Reset 					0x0000
#define CmdEmp_Status 					0x0100
#define CmdEmp_Poll 					0x0300
#define CmdEmp_CoinType					0x0400
#define CmdEmp_CoinRouting				0x0600
#define CmdEmp_ExpIdentification		0x8700
#define CmdEmp_ExpRequest       		0x8701
#define CmdEmp_ExpExtendedCoinType     	0x8720
#define CmdEmp_ExpExtendedRequest      	0x8721
#define CmdEmp_ExpMasterSlave          	0x8723

#define Emp_Sequence_INIT				0x0001
#define Emp_SetupTime					2000	     	//2000ms
#define EmpStatusRepeat					100

#define CmdChanger_Reset 				0x0000	//0x08
#define CmdChanger_Status 				0x0100	//0x09
#define CmdChanger_TubeStatus 			0x0200	//0x0A
#define CmdChanger_Poll 				0x0300  //0x0B	
#define CmdChanger_CoinType				0x0400  //0x0C
#define CmdChanger_Dispense				0x0500  //0x0D
#define CmdChanger_ExpIdentification		0x8700
#define CmdChanger_ExpFeatureEnable			0x8701
#define CmdChanger_ExpPayout				0x8702
#define CmdChanger_ExpPayoutStatus			0x8703
#define CmdChanger_ExpPayoutValuePoll		0x8704
#define CmdChanger_ExpSendDiagnosticStatus	0x8705
#define CmdChanger_ExpSendControlledManualFillReport	0x8706
#define CmdChanger_ExpSendControlledManualPayoutReport	0x8707
#define CmdChanger_ExpDiagnostic			0x87FF

#define	ExpDiag_SuppressCoinRouting			0x11
#define	ExpDiag_SecureCoinAcceptance		0x12
#define	ExpDiag_RecycledBill				0x13
#define	ExpDiag_TubeStatus					0x15
#define	ExpDiag_SetHopperCounter			0x16
#define	ExpDiag_SetTubeCounter				0x17

#define Changer_Sequence_INIT				0x0001
#define Changer_Sequence_COINTYPE			0x0002
#define Changer_Sequence_DISPENSE			0x0003

#define Changer_SetupTime				750		//750ms

//--------------Constants
#define ExandedCurrencyMode	  			0x000002	//32 Bit Currency

#define	SetupConfigData_VMC_Level 			3	//VMC IS LEVEL 1
#define	SetupConfigData_Columns   			16 	//Columns
#define	SetupConfigData_Rows      			2	//Rows
#define	SetupConfigData_Display   			1	//Full ASCII
#define	SetupMaxMinPrice_MaxPrice 			0xFFFFFFFF //Max Price
#define	SetupMaxMinPrice_MinPrice 			0x00000000 //Max Price

#define	ExpRequestID_Version1 				0x01	//Version 1.00
#define	ExpRequestID_Version2 				0x00
#define VMC_Currency1						0x19
#define VMC_Currency2						0x78

#define Cardreader_SetupTime			 	500		//500ms

//--------------Status
#define CardreaderStatus_JustReset				0x00
#define CardreaderStatus_ReaderConfigData		0x01
#define CardreaderStatus_DisplayRequest  		0x02
#define CardreaderStatus_BeginSession    		0x03
#define CardreaderStatus_SessionCancelRequest 	0x04
#define CardreaderStatus_VendApproved  			0x05
#define CardreaderStatus_VendDenied 			0x06
#define CardreaderStatus_EndSession  			0x07
#define CardreaderStatus_Cancelled	  			0x08
#define CardreaderStatus_PeripheralID  			0x09
#define CardreaderStatus_Malfunction  			0x0A
#define CardreaderStatus_CmdOutOfSequence		0x0B
#define CardreaderStatus_RevalueApproved		0x0D
#define CardreaderStatus_RevalueDenied 			0x0E
#define CardreaderStatus_RevalueLimitAmount		0x0F
#define CardreaderStatus_UserFileData  			0x10
#define CardreaderStatus_TimeDateRequest		0x11
#define CardreaderStatus_DataEntryRequest		0x12
#define CardreaderStatus_DataEntryCancel		0x13
#define CardreaderStatus_DiagnosticResponse		0xFF

//-------------Commands
#define CmdCardreader_Reset 					0x0000			//Level 1
#define CmdCardreader_SetupConfigData  			0x8100			//Level 1
#define CmdCardreader_SetupMaxMinPrice 			0x8101			//Level 1
#define CmdCardreader_Poll 						0x0200			//Level 1
#define CmdCardreader_VendRequest 				0x8300			//Level 1
#define CmdCardreader_VendCancel 				0x8301			//Level 1
#define CmdCardreader_VendSuccess 				0x8302			//Level 1
#define CmdCardreader_VendFailiure 				0x8303			//Level 1
#define CmdCardreader_VendSessionComplete		0x8304			//Level 1
#define CmdCardreader_VendCashSale 				0x8305			//Level 1
#define CmdCardreader_VendNegative 				0x8306			//Level 3
#define CmdCardreader_ReaderDisable				0x8400			//Level 1
#define CmdCardreader_ReaderEnable				0x8401			//Level 1
#define CmdCardreader_ReaderCancel				0x8402			//Level 1
#define CmdCardreader_ReaderDataEntryResponse	0x8403			//Level 3
#define CmdCardreader_RevalueRequest			0x8500			//Level 2
#define CmdCardreader_RevalueLimitRequest		0x8501			//Level 2
#define CmdCardreader_ExpRequestID				0x8700			//Level 1
#define CmdCardreader_ExpReadUserFile			0x8701			//Level 2 not for Future Use
#define CmdCardreader_ExpWriteUserFile			0x8702			//Level 2 not for Future Use
#define CmdCardreader_ExpWriteTimeDate			0x8703			//Level 3
#define CmdCardreader_ExpOptionalFeatureEnabled	0x8704		
#define CmdCardreader_ExpDiagnostic				0x87FF

#define CmdCardreader_FTL_REQ_TO_SEND			0x87FE
#define CmdCardreader_FTL_OK_TO_SEND			0x87FD
#define CmdCardreader_FTL_SEND_BLOCK			0x87FC
#define CmdCardreader_FTL_RETRY_DENY			0x87FB
#define CmdCardreader_FTL_REQ_TO_RCV			0x87FA

#define Cardreader_Sequence_INIT			0x0001
#define Cardreader_Sequence_SESSION			0x0002
#define Cardreader_Sequence_CANCELSESSION	0x0004
#define Cardreader_Sequence_VEND			0x0008
#define Cardreader_Sequence_CANCELVEND		0x0010
#define Cardreader_Sequence_CANCELREADER	0x0020
#define Cardreader_Sequence_FINISHVEND		0x0040

#define	ExpDiag_AgeVerificationOnOff		0x05
#define	ExpDiag_AgeVerificationStatus		0x06
#define	ExpDiag_ConfigureVendProcess		0x07

#define CmdValidator_Reset 						0x0000
#define CmdValidator_Status 					0x0100
#define CmdValidator_Security 					0x0200
#define CmdValidator_Poll 						0x0300
#define CmdValidator_BillType					0x0400
#define CmdValidator_Escrow						0x0500
#define CmdValidator_Stacker					0x0600
#define CmdValidator_ExpLevel1Identification	0x8700
#define CmdValidator_ExpLevel2FeatureEnable		0x8701
#define CmdValidator_ExpLevel2Identification	0x8702
#define CmdValidator_ExpDiagnostic				0x87FF

#define Validator_Sequence_INIT				0x0001
#define Validator_Sequence_ESCROW			0x0002
#define Validator_Sequence_STACKING			0x0003
#define Validator_Sequence_REJECT  			0x0004

#define Validator_SetupTime				1000  //500ms   
#define Validator_EscrowTime				3000

#define AVDStatus_JustReset				0x00
#define AVDStatus_TimeDateRequest		0x11
#define AVDStatus_DiagnosticResponse	0xFF


#define CmdAVD_Reset 				0x0000			//Level 1
#define CmdAVD_Poll 				0x0200			//Level 1
#define CmdAVD_ReaderEnable			0x8401			//Level 1
#define CmdAVD_ExpWriteTimeDate		0x8703			//Level 3
#define CmdAVD_ExpDiagnostic		0x87FF

#define AVD_SetupTime					2000		//2000ms



#define ParkIOStatus_JustReset				0x00

#define CmdParkIO_Reset 			0x0000			//Level 1
#define CmdParkIO_Poll 				0x0100			//Level 1	Rückgabe |16bit Statusbits|16bit Humidity|16bit Temp|32 bit Weight|16bit Scaling|16bit LoTemp|16bit HiTemp|
																	 //  |JustReset|MemChip|HX-Chip|WeightChip|In8|In7|In6|In5|RelHeat|RelVent|RelEx1|RelEx2|RelO1|RelO2|RelO3|RelO4|
#define CmdParkIO_Identification	0x0200			//Level 1	Rückgabe wie Changer2
#define CmdParkIO_Input  			0x0300			//Level 1	Rückgabe |8bit Pulse In8|8bit Pulse In7|8bit Pulse In6|8bit Pulse In5|
#define CmdParkIO_Output			0x0400			//Level 1   Setzen |8bit RelaisBits wie bei Poll|    Rückgabe |OK|
#define CmdParkIO_SetConfig			0x0500			//Level 1   Setzen |0xAA|16bit Scaling|16bit Tara| oder Setzen |0x55|16bit LoTemp|16bit HiTemp| R�ckgabe |OK|

#define ParkIO_SetupTime					2000		//2000ms

// ----Prototype-Declarations--------------------------------------------------------  

void HandleMDB(unsigned int);
void Analyze_MDB(unsigned char);
void MDBTimeout(void);
void handleMDBtimer(void);
void InitMDB(void);
void TxMDB_Byte(unsigned int);
void TxMDB_Cmd(unsigned int command,unsigned int adress);
void TxMDB_Chk(void); 

void TX_Handle_Emp(unsigned int);
void RX_Handle_Emp(unsigned char);

void TX_Handle_Changer(ChangerTag *MDB_Changer,unsigned int);
void TX_Handle_Cardreader(unsigned int adr);
void TX_Handle_Validator(unsigned int adr);
void TX_Handle_AVD(unsigned int adr);
void TX_Handle_ParkIO(unsigned int adr);

void RX_Handle_Changer(ChangerTag *MDB_Changer,unsigned char);
void RX_Handle_Cardreader(unsigned char);
unsigned char Handle_Cardreader_Status(unsigned char buffpos,unsigned char bufflen);
void RX_Handle_Validator(unsigned char);
void RX_Handle_AVD(unsigned char buff_point);
unsigned char Handle_AVD_Status(unsigned char buffpos,unsigned char bufflen);
void RX_Handle_ParkIO(unsigned char buff_point);
unsigned char Handle_ParkIO_Status(unsigned char buffpos,unsigned char bufflen);


void EnableCoins(unsigned int enable);
void EnableManualDispense(unsigned int enable);
void EnableIllumination();

void EnableBills(unsigned int enable);
//void DisableBills(void);

unsigned char DispenseAmount(unsigned long amount, unsigned char testmode);
//void SetAKZmax(unsigned long pay, unsigned char changemode);

//void CheckHopperFill();
void CheckCoinToMain();

void putMDBevent(MDB_EVENT *event);

extern void CalcCoinCRC(bool writeback, int callID);

extern bool trayLight;

#endif /* MDB_H */
// -----------------------------------------------------------------------------------

/****************************************************************************************************************************
 ****			   BECKMANN GmbH                                                                                 ****
 ****************************************************************************************************************************/
