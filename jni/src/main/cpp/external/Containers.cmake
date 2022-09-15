FILE(GLOB CONTAINERS_HEADERS Containers/src/include/*.h)
FILE(GLOB CONTAINERS_SOURCES Containers/src/*.c)

add_library(containers STATIC ${CONTAINERS_SOURCES} ${CONTAINERS_HEADERS})
target_include_directories(containers PUBLIC Containers/src/include)