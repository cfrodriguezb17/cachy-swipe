cmake_minimum_required(VERSION 3.25)
project(cachy_swipe VERSION 0.4.0 LANGUAGES CXX)

# KWin's Qt 6 dependencies include both versioned and unversioned Qt helpers.
# Keep those helpers on Qt 6 if Qt 5 was discovered earlier in the CMake run.
set(QT_DEFAULT_MAJOR_VERSION 6)
# KDEInstallDirs6 queries this variable to select the matching Qt plugin path.
set(QT_MAJOR_VERSION 6)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CTest)
option(BUILD_KWIN_PLUGIN "Build the native KWin effect" ON)
option(CACHY_SWIPE_INTEGRATION_TESTING "Enable synthetic gestures in an isolated test build" OFF)

if(BUILD_TESTING)
    add_executable(gesture_test tests/gesture_test.cpp)
    target_include_directories(gesture_test PRIVATE src)
    add_test(NAME gesture_test COMMAND gesture_test)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_FOUND)
        add_test(NAME auto_rebuild_test COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/auto_rebuild_test.py)
    endif()
endif()

if(BUILD_KWIN_PLUGIN)
    find_package(ECM REQUIRED NO_MODULE)
    list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
    include(KDEInstallDirs6)
    include(KDECMakeSettings)
    # KWin discovers effects from loadable modules, even if a caller requested
    # static libraries for other targets in the larger build.
    set(BUILD_SHARED_LIBS ON)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    find_package(Qt6 REQUIRED COMPONENTS Core DBus Quick Widgets)
    # The native switcher bridge uses private symbols checked for KWin 6.8.
    find_package(KWin 6.8...<6.9 REQUIRED)
    set(CMAKE_AUTOMOC ON)
    kcoreaddons_add_plugin(cachyswipe SOURCES src/cachyswipe.cpp
        INSTALL_NAMESPACE "kwin/effects/plugins")
    target_link_libraries(cachyswipe PRIVATE KWin::kwin)
    install(DIRECTORY tabbox/cachy-swipe-grid DESTINATION ${KDE_INSTALL_DATADIR}/kwin/tabbox)
    if(CACHY_SWIPE_INTEGRATION_TESTING)
        target_compile_definitions(cachyswipe PRIVATE CACHY_SWIPE_TESTING)
        add_executable(window_fixture tests/window_fixture.cpp)
        target_link_libraries(window_fixture PRIVATE Qt6::Widgets)
    endif()
endif()
