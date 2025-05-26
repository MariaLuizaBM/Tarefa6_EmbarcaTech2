cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico_w CACHE STRING "Board type")
include(pico_sdk_import.cmake)
set(FREERTOS_KERNEL_PATH "C:/Users/malub/FreeRTOS-Kernel")
include(C:/Users/malub/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

project(Tarefa6 C CXX ASM)
pico_sdk_init()

include_directories(${CMAKE_SOURCE_DIR}/lib)

add_executable(Tarefa6 
        Tarefa6.c 
        lib/ssd1306.c
        )

target_include_directories(Tarefa6 PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(Tarefa6
        pico_stdlib
        hardware_gpio
        hardware_i2c
        hardware_pwm
        FreeRTOS-Kernel 
        FreeRTOS-Kernel-Heap4
        )

pico_set_program_name(Tarefa6 "Tarefa6")
pico_set_program_version(Tarefa6 "0.1")

pico_enable_stdio_uart(Tarefa6 1)
pico_enable_stdio_usb(Tarefa6 1)

pico_add_extra_outputs(Tarefa6)

