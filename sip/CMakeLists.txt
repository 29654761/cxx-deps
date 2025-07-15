

project ("sip")


file(GLOB_RECURSE SRCS *.cpp *.c *.cc)

add_library (${PROJECT_NAME} STATIC ${SRCS})


include("${CXX_DEPS}/cmake/asio.cmake")
include("${CXX_DEPS}/cmake/openssl.cmake")

target_link_libraries(${PROJECT_NAME} PRIVATE
    sys2
    hypertext
)


