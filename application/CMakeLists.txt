set(RAPLAYER_APPLICATION_DEP_LIBRARIES "")
set(RAPLAYER_APPLICATION_DEP_INCLUDES "")

if(RAPLAYER_BUILD_DEPS)
    configure_file(${PROJECT_SOURCE_DIR}/cmake/build_deps/CMakeLists.txt.in.portaudio ${PROJECT_SOURCE_DIR}/submodules/portaudio/CMakeLists.txt)
    execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" . WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/submodules/portaudio)
    execute_process(COMMAND ${CMAKE_COMMAND} --build . WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/submodules/portaudio)

    find_library(PORTAUDIO_SHARED_LIBRARIES NAMES "portaudio" HINTS ${CMAKE_BINARY_DIR}/install/lib ${CMAKE_BINARY_DIR}/install/lib64
        REQUIRED
        NO_DEFAULT_PATH
        NO_PACKAGE_ROOT_PATH
        NO_CMAKE_PATH
        NO_CMAKE_ENVIRONMENT_PATH
        NO_SYSTEM_ENVIRONMENT_PATH
        NO_CMAKE_SYSTEM_PATH
        NO_CMAKE_INSTALL_PREFIX
        CMAKE_FIND_ROOT_PATH_BOTH)

    list(APPEND RAPLAYER_APPLICATION_DEP_LIBRARIES ${PORTAUDIO_SHARED_LIBRARIES})
    list(APPEND RAPLAYER_APPLICATION_DEP_INCLUDES ${CMAKE_BINARY_DIR}/install/include)
else()
    find_package(Portaudio REQUIRED)
    list(APPEND RAPLAYER_APPLICATION_DEP_LIBRARIES ${PORTAUDIO_LIBRARIES})
    list(APPEND RAPLAYER_APPLICATION_DEP_INCLUDES ${PORTAUDIO_INCLUDE_DIRS})
endif ()

function(make_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE raplayer ${RAPLAYER_APPLICATION_DEP_LIBRARIES})
    target_include_directories(${name} PRIVATE raplayer ${RAPLAYER_APPLICATION_DEP_INCLUDES})
    add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endfunction()

file(GLOB test_sources "${PROJECT_SOURCE_DIR}/application/*.c")
foreach(test_source ${test_sources})
    get_filename_component(source_name ${test_source} NAME_WE)
    make_test(${source_name} ${test_source})
endforeach()
