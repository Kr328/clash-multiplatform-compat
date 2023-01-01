project(clash-multiplatform-compat CXX)

cmake_minimum_required(VERSION 3.10)

if (NOT ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))
    message(FATAL_ERROR "Support GCC or Clang only, current ${CMAKE_CXX_COMPILER_ID}")
endif()

if (NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(C_FLAGS "${C_FLAGS} -O3 -fvisibility=hidden -fvisibility-inlines-hidden -fdata-sections -ffunction-sections -fno-rtti -fno-threadsafe-statics -Werror=format")
    set(LINKER_FLAGS "${LINKER_FLAGS} -Wl,--gc-sections")
else ()
    add_definitions(-DDEBUG)
    set(C_FLAGS "${C_FLAGS} -O0")
endif ()

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_CURRENT_LIST_DIR}/external")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION 1)
set(CMAKE_POSITION_INDEPENDENT_CODE 1)
set(CMAKE_SKIP_BUILD_RPATH 1)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${C_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${LINKER_FLAGS}")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${LINKER_FLAGS}")

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
    link_libraries(dwmapi)

    add_definitions("-DWINVER=0x0601" "-D_WIN32_WINNT=0x0601")

    set(PLATFORM_SRCS window_win32.cpp theme_win32.cpp process_win32.cpp os_win32.cpp shell_win32.cpp)
elseif ("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    find_package(X11 REQUIRED)
    find_package(DBus REQUIRED)

    include_directories("${X11_X11_INCLUDE_PATH}" "${DBUS_INCLUDE_DIRS}")
    link_libraries("${X11_X11_LIB}" "${DBUS_LIBRARIES}")
    add_definitions(-D_GNU_SOURCE)

    set(PLATFORM_SRCS window_linux.cpp theme_linux.cpp process_linux.cpp os_linux.cpp shell_linux.cpp)
else()
    message(FATAL_ERROR "Unsupported OS ${CMAKE_SYSTEM_NAME}")
endif()

find_package(JNI REQUIRED)
include_directories("${JNI_INCLUDE_DIRS}")
link_libraries("${JAVA_JVM_LIBRARY}")
link_libraries(-static-libstdc++)

set(SRCS main.cpp os.hpp jniutils.hpp jniutils.cpp window.hpp window.cpp theme.hpp theme.cpp process.hpp process.cpp shell.hpp shell.cpp)

add_library(compat SHARED ${SRCS} ${PLATFORM_SRCS})

if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET compat POST_BUILD
            COMMAND ${CMAKE_STRIP} --strip-all --remove-section=.comment "${PROJECT_BINARY_DIR}/libcompat${CMAKE_SHARED_LIBRARY_SUFFIX}")
endif ()
