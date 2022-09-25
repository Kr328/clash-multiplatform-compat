project(clash-multiplatform-compat C)

cmake_minimum_required(VERSION 3.10)

if (NOT ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "GNU"))
    message(FATAL_ERROR "Support GCC or Clang only, current ${CMAKE_C_COMPILER_ID}")
endif()

if (NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(C_FLAGS "${C_FLAGS} -O3 -fvisibility=hidden")
    set(LINKER_FLAGS "${LINKER_FLAGS} -Wl,-exclude-libs,ALL -Wl,--gc-sections")
else ()
    add_definitions(-DDEBUG)
    set(C_FLAGS "${C_FLAGS} -O0")
endif ()

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_CURRENT_LIST_DIR}/external")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION 1)
set(CMAKE_POSITION_INDEPENDENT_CODE 1)
set(CMAKE_SKIP_BUILD_RPATH 1)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${C_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${LINKER_FLAGS}")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${LINKER_FLAGS}")

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
    link_libraries(dwmapi)

    add_definitions("-DWINVER=0x0601" "-D_WIN32_WINNT=0x0601")
    set(PLATFORM_SRCS window_win32.c theme_win32.c process_win32.c)
elseif ("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    find_package(X11 REQUIRED)
    find_package(DBus REQUIRED)

    include_directories("${X11_X11_INCLUDE_PATH}" "${DBUS_INCLUDE_DIRS}")
    link_libraries(${X11_X11_LIB} ${DBUS_LIBRARIES})

    set(PLATFORM_SRCS window_linux.c theme_linux.c)
else()
    message(FATAL_ERROR "Unsupported OS ${CMAKE_SYSTEM_NAME}")
endif()

add_subdirectory(external)
find_package(JNI REQUIRED)

include_directories("${JNI_INCLUDE_DIRS}")
link_libraries("${JAVA_JVM_LIBRARY}" containers)

set(SRCS main.c cleaner.h window.h window.c theme.h theme.c process.h process.c)

add_library(compat SHARED ${SRCS} ${PLATFORM_SRCS})

if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET compat POST_BUILD
            COMMAND ${CMAKE_STRIP} --strip-all --remove-section=.comment "${PROJECT_BINARY_DIR}/libcompat${CMAKE_SHARED_LIBRARY_SUFFIX}")
endif ()