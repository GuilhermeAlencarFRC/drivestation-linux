cmake_minimum_required(VERSION 3.21)
project(DriveStationLinux VERSION 0.3.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)
include(CTest)
include(GNUInstallDirs)

add_library(drivestation_ui STATIC
  src/practice_sequence.cpp src/practice_sequence.h
  src/demo_session.cpp src/demo_session.h
  src/preferences.cpp src/preferences.h
  src/widgets.cpp src/widgets.h
  src/main_window.cpp src/main_window.h
)
target_include_directories(drivestation_ui PUBLIC src)
target_link_libraries(drivestation_ui PUBLIC Qt6::Widgets)
target_compile_definitions(drivestation_ui PUBLIC DRIVESTATION_VERSION="${PROJECT_VERSION}")
target_compile_options(drivestation_ui PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

add_executable(drivestation-linux src/main.cpp)
target_link_libraries(drivestation-linux PRIVATE drivestation_ui)

if(BUILD_TESTING)
  find_package(Qt6 REQUIRED COMPONENTS Test)
  add_executable(drivestation_tests tests/test_drivestation.cpp)
  target_link_libraries(drivestation_tests PRIVATE drivestation_ui Qt6::Test)
  add_test(NAME drivestation COMMAND drivestation_tests)
  set_tests_properties(drivestation PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_executable(practice_tests tests/test_practice.cpp)
  target_link_libraries(practice_tests PRIVATE drivestation_ui Qt6::Test)
  add_test(NAME practice COMMAND practice_tests)
  set_tests_properties(practice PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

install(TARGETS drivestation-linux RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/io.github.GuilhermeAlencarFRC.DriveStationLinux.desktop
  DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/drivestation-linux.svg
  DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
