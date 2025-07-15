

project ("ffmpeg")
add_definitions(-D__STDC_CONSTANT_MACROS)

file(GLOB_RECURSE SRCS *.cpp *.c *.cc)


set(CMAKE_CXX_STANDARD 17)

add_library(${PROJECT_NAME} STATIC ${SRCS})

include ("${CXX_DEPS}/cmake/ffmpeg.cmake")

#set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)