cmake_minimum_required(VERSION 3.21)
project(DriveStationLinux VERSION 0.10.0 LANGUAGES CXX)
set(DRIVESTATION_RELEASE_VERSION "${PROJECT_VERSION}-alpha.1")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Network)
find_package(SDL2 2.24 REQUIRED CONFIG)
include(CTest)
include(GNUInstallDirs)

add_library(drivestation_network STATIC
  src/robot_protocol.cpp src/robot_protocol.h
  src/robot_connection.cpp src/robot_connection.h
  src/udp_robot_link.cpp src/udp_robot_link.h
  src/clock_sync.cpp src/clock_sync.h
  src/robot_discovery_protocol.cpp src/robot_discovery_protocol.h
  src/robot_discovery.cpp src/robot_discovery.h
  src/robot_tcp_protocol.cpp src/robot_tcp_protocol.h
  src/tcp_robot_link.cpp src/tcp_robot_link.h
  src/telemetry_history.cpp src/telemetry_history.h
)
target_include_directories(drivestation_network PUBLIC src)
target_link_libraries(drivestation_network PUBLIC Qt6::Network)
target_compile_definitions(drivestation_network PUBLIC DRIVESTATION_VERSION="${DRIVESTATION_RELEASE_VERSION}")
target_compile_options(drivestation_network PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

add_library(drivestation_ui STATIC
  src/controller_backend.h src/sdl_controller_backend.cpp
  src/controller_hub.cpp src/controller_hub.h
  src/network_session.cpp src/network_session.h
  src/practice_sequence.cpp src/practice_sequence.h
  src/demo_session.cpp src/demo_session.h
  src/preferences.cpp src/preferences.h
  src/widgets.cpp src/widgets.h
  src/main_window.cpp src/main_window.h
)
target_include_directories(drivestation_ui PUBLIC src)
target_link_libraries(drivestation_ui PUBLIC Qt6::Widgets SDL2::SDL2 drivestation_network)
target_compile_definitions(drivestation_ui PUBLIC DRIVESTATION_VERSION="${DRIVESTATION_RELEASE_VERSION}")
target_compile_options(drivestation_ui PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

add_executable(drivestation-linux src/main.cpp)
target_link_libraries(drivestation-linux PRIVATE drivestation_ui)

add_executable(drivestation-network-check src/network_check.cpp)
target_link_libraries(drivestation-network-check PRIVATE drivestation_network)

if(BUILD_TESTING)
  find_package(Qt6 REQUIRED COMPONENTS Test)
  add_executable(network_tests tests/test_network.cpp)
  target_link_libraries(network_tests PRIVATE drivestation_network Qt6::Test)
  add_test(NAME network COMMAND network_tests)
  add_test(NAME network_check COMMAND drivestation-network-check --json)
  set_tests_properties(network network_check PROPERTIES TIMEOUT 30)
  add_executable(discovery_tests tests/test_discovery.cpp)
  target_link_libraries(discovery_tests PRIVATE drivestation_network Qt6::Test)
  add_test(NAME discovery COMMAND discovery_tests)
  set_tests_properties(discovery PROPERTIES TIMEOUT 30)
  add_executable(clock_tests tests/test_clock.cpp)
  target_link_libraries(clock_tests PRIVATE drivestation_network Qt6::Test)
  add_test(NAME clock COMMAND clock_tests)
  set_tests_properties(clock PROPERTIES TIMEOUT 30)
  add_executable(tcp_tests tests/test_tcp.cpp)
  target_link_libraries(tcp_tests PRIVATE drivestation_network Qt6::Test)
  add_test(NAME tcp COMMAND tcp_tests)
  set_tests_properties(tcp PROPERTIES TIMEOUT 30)
  add_executable(network_ui_tests tests/test_network_ui.cpp)
  target_link_libraries(network_ui_tests PRIVATE drivestation_ui Qt6::Test)
  add_test(NAME network_ui COMMAND network_ui_tests)
  set_tests_properties(network_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
  add_executable(telemetry_tests tests/test_telemetry.cpp)
  target_link_libraries(telemetry_tests PRIVATE drivestation_ui Qt6::Test)
  add_test(NAME telemetry COMMAND telemetry_tests)
  set_tests_properties(telemetry PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)
  add_executable(drivestation_tests tests/test_drivestation.cpp)
  target_link_libraries(drivestation_tests PRIVATE drivestation_ui Qt6::Test)
  add_test(NAME drivestation COMMAND drivestation_tests)
  set_tests_properties(drivestation PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_executable(controller_tests tests/test_controllers.cpp)
  target_link_libraries(controller_tests PRIVATE drivestation_ui Qt6::Test)
  add_test(NAME controllers COMMAND controller_tests)
  set_tests_properties(controllers PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_executable(practice_tests tests/test_practice.cpp)
  target_link_libraries(practice_tests PRIVATE drivestation_ui Qt6::Test)
  add_test(NAME practice COMMAND practice_tests)
  set_tests_properties(practice PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

install(TARGETS drivestation-linux drivestation-network-check
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)
install(FILES packaging/io.github.GuilhermeAlencarFRC.DriveStationLinux.desktop
  DESTINATION ${CMAKE_INSTALL_DATADIR}/applications COMPONENT Runtime)
install(FILES packaging/drivestation-linux.svg
  DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps COMPONENT Runtime)

# The downloadable alpha contains only installed artifacts, never the source checkout or config.
execute_process(COMMAND getconf GNU_LIBC_VERSION OUTPUT_VARIABLE DRIVESTATION_BUILD_LIBC
  OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
configure_file(packaging/LEIA-ME-ALFA.txt.in LEIA-ME-ALFA.txt @ONLY)
configure_file(packaging/build-info.json.in build-info.json @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/LEIA-ME-ALFA.txt"
  "${CMAKE_CURRENT_BINARY_DIR}/build-info.json" DESTINATION .
  COMPONENT AlphaArchive EXCLUDE_FROM_ALL)
install(PROGRAMS packaging/INICIAR-ALFA.sh DESTINATION . COMPONENT AlphaArchive EXCLUDE_FROM_ALL)
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT Runtime)
set(CPACK_GENERATOR TGZ)
set(CPACK_SOURCE_GENERATOR "")
set(CPACK_PACKAGE_NAME drivestation-linux)
set(CPACK_PACKAGE_VENDOR "DriveStation Linux — projeto independente")
set(CPACK_PACKAGE_VERSION "${DRIVESTATION_RELEASE_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "ALFA — VERSAO DE TESTES — DriveStation Linux")
set(CPACK_PACKAGE_FILE_NAME
  "drivestation-linux-${DRIVESTATION_RELEASE_VERSION}-linux-${CMAKE_SYSTEM_PROCESSOR}-TESTES")
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_PACKAGING_INSTALL_PREFIX "/")
set(CPACK_COMPONENTS_ALL Runtime AlphaArchive)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_ARCHIVE_UID 0)
set(CPACK_ARCHIVE_GID 0)
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
set(CPACK_COMPONENT_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
include(CPack)
