add_executable(Main GenTerrain.cpp)
add_custom_command(TARGET Main POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/seeds.csv
        ${CMAKE_CURRENT_BINARY_DIR})


add_custom_command(TARGET Main POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/seedtest.csv
        ${CMAKE_CURRENT_BINARY_DIR})