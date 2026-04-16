# Define minimal required version of CMake.
cmake_minimum_required(VERSION "3.25")

# Project definition
project(
	wisun_rcp
	VERSION 1.0
	LANGUAGES C CXX ASM
)

# Include the definition of the slc_wisun_rcp target,
# which contains the content of the SLC project
include(wisun_rcp.cmake)

add_executable(wisun_rcp
    # Add additional sources here
)

target_include_directories(wisun_rcp PUBLIC
    # Add additional include paths here
)

target_compile_definitions(wisun_rcp PUBLIC
    # Add additional macros here
)

target_compile_options(wisun_rcp PUBLIC
    # Set additional compiler flags here
)

target_link_options(wisun_rcp PUBLIC
    # Set additional linker flags here
)

# Link with the content defined in the SLC project
target_link_libraries(wisun_rcp PRIVATE
    slc
)

# Include managed project content if available
include(wisun_rcp_project.cmake OPTIONAL RESULT_VARIABLE managed_project)
if(managed_project)
    message(STATUS "Using managed project content from ${managed_project}")
endif()

# Force the gcc linker command
set_target_properties(wisun_rcp PROPERTIES LINKER_LANGUAGE C)

# Create .bin, .hex and .s37 artifacts after building the project
add_custom_command(TARGET wisun_rcp
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_SREC_CMD} "$<TARGET_FILE:wisun_rcp>" "$<TARGET_FILE_DIR:wisun_rcp>/$<TARGET_FILE_BASE_NAME:wisun_rcp>.s37"
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_IHEX_CMD} "$<TARGET_FILE:wisun_rcp>" "$<TARGET_FILE_DIR:wisun_rcp>/$<TARGET_FILE_BASE_NAME:wisun_rcp>.hex"
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_BIN_CMD}  "$<TARGET_FILE:wisun_rcp>" "$<TARGET_FILE_DIR:wisun_rcp>/$<TARGET_FILE_BASE_NAME:wisun_rcp>.bin" 
)

# Run post-build pipeline to perform additional post-processing
if(post_build_command)
add_custom_command(TARGET wisun_rcp
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
    COMMAND ${post_build_command}
)
endif()