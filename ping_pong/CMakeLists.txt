# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD pico)  # Altere para o board correto se necessário

include(pico_sdk_import.cmake)
project(ping_pong C CXX ASM)

pico_sdk_init()

# Adiciona o executável e os arquivos fonte
add_executable(ping_pong 
    main.c         # Código principal do jogo
    lib/ssd1306.c  lib/ssd1306.h lib/font.h
)

pico_set_program_name(ping_pong "Ping_Pong")
pico_set_program_version(ping_pong "1.0")

pico_enable_stdio_uart(ping_pong 1)
pico_enable_stdio_usb(ping_pong 1)

# Link com as bibliotecas do Pico SDK necessárias
target_link_libraries(ping_pong pico_stdlib hardware_adc hardware_i2c)

# Inclui os diretórios necessários
target_include_directories(ping_pong PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/lib)

pico_add_extra_outputs(ping_pong)
