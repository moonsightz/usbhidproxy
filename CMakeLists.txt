cmake_minimum_required(VERSION 3.12)

set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/pico-sdk")
set(PICO_TINYUSB_PATH "${CMAKE_CURRENT_LIST_DIR}/tinyusb")
set(PICO_PIO_USB_PATH "${CMAKE_CURRENT_LIST_DIR}/Pico-PIO-USB")
set(PICO_BOARD_HEADER_DIRS "${CMAKE_CURRENT_LIST_DIR}/board_include")


include(pico-sdk/external/pico_sdk_import.cmake)

project(usbhidproxy C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (PICO_SDK_VERSION_STRING VERSION_LESS "2.0.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 2.0.0 (or later) required. [${PICO_SDK_PATH} : ${PICO_SDK_VERSION_STRING}]")
endif()

if (NOT DEFINED PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS)
  set(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS 3000)
endif()

pico_sdk_init()


add_compile_options(-Wall -Wno-format -Wno-unused-function)
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-Wno-maybe-uninitailzed)
endif()

set(FAMILY rp2040)
set(BOARD pico_sdk)

set(PICO_BOARD picossci_usbhost)

set(target_name usbhidproxy)

add_executable(${target_name})

set(srcdir ${CMAKE_CURRENT_LIST_DIR}/src)
set(incdir ${CMAKE_CURRENT_LIST_DIR}/include)

target_sources(${target_name} PUBLIC
  ${srcdir}/usbhidproxy.c
  ${srcdir}/debug_func.c
)

target_include_directories(${target_name} PUBLIC ${incdir})

target_link_options(${target_name} PRIVATE -Xlinker --print-memory-usage)
target_compile_options(${target_name} PRIVATE -Wall -Wextra)

target_compile_definitions(${target_name} PRIVATE PIO_USB_USE_TINYUSB)

target_link_libraries(${target_name} PUBLIC pico_stdlib pico_unique_id tinyusb_pico_pio_usb tinyusb_device tinyusb_host tinyusb_board)

pico_add_extra_outputs(${target_name})

