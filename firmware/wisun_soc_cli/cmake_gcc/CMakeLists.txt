# Define minimal required version of CMake.
cmake_minimum_required(VERSION "3.25")

# Project definition
project(
	wisun_soc_cli
	VERSION 1.0
	LANGUAGES C CXX ASM
)

# Include the definition of the slc_wisun_soc_cli target,
# which contains the content of the SLC project
include(wisun_soc_cli.cmake)

add_executable(wisun_soc_cli
    # Add additional sources here
)

target_include_directories(wisun_soc_cli PUBLIC
    # Add additional include paths here
)

target_compile_definitions(wisun_soc_cli PUBLIC
    # Add additional macros here
)

target_compile_options(wisun_soc_cli PUBLIC
    # Set additional compiler flags here
)

target_link_options(wisun_soc_cli PUBLIC
    # Set additional linker flags here
)

# Link with the content defined in the SLC project
target_link_libraries(wisun_soc_cli PRIVATE
    slc
)

# Include managed project content if available
include(wisun_soc_cli_project.cmake OPTIONAL RESULT_VARIABLE managed_project)
if(managed_project)
    message(STATUS "Using managed project content from ${managed_project}")
endif()

# Force the gcc linker command
set_target_properties(wisun_soc_cli PROPERTIES LINKER_LANGUAGE C)

# Create .bin, .hex and .s37 artifacts after building the project
add_custom_command(TARGET wisun_soc_cli
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_SREC_CMD} "$<TARGET_FILE:wisun_soc_cli>" "$<TARGET_FILE_DIR:wisun_soc_cli>/$<TARGET_FILE_BASE_NAME:wisun_soc_cli>.s37"
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_IHEX_CMD} "$<TARGET_FILE:wisun_soc_cli>" "$<TARGET_FILE_DIR:wisun_soc_cli>/$<TARGET_FILE_BASE_NAME:wisun_soc_cli>.hex"
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_BIN_CMD}  "$<TARGET_FILE:wisun_soc_cli>" "$<TARGET_FILE_DIR:wisun_soc_cli>/$<TARGET_FILE_BASE_NAME:wisun_soc_cli>.bin" 
)

# Run post-build pipeline to perform additional post-processing
if(post_build_command)
add_custom_command(TARGET wisun_soc_cli
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
    COMMAND ${post_build_command}
)
endif()