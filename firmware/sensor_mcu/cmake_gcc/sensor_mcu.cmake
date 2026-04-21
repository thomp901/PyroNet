####################################################################
# Automatically-generated file. Do not edit!                       #
####################################################################

set(SDK_PATH "/Users/diegosmacbook/.silabs/slt/installs/conan/p/simpleb526998f4a4d/p")
set(COPIED_SDK_PATH "simplicity_sdk_2025.6.2")
set(PKG_PATH "/Users/diegosmacbook/.silabs/slt/installs")

add_library(slc OBJECT
    "../${COPIED_SDK_PATH}/platform/common/src/sl_assert.c"
    "../${COPIED_SDK_PATH}/platform/common/src/sl_core_cortexm.c"
    "../${COPIED_SDK_PATH}/platform/common/src/sl_syscalls.c"
    "../${COPIED_SDK_PATH}/platform/Device/SiliconLabs/EFR32FG28/Source/startup_efr32fg28.c"
    "../${COPIED_SDK_PATH}/platform/Device/SiliconLabs/EFR32FG28/Source/system_efr32fg28.c"
    "../${COPIED_SDK_PATH}/platform/driver/gpio/src/sl_gpio.c"
    "../${COPIED_SDK_PATH}/platform/emlib/src/em_cmu.c"
    "../${COPIED_SDK_PATH}/platform/emlib/src/em_emu.c"
    "../${COPIED_SDK_PATH}/platform/emlib/src/em_gpio.c"
    "../${COPIED_SDK_PATH}/platform/emlib/src/em_msc.c"
    "../${COPIED_SDK_PATH}/platform/emlib/src/em_system.c"
    "../${COPIED_SDK_PATH}/platform/peripheral/src/sl_hal_gpio.c"
    "../${COPIED_SDK_PATH}/platform/service/clock_manager/src/sl_clock_manager.c"
    "../${COPIED_SDK_PATH}/platform/service/clock_manager/src/sl_clock_manager_hal_s2.c"
    "../${COPIED_SDK_PATH}/platform/service/clock_manager/src/sl_clock_manager_init.c"
    "../${COPIED_SDK_PATH}/platform/service/clock_manager/src/sl_clock_manager_init_hal_s2.c"
    "../${COPIED_SDK_PATH}/platform/service/device_init/src/sl_device_init_dcdc_s2.c"
    "../${COPIED_SDK_PATH}/platform/service/device_init/src/sl_device_init_emu_s2.c"
    "../${COPIED_SDK_PATH}/platform/service/device_manager/clocks/sl_device_clock_efr32xg28.c"
    "../${COPIED_SDK_PATH}/platform/service/device_manager/src/sl_device_clock.c"
    "../${COPIED_SDK_PATH}/platform/service/device_manager/src/sl_device_gpio.c"
    "../${COPIED_SDK_PATH}/platform/service/interrupt_manager/src/sl_interrupt_manager_cortexm.c"
    "../${COPIED_SDK_PATH}/platform/service/memory_manager/src/sl_memory_manager_region.c"
    "../${COPIED_SDK_PATH}/platform/service/sl_main/src/sl_main_init.c"
    "../${COPIED_SDK_PATH}/platform/service/sl_main/src/sl_main_init_memory.c"
    "../${COPIED_SDK_PATH}/platform/service/sl_main/src/sl_main_process_action.c"
    "../app.c"
    "../autogen/sl_event_handler.c"
    "../main.c"
)

target_include_directories(slc PUBLIC
   "../config"
   "../autogen"
   "../."
    "../${COPIED_SDK_PATH}/platform/Device/SiliconLabs/EFR32FG28/Include"
    "../${COPIED_SDK_PATH}/platform/service/clock_manager/inc"
    "../${COPIED_SDK_PATH}/platform/service/clock_manager/src"
    "../${COPIED_SDK_PATH}/platform/CMSIS/Core/Include"
    "../${COPIED_SDK_PATH}/platform/common/inc"
    "../${COPIED_SDK_PATH}/platform/service/device_manager/inc"
    "../${COPIED_SDK_PATH}/platform/service/device_init/inc"
    "../${COPIED_SDK_PATH}/platform/emlib/inc"
    "../${COPIED_SDK_PATH}/platform/driver/gpio/inc"
    "../${COPIED_SDK_PATH}/platform/peripheral/inc"
    "../${COPIED_SDK_PATH}/platform/service/interrupt_manager/inc"
    "../${COPIED_SDK_PATH}/platform/service/interrupt_manager/src"
    "../${COPIED_SDK_PATH}/platform/service/interrupt_manager/inc/arm"
    "../${COPIED_SDK_PATH}/platform/service/memory_manager/inc"
    "../${COPIED_SDK_PATH}/platform/service/sl_main/inc"
    "../${COPIED_SDK_PATH}/platform/service/sl_main/src"
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

# BEGIN_SIMPLICITY_STUDIO_METADATA=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=END_SIMPLICITY_STUDIO_METADATA