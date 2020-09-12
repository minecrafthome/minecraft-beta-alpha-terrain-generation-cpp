add_executable(Main GenTerrain.cpp)
add_custom_command(TARGET Main POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/test.txt
        ${CMAKE_CURRENT_BINARY_DIR})


