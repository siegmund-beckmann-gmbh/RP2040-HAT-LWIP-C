/* ============================================================================================ *
 * routines for Front RGB								*
 * Header-File											* 
 * ============================================================================================ */
 
#ifndef ILLUMINATION_H
#define ILLUMINATION_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
  
  // declarations
/* I2C */
#define I2C_HW i2c1                                         // Alternativ i2c0 oder i2c1
#define PIN_SDA 26                                          // mit i2c0 geht: 0, 4,  8, 12, 16, 20, 24, 28
                                                            //     i2c1 geht: 2, 6, 10, 14, 18, 22, 26
#define PIN_SCL 27                                          // SCL-pin immer eine Nummer höher als SDA, also 1, 3, 5,...

#define SAA1064_0	0x70
#define SAA1064_1	0x72
#define SAA1064_2	0x74

#define LED_Current0	0x04
#define LED_Current1	0x04
#define LED_Current2	0x07

#define ANI_OFF		    0
#define ANI_ENTRY	    1
#define ANI_REDFLASH	2
#define ANI_RED    	    3
#define ANI_GREEN  	    4
#define ANI_GREENFLASH	5
#define ANI_ORA    	    6
#define ANI_CARD    	7

#define MAX_PCA	3
#define I2C_RETRY 5

#define	PCA9635_0	0x73 // 0xE6  
#define	PCA9635_1	0x77 // 0xEE 
#define	PCA9635_2	0x71 // 0xE2 BNA


unsigned char InitFRONTRGB(char adr);
void ScanFRONTRGB(char adr);

typedef struct __attribute__((packed)) RGB_An{
	unsigned int  Duration;		//  2
	unsigned int  Flags[5];		// 10
	unsigned char Bright[5][3]; // 15
	unsigned char Options;		//  1
	signed   char Next;			//	1
}RGBANI;						// 29

typedef struct __attribute__((packed)) FrTag{
	unsigned char Mode1;
	unsigned char Mode2;
	unsigned char PWM[16];
	unsigned char Grppwm;
	unsigned char Grpfreq;
	unsigned char Ledout[4];
	unsigned char Subadr[3];
	unsigned char Allcalladr;
}Fr_Tag;

typedef union {
	struct FrTag Reg;	
	unsigned char Data[32];
}Fr_RGB;
					
#define COL_BLACK	  0,  0,  0
#define COL_WHITE	160,255,160
#define COL_RED		255,  0,  0
#define COL_GREEN	  0,255,  0
#define COL_BLUE	  0,  0,127
#define COL_YELLOW  255,255,  0
#define COL_ORANGE  255,127,  0
#define COL_CYAN	  0,255,127

#define RGBANI_OFF		0
#define RGBANI_ENTER	1
#define RGBANI_REDFL	2
#define RGBANI_RED		3
#define RGBANI_GREEN	4
#define RGBANI_GREENFL	5
#define RGBANI_BLUE		6
#define RGBANI_ORANGEFL	7
#define RGBANI_ORANGE	8
#define RGBANI_WHITE	9
#define RGBANI_BLUEFADE 10
#define RGBANI_BNA_ACTIVE 11
#define RGBANI_CRT_ACTIVE 12
#define RGBANI_GREEN_FAST 13
#define RGBANI_RED_FAST 14
#define RGBANI_CUSTOM1 15
#define RGBANI_CUSTOM2 16
#define RGBANI_CUSTOM3 17

#define RGBANI_MAX_ANI 17

#define	RGBANI_DEFAULT	RGBANI_ENTER

#define COL_BLACK	  0,  0,  0
#define COL_WHITE	160,255,160
#define COL_RED		255,  0,  0
#define COL_GREEN	  0,255,  0
#define COL_BLUE	  0,  0,127
#define COL_YELLOW  255,255,  0
#define COL_ORANGE  255,127,  0
#define COL_CYAN	  0,255,127

// ----Prototype-Declarations--------------------------------------------------------  

void IlluminationInit(void);

void IlluminateCoinInsert(unsigned char on);
void IlluminateBillInsert(unsigned char on);
void IlluminateCardInsert(unsigned char on);

#endif /* ILLUMINATION_H */
// -----------------------------------------------------------------------------------

/****************************************************************************************************************************
 ****			   BECKMANN GmbH                                                                                 ****
 ****************************************************************************************************************************/
