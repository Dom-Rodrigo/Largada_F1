# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(ATIVIDADE_REVISAO C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(ATIVIDADE_REVISAO ATIVIDADE_REVISAO.c inc/ssd1306.c)

pico_set_program_name(ATIVIDADE_REVISAO "ATIVIDADE_REVISAO")
pico_set_program_version(ATIVIDADE_REVISAO "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(ATIVIDADE_REVISAO 1)
pico_enable_stdio_usb(ATIVIDADE_REVISAO 1)

pico_generate_pio_header(ATIVIDADE_REVISAO ${CMAKE_CURRENT_LIST_DIR}/ws2818b.pio)

# Add the standard library to the build
target_link_libraries(ATIVIDADE_REVISAO
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(ATIVIDADE_REVISAO PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(ATIVIDADE_REVISAO 
            pico_bootrom
            hardware_i2c
            hardware_adc
            hardware_clocks
            hardware_pio
            hardware_timer
            hardware_pwm
        )

pico_add_extra_outputs(ATIVIDADE_REVISAO)

