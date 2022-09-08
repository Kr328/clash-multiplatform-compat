project(clash-multiplatform-compat C)

cmake_minimum_required(VERSION 3.23)

if (${CMAKE_CROSSCOMPILING})
    message(FATAL_ERROR "Crosscompiling unsupported")
endif()

if (NOT ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "GNU"))
    message(FATAL_ERROR "Support GCC or Clang only, current ${CMAKE_C_COMPILER_ID}")
endif()

if (${CMAKE_SIZEOF_VOID_P} EQUAL 8)
    if (NOT ${IS_64})
        set(C_FLAGS "${C_FLAGS} -m32")
    endif()
elseif(${CMAKE_SIZEOF_VOID_P} EQUAL 4 AND ${IS_64})
    message(FATAL_ERROR "Only can compile 64-bit library by 64-bit compliler")
endif()

if(NOT (("${CMAKE_SIZEOF_VOID_P}" EQUAL 8 AND ${IS_64}) OR "${CMAKE_SIZEOF_VOID_P}" EQUAL 4 AND ${IS_64}))
    message(FATAL_ERROR "Unsupported arch")
endif()

if (NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(C_FLAGS "${C_FLAGS} -O3 -fvisibility=hidden -fvisibility-inlines-hidden")
    set(LINKER_FLAGS "${LINKER_FLAGS} -Wl,-exclude-libs,ALL -Wl,--gc-sections")
else ()
    add_definitions(-DDEBUG)
    set(C_FLAGS "${C_FLAGS} -O0")
endif ()

if (NOT EXISTS ${JDK_PATH})
    message(FATAL_ERROR "JDK ${JDK_PATH} invalid")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE 1)

include_directories("${JDK_PATH}/include")

set(SRCS main.c window.h window.c theme.h theme.c)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    include_directories("${JDK_PATH}/include/win32")
    add_definitions("-DWINVER=0x0601" "-D_WIN32_WINNT=0x0601")
    link_libraries(dwmapi)
    set(PLATFORM_SRCS window_win32.c theme_win32.c)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    include_directories("${JDK_PATH}/include/linux")
    set(PLATFORM_SRCS window_linux.c)
else()
    message(FATAL_ERROR "Unsupported OS ${CMAKE_SYSTEM_NAME}")
endif()

link_directories("${JDK_PATH}/lib")

link_libraries(jvm)

add_library(compat SHARED ${SRCS} ${PLATFORM_SRCS})
