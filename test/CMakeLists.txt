aux_source_directory(. DIR_SRCS)
add_executable(mini_graph_test ${DIR_SRCS})

target_link_libraries(mini_graph_test mini_graph gtest)
