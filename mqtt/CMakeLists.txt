

project ("mqtt")


file(GLOB_RECURSE SRCS *.cpp *.c *.cc)
add_library (${PROJECT_NAME} STATIC ${SRCS})


include("${CXX_DEPS}/cmake/mqtt-paho.cmake")

target_link_libraries(${PROJECT_NAME} PRIVATE
    sys2
)




