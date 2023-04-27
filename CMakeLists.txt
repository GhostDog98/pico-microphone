cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(data)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_subdirectory(./no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI build)


add_executable(data
    data.c
    hw_config.c
)




# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(${PROJECT_NAME} 
	pico_stdlib
	hardware_adc
	hardware_dma
	FatFs_SPI
)

# create map/bin/hex/uf2 file in addition to ELF.
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 1)

pico_add_extra_outputs(${PROJECT_NAME})
