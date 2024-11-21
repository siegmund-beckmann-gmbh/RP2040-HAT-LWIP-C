set(TARGET_NAME w5x00_dhcp_dns)
set(LWIP_PATH ${PICO_SDK_PATH}/lib/lwip)
set(TARGET_FILES        
    ${CMAKE_CURRENT_SOURCE_DIR}/flash_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sql_client/intercom.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sql_client/postgresql.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sql_client/md5.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sql_client/hexdump.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sql_client/mdb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sql_client/illumination.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sql_client/rtc.c
    w5x00_dhcp_dns.c
    )

add_definitions(
        -DSNTP_SERVER_DNS=1
        -DSNTP_SERVER_ADDRESS="pool.ntp.org"
        -DSNTP_SET_SYSTEM_TIME=set_system_time
        -DSNTP_STARTUP_DELAY=0
)

add_executable(${TARGET_NAME} ${TARGET_FILES})

target_include_directories(w5x00_dhcp_dns PUBLIC
        ${LWIP_PATH}/src/include
        ${CMAKE_CURRENT_SOURCE_DIR}/sql_client
        )


# This is required to include lwipopts.h
target_include_directories(pico_lwip INTERFACE        
        ${PORT_DIR}/lwip
        )

pico_generate_pio_header(${TARGET_NAME} ${CMAKE_CURRENT_LIST_DIR}/sql_client/uart_tx9.pio )
pico_generate_pio_header(${TARGET_NAME} ${CMAKE_CURRENT_LIST_DIR}/sql_client/uart_rx9.pio )


target_link_libraries(${TARGET_NAME} PRIVATE
        pico_stdlib
        pico_util
        pico_lwip_sntp
        hardware_spi
        hardware_dma
        hardware_pio
        hardware_flash
        hardware_i2c
        cmsis_core
        ETHERNET_FILES
        IOLIBRARY_FILES
        LWIP_FILES
        TIMER_FILES
        )

pico_add_extra_outputs(${TARGET_NAME})

pico_enable_stdio_usb(${TARGET_NAME} 0)
pico_enable_stdio_uart(${TARGET_NAME} 1)

pico_add_extra_outputs(${TARGET_NAME})

pico_set_linker_script(${TARGET_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/memmap_custom.ld)
