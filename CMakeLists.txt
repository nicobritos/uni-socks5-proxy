cmake_minimum_required(VERSION 3.1)
project(PC-2020A-6-TPE.SOCKSV5)

set(CMAKE_C STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic -Wall -Wextra -Werror -std=c11 -Wno-unused-parameter -D_GNU_SOURCE")
set(SOURCE_FILES
        src/utils/buffer.c
        src/utils/netutils.c
        src/utils/selector.c
        src/utils/stm.c
        src/socks5/socks5nio.c
        src/socks5/message/request.c
        src/socks5/message/parser/hello_parser.c
        src/socks5/message/parser/auth_user_pass_parser.c
        src/socks5/message/parser/request_parser.c
        src/socks5/message/auth_user_pass_helper.c
        src/utils/sorted_hashmap.c
)

add_executable(PC-2020A-6-TPE.SOCKSV5 src/main.c ${SOURCE_FILES})

set_target_properties(PC-2020A-6-TPE.SOCKSV5 PROPERTIES COMPILE_FLAGS -pthread LINK_FLAGS -pthread)