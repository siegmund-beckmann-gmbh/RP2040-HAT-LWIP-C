/* ============================================================================================ *
 * routines for RTC     								*
 * Program-File											* 
 * ============================================================================================ */
 
#include    "rtc.h" // Header File
#include "hexdump.h"

//*******************VARIABLES**************************

unsigned char TimeReg[7];
unsigned char CopyReg[7];
unsigned char TimeRegDEC[7];

bool trayLight = false;
bool directEntry = false;
bool directSignal = false;
bool relais4 = false;
bool relais5 = false;

const unsigned char DayTab[12] ={31,28,31,30,31,30,31,31,30,31,30,31};
const unsigned char DayTabLeap[12] ={31,29,31,30,31,30,31,31,30,31,30,31};
const unsigned int DayTabN[13] ={0,31,59,90,120,151,181,212,243,273,304,334,365};
const unsigned int DayTabS[13] ={0,31,60,91,121,152,182,213,244,274,305,335,366};

/*******************************************************************************
 * Function Declarations
 */

/*--------------------------------------------------------------*/
unsigned char BCD2Dec(unsigned char bcd)
{
    return (bcd/16)*10 + (bcd & 0x0F);
}


unsigned char InitClock()
{
    unsigned char sendData[8];
    unsigned char result=0;

	i2c_init(I2C_INTERNAL , 100000);

    gpio_set_function(PIN_SDA_CLK, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SCL_CLK, GPIO_FUNC_I2C);


    sendData[0] = 0x07;
    sendData[1] = 0x10;

    if (i2c_write_timeout_per_char_us(I2C_INTERNAL, DS1307, &sendData[0], 2, false, 1000) >= 0) {

        result=1;

        if (i2c_write_timeout_per_char_us(I2C_INTERNAL, DS1307, &sendData[0], 1, true, 1000) >= 0) {

            sendData[0] = 0x00;
        
            if (i2c_read_timeout_per_char_us(I2C_INTERNAL, DS1307, &TimeReg[0], 1, false, 1000) >= 0) {

                sendData[0] = 0x00;
                sendData[1] = TimeReg[0] & 0x7f;

                if (i2c_write_timeout_per_char_us(I2C_INTERNAL, DS1307, &sendData[0], 2, true, 1000) >= 0) {
                }
            }        
        }

    }
		
	return(result);
}


/* --------------------------------------------------------------------------*/
unsigned char SetClock()
{	
    unsigned char sendData[8];
    unsigned char result=0;

    sendData[0] = 0x00;
	sendData[1] = TimeReg[0] & 0x7F;  	//Sekunden
	sendData[2] = TimeReg[1];
	sendData[3] = TimeReg[2];

	TimeReg[3]=TimeReg[3] % 7;
    if (TimeReg[3]==0) 
         sendData[4] = 7;
    else sendData[4] = TimeReg[3];
	sendData[5] = TimeReg[4];
    sendData[6] = TimeReg[5];
    sendData[7] = TimeReg[6];

    if (i2c_write_timeout_per_char_us(I2C_INTERNAL, DS1307, &sendData[0], 8, false, 1000) >= 0) {

        result=1;
    }

	return(result);
}

/* --------------------------------------------------------------------------*/
void ReadClock()
{
    unsigned char sendData[8];
    unsigned char result=0;

    sendData[0] = 0x00;

    if (i2c_write_timeout_per_char_us(I2C_INTERNAL, DS1307, &sendData[0], 1, true, 1000) >= 0) {

        if (i2c_read_timeout_per_char_us(I2C_INTERNAL, DS1307, &TimeReg[0], 7, false, 1000) >= 0) {
            result = 1;
        }
    }
    
}

/* --------------------------------------------------------------------------*/
void ReadTimeFromClock()
{
 int i,korrekt,maxrepeat;
	
 	korrekt=1;
	maxrepeat=3;
	
	do
	{
 		ReadClock();
		for (i=0;i<7;i++) CopyReg[i]=TimeReg[i];
 		ReadClock();
		for (i=0;i<7;i++) 
	 	 if (CopyReg[i]!=TimeReg[i]) korrekt=0;
	}
	while ((--maxrepeat) && (!korrekt));
		
	if (BCD2Dec(TimeReg[2])>23) TimeReg[2]=0x23;
	if (BCD2Dec(TimeReg[1])>59) TimeReg[1]=0x59;
	if (BCD2Dec(TimeReg[0])>59) TimeReg[0]=0x59;
	
	if (BCD2Dec(TimeReg[4])>31) TimeReg[4]=0x31;
	if (BCD2Dec(TimeReg[5])>12) TimeReg[5]=0x12;
	if (BCD2Dec(TimeReg[6])>99) TimeReg[6]=0x99; 	 		

    for (int i=0;i<7;i++) TimeRegDEC[i] =  BCD2Dec(TimeReg[i] & 0x7F);
}

bool isDST(uint32_t sec)  // germany(europe)
{
    unsigned char wkdayNum,compdays;
    int day;

    struct tm current_time_val;
    time_t current_time = (time_t)sec;

#if defined(_WIN32) || defined(WIN32)
    localtime_s(&current_time_val, &current_time);
#else
    localtime_r(&current_time, &current_time_val);
#endif

	compdays = DayTab[current_time_val.tm_mon-1];	//referenzfehler beseitigt 25.10.2010
	if ((!(day=current_time_val.tm_year % 4)) && (current_time_val.tm_mon == 2)) compdays=29;

	wkdayNum=1;
	day=current_time_val.tm_mday;
	while ((day-=7)>0) wkdayNum++;
	
	//TodayInfo.wkdayNum=wkdayNum;

	day=current_time_val.tm_mday;
	while ((day+=7)<=compdays) wkdayNum++;
	
	//TodayInfo.wkdayMax=wkdayNum;


    return false;
}

uint8_t scan_pcf()
{
    uint8_t scanVal = 0xE0;

    scanVal |= (trayLight ? 0x00:0x01);
    scanVal |= (directEntry ? 0x00:0x02);
    scanVal |= (directSignal ? 0x00:0x04);
    scanVal |= (relais4 ? 0x00:0x08);
    scanVal |= (relais5 ? 0x00:0x10);

    uint8_t sendData[8];

    sendData[0] = scanVal;

    if (i2c_write_timeout_per_char_us(I2C_INTERNAL, PCF9574_0, &sendData[0], 1, true, 1000) >= 0) {

        if (i2c_read_timeout_per_char_us(I2C_INTERNAL, PCF9574_0, &sendData[0], 1, false, 1000) >= 0) {

            return sendData[0];
            
        }
    }
}

bool ReadEEPROM(uint8_t *mem) {

    unsigned char sendData[8];
    int result;
    sendData[0] = 0x00;

    result = i2c_write_timeout_per_char_us(I2C_INTERNAL, EEPROM2402, &sendData[0], 1, false, 1000);

    if (result >= 0) {
        result = i2c_read_timeout_per_char_us(I2C_INTERNAL, EEPROM2402, mem, 256, false, 1000);
        if (result >= 0) {
            return true;
        } else printf("EEPROM read error=%u!\n",result);
    } else printf("EEPROM no response! err=%u\n",result);
    
    return false;
}

bool writeEEPROMbyte(uint8_t adr, uint8_t byte) {

    unsigned char sendData[2];
    int result;
    sendData[0] = adr;
    sendData[1] = byte;

    result = i2c_write_timeout_per_char_us(I2C_INTERNAL, EEPROM2402, &sendData[0], 2, false, 1000);
    if (result < 0) {
        printf("EEPROM write error=%u!\n",result);
        return false;
    }

    return true;
}

bool writeEEPROM(uint8_t *mem) {

    unsigned char sendData[17];
    int result;

    for (int a=0;a<256;a+=16) {   // page write 16bytes
        sendData[0] = a;
        for (int d=0;d<16;d++) sendData[d+1]=mem[a+d];
        result = i2c_write_timeout_per_char_us(I2C_INTERNAL, EEPROM2402, &sendData[0], 17, false, 1000);
        if (result < 0) {
            printf("EEPROM write error=%u!\n",result);
            return false;
        }
        sleep_ms(6);
    }

    return true;
}

/****************************************************************************************************************************
 ****			   BECKMANN GmbH                                                                                 ****
 ****************************************************************************************************************************/
