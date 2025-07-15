# CMakeList.txt: 顶层 CMake 项目文件，在此处执行全局配置
# 并包含子项目。
#
project ("sys2")
set (CMAKE_CXX_STANDARD 17)
add_definitions(-DNOMINMAX)

file(GLOB_RECURSE SRCS *.cpp *.c *.cc)


add_library(${PROJECT_NAME} STATIC ${SRCS})


#include("${CXX_DEPS}/cmake/openssl.cmake")


if(MSVC)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Mswsock.lib
    )
endif()

if(STD_FS_EX)
    add_definitions(-D_STD_FS_EX)
    target_link_libraries(${PROJECT_NAME} PRIVATE stdc++fs)
endif()

