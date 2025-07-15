# CMakeList.txt: voip 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
project ("voip")
add_definitions(-DNOMINMAX)
# 将源代码添加到此项目的可执行文件。
file(GLOB_RECURSE SRCS *.cpp *.c *.cc *.cxx)
add_library (${PROJECT_NAME} ${SRCS})

if(CMAKE_SYSTEM_NAME MATCHES "Android")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-enum-constexpr-conversion")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-enum-constexpr-conversion")
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    if (CMAKE_VERSION VERSION_GREATER 3.12)
      set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 14)
    endif()
else()
    if (CMAKE_VERSION VERSION_GREATER 3.12)
      set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
    endif()
endif()
# TODO: 如有需要，请添加测试并安装目标。
include(${CXX_DEPS}/cmake/opal.cmake)

target_link_libraries(${PROJECT_NAME} PRIVATE
    hypertext
    litertp
)

