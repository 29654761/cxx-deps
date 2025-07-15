# CMakeList.txt: OpalTest 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
project ("voip")

add_definitions(-DNTDDI_VERSION=0x5020000)
#add_definitions(-DP_STD_ATOMIC)



# 将源代码添加到此项目的可执行文件。
file(GLOB_RECURSE SRCS *.cpp *.c *.cc)

add_library (${PROJECT_NAME} ${SRCS})

#set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")


#if (CMAKE_VERSION VERSION_GREATER 3.12)
#  set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
#endif()

# TODO: 如有需要，请添加测试并安装目标。
include("${CXX_DEPS}/cmake/opal.cmake")




target_link_libraries(${PROJECT_NAME} PRIVATE
    sys2
)

if(NOT MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE
        -static-libgcc
        -static-libstdc++
    )

else()


endif()

set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
