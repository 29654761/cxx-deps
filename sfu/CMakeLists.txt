# CMakeList.txt: sfu 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
project ("sfu")


# 将源代码添加到此项目的可执行文件。
file(GLOB_RECURSE SRCS *.cpp *.c *.cc)

message("src=${SRCS}")

add_library (${PROJECT_NAME} STATIC ${SRCS})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET sfu PROPERTY CXX_STANDARD 20)
endif()

include("${CXX_DEPS}/cmake/jsoncpp.cmake")
include("${CXX_DEPS}/cmake/srtc.cmake")

message("LIBRTC_INCLUDE=${LIBRTC_INCLUDE}")
message("LIBRTC_LIB=${LIBRTC_LIB}")
# TODO: 如有需要，请添加测试并安装目标。
target_include_directories(${PROJECT_NAME} PUBLIC
${LIBRTC_INCLUDE}
)


target_link_libraries(${PROJECT_NAME} PRIVATE
    sys2
    asio
    hypertext
    http
    rtpx
    ${LIBRTC_LIB}
)