cmake_minimum_required(VERSION 2.8)

file(GLOB SRC *.cpp)
file(GLOB INC *.h)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/apps/slave-modbus)
add_executable(slave-modbus ${SRC})
target_link_libraries(slave-modbus spider-slave modbus)

add_custom_command(TARGET slave-modbus
    POST_BUILD
    COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/app.json ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
