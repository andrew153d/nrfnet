cmake_minimum_required(VERSION 3.10)
project(nrfnet)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set the source files
set(SOURCES
    src/config_parser.cc
    src/nerfnet_main.cc
    src/nrftime.cc
    src/primary_radio_interface.cc
    src/radio_interface.cc
    src/secondary_radio_interface.cc
    src/mesh_radio_interface.cc
    src/tunnel_interface.cc
    src/ack_handling_layer.cc
    src/message_fragmentation_layer.cc
)

# Include header directories
include_directories(src)

# Add the executable
add_executable(nrfnet ${SOURCES})

# Link the librf24 shared library
# Assuming librf24.so is in the root directory
target_link_libraries(nrfnet PRIVATE ${CMAKE_SOURCE_DIR}/librf24.so)

# Optionally, set rpath so the executable can find librf24.so at runtime
set_target_properties(nrfnet PROPERTIES
    BUILD_RPATH "$ORIGIN"
)
