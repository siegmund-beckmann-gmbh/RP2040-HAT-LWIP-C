/**************************************************************************
*** 			    Beckmann GmbH  		                                ***
**************************************************************************/

#include    "flash_utils.h" // Header File

/********************VARIABLES********************************************/


/**************************************************************************
 *** Function Declarations                                              ***
 *************************************************************************/
void nvs_print_info(void){

    printf("Persistent Memory address = %x\n", getAddressPersistent());
    printf("Persistent Memory size    = %u bytes\n", NVS_SIZE);
    printf("Persistent Memory blocks  = %u\n", NVS_BLOCKS);
    printf("Persistent Memory pages   = %u\n", NVS_PAGES);
    uint8_t *nvs = (uint8_t*)getAddressPersistent();
    hexdump(nvs,256,16,8);    // show first page

}

void nvs_erase_block(uint16_t blockNo) {
    if (blockNo<NVS_BLOCKS) {

    }
    else printf("NVS block no=%u out of range (max=%u)!",blockNo,NVS_BLOCKS-1);
}

uint8_t* nvs_get_pagePointer(uint16_t pageNo){    
    return (uint8_t*)getAddressPersistent()+ (pageNo * 0x100);
}

void nvs_write_page(uint16_t pageNo, uint8_t *buf ){
    if (pageNo<NVS_PAGES) {
        uint8_t *nvs_page = (uint8_t*)getAddressPersistent()+ (pageNo * 0x100);
        printf("flash offset =%x\n",((uint32_t)nvs_page - 0x10000000));                                                        

        uint32_t ints = save_and_disable_interrupts();
        flash_range_program(((uint32_t)nvs_page - 0x10000000), buf, 256);
        restore_interrupts(ints);
        
        hexdump(nvs_page,256,16,8);    // show first page
    }
    else printf("NVS page no=%u out of range (max=%u)!",pageNo, NVS_PAGES-1);
}

/**************************************************************************
*** 			    Beckmann GmbH  		                                ***
**************************************************************************/
