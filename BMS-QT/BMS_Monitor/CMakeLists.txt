cmake_minimum_required(VERSION 3.19)
project(BMS_Monitor LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets SerialBus)

qt_standard_project_setup()

qt_add_executable(BMS_Monitor
    WIN32 MACOSX_BUNDLE
    main.cpp
    chartwidgets.cpp
    chartwidgets.h
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    canconnection.h
    canconnection.cpp
    bmscanprotocol.h
    bmscanprotocol.cpp
)

target_link_libraries(BMS_Monitor
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::SerialBus
)

target_include_directories(BMS_Monitor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(MSVC)
    target_compile_options(BMS_Monitor PRIVATE /utf-8)
endif()

# Optional offscreen layout regression test; never opens a CAN connection.
option(BMS_BUILD_LAYOUT_TESTS "Build the offscreen dashboard layout test" OFF)
if(BMS_BUILD_LAYOUT_TESTS)
    enable_testing()
    qt_add_executable(BMS_LayoutSmoke tests/layout_smoke.cpp chartwidgets.cpp chartwidgets.h mainwindow.ui)
    set_target_properties(BMS_LayoutSmoke PROPERTIES AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}")
    target_include_directories(BMS_LayoutSmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(BMS_LayoutSmoke PRIVATE Qt::Widgets)
    if(MSVC)
        target_compile_options(BMS_LayoutSmoke PRIVATE /utf-8)
    endif()
    add_test(NAME dashboard_layout COMMAND BMS_LayoutSmoke -platform offscreen)
    add_test(NAME dashboard_layout_hidpi COMMAND BMS_LayoutSmoke -platform offscreen --hidpi)
endif()

install(TARGETS BMS_Monitor
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET BMS_Monitor
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
