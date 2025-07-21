

project ("http")

option(HYPERTEXT_CURL "Build with curl" ON)

message("HTTP module for curl is ${HYPERTEXT_CURL}")


file(GLOB_RECURSE SRCS *.cpp *.c *.cc)

add_library (${PROJECT_NAME} STATIC ${SRCS})

if(HYPERTEXT_CURL)
    include("${CXX_DEPS}/cmake/curl.cmake")
endif()
include("${CXX_DEPS}/cmake/openssl.cmake")

if(HYPERTEXT_CURL)
    target_compile_definitions(${PROJECT_NAME} PUBLIC HYPERTEXT_CURL NOMINMAX)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    sys2
    hypertext
    asio
)

