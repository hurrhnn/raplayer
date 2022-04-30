cmake_minimum_required(VERSION 3.1)
project(raplayer C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS_RELEASE "-O3")

add_library(opus STATIC IMPORTED)
add_library(portaudio STATIC IMPORTED)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    find_library(CoreServices.framework CoreServices)
    find_library(CoreFoundation.framework CoreFoundation)
    find_library(AudioUnit.framework AudioUnit)
    find_library(AudioToolbox.framework AudioToolbox)
    find_library(CoreAudio.framework CoreAudio)
endif()

configure_file(CMakeLists.txt.in.opus ${CMAKE_CURRENT_SOURCE_DIR}/submodules/libopus/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" . WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/submodules/libopus)
execute_process(COMMAND ${CMAKE_COMMAND} --build . WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/submodules/libopus)

configure_file(CMakeLists.txt.in.portaudio ${CMAKE_CURRENT_SOURCE_DIR}/submodules/portaudio/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" . WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/submodules/portaudio)
execute_process(COMMAND ${CMAKE_COMMAND} --build . WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/submodules/portaudio)

if (WIN32)
    set(prefix "")
    set(suffix ".lib")
else ()
    set(prefix "lib")
    set(suffix ".a")
endif ()

include_directories(${CMAKE_CURRENT_BINARY_DIR}/install/include)
find_library(OPUS_LIBRARIES NAMES "${prefix}opus${suffix}" HINTS ${CMAKE_CURRENT_BINARY_DIR}/install/lib ${CMAKE_CURRENT_BINARY_DIR}/install/lib64)
find_library(PORTAUDIO_LIBRARIES NAMES "${prefix}portaudio${suffix}" HINTS ${CMAKE_CURRENT_BINARY_DIR}/install/lib ${CMAKE_CURRENT_BINARY_DIR}/install/lib64)


set_target_properties(opus PROPERTIES IMPORTED_LOCATION ${OPUS_LIBRARIES})
set_target_properties(portaudio PROPERTIES IMPORTED_LOCATION ${PORTAUDIO_LIBRARIES})

add_executable(raplayer src/main.c src/ra_client.c src/ra_server.c src/ra_client.h src/ra_server.h src/chacha20/chacha20.h src/chacha20/chacha20.c src/task_scheduler/task_scheduler.c src/task_scheduler/task_scheduler.h src/task_scheduler/task_queue/task/task.h src/task_dispatcher/task_dispatcher.c src/task_dispatcher/task_dispatcher.h src/task_scheduler/task_queue/task_queue.c src/task_scheduler/task_queue/task_queue.h)
add_dependencies(raplayer opus portaudio)


if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(raplayer opus portaudio m dl pthread ${CoreServices.framework} ${CoreFoundation.framework} ${AudioUnit.framework} ${AudioToolbox.framework} ${CoreAudio.framework})
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    target_link_libraries(raplayer opus portaudio asound m dl pthread)
elseif (CYGWIN)
    target_link_libraries(raplayer opus portaudio rt winmm m dl pthread)
else ()
    target_link_libraries(raplayer opus portaudio m dl pthread)
endif ()
