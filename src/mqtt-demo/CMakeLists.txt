cmake_minimum_required(VERSION 3.12)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/apps/)

execute_process(COMMAND mkdir -p ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
execute_process(COMMAND cp -a ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
execute_process(COMMAND npm --prefix ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mqtt-demo install)
