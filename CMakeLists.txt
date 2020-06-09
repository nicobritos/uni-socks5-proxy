cmake_minimum_required(VERSION 3.1)
project(PC-2020A-6-TPE.SOCKSV5)

set(CMAKE_C STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -c")
set(SOURCE_FILES
        src/buffer.c
        src/netutils.c
        src/selector.c
        src/stm.c
)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(PC-2020A-6-TPE.SOCKSV5 src/main.c ${SOURCE_FILES})

target_link_libraries(PC-2020A-6-TPE.SOCKSV5 Threads::Threads)