cmake_minimum_required(VERSION 3.21)
project(DriveStationLinux VERSION 0.5.0 LANGUAGES CXX)

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
)
target_include_directories(drivestation_network PUBLIC src)
target_link_libraries(drivestation_network PUBLIC Qt6::Network)
target_compile_definitions(drivestation_network PUBLIC DRIVESTATION_VERSION="${PROJECT_VERSION}")
target_compile_options(drivestation_network PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

add_library(drivestation_ui STATIC
  src/controller_backend.h src/sdl_controller_backend.cpp
  src/controller_hub.cpp src/controller_hub.h
  src/practice_sequence.cpp src/practice_sequence.h
  src/demo_session.cpp src/demo_session.h
  src/preferences.cpp src/preferences.h
  src/widgets.cpp src/widgets.h
  src/main_window.cpp src/main_window.h
)
target_include_directories(drivestation_ui PUBLIC src)
target_link_libraries(drivestation_ui PUBLIC Qt6::Widgets SDL2::SDL2)
target_compile_definitions(drivestation_ui PUBLIC DRIVESTATION_VERSION="${PROJECT_VERSION}")
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

install(TARGETS drivestation-linux drivestation-network-check RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/io.github.GuilhermeAlencarFRC.DriveStationLinux.desktop
  DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/drivestation-linux.svg
  DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
