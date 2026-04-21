####################################################################
# Automatically-generated file. Do not edit!                       #
####################################################################

set(SDK_PATH "/Users/diegosmacbook/.silabs/slt/installs/conan/p/simpleb526998f4a4d/p")
set(COPIED_SDK_PATH "simplicity_sdk_2025.6.2")
set(PKG_PATH "/Users/diegosmacbook/.silabs/slt/installs")

add_library(slc OBJECT
    "${SDK_PATH}/platform/common/src/sl_assert.c"
    "${SDK_PATH}/platform/common/src/sl_core_cortexm.c"
    "${SDK_PATH}/platform/common/src/sl_syscalls.c"
    "${SDK_PATH}/platform/Device/SiliconLabs/EFR32FG28/Source/startup_efr32fg28.c"
    "${SDK_PATH}/platform/Device/SiliconLabs/EFR32FG28/Source/system_efr32fg28.c"
    "${SDK_PATH}/platform/driver/gpio/src/sl_gpio.c"
    "${SDK_PATH}/platform/emlib/src/em_cmu.c"
    "${SDK_PATH}/platform/emlib/src/em_emu.c"
    "${SDK_PATH}/platform/emlib/src/em_gpio.c"
    "${SDK_PATH}/platform/emlib/src/em_msc.c"
    "${SDK_PATH}/platform/emlib/src/em_system.c"
    "${SDK_PATH}/platform/peripheral/src/sl_hal_gpio.c"
    "${SDK_PATH}/platform/service/clock_manager/src/sl_clock_manager.c"
    "${SDK_PATH}/platform/service/clock_manager/src/sl_clock_manager_hal_s2.c"
    "${SDK_PATH}/platform/service/clock_manager/src/sl_clock_manager_init.c"
    "${SDK_PATH}/platform/service/clock_manager/src/sl_clock_manager_init_hal_s2.c"
    "${SDK_PATH}/platform/service/device_init/src/sl_device_init_dcdc_s2.c"
    "${SDK_PATH}/platform/service/device_init/src/sl_device_init_emu_s2.c"
    "${SDK_PATH}/platform/service/device_manager/clocks/sl_device_clock_efr32xg28.c"
    "${SDK_PATH}/platform/service/device_manager/src/sl_device_clock.c"
    "${SDK_PATH}/platform/service/device_manager/src/sl_device_gpio.c"
    "${SDK_PATH}/platform/service/interrupt_manager/src/sl_interrupt_manager_cortexm.c"
    "${SDK_PATH}/platform/service/memory_manager/src/sl_memory_manager_region.c"
    "${SDK_PATH}/platform/service/sl_main/src/sl_main_init.c"
    "${SDK_PATH}/platform/service/sl_main/src/sl_main_init_memory.c"
    "${SDK_PATH}/platform/service/sl_main/src/sl_main_process_action.c"
    "../app.c"
    "../autogen/sl_event_handler.c"
    "../main.c"
)

target_include_directories(slc PUBLIC
   "../config"
   "../autogen"
   "../."
    "${SDK_PATH}/platform/Device/SiliconLabs/EFR32FG28/Include"
    "${SDK_PATH}/platform/service/clock_manager/inc"
    "${SDK_PATH}/platform/service/clock_manager/src"
    "${SDK_PATH}/platform/CMSIS/Core/Include"
    "${SDK_PATH}/platform/common/inc"
    "${SDK_PATH}/platform/service/device_manager/inc"
    "${SDK_PATH}/platform/service/device_init/inc"
    "${SDK_PATH}/platform/emlib/inc"
    "${SDK_PATH}/platform/driver/gpio/inc"
    "${SDK_PATH}/platform/peripheral/inc"
    "${SDK_PATH}/platform/service/interrupt_manager/inc"
    "${SDK_PATH}/platform/service/interrupt_manager/src"
    "${SDK_PATH}/platform/service/interrupt_manager/inc/arm"
    "${SDK_PATH}/platform/service/memory_manager/inc"
    "${SDK_PATH}/platform/service/sl_main/inc"
    "${SDK_PATH}/platform/service/sl_main/src"
)

target_compile_definitions(slc PUBLIC
    "DEBUG_EFM=1"
    "EFR32FG28A122F1024GM48=1"
    "SL_CODE_COMPONENT_SYSTEM=system"
    "SL_CODE_COMPONENT_CLOCK_MANAGER=clock_manager"
    "SL_COMPONENT_CATALOG_PRESENT=1"
    "SL_CODE_COMPONENT_GPIO=gpio"
    "SL_CODE_COMPONENT_HAL_COMMON=hal_common"
    "SL_CODE_COMPONENT_HAL_GPIO=hal_gpio"
    "SL_CODE_COMPONENT_INTERRUPT_MANAGER=interrupt_manager"
    "CMSIS_NVIC_VIRTUAL=1"
    "CMSIS_NVIC_VIRTUAL_HEADER_FILE=\"cmsis_nvic_virtual.h\""
    "SL_CODE_COMPONENT_CORE=core"
)

target_link_libraries(slc PUBLIC
    "-Wl,--start-group"
    "gcc"
    "c"
    "m"
    "nosys"
    "-Wl,--end-group"
)
target_compile_options(slc PUBLIC
    $<$<COMPILE_LANGUAGE:C>:-mcpu=cortex-m33>
    $<$<COMPILE_LANGUAGE:C>:-mthumb>
    $<$<COMPILE_LANGUAGE:C>:-mfpu=fpv5-sp-d16>
    $<$<COMPILE_LANGUAGE:C>:-mfloat-abi=hard>
    $<$<COMPILE_LANGUAGE:C>:-mcmse>
    $<$<COMPILE_LANGUAGE:C>:-Wall>
    $<$<COMPILE_LANGUAGE:C>:-Wextra>
    $<$<COMPILE_LANGUAGE:C>:-Os>
    $<$<COMPILE_LANGUAGE:C>:-fdata-sections>
    $<$<COMPILE_LANGUAGE:C>:-ffunction-sections>
    $<$<COMPILE_LANGUAGE:C>:-fomit-frame-pointer>
    $<$<COMPILE_LANGUAGE:C>:-g>
    $<$<COMPILE_LANGUAGE:C>:-fno-lto>
    $<$<COMPILE_LANGUAGE:C>:--specs=nano.specs>
    $<$<COMPILE_LANGUAGE:CXX>:-mcpu=cortex-m33>
    $<$<COMPILE_LANGUAGE:CXX>:-mthumb>
    $<$<COMPILE_LANGUAGE:CXX>:-mfpu=fpv5-sp-d16>
    $<$<COMPILE_LANGUAGE:CXX>:-mfloat-abi=hard>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-mcmse>
    $<$<COMPILE_LANGUAGE:CXX>:-Wall>
    $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
    $<$<COMPILE_LANGUAGE:CXX>:-Os>
    $<$<COMPILE_LANGUAGE:CXX>:-fdata-sections>
    $<$<COMPILE_LANGUAGE:CXX>:-ffunction-sections>
    $<$<COMPILE_LANGUAGE:CXX>:-fomit-frame-pointer>
    $<$<COMPILE_LANGUAGE:CXX>:-g>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-lto>
    $<$<COMPILE_LANGUAGE:CXX>:--specs=nano.specs>
    $<$<COMPILE_LANGUAGE:ASM>:-mcpu=cortex-m33>
    $<$<COMPILE_LANGUAGE:ASM>:-mthumb>
    $<$<COMPILE_LANGUAGE:ASM>:-mfpu=fpv5-sp-d16>
    $<$<COMPILE_LANGUAGE:ASM>:-mfloat-abi=hard>
    "$<$<COMPILE_LANGUAGE:ASM>:SHELL:-x assembler-with-cpp>"
)

set(post_build_command )
set_property(TARGET slc PROPERTY C_STANDARD 17)
set_property(TARGET slc PROPERTY CXX_STANDARD 17)
set_property(TARGET slc PROPERTY CXX_EXTENSIONS OFF)

target_link_options(slc INTERFACE
    -mcpu=cortex-m33
    -mthumb
    -mfpu=fpv5-sp-d16
    -mfloat-abi=hard
    -T${CMAKE_CURRENT_LIST_DIR}/../autogen/linkerfile.ld
    --specs=nano.specs
    "SHELL:-Xlinker -Map=$<TARGET_FILE_DIR:sensor_mcu>/sensor_mcu.map"
    -fno-lto
    -Wl,--gc-sections
)

# BEGIN_SIMPLICITY_STUDIO_METADATA=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END_SIMPLICITY_STUDIO_METADATA