/* ============================================================================================ *
 * routines for Front RGB								*
 * Program-File											* 
 * ============================================================================================ */
 
#include    "illumination.h" // Header File

//*******************VARIABLES**************************

RGBANI RGB_custom[3][16];

unsigned char CoinIllum		= 0;
unsigned char CoinIllumAni	= ANI_OFF;
unsigned char BillIllum		= 0;
unsigned char BillIllumAni	= ANI_OFF;
unsigned char CardIllum		= 0;
unsigned char CardIllumAni	= ANI_OFF;


const unsigned char MLED_grn[]= {0x08,		//LEN
				 0xFE,0x00,
				 0xFD,0x00,
				 0xFB,0x00,
				 0xF7,0x00,
				 0xEF,0x00,
				 0xDF,0x00,
				 0xBF,0x00,
				 0x7F,0x00};

const unsigned char MLED_red[]= {0x04,		//LEN
				 0x00,0xFF,
				 0x00,0xFF,
				 0x00,0x00,
				 0x00,0x00};

const unsigned char MLED_gfl[]= {0x04,		//LEN
				 0xFF,0x00,
				 0xFF,0x00,
				 0x00,0x00,
				 0x00,0x00};
	
const unsigned char MLED_rcn[]= {0x01,		//LEN
				 0x00,0xFF};
const unsigned char MLED_gcn[]= {0x01,		//LEN
				 0xFF,0x00};
const unsigned char MLED_ora[]= {0x01,		//LEN
				 0xFF,0xFF};
const unsigned char MLED_off[]= {0x01,		//LEN
				 0x00,0x00};	
const unsigned char MLED_crd[]= {0x06,		//LEN
				 0x1F,0x00,
				 0x0E,0x00,
				 0x04,0x00,
				 0x00,0x00,
				 0x11,0x00,
				 0x1B,0x00
                 };	
				 
const char *animation[] = {
	MLED_off,
	MLED_grn,
	MLED_red,
	MLED_rcn,
	MLED_gcn,
	MLED_gfl,
	MLED_ora,
	MLED_crd
    };

unsigned char PCA[MAX_PCA]={PCA9635_0,PCA9635_1,PCA9635_2};
unsigned char FRONTRGBStatus[MAX_PCA];
unsigned char FRONTRGB_Lost[MAX_PCA];
unsigned char FRONTRGB_Adr[MAX_PCA];
unsigned char FRONTRGB_Count[MAX_PCA];
unsigned char FRONTRGB_Scale1[MAX_PCA];
unsigned char FRONTRGB_Scale2[MAX_PCA];
unsigned char FRONTRGB_Scale3[MAX_PCA];
unsigned char FRONTRGB_Init[MAX_PCA];
unsigned char FRONTRGB_AniSelect[MAX_PCA];
unsigned char FRONTRGB_AniCount[MAX_PCA];
unsigned char FRONTRGB_AniCountOld[MAX_PCA];
unsigned char FRONTRGB_Ani[MAX_PCA];
unsigned char FRONTRGB_AniOld[MAX_PCA];
unsigned char FRONTRGB_Rot[MAX_PCA];
unsigned int  FRONTRGB_Var[MAX_PCA];
unsigned int  FRONTRGB_Time[MAX_PCA];

Fr_RGB	FRONTRGB[MAX_PCA];

const unsigned char PCAT[16]={0,2,1,3,5,4,6,8,7,9,11,10,12,14,13,15};

							//Time---Flags1-Flags2-Flags3-Flags4-Flags5--R1--G1--B1--R2--G2--B2--R3--G3--B3--R4--G4--B4--R5--G5--B5---Opt----Nxt-

const RGBANI RGB_off[]= 	{ 10,	 0x0000,0x0000,0x0000,0x0000,0x0000 ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  , 0x01,  0};		//Black
const RGBANI RGB_white[]= 	{ 10,    0x0000,0x0000,0x0000,0x0000,0x0000 ,COL_WHITE  ,COL_WHITE  ,COL_WHITE  ,COL_WHITE  ,COL_WHITE  , 0x01,  0};		//1 Const White			

const RGBANI RGB_Fred[]= 	{ 10,	 0x0111,0x0111,0x0111,0x0111,0x0111 ,COL_RED    ,COL_RED    ,COL_RED    ,COL_RED    ,COL_RED    , 0x01,  0};  	//2	Flash RED
const RGBANI RGB_Fgreen[]= 	{ 10, 	 0x0111,0x0111,0x0111,0x0111,0x0111 ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  , 0x01,  0};	//3 Flash GREEN
const RGBANI RGB_Forange[]=	{ 10, 	 0x0111,0x0111,0x0111,0x0111,0x0111 ,COL_ORANGE ,COL_ORANGE ,COL_ORANGE ,COL_ORANGE ,COL_ORANGE , 0x01,  0};	//4	Flash ORANGE
			
const RGBANI RGB_red[]= 	{ 10,	 0x0000,0x0000,0x0000,0x0000,0x0000 ,COL_RED    ,COL_RED    ,COL_RED    ,COL_RED    ,COL_RED    , 0x01,  0};	//5	Const RED
const RGBANI RGB_green[]= 	{ 10, 	 0x0000,0x0000,0x0000,0x0000,0x0000 ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  , 0x01,  0};	//6 Const GREEN
const RGBANI RGB_blue[]= 	{ 10, 	 0x0000,0x0000,0x0000,0x0000,0x0000 ,COL_BLUE   ,COL_BLUE   ,COL_BLUE   ,COL_BLUE   ,COL_BLUE   , 0x01,  0};	//7	Const BLUE
const RGBANI RGB_orange[]= 	{ 10, 	 0x0000,0x0000,0x0000,0x0000,0x0000 ,COL_ORANGE ,COL_ORANGE ,COL_ORANGE ,COL_ORANGE ,COL_ORANGE , 0x01,  0};	//8	Const ORANGE

const RGBANI RGB_bluefade[]={{ 5,    0x0444,0x0444,0x0444,0x0444,0x0444 ,COL_BLUE   ,COL_BLUE   ,COL_BLUE   ,COL_BLUE   ,COL_BLUE   , 0x00,  1},	//11 BLUE Ani
							 { 1, 	 0x0000,0x0000,0x0000,0x0000,0x0000 ,COL_BLUE   ,COL_BLUE   ,COL_BLUE   ,COL_BLUE   ,COL_BLUE   , 0x00,  1},	//11 BLUE Ani
							 { 5,    0x0222,0x0222,0x0222,0x0222,0x0222 ,COL_BLUE   ,COL_BLUE   ,COL_BLUE   ,COL_BLUE   ,COL_BLUE   , 0x00,  1},	//11 BLUE Ani
							 { 1, 	 0x0000,0x0000,0x0000,0x0000,0x0000 ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  , 0x00, -3}};	//11 BLUE Ani

const RGBANI RGB_enter[]=  {{  2, 	 0x0444,0x0000,0x0000,0x0000,0x0000 ,COL_WHITE  ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  , 0x00,  1},	//
							{  2, 	 0x0000,0x0444,0x0000,0x0000,0x0000 ,COL_WHITE  ,COL_WHITE  ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  , 0x00,  1},	//
							{  2, 	 0x0000,0x0000,0x0444,0x0000,0x0000 ,COL_WHITE  ,COL_WHITE  ,COL_WHITE  ,COL_BLACK  ,COL_BLACK  , 0x00,  1},	//
							{  2, 	 0x0000,0x0000,0x0000,0x0444,0x0000 ,COL_WHITE  ,COL_WHITE  ,COL_WHITE  ,COL_WHITE  ,COL_BLACK  , 0x00,  1},	//
							{  2, 	 0x0000,0x0000,0x0000,0x0000,0x0444 ,COL_WHITE  ,COL_WHITE  ,COL_WHITE  ,COL_WHITE  ,COL_WHITE  , 0x00,  1},	//
							{  10, 	 0x0000,0x0000,0x0000,0x0000,0x0000 ,COL_WHITE  ,COL_WHITE  ,COL_WHITE  ,COL_WHITE  ,COL_WHITE  , 0x00,  1},	//
							{  2, 	 0x0222,0x0000,0x0000,0x0000,0x0000 ,COL_WHITE  ,COL_WHITE  ,COL_WHITE  ,COL_WHITE  ,COL_WHITE  , 0x00,  1},	//
							{  2, 	 0x0000,0x0222,0x0000,0x0000,0x0000 ,COL_BLACK  ,COL_WHITE  ,COL_WHITE  ,COL_WHITE  ,COL_WHITE  , 0x00,  1},	//
							{  2, 	 0x0000,0x0000,0x0222,0x0000,0x0000 ,COL_BLACK  ,COL_BLACK  ,COL_WHITE  ,COL_WHITE  ,COL_WHITE  , 0x00,  1},	//
							{  2, 	 0x0000,0x0000,0x0000,0x0222,0x0000 ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  ,COL_WHITE  ,COL_WHITE  , 0x00,  1},	//
							{  2, 	 0x0000,0x0000,0x0000,0x0000,0x0222 ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  ,COL_WHITE  , 0x00,  1},	//
							{  1, 	 0x0000,0x0000,0x0000,0x0000,0x0000 ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  , 0x00, -11}};	//

const RGBANI RGB_BNA_active[]= {{  1, 	 0x0444,0x0000,0x0000,0x0000,0x0000 ,COL_GREEN  ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  , 0x00,  1},	//
								{  1, 	 0x0000,0x0444,0x0000,0x0000,0x0000 ,COL_GREEN  ,COL_GREEN  ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  , 0x00,  1},	//
								{  1, 	 0x0000,0x0000,0x0444,0x0000,0x0000 ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  ,COL_BLACK  ,COL_BLACK  , 0x00,  1},	//
								{  1, 	 0x0000,0x0000,0x0000,0x0444,0x0000 ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  ,COL_BLACK  , 0x00,  1},	//
								{  1, 	 0x0000,0x0000,0x0000,0x0000,0x0444 ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  , 0x00,  1},	//
								{  1, 	 0x0000,0x0000,0x0000,0x0000,0x0000 ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  , 0x00,  1},	//
								{  1, 	 0x0222,0x0000,0x0000,0x0000,0x0000 ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  , 0x00,  1},	//
								{  1, 	 0x0000,0x0222,0x0000,0x0000,0x0000 ,COL_BLACK  ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  , 0x00,  1},	//
								{  1, 	 0x0000,0x0000,0x0222,0x0000,0x0000 ,COL_BLACK  ,COL_BLACK  ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  , 0x00,  1},	//
								{  1, 	 0x0000,0x0000,0x0000,0x0222,0x0000 ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  ,COL_GREEN  ,COL_GREEN  , 0x00,  1},	//
								{  1, 	 0x0000,0x0000,0x0000,0x0000,0x0222 ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  ,COL_GREEN  , 0x00, -10}};	//

const RGBANI RGB_CRT_active[]= {{  1, 	 0x0444,0x0000,0x0000,0x0000,0x0444 ,COL_GREEN  ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  ,COL_GREEN  , 0x00,  1},	//
								{  1, 	 0x0000,0x0444,0x0000,0x0444,0x0000 ,COL_GREEN  ,COL_GREEN  ,COL_BLACK  ,COL_GREEN  ,COL_GREEN  , 0x00,  1},	//
								{  1, 	 0x0000,0x0000,0x0444,0x0000,0x0000 ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  , 0x00,  1},	//
								{  1, 	 0x0222,0x0000,0x0000,0x0000,0x0222 ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  , 0x00,  1},	//
								{  1, 	 0x0000,0x0222,0x0000,0x0222,0x0000 ,COL_BLACK  ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  ,COL_BLACK  , 0x00,  1},	//
								{  1, 	 0x0000,0x0000,0x0222,0x0000,0x0000 ,COL_BLACK  ,COL_BLACK  ,COL_GREEN  ,COL_BLACK  ,COL_BLACK  , 0x00,  1},	//
								{  1, 	 0x0000,0x0000,0x0000,0x0000,0x0000 ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  , 0x00, -6}};	//								

const RGBANI RGB_FASTgreen[]=  {{  1, 	 0x0000,0x0000,0x0000,0x0000,0x0000 ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  ,COL_GREEN  , 0x00,  1},	//
								{  1,	 0x0000,0x0000,0x0000,0x0000,0x0000 ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  , 0x00, -1}};	//

const RGBANI RGB_FASTred[]=    {{  1, 	 0x0000,0x0000,0x0000,0x0000,0x0000 ,COL_RED    ,COL_RED    ,COL_RED    ,COL_RED    ,COL_RED    , 0x00,  1},	//
								{  1,	 0x0000,0x0000,0x0000,0x0000,0x0000 ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  ,COL_BLACK  , 0x00, -1}};	//


const RGBANI *RGB_Animation[]={	(RGBANI *)&RGB_off[0],
	 		 					(RGBANI *)&RGB_enter[0],
	 		 					(RGBANI *)&RGB_Fred[0],
	 		 					(RGBANI *)&RGB_red[0],
	 		 					(RGBANI *)&RGB_green[0],
	 		 					(RGBANI *)&RGB_Fgreen[0],
	 		 					(RGBANI *)&RGB_blue[0],
	 		 					(RGBANI *)&RGB_Forange[0],
	 		 					(RGBANI *)&RGB_orange[0],
 								(RGBANI *)&RGB_white[0],
								(RGBANI *)&RGB_bluefade[0],
								(RGBANI *)&RGB_BNA_active[0],
								(RGBANI *)&RGB_CRT_active[0],
								(RGBANI *)&RGB_FASTgreen[0],
								(RGBANI *)&RGB_FASTred[0],
								(RGBANI *)&RGB_custom[0][0],
								(RGBANI *)&RGB_custom[1][0],
								(RGBANI *)&RGB_custom[2][0]
                                };


/*******************************************************************************
 * Function Declarations
 */
int reg_write(i2c_inst_t *i2c, 
                const uint addr, 
                const uint8_t reg, 
                uint8_t *buf,
                const uint8_t nbytes);

int reg_read(   i2c_inst_t *i2c,
                const uint addr,
                const uint8_t reg,
                uint8_t *buf,
                const uint8_t nbytes);

/*******************************************************************************
 * Function Definitions
 */

// Write 1 byte to the specified register
int reg_write(  i2c_inst_t *i2c, 
                const uint addr, 
                const uint8_t reg, 
                uint8_t *buf,
                const uint8_t nbytes) {

    int num_bytes_read = 0;
    uint8_t msg[nbytes + 1];

    // Check to make sure caller is sending 1 or more bytes
    if (nbytes < 1) {
        return 0;
    }

    // Append register address to front of data packet
    msg[0] = reg;
    for (int i = 0; i < nbytes; i++) {
        msg[i + 1] = buf[i];
    }

    // Write data to register(s) over I2C
    i2c_write_blocking(i2c, addr, msg, (nbytes + 1), false);

    return num_bytes_read;
}

// Read byte(s) from specified register. If nbytes > 1, read from consecutive
// registers.
int reg_read(  i2c_inst_t *i2c,
                const uint addr,
                const uint8_t reg,
                uint8_t *buf,
                const uint8_t nbytes) {

    int num_bytes_read = 0;
	int num_bytes_written = 0;

    // Check to make sure caller is asking for 1 or more bytes
    if (nbytes < 1) {
        return 0;
    }

    // Read data from register(s) over I2C
    num_bytes_written = i2c_write_blocking(i2c, addr, &reg, 1, true);

	if (num_bytes_written != PICO_ERROR_GENERIC) {
    	num_bytes_read = i2c_read_blocking(i2c, addr, buf, nbytes, false);
	}

	return num_bytes_read;
}

//-----------------------------------------------------------------------------------

void IlluminationInit()
{
    int i,c;

	i2c_init(I2C_HW , 400000);

    gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_SDA);
    gpio_pull_up(PIN_SCL);   
	

    /*
    SysVar.CRTactAni	=RGBANI_CRT_ACTIVE;
    SysVar.CRTerrAni	=RGBANI_RED;
    SysVar.CRToffAni	=RGBANI_OFF;
    SysVar.CRTremAni	=RGBANI_GREENFL;
 
    SysVar.BNAactAni	=RGBANI_BNA_ACTIVE;
    SysVar.BNAerrAni	=RGBANI_REDFL;
    SysVar.BNAoffAni	=RGBANI_OFF;
 
    SysVar.COINactAni	=RGBANI_GREEN;
    SysVar.COINerrAni	=RGBANI_RED;
    SysVar.COINoffAni	=RGBANI_OFF;

    SysVar.RGBdimming	=0xFF;
    SysVar.RGBprescale =1;	
    */
    			//1791
    for (i=0;i<3;i++) 
        for (c=0;c<16;c++) 
	        RGB_custom[i][c].Duration=0;

    FRONTRGBStatus[0]=InitFRONTRGB(0);
    FRONTRGBStatus[1]=InitFRONTRGB(1);
    FRONTRGBStatus[2]=InitFRONTRGB(2);
}

/*
void writeIllum()
{
 unsigned char a,b, r;
 unsigned char far* Led_Animation;

        Led_Animation = (char far*)animation[CoinIllumAni];

	a = Led_Animation[(CoinIllum*2)+1];
	b = Led_Animation[(CoinIllum*2)+2];
	if(++CoinIllum >= Led_Animation[0])
	  CoinIllum = 0;
	
	StartI2C_KB();
	if (WriteI2C_KB(SAA1064_0) ==0)
	{	
	 	r = WriteI2C_KB(0x00);
	 	r = WriteI2C_KB(0x07+((LED_Current0&0x07)<<4));	//Control full Current
	 	r = WriteI2C_KB(b);
	 	r = WriteI2C_KB(0x00);
	 	r = WriteI2C_KB(0x00);
	 	r = WriteI2C_KB(a);
   	}
	StopI2C_KB();	

        Led_Animation = (char far*)animation[BillIllumAni];

	a = Led_Animation[(BillIllum*2)+1];
	b = Led_Animation[(BillIllum*2)+2];
	if(++BillIllum >= Led_Animation[0])
	  BillIllum = 0;

	StartI2C_KB();
	if (WriteI2C_KB(SAA1064_1) ==0)
	{	
	 	r = WriteI2C_KB(0x00);
	 	r = WriteI2C_KB(0x07+((LED_Current1&0x07)<<4));	//Control full Current
	 	r = WriteI2C_KB(b);
	 	r = WriteI2C_KB(0x00);
	 	r = WriteI2C_KB(0x00);
	 	r = WriteI2C_KB(a);
   	}
	StopI2C_KB();	
		
        Led_Animation = (char far*)animation[CardIllumAni];

	a = Led_Animation[(CardIllum*2)+1];
	b = Led_Animation[(CardIllum*2)+2];
	if(++CardIllum >= Led_Animation[0])
	  CardIllum = 0;
	
	StartI2C_KB();
	if (WriteI2C_KB(SAA1064_2) ==0)
	{	
	 	r = WriteI2C_KB(0x00);
	 	r = WriteI2C_KB(0x07+((LED_Current2&0x07)<<4));	//Control full Current
	 	r = WriteI2C_KB(b);
	 	r = WriteI2C_KB(0x00);
	 	r = WriteI2C_KB(0x00);
	 	r = WriteI2C_KB(a);
   	}
	StopI2C_KB();	
	
}
*/

//-----------------------------------------------------------------------------------

void IlluminateCoinInsert(unsigned char on)
{
	if (on) 
	{
		CoinIllumAni=4;		
		FRONTRGB_Ani[0] = RGBANI_GREEN;     // SysVar.COINactAni
	}
	else	
	{
		FRONTRGB_Ani[0] = RGBANI_OFF;       // SysVar.COINoffAni;
		CoinIllumAni=0;
	}
}

//-----------------------------------------------------------------------------------

void IlluminateBillInsert(unsigned char on)
{
	if (on) 
	{
		BillIllumAni=4;
		FRONTRGB_Ani[2] = RGBANI_REDFL;    // SysVar.BNAactAni
	}
	else	
	{
		BillIllumAni=0;
		FRONTRGB_Ani[2] = RGBANI_OFF;           // SysVar.BNAoffAni
	}
}

//-----------------------------------------------------------------------------------

void IlluminateCardInsert(unsigned char on)
{
	if (on) 
	{
		CardIllumAni=4;
		FRONTRGB_Ani[1] = RGBANI_BLUEFADE;    // SysVar.BNAactAni
	}
	else	
	{
		CardIllumAni=0;
		FRONTRGB_Ani[1] = RGBANI_OFF;           // SysVar.BNAoffAni
	}
}

//-----------------------------------------------------------------------------------

unsigned char InitFRONTRGB(char adr)
{
	unsigned char r=0;
	unsigned char result=0;

	// Buffer to store raw reads
    uint8_t data[16];	

	FRONTRGB_AniSelect[adr]=0;
	FRONTRGB_Ani[adr]=0;
	FRONTRGB_Time[adr]=0;
	FRONTRGB_Scale1[adr]=0;
	FRONTRGB_Scale2[adr]=0;
	FRONTRGB_Scale3[adr]=0;
	FRONTRGB_Lost[adr]=0;
	FRONTRGB_Adr[adr]=0;
	FRONTRGB_Count[adr]=0;
	FRONTRGB_Rot[adr]=0;
	FRONTRGB_Var[adr]=0;

	FRONTRGB[adr].Reg.Mode1 = 0x80;	//Normal Mode, AI
	FRONTRGB[adr].Reg.Mode2 = 0x04;  //INVRT=0, OUTDRV=1, 
	for(r=0;r<5;r++) 
	{
		FRONTRGB[adr].Reg.PWM[(r*3)+0] = 0;
		FRONTRGB[adr].Reg.PWM[(r*3)+1] = 0;
		FRONTRGB[adr].Reg.PWM[(r*3)+2] = 0;
	}
	FRONTRGB[adr].Reg.PWM[15] = 0x0;
	FRONTRGB[adr].Reg.Grppwm = 0x00;
	FRONTRGB[adr].Reg.Grpfreq =0x00;
	for(r=0;r<4;r++) FRONTRGB[adr].Reg.Ledout[r] = 0xFF;		//PWM Control + GroupControl
	FRONTRGB[adr].Reg.Ledout[3] &= 0x3F;
	FRONTRGB_AniCount[adr]=0;
	FRONTRGB_AniCountOld[adr]=0xFF;
	FRONTRGB_AniOld[adr]=0xFF;		

	if (reg_read(I2C_HW, PCA[adr], 0, data, 1))
	{
	   FRONTRGB_Lost[adr]=I2C_RETRY;
	   result =1;
	   FRONTRGB_Init[adr]=1;	   
	   //printf("FRONT_RGB[%u]     available!\n", adr);
   	}
	//else printf("FRONT_RGB[%u] not available! read=%02X\n", adr, data[0]);
    
	return(result);
}

void ScanFRONTRGB(char adr)	//alle 20ms
{
	unsigned char r,t,count,bri[3],hchip[4];	
	unsigned int mask;
	RGBANI *pRGBAni;
		
	
	FRONTRGB_Var[adr]++;						//Dauerint
	
	if(FRONTRGB_Scale3[adr]++ > 5) FRONTRGB_Scale3[adr]=0;

	//---------------------------------ANIMATION--------------------------------------------	
	if(FRONTRGB_Ani[adr] != FRONTRGB_AniOld[adr])
	{
		FRONTRGB_AniOld[adr] = FRONTRGB_Ani[adr];
		FRONTRGB_AniCountOld[adr] = 0xFF;
		FRONTRGB_AniCount[adr] = 0;
	}
	
	pRGBAni = (RGBANI *)&RGB_Animation[FRONTRGB_Ani[adr]][FRONTRGB_AniCount[adr]];
		
	if(FRONTRGB_AniCount[adr] != FRONTRGB_AniCountOld[adr])		//Load Ani
	{
		FRONTRGB_AniCountOld[adr] = FRONTRGB_AniCount[adr];
		for(r=0;r<15;r++) FRONTRGB[adr].Reg.PWM[r] = 0;
		FRONTRGB_Time[adr] = pRGBAni->Duration * 10;
	}

	if(FRONTRGB_Time[adr])
	{
		FRONTRGB[adr].Reg.Grppwm = 0xFF;		//Volle helligkeit (SysVar.RGBdimming)
		for(t=0;t<5;t++)
		{
			for(r=0;r<3;r++) 	//Colours
			{
				FRONTRGB[adr].Reg.PWM[PCAT[(t*3)+r]] = 0;	//PWMS löschen
				mask=pRGBAni->Flags[t]>>((2-r)*4);
				if((mask&0x1)&&(FRONTRGB_Var[adr]&0x0020))	continue;	//FLASH
				if(mask&0x2)								//FADE ON
				{
					FRONTRGB[adr].Reg.PWM[PCAT[(t*3)+r]] = (FRONTRGB_Time[adr] * (unsigned long)pRGBAni->Bright[t][r])/((unsigned long)pRGBAni->Duration*10);
					continue;
				}
				if(mask&0x4)								//FADE OFF
				{
					FRONTRGB[adr].Reg.PWM[PCAT[(t*3)+r]] = pRGBAni->Bright[t][r] - ((FRONTRGB_Time[adr] * (unsigned long)pRGBAni->Bright[t][r])/((unsigned long)pRGBAni->Duration*10));
					continue;
				}
				FRONTRGB[adr].Reg.PWM[PCAT[(t*3)+r]] =	pRGBAni->Bright[t][r];
			}
		}

		FRONTRGB_Time[adr]--;	
		if(!FRONTRGB_Time[adr]) 
		{
			FRONTRGB_AniCountOld[adr] = 0xFF;
			if(!pRGBAni->Next)	
			{
				if(((pRGBAni->Options)&0x01)==0) FRONTRGB_Ani[adr] = 0;	//aus
			}
			else
				FRONTRGB_AniCount[adr] += pRGBAni->Next;		// Load Next
			
		}

	}
	else
	{
	  	FRONTRGB[adr].Reg.Grppwm = 0;
	  	FRONTRGB_AniCount[adr] = 0;
	}
		
	//--------------------------------------------------------------------------------------	

	unsigned char sendData[32];

	if(FRONTRGB_Init[adr])		//init prüfen
	{
		FRONTRGB_Init[adr]=0;
	 	FRONTRGB_Adr[adr]=0;
	   	FRONTRGB_Count[adr]=0x18;	//all ex I2C Addr
		sendData[0] = 0x80;
	}
	else
	{
	 	FRONTRGB_Adr[adr]=2;
	   	FRONTRGB_Count[adr]=17;	    //all PWM + GrpPWM 
		sendData[0] = 0x82;
	}

	// pack adress and data to String
	for (int c=0;c<FRONTRGB_Count[adr];c++) sendData[c+1]=FRONTRGB[adr].Data[(FRONTRGB_Adr[adr]+c)];
	
    if (i2c_write_blocking(I2C_HW,PCA[adr],&sendData[0],FRONTRGB_Count[adr]+1,false) == PICO_ERROR_GENERIC) {
   	   	if(FRONTRGB_Lost[adr])
        {
     	    FRONTRGB_Lost[adr]--;
            if(FRONTRGB_Lost[adr] == 0) FRONTRGBStatus[adr] = 0;
        }
    }
    else
    {
	    FRONTRGB_Lost[adr]=I2C_RETRY;
		if (!FRONTRGBStatus[adr])
		{			
			FRONTRGB_Init[adr]=1;		
			FRONTRGBStatus[adr] = 1;
		}
    }	
}

/****************************************************************************************************************************
 ****			   BECKMANN GmbH                                                                                 ****
 ****************************************************************************************************************************/
