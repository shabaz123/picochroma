cmake_minimum_required(VERSION 3.12)
include(pico_sdk_import.cmake)
project(picochroma)

pico_sdk_init()


# rest of your project
add_executable(picochroma
    main.c
)

target_include_directories(picochroma PUBLIC
        )

target_link_libraries(picochroma pico_stdlib hardware_clocks
        hardware_dma hardware_pwm
        )

# enable usb output, disable uart output
pico_enable_stdio_usb(picochroma 1)
pico_enable_stdio_uart(picochroma 0)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(picochroma)
