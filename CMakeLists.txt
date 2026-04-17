cmake_minimum_required(VERSION 3.21)

set(PICO_BOARD pico2_w CACHE STRING "Pico board target")
set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/third_party/pico-sdk" CACHE PATH "Path to pico-sdk")
set(PIMORONI_PICO_PATH "${CMAKE_CURRENT_LIST_DIR}/third_party/pimoroni-pico" CACHE PATH "Path to pimoroni-pico")

if(NOT EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
  message(FATAL_ERROR "pico-sdk was not found. Run scripts/clone_deps.sh or pass -DPICO_SDK_PATH=/path/to/pico-sdk.")
endif()

include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")

project(claude_buddy_pico C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT EXISTS "${PIMORONI_PICO_PATH}/pimoroni_pico_import.cmake")
  message(FATAL_ERROR "pimoroni-pico was not found. Run scripts/clone_deps.sh or pass -DPIMORONI_PICO_PATH=/path/to/pimoroni-pico.")
endif()

include("${PIMORONI_PICO_PATH}/pimoroni_pico_import.cmake")

pico_sdk_init()

include("${PIMORONI_PICO_PATH}/common/pimoroni_bus.cmake")

add_subdirectory("${PIMORONI_PICO_PATH}/drivers/button" "${CMAKE_BINARY_DIR}/pimoroni-button")
add_subdirectory("${PIMORONI_PICO_PATH}/drivers/rgbled" "${CMAKE_BINARY_DIR}/pimoroni-rgbled")
add_subdirectory("${PIMORONI_PICO_PATH}/drivers/st7789" "${CMAKE_BINARY_DIR}/pimoroni-st7789")
add_subdirectory("${PIMORONI_PICO_PATH}/libraries/pico_graphics" "${CMAKE_BINARY_DIR}/pimoroni-pico-graphics")
add_subdirectory("${PIMORONI_PICO_PATH}/libraries/pico_display_28" "${CMAKE_BINARY_DIR}/pimoroni-pico-display-28")

if(TARGET pico_graphics)
  target_compile_definitions(pico_graphics PUBLIC M_PI=3.14159265358979323846)
endif()

add_executable(
  claude_buddy_pico
  src/claude_buddy_pico.cpp
  src/buddy.cpp
  src/buddies/axolotl.cpp
  src/buddies/blob.cpp
  src/buddies/cactus.cpp
  src/buddies/capybara.cpp
  src/buddies/cat.cpp
  src/buddies/chonk.cpp
  src/buddies/dragon.cpp
  src/buddies/duck.cpp
  src/buddies/ghost.cpp
  src/buddies/goose.cpp
  src/buddies/mushroom.cpp
  src/buddies/octopus.cpp
  src/buddies/owl.cpp
  src/buddies/penguin.cpp
  src/buddies/rabbit.cpp
  src/buddies/robot.cpp
  src/buddies/snail.cpp
  src/buddies/turtle.cpp
)

target_include_directories(
  claude_buddy_pico
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/src
)

pico_btstack_make_gatt_header(
  claude_buddy_pico
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/src/claude_buddy_pico.gatt
  ${PICO_SDK_PATH}/lib/btstack/src/ble/gatt-service
)

target_link_libraries(
  claude_buddy_pico
  pico_stdlib
  pico_cyw43_arch_none
  pico_btstack_ble
  pico_btstack_cyw43
  pico_btstack_flash_bank
  pico_flash
  pico_btstack_run_loop_async_context
  hardware_adc
  hardware_dma
  hardware_flash
  hardware_pwm
  hardware_spi
  hardware_sync
  hardware_timer
  button
  pico_display_28
  pico_graphics
  rgbled
  st7789
)

pico_enable_stdio_usb(claude_buddy_pico 1)
pico_enable_stdio_uart(claude_buddy_pico 0)
pico_add_extra_outputs(claude_buddy_pico)

add_executable(
  claude_buddy_pico_smoke
  src/smoke_test.cpp
)

target_include_directories(
  claude_buddy_pico_smoke
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(
  claude_buddy_pico_smoke
  pico_stdlib
  pico_cyw43_arch_none
  hardware_adc
  hardware_dma
  hardware_pwm
  hardware_spi
  button
  pico_display_28
  pico_graphics
  rgbled
  st7789
)

pico_enable_stdio_usb(claude_buddy_pico_smoke 1)
pico_enable_stdio_uart(claude_buddy_pico_smoke 0)
pico_add_extra_outputs(claude_buddy_pico_smoke)

add_executable(
  claude_buddy_ble_smoke
  src/claude_buddy_ble_smoke.cpp
)

target_include_directories(
  claude_buddy_ble_smoke
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/src
)

pico_btstack_make_gatt_header(
  claude_buddy_ble_smoke
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/src/claude_buddy_ble_smoke.gatt
  ${PICO_SDK_PATH}/lib/btstack/src/ble/gatt-service
)

target_link_libraries(
  claude_buddy_ble_smoke
  pico_stdlib
  pico_cyw43_arch_none
  pico_btstack_ble
  pico_btstack_cyw43
  pico_btstack_flash_bank
  pico_btstack_run_loop_async_context
  hardware_adc
  hardware_dma
  hardware_pwm
  hardware_spi
  button
  pico_display_28
  pico_graphics
  rgbled
  st7789
)

pico_enable_stdio_usb(claude_buddy_ble_smoke 1)
pico_enable_stdio_uart(claude_buddy_ble_smoke 0)
pico_add_extra_outputs(claude_buddy_ble_smoke)
