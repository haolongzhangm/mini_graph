cmake_minimum_required(VERSION 3.15.2)

project(mini_graph
	VERSION 0.1.0
	DESCRIPTION "A simple graph imp")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# export database to compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCES src/*.cpp)
add_library(mini_graph STATIC ${SOURCES})
target_include_directories(mini_graph PUBLIC include)
install(TARGETS mini_graph DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)

set(GOOGLETEST_VERSION 1.15.2)
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/googletest/googletest)
#enable_testing()
add_subdirectory(test)

# copy compile_commands.json to project root
add_custom_target(copy_compile_commands ALL
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/compile_commands.json ${PROJECT_SOURCE_DIR}
	DEPENDS ${CMAKE_BINARY_DIR}/compile_commands.json
)
