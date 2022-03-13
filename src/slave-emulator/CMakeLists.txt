cmake_minimum_required(VERSION 3.12)

file(GLOB SRC *.cpp)
file(GLOB INC *.h)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/apps/slave-emulator)
add_executable(slave-emulator ${SRC})
target_link_libraries(slave-emulator spider)

add_custom_command(TARGET slave-emulator
    POST_BUILD
    COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/app.json ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
