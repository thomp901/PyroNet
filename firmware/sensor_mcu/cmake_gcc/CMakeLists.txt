# Define minimal required version of CMake.
cmake_minimum_required(VERSION "3.25")

# Project definition
project(
	sensor_mcu
	VERSION 1.0
	LANGUAGES C CXX ASM
)

# Include the definition of the slc_sensor_mcu target,
# which contains the content of the SLC project
include(sensor_mcu.cmake)

target_sources(slc PRIVATE
    ../app/app_sensor_runtime.c
    ../app/app_boundary_tx.c
    ../app/app_host_events.c
    ../app/app_provisioning.c
    ../app/app_time_anchor.c
    ../board_i2c.c
    ../sensor_bus.c
    ../services/air_quality/bsec_service.c
    ../services/air_quality/bsec_state_store.c
    ${SDK_PATH}/platform/emlib/src/em_i2c.c
    ${SDK_PATH}/platform/emlib/src/em_eusart.c
    ../services/particulate/sps30_pm25.c
    ../drivers/bme688/bme688_base.c
    ../drivers/bme688/bme68x_port.c
    ../drivers/sps30/sps30_base.c
    ../drivers/sps30/sps30_port.c
    ../platform/monotonic_time.c
    ../services/risk/pyronet_risk_engine.c
    ../services/risk/pyronet_risk_service.c
    ../third_party/bme68x/bme68x.c
    ../third_party/bsec2/config/bsec_iaq.c
    ../third_party/sps30/sensirion_common.c
    ../third_party/sps30/sensirion_i2c.c
    ../third_party/sps30/sps30_i2c.c
    ../transport/host_proto.c
    ../transport/host_uart.c
    ../transport/host_link.c
)

target_include_directories(slc PUBLIC
    ${SDK_PATH}/platform/emlib/inc
    ..
)

add_executable(sensor_mcu
    ../debug_console.c
    ../fault_handlers.c
)

target_include_directories(sensor_mcu PUBLIC
    # Add additional include paths here
)

target_compile_definitions(sensor_mcu PUBLIC
    # Add additional macros here
)

target_compile_options(sensor_mcu PUBLIC
    # Set additional compiler flags here
)

target_link_options(sensor_mcu PUBLIC
    # Set additional linker flags here
)

# Link with the content defined in the SLC project
target_link_libraries(sensor_mcu PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../third_party/bsec2/lib/libalgobsec.a
    slc
)

# Include managed project content if available
include(sensor_mcu_project.cmake OPTIONAL RESULT_VARIABLE managed_project)
if(managed_project)
    message(STATUS "Using managed project content from ${managed_project}")
endif()

# Force the gcc linker command
set_target_properties(sensor_mcu PROPERTIES LINKER_LANGUAGE C)

# Create .bin, .hex and .s37 artifacts after building the project
add_custom_command(TARGET sensor_mcu
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} --remove-section=.internal_storage ${OBJCOPY_SREC_CMD} "$<TARGET_FILE:sensor_mcu>" "$<TARGET_FILE_DIR:sensor_mcu>/$<TARGET_FILE_BASE_NAME:sensor_mcu>.s37"
    COMMAND ${CMAKE_OBJCOPY} --remove-section=.internal_storage ${OBJCOPY_IHEX_CMD} "$<TARGET_FILE:sensor_mcu>" "$<TARGET_FILE_DIR:sensor_mcu>/$<TARGET_FILE_BASE_NAME:sensor_mcu>.hex"
    COMMAND ${CMAKE_OBJCOPY} --remove-section=.internal_storage ${OBJCOPY_BIN_CMD}  "$<TARGET_FILE:sensor_mcu>" "$<TARGET_FILE_DIR:sensor_mcu>/$<TARGET_FILE_BASE_NAME:sensor_mcu>.bin" 
)

# Run post-build pipeline to perform additional post-processing
if(post_build_command)
add_custom_command(TARGET sensor_mcu
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
    COMMAND ${post_build_command}
)
endif()
