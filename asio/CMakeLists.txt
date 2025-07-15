# CMakeList.txt: 顶层 CMake 项目文件，在此处执行全局配置
# 并包含子项目。
#
project ("asio")
set (CMAKE_CXX_STANDARD 17)
add_definitions(-DNOMINMAX)

file(GLOB_RECURSE SRCS *.cpp *.c *.cc)


add_library(${PROJECT_NAME} STATIC ${SRCS})

target_include_directories(${PROJECT_NAME} PUBLIC
    "${CXX_DEPS}/asio"    
)


