cmake_minimum_required(VERSION 3.10)
project(raplayer VERSION 2.2.0 LANGUAGES C)

include(GNUInstallDirs)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_C_STANDARD 11)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -O2")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -O0 -g")

option(BUILD_SHARED_LIBS "Build using shared libraries." ON)
option(RAPLAYER_BUILD_DEPS "Try to build core dependencies." ON)
option(RAPLAYER_DISABLE_APPLICATION "Disable to build raplayer cli application." OFF)
option(RAPLAYER_INSTALL_PKGCONFIG "install pkg-config module." ON)

set(RAPLAYER_LIBRARIES "")
set(RAPLAYER_INCLUDES "")

if (CMAKE_BUILD_TYPE MATCHES Debug)
    if (CMAKE_C_COMPILER_ID MATCHES "(GNU|Clang)" OR
            CMAKE_CXX_COMPILER_ID MATCHES "(GNU|Clang)")
        include(CodeCoverage)
        APPEND_COVERAGE_COMPILER_FLAGS()
        setup_target_for_coverage_gcovr_sonarqube(NAME coverage EXECUTABLE ctest -j2 --output-on-failure --force-new-ctest-process --progress)
    endif ()
    set(RAPLAYER_ENABLE_DEBUG ON)
endif ()

if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    find_library(CoreServices.framework CoreServices)
    find_library(CoreFoundation.framework CoreFoundation)
    find_library(AudioUnit.framework AudioUnit)
    find_library(AudioToolbox.framework AudioToolbox)
    find_library(CoreAudio.framework CoreAudio)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Android")
    file(TO_CMAKE_PATH "${CMAKE_MAKE_PROGRAM}" CMAKE_MAKE_PROGRAM)
    set(CMAKE_C_LIBRARY_ARCHITECTURE ${ANDROID_TOOLCHAIN_NAME})
    set(RAPLAYER_CMAKE_ARGS
            -DANDROID_ABI:STRING=${ANDROID_ABI}
            -DANDROID_NATIVE_API_LEVEL:STRING=${ANDROID_NATIVE_API_LEVEL}
            -DANDROID_NDK:STRING=${ANDROID_NDK}
            -DANDROID_PLATFORM:STRING=${ANDROID_PLATFORM}
            -DANDROID_STL:STRING=${ANDROID_STL}
            -DANDROID_TOOLCHAIN:STRING=${ANDROID_TOOLCHAIN}
            -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}
            -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
            -DCMAKE_C_COMPILER:STRING=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER:STRING=${CMAKE_CXX_COMPILER}
            -DCMAKE_GENERATOR:STRING=${CMAKE_GENERATOR}
            -DCMAKE_MAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}
            -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE}
            -DOPUS_DISABLE_INTRINSICS:BOOL=ON)
endif()

if(RAPLAYER_BUILD_DEPS)
    configure_file(cmake/build_deps/CMakeLists.txt.in.opus ${CMAKE_CURRENT_SOURCE_DIR}/submodules/libopus/CMakeLists.txt)
    execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" . WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/submodules/libopus)
    execute_process(COMMAND ${CMAKE_COMMAND} --build . WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/submodules/libopus)

    find_library(OPUS_BUILDED_LIBRARIES NAMES "opus" HINTS ${CMAKE_CURRENT_BINARY_DIR}/install/lib ${CMAKE_CURRENT_BINARY_DIR}/install/lib64
            REQUIRED
            NO_DEFAULT_PATH
            NO_PACKAGE_ROOT_PATH
            NO_CMAKE_PATH
            NO_CMAKE_ENVIRONMENT_PATH
            NO_SYSTEM_ENVIRONMENT_PATH
            NO_CMAKE_SYSTEM_PATH
            NO_CMAKE_INSTALL_PREFIX
            CMAKE_FIND_ROOT_PATH_BOTH)

    list(APPEND RAPLAYER_LIBRARIES ${OPUS_BUILDED_LIBRARIES})
    list(APPEND RAPLAYER_INCLUDES ${CMAKE_CURRENT_BINARY_DIR}/install/include)
else()
    find_package(LibOpus REQUIRED)

    list(APPEND RAPLAYER_LIBRARIES ${OPUS_LIBRARIES})
    list(APPEND RAPLAYER_INCLUDES ${OPUS_INCLUDE_DIRS})
endif()

FILE(GLOB_RECURSE RAPLAYER_SOURCES "src/*.c")
if(BUILD_SHARED_LIBS)
add_library(raplayer SHARED ${RAPLAYER_SOURCES})
else()
add_library(raplayer STATIC ${RAPLAYER_SOURCES})
endif()

configure_file(${PROJECT_SOURCE_DIR}/include/raplayer/config.h.in ${PROJECT_SOURCE_DIR}/include/raplayer/config.h)
FILE(GLOB_RECURSE RAPLAYER_PUBLIC_HEADERS "include/*.h")

list(APPEND RAPLAYER_LIBRARIES m)
list(APPEND RAPLAYER_LIBRARIES dl)
if (NOT CMAKE_SYSTEM_NAME MATCHES "Android")
list(APPEND RAPLAYER_LIBRARIES pthread)
endif()
target_link_libraries(raplayer ${RAPLAYER_LIBRARIES})

set_target_properties(raplayer
        PROPERTIES SOVERSION
        ${PROJECT_VERSION_MAJOR}
        VERSION
        ${CMAKE_PROJECT_VERSION}
        PUBLIC_HEADER
        "${RAPLAYER_PUBLIC_HEADERS}")

target_include_directories(
        raplayer
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/raplayer>
        ${RAPLAYER_INCLUDES})

install(TARGETS raplayer
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/raplayer)

if (RAPLAYER_INSTALL_PKGCONFIG)
    set(prefix ${CMAKE_INSTALL_PREFIX})
    set(exec_prefix ${CMAKE_INSTALL_PREFIX})
    set(libdir ${CMAKE_INSTALL_FULL_LIBDIR})
    set(includedir ${CMAKE_INSTALL_FULL_INCLUDEDIR})
    set(VERSION ${CMAKE_PROJECT_VERSION})

    configure_file(raplayer.pc.in raplayer.pc)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/raplayer.pc
            DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/pkgconfig)
endif ()

if (NOT TARGET uninstall)
    configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake"
            IMMEDIATE @ONLY)

    add_custom_target(uninstall
            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake)
endif ()

if (NOT RAPLAYER_DISABLE_APPLICATION)
    add_subdirectory(application)
endif ()
