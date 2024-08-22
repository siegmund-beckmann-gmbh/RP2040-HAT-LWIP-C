/* ============================================================================================ *
 * routines for RTC     								*
 * Program-File											* 
 * ============================================================================================ */
 
#include    "rtc.h" // Header File

//*******************VARIABLES**************************

unsigned char TimeReg[7];
unsigned char CopyReg[7];
unsigned char TimeRegDEC[7];


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

	i2c_init(I2C_CLOCK , 100000);

    gpio_set_function(PIN_SDA_CLK, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SCL_CLK, GPIO_FUNC_I2C);


    sendData[0] = 0x07;
    sendData[1] = 0x10;

    if (i2c_write_timeout_per_char_us(I2C_CLOCK, DS1307, &sendData[0], 2, false, 1000) != PICO_ERROR_GENERIC) {

        result=1;
        
        if (i2c_read_timeout_per_char_us(I2C_CLOCK, DS1307, &TimeReg[0], 1, false, 1000) != PICO_ERROR_GENERIC) {

            sendData[0] = 0x00;
            sendData[1] = TimeReg[0] & 0x7f;

            if (i2c_write_timeout_per_char_us(I2C_CLOCK, DS1307, &sendData[0], 2, false, 1000) != PICO_ERROR_GENERIC) {
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

    if (i2c_write_timeout_per_char_us(I2C_CLOCK, DS1307, &sendData[0], 8, false, 1000) != PICO_ERROR_GENERIC) {

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

    if (i2c_write_timeout_per_char_us(I2C_CLOCK, DS1307, &sendData[0], 1, true, 1000) != PICO_ERROR_GENERIC) {

        if (i2c_read_timeout_per_char_us(I2C_CLOCK, DS1307, &TimeReg[0], 7, false, 1000) != PICO_ERROR_GENERIC) {
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


/****************************************************************************************************************************
 ****			   BECKMANN GmbH                                                                                 ****
 ****************************************************************************************************************************/
