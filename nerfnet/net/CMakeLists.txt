################################################################################
#
# net build
#
################################################################################

# nerfnet ######################################################################

add_executable(nerfnet
  nerfnet_main.cc
  radio_interface.cc
  primary_radio_interface.cc
  secondary_radio_interface.cc
  common_radio_interface.cc
  ms_bidirectional_radio_interface.cc
)

target_include_directories(nerfnet PRIVATE
  ${tclap_INCLUDE_DIRS}
)

target_include_directories(nerfnet PUBLIC
  ${PROJECT_SOURCE_DIR}
)

target_link_libraries(nerfnet PUBLIC
  pthread
  rf24
  util
)

# Add the config_parser files to the build
add_library(config_parser
  config_parser.cc
)

target_include_directories(config_parser PUBLIC
  ${PROJECT_SOURCE_DIR}
)

target_link_libraries(nerfnet PRIVATE config_parser)
