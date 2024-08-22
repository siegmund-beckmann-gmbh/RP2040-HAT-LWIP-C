/* ============================================================================================ *
 * routines for RTC     								*
 * Header-File											* 
 * ============================================================================================ */
 
#ifndef RTC_H
#define RTC_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
  
  // declarations
/* I2C */
#define I2C_CLOCK i2c0                                      // Alternativ i2c0 oder i2c1
#define PIN_SDA_CLK 8                                       // mit i2c0 geht: 0, 4,  8, 12, 16, 20, 24, 28
                                                            //     i2c1 geht: 2, 6, 10, 14, 18, 22, 26
#define PIN_SCL_CLK 9                                       // SCL-pin immer eine Nummer höher als SDA, also 1, 3, 5,...

#define DS1307	    0x68 //0xD0

// ----Prototype-Declarations--------------------------------------------------------  

unsigned char InitClock();
unsigned char SetClock();
void ReadClock();
void ReadTimeFromClock();


#endif /* RTC_H */
// -----------------------------------------------------------------------------------

/****************************************************************************************************************************
 ****			   BECKMANN GmbH                                                                                 ****
 ****************************************************************************************************************************/


