add_library(SH1106 SH1106.c)

target_include_directories(SH1106 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}  
)

target_link_libraries(SH1106
    pico_stdlib
    hardware_i2c
    hardware_clocks
    hardware_dma
)