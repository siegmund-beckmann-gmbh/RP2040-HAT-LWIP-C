/**************************************************************************
*** 			    Beckmann GmbH  		                                ***
**************************************************************************/

#ifndef FLASH_UTILS_H
#define FLASH_UTILS_H


#include <stdint.h>
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hexdump.h"

extern uint32_t ADDR_PERSISTENT[];

inline uint32_t *getAddressPersistent() {
    return ADDR_PERSISTENT;
}

#define NVS_SIZE    (0x10200000 - (uint)&ADDR_PERSISTENT) 
#define NVS_BLOCKS  NVS_SIZE / 4096
#define NVS_PAGES   NVS_SIZE / 256

/********************VARIABLES********************************************/

void nvs_print_info(void);
void nvs_erase_block(uint16_t blockNo);
void nvs_write_page(uint16_t pageNo, uint8_t *buf );
uint8_t* nvs_get_pagePointer(uint16_t pageNo);


#endif /* FLASH_UTILS_H */
// -----------------------------------------------------------------------------------

/**************************************************************************
*** 			    Beckmann GmbH  		                                ***
**************************************************************************/
