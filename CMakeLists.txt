cmake_minimum_required(VERSION 3.22)
project(Snake C)

add_compile_definitions(_POSIX_C_SOURCE=200809L)

add_library(common STATIC
  common/protocol.c
  common/unixsock.c
  common/util.c
)
target_compile_options(common PRIVATE "-std=c11" "-Wall" "-Wextra" "-Wpedantic")
target_include_directories(common PUBLIC .)

add_executable(server
  server/main.c
  server/world.c
  server/snake.c
)
target_link_libraries(server PRIVATE common pthread)

add_executable(client
  client/main.c
  client/ui.c
)
target_link_libraries(client PRIVATE common pthread ncurses)
